/*
 * Input.h
 *
 * Purpose:
 *   Declares the Input helper that wraps GLFW input polling and scroll callbacks.
 *   Provides per-frame mouse delta (cursor movement) and a consumable scroll-wheel accumulator.
 *
 * Design notes:
 *   - Avoids including GLFW headers in this header to prevent indirect inclusion of OpenGL headers,
 *     which can conflict with glad's include order. Uses a forward declaration of GLFWwindow instead.
 *
 * Usage:
 *   - Construct with a valid GLFWwindow*.
 *   - Call update() once per frame to refresh mouse deltas.
 *   - Use keyDown()/mouseButtonDown() for immediate state queries.
 *   - Use consumeScrollY() to retrieve and reset accumulated scroll input.
 */

#pragma once

// Forward declaration only: including GLFW headers here may indirectly include OpenGL headers and break glad.
struct GLFWwindow;

class Input {
public:
    /*
     * Constructor.
     *
     * Parameters:
     *   window : Valid GLFWwindow pointer used for polling input state and registering callbacks.
     *
     * Side effects:
     *   - Registers internal callbacks (implemented in Input.cpp).
     */
    explicit Input(GLFWwindow* window);

    /*
     * Updates per-frame mouse delta state.
     *
     * Notes:
     *   - Intended to be called once per frame.
     *   - Mouse delta is measured in cursor-position units (typically pixels in screen space).
     */
    void update();

    /*
     * Immediate keyboard state query.
     *
     * Parameters:
     *   glfwKey : GLFW key code (e.g., GLFW_KEY_W).
     *
     * Returns:
     *   true if the key is currently pressed.
     */
    bool keyDown(int glfwKey) const;

    /*
     * Immediate mouse button state query.
     *
     * Parameters:
     *   glfwBtn : GLFW mouse button code (e.g., GLFW_MOUSE_BUTTON_RIGHT).
     *
     * Returns:
     *   true if the button is currently pressed.
     */
    bool mouseButtonDown(int glfwBtn) const;

    // Mouse movement delta since the last update() call (cursor units, typically pixels).
    double mouseDeltaX() const { return m_deltaX; }
    double mouseDeltaY() const { return m_deltaY; }

    /*
     * Consumes accumulated vertical scroll wheel delta.
     *
     * Returns:
     *   Sum of yoffset values since the last call; resets internal accumulator to 0.0.
     */
    double consumeScrollY();

private:
    GLFWwindow* m_window = nullptr;

    // Previous cursor position used to compute per-frame delta.
    double m_lastX = 0.0;
    double m_lastY = 0.0;

    // Cursor delta since the last update() call.
    double m_deltaX = 0.0;
    double m_deltaY = 0.0;

    // First-update guard to avoid large deltas on initialization.
    bool   m_firstMouse = true;

    // Accumulated vertical scroll wheel input (sum of callback yoffset values).
    double m_scrollY = 0.0;

    /*
     * GLFW scroll callback (static).
     *
     * Parameters:
     *   window  : GLFWwindow that received the event.
     *   xoffset : Horizontal scroll offset.
     *   yoffset : Vertical scroll offset (accumulated).
     *
     * Notes:
     *   - Implemented as static for GLFW C-style callback compatibility.
     *   - Forwards to the instance using glfwSetWindowUserPointer().
     */
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
