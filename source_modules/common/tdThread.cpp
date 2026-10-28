/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#ifdef _WIN32
  #ifndef _CRT_SECURE_NO_WARNINGS
    #define _CRT_SECURE_NO_WARNINGS
  #endif
#endif
#include "tdThread.h"

//#define DEBUG_THREAD_POOL

bool tdThread::Create(size_t (THREAD_CALL *thread_func)(void*), void* param)
{
    if(m_ThreadId)
        return false;
#ifdef _WIN32
    DWORD  gThreadId = 0;
    HANDLE ret = CreateThread(NULL, 0, (unsigned long(THREAD_CALL*)(void*))thread_func, param, 0, &gThreadId);
#else
    pthread_t *ret = (pthread_t*)malloc(sizeof(pthread_t));
    if(pthread_create(ret, NULL, (void*(*)(void*))thread_func, param) != 0)
    {
        free(ret);
        return NULL;
    }
#endif
    m_ThreadId = (void*)ret;
    return m_ThreadId != NULL;
}

void tdThread::Terminate()
{
    if(!m_ThreadId)
        return;
#ifdef _WIN32
    TerminateThread((HANDLE)m_ThreadId, 0);
#else
    pthread_kill(*((pthread_t*)m_ThreadId), 0);
#endif
    this->Close();
}

void tdThread::Close()
{
    if(m_ThreadId == NULL)
        return;
#ifdef _WIN32
    CloseHandle((HANDLE)m_ThreadId);
#else
    pthread_detach(*((pthread_t*)m_ThreadId));
    free((void*)m_ThreadId);
#endif
    m_ThreadId = NULL;
}

bool tdThread::Wait(uint32_t& exitCode)
{
    if(m_ThreadId == NULL)
        return false;
#ifdef _WIN32
    DWORD _e;
    WaitForSingleObject((HANDLE)m_ThreadId, INFINITE);
    bool ret = GetExitCodeThread((HANDLE)m_ThreadId, &_e) != FALSE;
    exitCode = _e;
    return ret;
#else
    uint32_t *_e = NULL;
    exitCode = 0;
    bool ret = pthread_join(*((pthread_t*)m_ThreadId), (void**)&_e) == 0;
    if(_e)
        exitCode = *_e;
    return ret;
#endif
}


#ifdef DEBUG_THREAD_POOL
#include <tdFile.hpp>
#endif


size_t THREAD_CALL tdThreadPool::static_thread_loop(void* lParam)
{
    bool endThread = false;
    tdThreadPool* _this = (tdThreadPool*)lParam;
    _this->m_spin_Task.lock();
    _this->m_threadsRun ++;
    _this->m_spin_Task.unlock();

#ifdef DEBUG_THREAD_POOL
    size_t idThread = size_t(&lParam);
    size_t timeGet  = 0;
    size_t timeJob  = 0;
    size_t timeFree = 0;
    size_t timeAll  = 0;
    tdTimeCpu cpuGet, cpuJob, cpuFree, cpuAll;
#endif
    while(true)
    {
        TaskInf* task = NULL;
        try
        {
            while(true)
            {
#ifdef DEBUG_THREAD_POOL
                cpuGet.Start();
                task = _this->GetTask();
                timeGet += cpuGet.Delta();
                if(task == NULL)
                {
                    endThread = true;
                    break;
                }
                cpuJob.Start();
                task->m_fn(task->m_param1, task->m_param2, task->m_param3);
                timeJob += cpuJob.Delta();
                cpuFree.Start();
                _this->FreeTask(task);
                timeFree += cpuFree.Delta();
#else
                task = _this->GetTask();
                if(task == NULL)
                {
                    endThread = true;
                    break;
                }
                task->m_fn(task->m_param1, task->m_param2, task->m_param3);
                _this->FreeTask(task);
#endif
            }
        }catch(...){
            tdThreadPool::tdThreadPoolExceptionsFn excpFn = _this->m_exceptionsFn;
            if(excpFn != NULL)
            {
                TaskID taskID;
                taskID.p1 = task;
                taskID.p2 = task->m_ID;
                try{
                    excpFn(taskID, task->m_fn, task->m_param1, task->m_param2, task->m_param3);
                }catch(...){}
            }
            _this->FreeTask(task);
        }
        if(endThread)
            break;
    }
#ifdef DEBUG_THREAD_POOL
    char data[128];
    sprintf(data, "./Thread_%.8X.log", idThread);
    tdFile file;
    file.Open(data, tdFile::crw);
    sprintf(data, "All: %u\nGet: %u\nJob: %u\nFree: %u\n", int(cpuAll.DeltaMs()), int(timeGet/1000), int(timeJob/1000), int(timeFree/1000));
    file.Write(data, strlen(data));
    file.Close();
#endif
    _this->m_spin_Task.lock();
    _this->m_threadsRun --;
    _this->m_spin_Task.unlock();
    return 0;
}


