#include "Engine/Render/Camera.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/gtc/matrix_transform.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

glm::mat4 Camera::projectionMatrix() const
{
    return glm::perspectiveZO(fovY, aspectRatio, zNear, zFar);
}

}
