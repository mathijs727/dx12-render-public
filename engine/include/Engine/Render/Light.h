#pragma once
#include "Engine/Render/ShaderInputs/structs/DirectionalLight.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()

namespace Render {

struct DirectionalLight : public ShaderInputs::DirectionalLight { };

}
