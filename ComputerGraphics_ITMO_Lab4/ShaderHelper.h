#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

class ShaderHelper {
 public:
  static ComPtr<ID3DBlob> CompileShader(
      const std::wstring& filename, const std::string& entryPoint,
      const std::string& target, const D3D_SHADER_MACRO* defines = nullptr) {
    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors = nullptr;

    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(filename.c_str(), defines,
                                    D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                    entryPoint.c_str(), target.c_str(),
                                    compileFlags, 0, &byteCode, &errors);

    if (FAILED(hr)) {
      if (errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
        MessageBoxA(nullptr, (char*)errors->GetBufferPointer(),
                    "Shader Compilation Error", MB_OK | MB_ICONERROR);
      }
      throw std::runtime_error("Failed to compile shader");
    }

    return byteCode;
  }

  static ComPtr<ID3DBlob> CompileShaderFromSource(
      const std::wstring& filename, const std::string& entryPoint,
      const std::string& target, const D3D_SHADER_MACRO* defines = nullptr) {
    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors = nullptr;

    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Читаем файл
    std::ifstream shaderFile(filename);
    if (!shaderFile.is_open()) {
      std::wstring errorMsg = L"Failed to open shader file: " + filename;
      MessageBox(nullptr, errorMsg.c_str(), L"Error", MB_OK | MB_ICONERROR);
      return nullptr;
    }

    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    std::string shaderCode = shaderStream.str();
    shaderFile.close();

    HRESULT hr =
        D3DCompile(shaderCode.c_str(), shaderCode.length(), nullptr, defines,
                   D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(),
                   target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (FAILED(hr)) {
      if (errors != nullptr) {
        OutputDebugStringA((char*)errors->GetBufferPointer());
        MessageBoxA(nullptr, (char*)errors->GetBufferPointer(),
                    "Shader Compilation Error", MB_OK | MB_ICONERROR);
      }
      throw std::runtime_error("Failed to compile shader");
    }

    return byteCode;
  }
};
