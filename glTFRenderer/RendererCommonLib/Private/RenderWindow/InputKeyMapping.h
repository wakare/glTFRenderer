#pragma once

#include "RenderWindow/RendererInputDevice.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cstddef>

struct InputKeyMappingEntry
{
    int glfw_key;
    InputDeviceKeyType device_key;
};

inline constexpr std::array<InputKeyMappingEntry, 19> g_input_key_mappings = {{
    {GLFW_KEY_W, InputDeviceKeyType::KEY_W},
    {GLFW_KEY_S, InputDeviceKeyType::KEY_S},
    {GLFW_KEY_A, InputDeviceKeyType::KEY_A},
    {GLFW_KEY_D, InputDeviceKeyType::KEY_D},
    {GLFW_KEY_O, InputDeviceKeyType::KEY_O},
    {GLFW_KEY_F, InputDeviceKeyType::KEY_F},
    {GLFW_KEY_Q, InputDeviceKeyType::KEY_Q},
    {GLFW_KEY_E, InputDeviceKeyType::KEY_E},
    {GLFW_KEY_R, InputDeviceKeyType::KEY_R},
    {GLFW_KEY_C, InputDeviceKeyType::KEY_C},
    {GLFW_KEY_V, InputDeviceKeyType::KEY_V},
    {GLFW_KEY_ENTER, InputDeviceKeyType::KEY_ENTER},
    {GLFW_KEY_SPACE, InputDeviceKeyType::KEY_SPACE},
    {GLFW_KEY_LEFT_CONTROL, InputDeviceKeyType::KEY_LEFT_CONTROL},
    {GLFW_KEY_RIGHT_CONTROL, InputDeviceKeyType::KEY_RIGHT_CONTROL},
    {GLFW_KEY_UP, InputDeviceKeyType::KEY_UP},
    {GLFW_KEY_DOWN, InputDeviceKeyType::KEY_DOWN},
    {GLFW_KEY_LEFT, InputDeviceKeyType::KEY_LEFT},
    {GLFW_KEY_RIGHT, InputDeviceKeyType::KEY_RIGHT},
}};

static_assert(g_input_key_mappings.size() == static_cast<std::size_t>(InputDeviceKeyType::KEY_COUNT) - 1,
              "Input key mapping table must cover all InputDeviceKeyType values except KEY_UNKNOWN.");

inline InputDeviceKeyType GLFWKeyToInputDeviceKey(int key_code)
{
    for (const auto& mapping : g_input_key_mappings)
    {
        if (mapping.glfw_key == key_code)
        {
            return mapping.device_key;
        }
    }
    return InputDeviceKeyType::KEY_UNKNOWN;
}
