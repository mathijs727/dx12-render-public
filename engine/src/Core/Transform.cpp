#include "Engine/Core/Transform.h"
#include "Engine/Core/Keyboard.h"
#include "Engine/Core/Mouse.h"
#include "Engine/Core/Transform.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
DISABLE_WARNINGS_POP()

namespace Core {

Transform Transform::lookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up)
{
    Transform out;
    out.position = eye;
    out.rotation = glm::quatLookAt(glm::normalize(center - eye), glm::normalize(up));
    return out;
}

glm::mat4 Transform::viewMatrix() const
{
    auto matrix = glm::translate(glm::identity<glm::mat4>(), position);
    matrix *= glm::mat4_cast(rotation);
    return glm::inverse(matrix);
    // return glm::mat4_cast(glm::inverse(rotation)) * glm::translate(glm::identity<glm::mat4>(), -position);
}

glm::mat4 Transform::matrix() const
{
    auto matrix = glm::translate(glm::identity<glm::mat4>(), position);
    matrix *= glm::mat4_cast(rotation);
    matrix = glm::scale(matrix, scale);
    return matrix;
}

glm::mat3 Transform::normalMatrix() const
{
    return glm::inverseTranspose(glm::mat3(matrix()));
}

glm::mat4 Transform::inverseMatrix() const
{
    /*auto matrix = glm::translate(glm::identity<glm::mat4>(), position);
    matrix *= glm::mat4_cast(rotation);
    matrix = glm::scale(matrix, scale);
    return matrix;*/
    return glm::inverse(matrix());
}

Transform Transform::operator*(const Transform& other) const
{
    Transform out;
    out.position = scale * (rotation * other.position) + position;
    out.rotation = rotation * other.rotation;
    out.scale = other.scale * scale; // NOTE: does not properly handle skewing from rotation+translation.
    return out;
}

FPSCameraControls::FPSCameraControls(Core::Mouse* pMouse, const Core::Keyboard* pKeyboard)
    : m_pMouse(pMouse)
    , m_pKeyboard(pKeyboard)
{
}

void FPSCameraControls::tick(float timestep, Transform& inoutTransform)
{
    static constexpr float moveSpeed = 0.0075f;
    static constexpr float lookSpeed = 0.001f;

    if (m_pKeyboard->isKeyRelease(Core::Key::L)) {
        m_mouseCaptured = !m_mouseCaptured;
        m_pMouse->setCapture(m_mouseCaptured);
    }

    glm::vec3 forward = inoutTransform.rotation * glm::vec3(0, 0, -1); // Camera looks along negative z
    constexpr glm::vec3 yAxis { 0, 1, 0 };
    const glm::vec3 horAxis = glm::cross(yAxis, forward);
    glm::vec3 up = glm::cross(forward, horAxis);

    glm::ivec2 mouseDelta = m_pMouse->getPositionDelta();
    if (m_mouseCaptured) {
        if (mouseDelta.x != 0 || mouseDelta.y != 0) {
            // Rotate around the horizontal axis.
            forward = glm::angleAxis(mouseDelta.y * lookSpeed, horAxis) * forward;
            up = glm::cross(forward, horAxis);

            // Rotate around the y-axis.
            forward = glm::angleAxis(-mouseDelta.x * lookSpeed, yAxis) * forward;
            up = glm::cross(forward, horAxis);

            inoutTransform.rotation = glm::quatLookAt(forward, up);
        }
    }

    const glm::vec3 left = inoutTransform.rotation * glm::vec3(-1, 0, 0);
    const float moveAmount = moveSpeed * timestep * (m_pKeyboard->isKeyDown(Key::SHIFT) ? 2.5f : 1.0f);
    if (m_pKeyboard->isKeyDown(Core::Key::W))
        inoutTransform.position += forward * moveAmount;
    if (m_pKeyboard->isKeyDown(Core::Key::S))
        inoutTransform.position -= forward * moveAmount;
    if (m_pKeyboard->isKeyDown(Core::Key::A))
        inoutTransform.position += left * moveAmount;
    if (m_pKeyboard->isKeyDown(Core::Key::D))
        inoutTransform.position -= left * moveAmount;
    if (m_pKeyboard->isKeyDown(Core::Key::CONTROL))
        inoutTransform.position -= up * moveAmount;
    if (m_pKeyboard->isKeyDown(Core::Key::SPACE))
        inoutTransform.position += up * moveAmount;
}

}
