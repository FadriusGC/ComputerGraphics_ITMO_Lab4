#pragma once

#include <SimpleMath.h>
#include <comdef.h>  // для _com_error
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

// Расширенный макрос проверки HRESULT с указанием файла, строки и функции
#define ThrowIfFailed(x)                                                     \
  do {                                                                       \
    HRESULT hr__ = (x);                                                      \
    if (FAILED(hr__)) {                                                      \
      std::stringstream ss__;                                                \
      ss__ << "Error at " << __FILE__ << "(" << __LINE__ << ") in function " \
           << __FUNCTION__ << ": ";                                          \
      _com_error err__(hr__);                                                \
      ss__ << "HRESULT 0x" << std::hex << hr__ << std::dec << " - "          \
           << err__.ErrorMessage();                                          \
      std::string msg__ = ss__.str();                                        \
      MessageBoxA(nullptr, msg__.c_str(), "DirectX Error",                   \
                  MB_OK | MB_ICONERROR);                                     \
      std::terminate();                                                      \
    }                                                                        \
  } while (false)

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
