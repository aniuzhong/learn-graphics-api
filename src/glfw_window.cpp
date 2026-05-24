#include "glfw_window.h"

namespace glfw {

void Window::ApplyHints(const Hints &h) {
    // Context
    glfwWindowHint(GLFW_CLIENT_API,               h.client_api);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API,     h.context_creation_api);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,    h.context_version_major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,    h.context_version_minor);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,    h.context_forward_compat);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT,     h.context_debug);
    glfwWindowHint(GLFW_OPENGL_PROFILE,           h.context_profile);
    glfwWindowHint(GLFW_CONTEXT_ROBUSTNESS,       h.context_robustness);
    glfwWindowHint(GLFW_CONTEXT_RELEASE_BEHAVIOR, h.context_release_behavior);
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR,         h.context_no_error);

    // Window behavior
    glfwWindowHint(GLFW_RESIZABLE,         h.resizable);
    glfwWindowHint(GLFW_VISIBLE,           h.visible);
    glfwWindowHint(GLFW_DECORATED,         h.decorated);
    glfwWindowHint(GLFW_FOCUSED,           h.focused);
    glfwWindowHint(GLFW_AUTO_ICONIFY,      h.auto_iconify);
    glfwWindowHint(GLFW_FLOATING,          h.floating);
    glfwWindowHint(GLFW_MAXIMIZED,         h.maximized);
    glfwWindowHint(GLFW_CENTER_CURSOR,     h.center_cursor);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW,     h.focus_on_show);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR,  h.scale_to_monitor);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, h.mouse_passthrough);

    // Position
    glfwWindowHint(GLFW_POSITION_X, h.position_x);
    glfwWindowHint(GLFW_POSITION_Y, h.position_y);

    // Framebuffer
    glfwWindowHint(GLFW_RED_BITS,                h.red_bits);
    glfwWindowHint(GLFW_GREEN_BITS,              h.green_bits);
    glfwWindowHint(GLFW_BLUE_BITS,               h.blue_bits);
    glfwWindowHint(GLFW_ALPHA_BITS,              h.alpha_bits);
    glfwWindowHint(GLFW_DEPTH_BITS,              h.depth_bits);
    glfwWindowHint(GLFW_STENCIL_BITS,            h.stencil_bits);
    glfwWindowHint(GLFW_ACCUM_RED_BITS,          h.accum_red_bits);
    glfwWindowHint(GLFW_ACCUM_GREEN_BITS,        h.accum_green_bits);
    glfwWindowHint(GLFW_ACCUM_BLUE_BITS,         h.accum_blue_bits);
    glfwWindowHint(GLFW_ACCUM_ALPHA_BITS,        h.accum_alpha_bits);
    glfwWindowHint(GLFW_AUX_BUFFERS,             h.aux_buffers);
    glfwWindowHint(GLFW_SAMPLES,                 h.samples);
    glfwWindowHint(GLFW_REFRESH_RATE,            h.refresh_rate);
    glfwWindowHint(GLFW_STEREO,                  h.stereo);
    glfwWindowHint(GLFW_SRGB_CAPABLE,            h.srgb_capable);
    glfwWindowHint(GLFW_DOUBLEBUFFER,            h.doublebuffer);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, h.transparent_fb);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER,       h.scale_framebuffer);
}

Window::Window(int width, int height, const char *title, Hints hints) {
    ApplyHints(hints);
    handle_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, ForwardFramebufferSize);
    glfwSetKeyCallback(handle_, ForwardKey);
    glfwSetMouseButtonCallback(handle_, ForwardMouseButton);
    glfwSetCursorPosCallback(handle_, ForwardCursorPos);
    glfwSetScrollCallback(handle_, ForwardScroll);
}

Window::~Window() {
    if (handle_)
        glfwDestroyWindow(handle_);
}

Window::Window(Window &&other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      on_framebuffer_size_(std::move(other.on_framebuffer_size_)),
      on_key_(std::move(other.on_key_)),
      on_mouse_button_(std::move(other.on_mouse_button_)),
      on_cursor_pos_(std::move(other.on_cursor_pos_)),
      on_scroll_(std::move(other.on_scroll_)) {
    if (handle_)
        glfwSetWindowUserPointer(handle_, this);
}

Window &Window::operator=(Window &&other) noexcept {
    if (this != &other) {
        if (handle_)
            glfwDestroyWindow(handle_);
        handle_              = std::exchange(other.handle_, nullptr);
        on_framebuffer_size_ = std::move(other.on_framebuffer_size_);
        on_key_              = std::move(other.on_key_);
        on_mouse_button_     = std::move(other.on_mouse_button_);
        on_cursor_pos_       = std::move(other.on_cursor_pos_);
        on_scroll_           = std::move(other.on_scroll_);
        if (handle_)
            glfwSetWindowUserPointer(handle_, this);
    }
    return *this;
}

void Window::ForwardFramebufferSize(GLFWwindow *w, int width, int height) {
    auto *s = Self(w);
    if (s && s->on_framebuffer_size_)
        s->on_framebuffer_size_(width, height);
}

void Window::ForwardKey(GLFWwindow *w, int key, int scancode, int action, int mods) {
    auto *s = Self(w);
    if (s && s->on_key_)
        s->on_key_(key, scancode, action, mods);
}

void Window::ForwardCursorPos(GLFWwindow *w, double x, double y) {
    auto *s = Self(w);
    if (s && s->on_cursor_pos_)
        s->on_cursor_pos_(x, y);
}

void Window::ForwardMouseButton(GLFWwindow *w, int button, int action, int mods) {
    auto *s = Self(w);
    if (s && s->on_mouse_button_)
        s->on_mouse_button_(button, action, mods);
}

void Window::ForwardScroll(GLFWwindow *w, double x, double y) {
    auto *s = Self(w);
    if (s && s->on_scroll_)
        s->on_scroll_(x, y);
}

} // namespace glfw
