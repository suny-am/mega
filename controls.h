#include "application.h"
#include <glm/glm/glm.hpp>

class Controls {
public:
    using DragState = Application::DragState;
    using CameraState = Application::CameraState;
    using vec2 = glm::vec2;

    static void updateMouseMove(double xPos, double yPos, DragState& drag, CameraState& cameraState);
    static void smoothOut(double xPos, double yPos, DragState& drag);
    static void updateMouseButton(int button, int action, int mods, DragState& drag, CameraState& cameraState, GLFWwindow*& window);
    static void updateScroll(double /* xOffset */, double yOffset, DragState& drag, CameraState& cameraState);
    static bool updateDragInertia(DragState& drag, CameraState& cameraState);
};