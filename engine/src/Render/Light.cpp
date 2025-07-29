#include "Engine/Render/Light.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/vec3.hpp>
DISABLE_WARNINGS_POP()
#include <array>
#include <cmath>

namespace Render {

struct LightConstantData {
    glm::vec3 position;
    glm::vec3 intensity;
};

/* Convert between Temperature and RGB.
 * Base on information from http://www.brucelindbloom.com/
 * The fit for D-illuminant between 4000K and 23000K are from CIE
 * The generalization to 2000K < T < 4000K and the blackbody fits
 * are my own and should be taken with a grain of salt.
 */
static constexpr std::array XYZ_to_RGB {
    glm::vec3 { 3.24071f, -0.969258f, 0.0556352f },
    glm::vec3 { -1.53726f, 1.87599f, -0.203996f },
    glm::vec3 { -0.498571f, 0.0415557f, 1.05707f }
};

// Color temperature (Kelvin) to RGB conversion.
//
// Copied from:
// https://stackoverflow.com/questions/13975917/calculate-colour-temperature-in-k
glm::vec3 colorTemperatureToRgb(float T)
{
    float xD;
    // Fit for CIE Daylight illuminant.
    if (T <= 4000.0f) {
        xD = 0.27475e9f / (T * T * T) - 0.98598e6f / (T * T) + 1.17444e3f / T + 0.145986f;
    } else if (T <= 7000.0f) {
        xD = -4.6070e9f / (T * T * T) + 2.9678e6f / (T * T) + 0.09911e3f / T + 0.244063f;
    } else {
        xD = -2.0064e9f / (T * T * T) + 1.9018e6f / (T * T) + 0.24748e3f / T + 0.237040f;
    }
    const float yD = -3.0f * xD * xD + 2.87f * xD - 0.275f;

    // Fit for Blackbody using CIE standard observer function at 2 degrees
    //xD = -1.8596e9/(T*T*T) + 1.37686e6/(T*T) + 0.360496e3/T + 0.232632;
    //yD = -2.6046*xD*xD + 2.6106*xD - 0.239156;

    // Fit for Blackbody using CIE standard observer function at 10 degrees
    //xD = -1.98883e9/(T*T*T) + 1.45155e6/(T*T) + 0.364774e3/T + 0.231136;
    //yD = -2.35563*xD*xD + 2.39688*xD - 0.196035;

    const float X = xD / yD;
    const float Y = 1;
    const float Z = (1 - xD - yD) / yD;
    glm::vec3 rgb = X * XYZ_to_RGB[0] + Y * XYZ_to_RGB[1] + Z * XYZ_to_RGB[2];
    rgb /= glm::compMax(rgb);
    return rgb;
}

/*// Color temperature (Kelvin) to RGB conversion.
//
// Copied from:
// https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
glm::vec3 colorTemperatureToRgb(temperature<kelvin, float> temp)
{
    const float fTemperature = temp.count() / 100;

    // Start with a temperature, in Kelvin, somewhere between 1000 and 40000.  (Other values may work,
    // but I can't make any promises about the quality of the algorithm's estimates above 40000 K.)
    // Note also that the temperature and color variables need to be declared as floating-point.
    glm::vec3 rgb;
    if (fTemperature <= 66.0f) {
        rgb.r = 255.0f;
    } else {
        rgb.r = fTemperature - 60.0f;
        rgb.r = 329.698727446f * std::pow(rgb.r, -0.1332047592f);
    }

    if (fTemperature <= 66.0f) {
        rgb.g = 99.4708025861f * std::log(fTemperature) - 161.1195681661f;
    } else {
        rgb.g = fTemperature - 60.0f;
        rgb.g = 288.1221695283f * std::pow(rgb.g, -0.0755148492f);
    }

    if (fTemperature >= 66.0f) {
        rgb.b = 255.0f;
    } else {
        if (fTemperature <= 19.0f) {
            rgb.b = 0.0f;
        } else {
            rgb.b = fTemperature - 10.0f;
            rgb.b = 138.5177312231f * std::log(fTemperature) - 305.0447927307f;
        }
    }

    rgb = glm::clamp(rgb / 255.0f, 0.0f, 1.0f);
    const glm::vec3 linearRGB = glm::pow(rgb, glm::vec3(2.2f));
    return linearRGB;
}*/


}