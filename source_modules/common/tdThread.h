/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once
#ifndef _TDTHREAD_H_
#define _TDTHREAD_H_
#include "tdSync.hpp"

#ifdef _WIN32
#define THREAD_CALL __stdcall
#else
#define THREAD_CALL
#endif

class tdThread
{
    void* m_ThreadId;
public:
    tdThread(){ m_ThreadId = (void*)0; }
    ~tdThread(){ this->Close(); }
    bool Create(size_t (THREAD_CALL *thread_func)(void*), void* param); //создать тред
    bool Wait(uint32_t& exitCode);  // ждем завершения треда
    void Terminate(); // прибить тред
    void Close();     // освободить дескриптор (отпустить тред)
};



class tdThreadPool
{
public:
    //дескриптор задания (не требует освобождения ресурсов)
    class TaskID
    {
        friend class tdThreadPool;
    protected:
        const void* p1;
        size_t      p2;
    public:
        TaskID():p2(0){};
    };
    //прототип регистрируемого обработчика задания ()
    typedef void (*tdThreadPoolFn)(size_t param1, void* param2, void* param3);

    //прототип регистрируемого обработчика исключения (общий для всех заданий)
    typedef void (*tdThreadPoolExceptionsFn)(TaskID taskID, tdThreadPoolFn fn, size_t param1, void* param2, void* param3);

private:
    struct SyncList
    {
        SyncList():m_next(NULL){}
        SyncList* m_next;
        tdSync    m_sync;
    };

    struct TaskInf
    {
        TaskInf*  m_next;
        size_t    m_ID;
        SyncList* m_syncList;
        TaskID    m_parentTaskID;

        tdThreadPoolFn m_fn;
        size_t         m_param1;
        void*          m_param2;
        void*          m_param3;

        size_t m_temp[7];
    };

    TaskID m_nilID;

    //список свободных синхронизаций
    SyncList*  m_listSync;
    size_t     m_listSyncFree;
    size_t     m_listSyncCount;
    tdSpinLock m_spin_list;

    tdSpinLock m_spin_Task;  // спинлок на доступ к заданиям
    TaskInf*   m_taskTop;    // конец списка заполненных заданий
    TaskInf*   m_taskBottom; // начало списка заполненных заданий
    TaskInf*   m_taskFree;   // список свободных заданий
    size_t     m_taskID;     // счетчик заданий
    size_t     m_taskCount;

    void     FreeTask(TaskInf* taskInf); //освободить выполненное задание
    TaskInf* GetFreeTask(); //запросить свободное задание
    TaskInf* GetTask();     //запросить заполненное задание

    volatile size_t m_threadsRun;   // сколько тредов запущено
    volatile size_t m_threadsSleep; // сколько тредов спит
    tdSync m_sync_ThreadSleep;      // синхронизация для спячки треда

    bool m_closeAllThreads; //флаг закрытия тредов

    tdThreadPoolExceptionsFn  m_exceptionsFn;

    static size_t THREAD_CALL static_thread_loop(void* lParam);
public:
    tdThreadPool();
    ~tdThreadPool();

    // подключить тред к пулу (управление вернет при закрытии пула)
    void BindThread();

    //зарегистрировать обработчик исключений
    void SetErrorFn(tdThreadPoolExceptionsFn fn);

    //добавить нужное количество тредов
    void Start(size_t countAddThread);

    // прибить все треды (дожидается завершения всех заданий в очереди)
    void Close();

    // добавить задание, и начать выполнение только после завершения родителя
    TaskID AddTask(const TaskID parentTaskID, tdThreadPoolFn fn, size_t param1 = 0, void* param2 = 0, void* param3 = NULL);

    // добавить задание на выполнение
    inline TaskID AddTask(tdThreadPoolFn fn, size_t param1 = 0, void* param2 = 0, void* param3 = NULL)
    {
        return AddTask(m_nilID, fn, param1, param2, param3);
    }

    //дождаться выполнения задания
    void WaitTask(const TaskID taskID);

    // сколько тредов в пуле
    inline size_t CountThread() const { return m_threadsRun; }

    // сколько заданий в очереди ожидания
    inline size_t QueueSize() const { return m_taskCount; }

    // сколько тредов припаркованы (спят)
    inline size_t CountSleepThread() const { return m_threadsSleep; }
};


#endif //_TDTHREAD_H_

