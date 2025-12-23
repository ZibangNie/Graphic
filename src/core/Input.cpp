/*
 * Input.cpp
 *
 * Purpose:
 *   Implements the Input helper that wraps GLFW input polling and callbacks.
 *   Tracks per-frame mouse delta (cursor movement) and accumulates scroll wheel input.
 *
 * Key behaviors:
 *   - Mouse delta is computed from consecutive cursor positions each frame via update().
 *   - Scroll wheel Y offset is accumulated via a GLFW scroll callback and consumed via consumeScrollY().
 *
 * Notes:
 *   - glfwSetWindowUserPointer() is used to associate the Input instance with the GLFWwindow so the static
 *     callback can forward events to the correct object.
 *   - Mouse delta is zeroed on the first update() call to avoid a large jump.
 */

#include "core/Input.h"
#include <GLFW/glfw3.h>

/*
 * Constructor.
 *
 * Parameters:
 *   window : Valid GLFWwindow pointer used for polling cursor/keyboard/mouse state and registering callbacks.
 *
 * Side effects:
 *   - Installs the Input instance as the GLFW window user pointer.
 *   - Registers a scroll callback to capture wheel input.
 */
Input::Input(GLFWwindow* window) : m_window(window) {
    // Bind scroll callback and store this instance in the GLFW window user pointer.
    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, Input::ScrollCallback);
}

/*
 * Updates per-frame input state derived from cursor position.
 *
 * Behavior:
 *   - Reads current cursor position from GLFW.
 *   - Computes m_deltaX/m_deltaY as the difference from the previous cursor position.
 *   - On the first call, initializes the last cursor position and returns with zero deltas.
 *
 * Notes:
 *   - Intended to be called once per frame prior to consuming mouse deltas.
 */
void Input::update() {
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);

    if (m_firstMouse) {
        m_lastX = x;
        m_lastY = y;
        m_firstMouse = false;
        m_deltaX = 0.0;
        m_deltaY = 0.0;
        return;
    }

    m_deltaX = x - m_lastX;
    m_deltaY = y - m_lastY;

    m_lastX = x;
    m_lastY = y;
}

/*
 * Queries whether a keyboard key is currently pressed.
 *
 * Parameters:
 *   glfwKey : GLFW key code (e.g., GLFW_KEY_W).
 *
 * Returns:
 *   true if the key is in GLFW_PRESS state; false otherwise.
 */
bool Input::keyDown(int glfwKey) const {
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

/*
 * Queries whether a mouse button is currently pressed.
 *
 * Parameters:
 *   glfwBtn : GLFW mouse button code (e.g., GLFW_MOUSE_BUTTON_RIGHT).
 *
 * Returns:
 *   true if the button is in GLFW_PRESS state; false otherwise.
 */
bool Input::mouseButtonDown(int glfwBtn) const {
    return glfwGetMouseButton(m_window, glfwBtn) == GLFW_PRESS;
}

/*
 * Consumes the accumulated vertical scroll wheel delta.
 *
 * Returns:
 *   The accumulated scroll Y value since the last call to consumeScrollY().
 *
 * Side effects:
 *   - Resets the internal scroll accumulator to 0.0.
 *
 * Notes:
 *   - Typical usage is once per frame to apply zoom or other scroll-driven behavior.
 */
double Input::consumeScrollY() {
    double v = m_scrollY;
    m_scrollY = 0.0;
    return v;
}

/*
 * GLFW scroll callback (static).
 *
 * Parameters:
 *   window  : GLFWwindow that received the event.
 *   xoffset : Horizontal scroll offset (unused).
 *   yoffset : Vertical scroll offset; accumulated into m_scrollY.
 *
 * Behavior:
 *   - Retrieves the Input instance from the GLFW window user pointer and accumulates yoffset.
 */
void Input::ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* self = reinterpret_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->m_scrollY += yoffset;
}
