#pragma once
#include "Engine/Core/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

struct Camera {
    float aspectRatio { 1.0f };
    float fovY { glm::radians(60.0f) };
    float zNear { 0.1f };
    float zFar { 1000.0f };

    float apertureFstops { 1.4f };
    float shutterTimeSeconds { 1.0f / 60.0f };
    float sensorSensitivityISO { 800.0f };

public:
    glm::mat4 projectionMatrix() const;
};

}
