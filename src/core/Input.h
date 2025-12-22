#pragma once

// 不要在头文件里 include GLFW（否则会间接引入 OpenGL 头，导致 glad 报错）
struct GLFWwindow;

class Input {
public:
    explicit Input(GLFWwindow* window);

    void update();                  // 每帧调用一次，刷新鼠标 delta
    bool keyDown(int glfwKey) const;
    bool mouseButtonDown(int glfwBtn) const;

    // 鼠标移动增量（像素）
    double mouseDeltaX() const { return m_deltaX; }
    double mouseDeltaY() const { return m_deltaY; }

    double consumeScrollY();

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
