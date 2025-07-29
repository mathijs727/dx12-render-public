#pragma once
#define GLM_ENABLE_EXPERIMENTAL 1
#include "Engine/Core/ForwardDeclares.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

namespace Core {

struct Transform {
public:
    glm::vec3 position { 0.0f };
    glm::quat rotation { glm::identity<glm::quat>() };
    glm::vec3 scale { 1.0f };

public:
    [[nodiscard]] Transform operator*(const Core::Transform& rhs) const;
    [[nodiscard]] static Transform lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::mat4 matrix() const; // local to world
    [[nodiscard]] glm::mat3 normalMatrix() const;
    [[nodiscard]] glm::mat4 inverseMatrix() const; // world to local
};

struct FPSCameraControls {
public:
    FPSCameraControls(Core::Mouse* pMouse, const Core::Keyboard* pKeyboard);

    void tick(float timestep, Transform& inoutTransform);

private:
    bool m_mouseCaptured = false;
    Core::Mouse* m_pMouse;
    const Core::Keyboard* m_pKeyboard;
};

}
