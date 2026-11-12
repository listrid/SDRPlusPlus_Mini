#include <backend.h>
#include <tdSync.hpp>
#include <backend_common.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <utils/flog.h>
#include <utils/opengl_include_code.h>
#include <version.h>
#include <core.h>
// Desktop integration that applies to every Unix-like platform driving GLFW's
// X11/Wayland backends, as opposed to macOS (bundle ICNS) and Windows (ICO
// resource), which identify the application through packaging metadata instead.
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define SDRPP_GLFW_UNIX_DESKTOP 1
#endif

#if defined(SDRPP_GLFW_UNIX_DESKTOP)
#include <stb_image.h>
#endif

namespace backend
{
    const char* OPENGL_VERSIONS_GLSL[] =
    {
        "#version 120",
        "#version 300 es",
        "#version 120"
    };

    const int OPENGL_VERSIONS_MAJOR[]  = { 3, 3, 2 };
    const int OPENGL_VERSIONS_MINOR[]  = { 0, 1, 1 };
    const bool OPENGL_VERSIONS_IS_ES[] = { false, true, false };

    #define OPENGL_VERSION_COUNT (sizeof(OPENGL_VERSIONS_GLSL) / sizeof(char*))

    bool g_maximized = false;
    bool g_fullScreen = false;
    // Defaults match defConfig's windowSize, and stand in when the key is absent.
    int g_winHeight = 720;
    int g_winWidth = 1280;
    bool g_old_maximized = g_maximized;
    int g_fsWidth, g_fsHeight, g_fsPosX, g_fsPosY;
    int g_old_winWidth, g_old_winHeight;
    GLFWwindow* g_window;
    GLFWmonitor* g_monitor;
    common::ScaleState g_scaleState;

    static void glfw_error_callback(int error, const char* description)
    {
        flog::error("Glfw Error {0}: {1}", error, description);
    }

    static void maximized_callback(GLFWwindow* window, int n)
    {
        if (n == GLFW_TRUE)
        {
            g_maximized = true;
        }else{
            g_maximized = false;
        }
    }

    static void onWindowContentScaleChanged(GLFWwindow* w, float xscale, float yscale)
    {
        common::queueContentScale(g_scaleState, xscale);
    }

    void setUserScaleFactor(float newFactor)
    {
        common::setUserScaleFactor(g_scaleState, newFactor);
    }

    int init(std::string resDir)
    {
        // Load config
        {
            auto configAccess = core::configManager.read();
            ConfigManager::ReadSection size = configAccess.section("windowSize");
            size.tryGet("w", g_winWidth);
            size.tryGet("h", g_winHeight);
            configAccess.tryGet("maximized", g_maximized);
            configAccess.tryGet("fullscreen", g_fullScreen);
        }
        float userScaleFactor = common::configUserScaleFactor();

        // Setup window
        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
            return 1;

    #ifdef __APPLE__
        // GL 3.2 + GLSL 150
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac

        // Create window with graphics context
        g_monitor = glfwGetPrimaryMonitor();
        g_window = glfwCreateWindow(g_winWidth, g_winHeight, "SDRPP_MINI (fork SDRIAK) v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")", NULL, NULL);
        if (g_window == NULL)
            return 1;
        glfwMakeContextCurrent(g_window);
    #else
        const char* glsl_version = "#version 120";
        g_monitor = NULL;
        // Both must match the desktop file's basename (and its StartupWMClass)
        // for the shell to bind the window to sdriak.desktop and pick up the
        // hicolor icon. X11 keys off WM_CLASS, Wayland off the xdg app ID.
    #if defined(SDRPP_GLFW_UNIX_DESKTOP) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "sdrpp_mini");
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "sdrpp_mini");
    #endif
    #if defined(SDRPP_GLFW_UNIX_DESKTOP) && (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, "sdrpp_mini");
    #endif
        for (int i = 0; i < OPENGL_VERSION_COUNT; i++)
        {
            glsl_version = OPENGL_VERSIONS_GLSL[i];
            glfwWindowHint(GLFW_CLIENT_API, OPENGL_VERSIONS_IS_ES[i] ? GLFW_OPENGL_ES_API : GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, OPENGL_VERSIONS_MAJOR[i]);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, OPENGL_VERSIONS_MINOR[i]);
            
