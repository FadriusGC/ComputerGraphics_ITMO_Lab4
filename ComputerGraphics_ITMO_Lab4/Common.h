#pragma once

#include <SimpleMath.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// Макрос для проверки HRESULT
#define ThrowIfFailed(x)                                                     \
  {                                                                          \
    HRESULT hr__ = (x);                                                      \
    if (FAILED(hr__)) {                                                      \
      MessageBox(nullptr, L"DirectX Error", L"Error", MB_OK | MB_ICONERROR); \
      std::terminate();                                                      \
    }                                                                        \
  }

// clamp для C++14
template <typename T>
const T& clamp_val(const T& value, const T& min, const T& max) {
  return (value < min) ? min : (value > max) ? max : value;
}

// Макросы для извлечения координат из lParam
#define GET_X_LPARAM(lParam) ((int)(short)LOWORD(lParam))
#define GET_Y_LPARAM(lParam) ((int)(short)HIWORD(lParam))

// Константы
const int WIDTH = 800;
const int HEIGHT = 600;
const int SwapChainBufferCount = 2;
const DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
const DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
