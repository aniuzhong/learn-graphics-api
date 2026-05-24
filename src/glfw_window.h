#pragma once

#include <functional>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace glfw {

class Window {
public:
    struct Hints {
        // Context
        int  client_api               = GLFW_OPENGL_API;
        int  context_creation_api     = GLFW_NATIVE_CONTEXT_API;
        int  context_version_major    = 1;
        int  context_version_minor    = 0;
        bool context_forward_compat   = false;
        bool context_debug            = false;
        int  context_profile          = 0; // GLFW_OPENGL_ANY_PROFILE
        int  context_robustness       = 0; // GLFW_NO_ROBUSTNESS
        int  context_release_behavior = 0; // GLFW_ANY_RELEASE_BEHAVIOR
        bool context_no_error         = false;

        // Window behavior
        bool resizable          = true;
        bool visible            = true;
        bool decorated          = true;
        bool focused            = true;
        bool auto_iconify       = true;
        bool floating           = false;
        bool maximized          = false;
        bool center_cursor      = true;
        bool focus_on_show      = true;
        bool scale_to_monitor   = false;
        bool mouse_passthrough  = false;

        // Position
        int  position_x = static_cast<int>(GLFW_ANY_POSITION);
        int  position_y = static_cast<int>(GLFW_ANY_POSITION);

        // Framebuffer
        int  red_bits           = 8;
        int  green_bits         = 8;
        int  blue_bits          = 8;
        int  alpha_bits         = 8;
        int  depth_bits         = 24;
        int  stencil_bits       = 8;
        int  accum_red_bits     = 0;
        int  accum_green_bits   = 0;
        int  accum_blue_bits    = 0;
        int  accum_alpha_bits   = 0;
        int  aux_buffers        = 0;
        int  samples            = 0;
        int  refresh_rate       = GLFW_DONT_CARE;
        bool stereo             = false;
        bool srgb_capable       = false;
        bool doublebuffer       = true;
        bool transparent_fb     = false;
        bool scale_framebuffer  = true;
    };

    using FramebufferSizeCallback = std::function<void(int width, int height)>;
    using KeyCallback             = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseButtonCallback     = std::function<void(int button, int action, int mods)>;
    using CursorPosCallback       = std::function<void(double xpos, double ypos)>;
    using ScrollCallback          = std::function<void(double xoffset, double yoffset)>;

    Window(int width, int height, const char *title, Hints hints = {});
    ~Window();
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&other) noexcept;
    Window &operator=(Window &&other) noexcept;

    GLFWwindow *Get() const { return handle_; }

    bool ShouldClose()                               const { return glfwWindowShouldClose(handle_); }
    bool GetKey(int key)                             const { return glfwGetKey(handle_, key) == GLFW_PRESS; }
    bool GetMouseButton(int button)                  const { return glfwGetMouseButton(handle_, button) == GLFW_PRESS; }
    void GetWindowSize(int *width, int *height)      const { glfwGetWindowSize(handle_, width, height); }
    void GetFramebufferSize(int *width, int *height) const { glfwGetFramebufferSize(handle_, width, height); }
    void SwapBuffers()                                     { glfwSwapBuffers(handle_); }
    void MakeContextCurrent()                              { glfwMakeContextCurrent(handle_); }
    void SetSwapInterval(int interval)                     { glfwSwapInterval(interval); }
    void SetShouldClose(bool v)                            { glfwSetWindowShouldClose(handle_, v ? GLFW_TRUE : GLFW_FALSE); }
    void SetInputMode(int mode, int value)                 { glfwSetInputMode(handle_, mode, value); }

    FramebufferSizeCallback on_framebuffer_size;
    KeyCallback             on_key;
    MouseButtonCallback     on_mouse_button;
    CursorPosCallback       on_cursor_pos;
    ScrollCallback          on_scroll;

private:
    static Window *Self(GLFWwindow *w) { return static_cast<Window *>(glfwGetWindowUserPointer(w)); }
    static void ApplyHints(const Hints &hints);
    static void ForwardFramebufferSize(GLFWwindow *w, int width, int height);
    static void ForwardKey(GLFWwindow *w, int key, int scancode, int action, int mods);
    static void ForwardMouseButton(GLFWwindow *w, int button, int action, int mods);
    static void ForwardCursorPos(GLFWwindow *w, double xpos, double ypos);
    static void ForwardScroll(GLFWwindow *w, double xoffset, double yoffset);

    GLFWwindow *handle_ = nullptr;
};

} // namespace glfw
