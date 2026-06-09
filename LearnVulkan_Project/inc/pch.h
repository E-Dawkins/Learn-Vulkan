#pragma once

// glfw (we let this include vulkan for us)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// glm
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

// stb headers
#include <stb_image.h>

// tinyobj
#include <tiny_obj_loader.h>

// stl
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>
