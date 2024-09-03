#include "controls.h"
#include <glm/glm/glm.hpp>
#include <glm/glm/ext.hpp>
#include "imgui.h"
#include "GLFW/glfw3.h"

constexpr float PI = 3.14159265358979323846f;

void Controls::updateMouseMove(double xPos, double yPos, DragState& drag, CameraState& cameraState) {
    if (drag.active) {
        vec2 currentPos = vec2(-(float)xPos, (float)yPos);
        vec2 delta = (currentPos - drag.startPos) * drag.sensitivity;
        cameraState.angles = drag.startCameraState.angles + delta;
        cameraState.angles.y = glm::clamp(cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);

    }
}

void Controls::smoothOut(double xPos, double yPos, DragState& drag) {
    if (drag.active) {
        vec2 currentPos = vec2(-(float)xPos, (float)yPos);
        vec2 delta = (currentPos - drag.startPos) * drag.sensitivity;
        drag.velocity = delta - drag.previousDelta;
        drag.previousDelta = delta;
    }
}

void Controls::updateMouseButton(int button, int action, int /* modifiers */, DragState& drag, CameraState& cameraState, GLFWwindow*& window) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        switch (action) {
        case GLFW_PRESS:
            drag.active = true;
            double xPos, yPos;
            glfwGetCursorPos(window, &xPos, &yPos);
            drag.startPos = vec2(-(float)xPos, (float)yPos);
            drag.startCameraState = cameraState;
            break;
        case GLFW_RELEASE:
            drag.active = false;
            break;
        }
    }
}

void Controls::updateScroll(double /* xOffset */, double yOffset, DragState& drag, CameraState& cameraState) {
    cameraState.zoom += drag.scrollSensitivity * static_cast<float>(yOffset);
    cameraState.zoom = glm::clamp(cameraState.zoom, -2.0f, 2.0f);
}

bool Controls::updateDragInertia(DragState& drag, CameraState& cameraState) {
    if (drag.active) {
        constexpr float eps = 1e-4f;
        if (std::abs(drag.velocity.x) < eps && std::abs(drag.velocity.y) < eps) {
            return false;
        }
        cameraState.angles += drag.velocity;
        cameraState.angles.y = glm::clamp(cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
        drag.velocity *= drag.inertia;
    }
    return drag.active;
}