tdThreadPool::tdThreadPool()
{
    m_taskTop    = NULL;
    m_taskBottom = NULL;
    m_taskFree   = NULL;
    m_taskID     = 1;  // инкримент по 2 (валидный id всегда нечетный)
    m_taskCount  = 0;
    m_threadsRun   = 0;
    m_threadsSleep = 0;
    m_spin_Task.Init();

    m_listSyncFree  = 0;
    m_listSyncCount = 0;
    m_listSync      = NULL;

    m_exceptionsFn  = NULL;

    TaskInf* tmpTask = this->GetFreeTask();
    m_nilID.p1 = tmpTask;
    m_nilID.p2 = 2;
    this->FreeTask(tmpTask);
}


tdThreadPool::~tdThreadPool()
{
    Close();
    while(m_taskFree)
    {
        TaskInf* temp = m_taskFree;
        m_taskFree = m_taskFree->m_next;
        delete temp;
    }
    while(m_listSyncFree != m_listSyncCount)//может кто задержался на ожидании
        tdSleep(1);
    while(m_listSync != NULL)
    {
        SyncList* tmp = m_listSync;
        m_listSync = m_listSync->m_next;
        delete tmp;
    }
}


void tdThreadPool::Start(size_t countThread)
{
    if(countThread == 0)
        return;
    if(countThread > 64)
        countThread = 64;
    size_t startT = m_threadsRun;
    m_closeAllThreads = false;
    tdThread tread;
    for(size_t i = 0; i < countThread; i++)
    {
        tread.Create(static_thread_loop, this);
        tread.Close();
    }
    countThread += startT;
    while(m_threadsRun != countThread)//ждем запуска всех тредов
        tdSleep(1);
}


void tdThreadPool::Close()
{
    if(!m_threadsRun)
        return;
    // ожидаем остановки всех тредов
    m_closeAllThreads = true;
    while(m_threadsSleep)
    {
        m_sync_ThreadSleep.unlock();
        tdSleep(1);//переключим контекст (возможны задержки в шедуллере)
    }
    while(m_threadsRun)
        tdSleep(1);
}


void tdThreadPool::FreeTask(tdThreadPool::TaskInf* taskInf)
{
    SyncList* syncList;
    m_spin_Task.lock();
    syncList = taskInf->m_syncList;
    taskInf->m_ID       = 0;
    taskInf->m_syncList = NULL;
    taskInf->m_next     = m_taskFree;
    m_taskFree = taskInf;
    m_spin_Task.unlock();

    while(syncList)
    {
       SyncList* tmp = syncList->m_next;//tmp нельзя убирать (после unlock, SyncList может использоваться снова)
       syncList->m_sync.unlock();
       syncList = tmp;
    }
}


tdThreadPool::TaskInf* tdThreadPool::GetFreeTask()
{
    tdAutoSync<tdSpinLock> autoSpin(m_spin_Task);
    tdThreadPool::TaskInf* ret;
    if(m_taskFree)
    {
        ret = m_taskFree;
        m_taskFree = m_taskFree->m_next;
        return ret;
    }
    ret = new tdThreadPool::TaskInf;
    ret->m_ID       = 0;
    ret->m_syncList = NULL;
    return ret;
}