            // Create window with graphics context
            g_monitor = glfwGetPrimaryMonitor();
            g_window = glfwCreateWindow(g_winWidth, g_winHeight, "SDRPP_MINI v" VERSION_STR " (Built at " __TIME__ ", " __DATE__ ")", NULL, NULL);
            if (g_window == NULL)
            {
                flog::info("OpenGL {0}.{1} {2}was not supported", OPENGL_VERSIONS_MAJOR[i], OPENGL_VERSIONS_MINOR[i], OPENGL_VERSIONS_IS_ES[i] ? "ES " : "");
                continue;
            }
            flog::info("Using OpenGL {0}.{1}{2}", OPENGL_VERSIONS_MAJOR[i], OPENGL_VERSIONS_MINOR[i], OPENGL_VERSIONS_IS_ES[i] ? " ES" : "");
            glfwMakeContextCurrent(g_window);
            break;
        }

    #endif

        // Query the content scale of the display the window actually landed on
        if (g_window)
        {
            float xscale = 1.0f, yscale = 1.0f;
            glfwGetWindowContentScale(g_window, &xscale, &yscale);
            common::initScaleState(g_scaleState, resDir, xscale, userScaleFactor);
        }

#if defined(SDRPP_GLFW_UNIX_DESKTOP)
        // X11 needs _NET_WM_ICON data on the window. Wayland instead
        // identifies the application through its desktop-file ID. Windows uses
        // the embedded ICO resource and macOS uses the application bundle's ICNS.
        const std::string iconPath = resDir + "/icons/sdrpp_mini.png";
        GLFWimage icon = {};
        icon.pixels = stbi_load(iconPath.c_str(), &icon.width, &icon.height, 0, 4);
        if (icon.pixels == nullptr)
        {
            flog::error("Failed to load application icon '{0}'", iconPath);
            return 1;
        }
        glfwSetWindowIcon(g_window, 1, &icon);
        stbi_image_free(icon.pixels);
