#pragma once

// Windows / D3D
#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

// OpenGL loader
#include <glad/glad.h>

// GLFW — order matters: macros first, then glfw3.h, then glfw3native.h
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
