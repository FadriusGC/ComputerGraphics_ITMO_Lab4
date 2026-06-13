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
      // Build a detailed message. If "errors" is empty the file was almost
      // certainly not found at the given path (relative to the working dir).
      std::ostringstream oss;
      oss << "Shader file: "
          << std::string(filename.begin(), filename.end()) << "\r\n"
          << "Entry: " << entryPoint << "   Target: " << target << "\r\n"
          << "HRESULT: 0x" << std::hex << std::uppercase
          << static_cast<unsigned>(hr) << "\r\n\r\n";
      if (errors != nullptr && errors->GetBufferSize() > 0) {
        oss << std::string(static_cast<char*>(errors->GetBufferPointer()),
                           errors->GetBufferSize());
      } else {
        oss << "(No compiler output. An empty message almost always means the "
               ".hlsl file was NOT FOUND at the path above, relative to the "
               "working directory. HRESULT 0x80070002 = ERROR_FILE_NOT_FOUND. "
               "Place the file next to the other shaders that load correctly.)";
      }
      std::string msg = oss.str();
      OutputDebugStringA(msg.c_str());
      MessageBoxA(nullptr, msg.c_str(), "Shader Compilation Error",
                  MB_OK | MB_ICONERROR);
      throw std::runtime_error("Failed to compile shader");
    }

    return byteCode;
  }

  // Compiles a shader from an in-memory source string via D3DCompile.
  // Unlike CompileShader, this does NOT touch the file system, so it does not
  // depend on the current working directory or on .hlsl files being present.
  static ComPtr<ID3DBlob> CompileSource(const std::string& source,
                                        const std::string& entryPoint,
                                        const std::string& target,
                                        const char* sourceName = "embedded") {
    ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors = nullptr;

    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompile(source.c_str(), source.size(), sourceName, nullptr,
                            nullptr, entryPoint.c_str(), target.c_str(),
                            compileFlags, 0, &byteCode, &errors);

    if (FAILED(hr)) {
      std::ostringstream oss;
      oss << "Embedded shader: " << sourceName << "\r\n"
          << "Entry: " << entryPoint << "   Target: " << target << "\r\n"
          << "HRESULT: 0x" << std::hex << std::uppercase
          << static_cast<unsigned>(hr) << "\r\n\r\n";
      if (errors != nullptr && errors->GetBufferSize() > 0) {
        oss << std::string(static_cast<char*>(errors->GetBufferPointer()),
                           errors->GetBufferSize());
      }
      std::string msg = oss.str();
      OutputDebugStringA(msg.c_str());
      MessageBoxA(nullptr, msg.c_str(), "Shader Compilation Error",
                  MB_OK | MB_ICONERROR);
      throw std::runtime_error("Failed to compile embedded shader");
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