#endif

        // Add callback for max/min if GLFW supports it
    #if (GLFW_VERSION_MAJOR == 3) && (GLFW_VERSION_MINOR >= 3)
        if (g_maximized)
            glfwMaximizeWindow(g_window);

        glfwSetWindowMaximizeCallback(g_window, maximized_callback);
    #endif

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = NULL;

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(g_window, true);
        glfwSetWindowContentScaleCallback(g_window, onWindowContentScaleChanged);

        if (!ImGui_ImplOpenGL3_Init(glsl_version))
        {
            // If init fail, try to fall back on GLSL 1.2
            flog::warn("Could not init using OpenGL with normal GLSL version, falling back to GLSL 1.2");
            if (!ImGui_ImplOpenGL3_Init("#version 120"))
            {
                flog::error("Failed to initialize OpenGL with GLSL 1.2");
                return -1;
            }
        }

        // Set window size and fullscreen state
        glfwGetWindowSize(g_window, &g_old_winWidth, &g_old_winHeight);
        if (g_fullScreen)
        {
            flog::info("Fullscreen: ON");
            g_fsWidth = g_old_winWidth;
            g_fsHeight = g_old_winHeight;
            glfwGetWindowPos(g_window, &g_fsPosX, &g_fsPosY);
            const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            glfwSetWindowMonitor(g_window, g_monitor, 0, 0, mode->width, mode->height, 0);
        }

        // Everything went according to plan
        return 0;
    }

    void beginFrame()
    {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        if (ImGui::GetIO().Fonts->TexPixelsRGBA32)
            ImGui::GetIO().Fonts->ClearTexData();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void render(bool vsync)
    {
        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(g_window, &display_w, &display_h);
        common::renderOpenGLFrame(display_w, display_h);

        glfwSwapInterval(vsync);
        glfwSwapBuffers(g_window);
    }

    float getContentScale()
    {
        return g_scaleState.detectedContentScale;
    }

    void getMouseScreenPos(double& x, double& y)
    {
        glfwGetCursorPos(g_window, &x, &y);
    }

    void setMouseScreenPos(double x, double y)
    {
        // Tell GLFW to move the cursor and then manually fire the event
        glfwSetCursorPos(g_window, x, y);
        ImGui_ImplGlfw_CursorPosCallback(g_window, x, y);
    }

    int renderLoop()
    {
        
        tdTimeCpu cpu;
        size_t wait_flush = 0;
        uint64_t prev_time = cpu.DeltaMs();
        // Main loop
        while (!glfwWindowShouldClose(g_window))
        {
            if(!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                uint64_t new_time = cpu.DeltaMs();
                size_t dt = new_time - prev_time;
                if(dt < 40)//25 FPS
                {
                    tdSleep(40 - dt);
                    continue;
                }
                prev_time = new_time;
                if(wait_flush++ > 25*60*5)//5 min
                {
                    if(ConfigManagerImpl::flushAll())
                        wait_flush = 0;
                }
            }

            glfwPollEvents();

            common::applyPendingScaleChanges(g_scaleState, g_scaleState.detectedContentScale, false);

            beginFrame();
            
            if (g_old_maximized != g_maximized)
            {
                g_old_maximized = g_maximized;
                int restoreW = 0, restoreH = 0;
                {
                    auto configAccess = core::configManager.edit();
                    configAccess["maximized"] = g_old_maximized;
                    if (!g_maximized)
                    {
                        ConfigManager::EditSection size = configAccess.section("windowSize");
                        restoreW = (int)size.value("w", (int64_t)g_winWidth);
                        restoreH = (int)size.value("h", (int64_t)g_winHeight);
                    }
                }
                // Outside the transaction: resizing runs the GLFW callbacks, which
                // reach for the config themselves.
                if (!g_maximized)
                    glfwSetWindowSize(g_window, restoreW, restoreH);
            }

            glfwGetWindowSize(g_window, &g_old_winWidth, &g_old_winHeight);

            if (ImGui::IsKeyPressed(GLFW_KEY_F11))
            {
                g_fullScreen = !g_fullScreen;
                if (g_fullScreen)
                {
                    flog::info("Fullscreen: ON");
                    g_fsWidth = g_old_winWidth;
                    g_fsHeight = g_old_winHeight;
                    glfwGetWindowPos(g_window, &g_fsPosX, &g_fsPosY);
                    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
                    glfwSetWindowMonitor(g_window, g_monitor, 0, 0, mode->width, mode->height, 0);
                    core::configManager.edit()["fullscreen"] = true;
                }else{
                    flog::info("Fullscreen: OFF");
                    glfwSetWindowMonitor(g_window, nullptr, g_fsPosX, g_fsPosY, g_fsWidth, g_fsHeight, 0);
                    core::configManager.edit()["fullscreen"] = false;
                }
            }

            if ((g_old_winWidth != g_winWidth || g_old_winHeight != g_winHeight) && !g_maximized && g_old_winWidth > 0 && g_old_winHeight > 0)
            {
                g_winWidth = g_old_winWidth;
                g_winHeight = g_old_winHeight;
                auto configAccess = core::configManager.edit();
                ConfigManager::EditSection size = configAccess.section("windowSize");
                size["w"] = g_winWidth;
                size["h"] = g_winHeight;
            }

            if (g_winWidth > 0 && g_winHeight > 0)
                common::drawMainWindow(ImVec2(g_old_winWidth, g_old_winHeight));
            render();
        }
        return 0;
    }

    int end()
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(g_window);
        glfwTerminate();

        return 0; // TODO: Int really needed?
    }
}