tdThreadPool::TaskInf* tdThreadPool::GetTask()
{
    TaskInf* task = NULL;
    m_spin_Task.lock();
    while(true)
    {
        if(m_taskBottom)
        {
            TaskInf* prewTask = NULL;
            task = m_taskBottom;
            while(task != NULL)//ищем выполненное
            {
                if(((TaskInf*)task->m_parentTaskID.p1)->m_ID != task->m_parentTaskID.p2)
                    break;
                prewTask = task;
                task = task->m_next;
            }
            if(task == NULL)
                task = m_taskBottom;
            if(m_taskBottom == task)
            {
                m_taskBottom = task->m_next;
                if(m_taskBottom == NULL)
                    m_taskTop = NULL;
            }else{
                prewTask->m_next = task->m_next;
                if(task->m_next == NULL)
                    m_taskTop = prewTask;
            }
            m_taskCount --;
            break;
        }
        if(m_closeAllThreads)
            break;
        //паркуемся
        m_threadsSleep++;
        m_spin_Task.unlock();

        m_sync_ThreadSleep.lock();

        //проснулись
        m_spin_Task.lock();
        m_threadsSleep--;
        if(m_taskCount > 1 && m_threadsSleep)//fix латентности шедуллера (т.к. семафор бинарный)
            m_sync_ThreadSleep.unlock();
    }
    m_spin_Task.unlock();
    if(task)
    {
        if(((TaskInf*)task->m_parentTaskID.p1)->m_ID == task->m_parentTaskID.p2)//дожидаемся завершения родителя
            this->WaitTask(task->m_parentTaskID);
    }
    return task;
}


tdThreadPool::TaskID tdThreadPool::AddTask(const TaskID prewTaskID, tdThreadPoolFn fn, size_t param1, void* param2, void* param3)
{
    TaskID ret;
    if(fn == NULL)
        return m_nilID;

    TaskInf* taskInf  = this->GetFreeTask();
    taskInf->m_fn     = fn;
    taskInf->m_param1 = param1;
    taskInf->m_param2 = param2;
    taskInf->m_param3 = param3;
    taskInf->m_next   = NULL;
    if(prewTaskID.p2 == 0)
    {
        taskInf->m_parentTaskID = m_nilID;
    }else{
        taskInf->m_parentTaskID = prewTaskID;
    }

    ret.p1 = taskInf;

    m_spin_Task.lock();
    if(m_taskTop == NULL)
    {
        m_taskTop = m_taskBottom = taskInf;
    }else{
        m_taskTop->m_next = taskInf;
        m_taskTop = taskInf;
    }
    ret.p2        = m_taskID;
    taskInf->m_ID = m_taskID;
    m_taskID += 2;
    m_taskCount ++;
    m_spin_Task.unlock();

    if(m_threadsSleep)//будим
        m_sync_ThreadSleep.unlock();
    return ret;
}


void tdThreadPool::WaitTask(const tdThreadPool::TaskID taskID)
{
    if(taskID.p2 == 0)
        return;
    TaskInf* taskInf = (TaskInf*)taskID.p1;
    if(taskID.p2 != taskInf->m_ID)
        return;

    SyncList* waitSync;
    m_spin_list.lock();
    if(m_listSync)
    {
        m_listSyncFree--;
        waitSync = m_listSync;
        m_listSync = m_listSync->m_next;
    }else{
        waitSync = new SyncList;
        m_listSyncCount++;
        waitSync->m_sync.lock();
    }
    m_spin_list.unlock();
    //запишем
    m_spin_Task.lock();
    if(taskID.p2 == taskInf->m_ID)
    {
        waitSync->m_next    = taskInf->m_syncList;
        taskInf->m_syncList = waitSync;
    }else{
        waitSync->m_sync.unlock();
    }
    m_spin_Task.unlock();

    waitSync->m_sync.lock();
    //вернем синхру
    m_spin_list.lock();
    waitSync->m_next = m_listSync;
    m_listSync = waitSync;
    m_listSyncFree++;
    m_spin_list.unlock();
}


void tdThreadPool::SetErrorFn(tdThreadPool::tdThreadPoolExceptionsFn fn)
{
    m_exceptionsFn = fn;
}


void tdThreadPool::BindThread()
{
    static_thread_loop(this);
}
