#pragma once
//vuklan相关
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>
//glm相关
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
//assimp相关
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/vector3.h>
//标准库
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <optional>
#include <functional>
#include <set>
#include <vector>
#include <string>
#include <chrono>
#include <array>
#include <memory>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

namespace StarryEngine{

#define PI 3.1415
}


