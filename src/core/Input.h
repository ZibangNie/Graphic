#pragma once

#include <GLFW/glfw3.h>

class Input {
public:
    explicit Input(GLFWwindow* window);

    void update(); // 每帧调用一次，刷新鼠标 delta

    bool keyDown(int glfwKey) const;

    // 鼠标移动增量（像素）
    double mouseDeltaX() const { return m_deltaX; }
    double mouseDeltaY() const { return m_deltaY; }

    // 滚轮（每帧消费后清零）
    double consumeScrollY();

    // 可选：按住右键才旋转相机
    bool mouseButtonDown(int glfwBtn) const;

private:
    GLFWwindow* m_window = nullptr;

    double m_lastX = 0.0;
    double m_lastY = 0.0;
    double m_deltaX = 0.0;
    double m_deltaY = 0.0;
    bool   m_firstMouse = true;

    double m_scrollY = 0.0;

    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
};
