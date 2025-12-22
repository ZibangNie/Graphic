#include "core/Input.h"
#include <GLFW/glfw3.h>

Input::Input(GLFWwindow* window) : m_window(window) {
    // 绑定滚轮回调
    glfwSetWindowUserPointer(m_window, this);
    glfwSetScrollCallback(m_window, Input::ScrollCallback);
}

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

bool Input::keyDown(int glfwKey) const {
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

bool Input::mouseButtonDown(int glfwBtn) const {
    return glfwGetMouseButton(m_window, glfwBtn) == GLFW_PRESS;
}

double Input::consumeScrollY() {
    double v = m_scrollY;
    m_scrollY = 0.0;
    return v;
}

void Input::ScrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* self = reinterpret_cast<Input*>(glfwGetWindowUserPointer(window));
    if (!self) return;
    self->m_scrollY += yoffset;
}
