#pragma once

#include <SimpleMath.h>

#include <string>

struct MaterialConstants {
  DirectX::SimpleMath::Vector4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
  DirectX::SimpleMath::Vector3 FresnelR0 = {0.04f, 0.04f, 0.04f};
  float Roughness = 0.25f;
  float HasNormalMap = 0.0f;
  float HasDisplacementMap = 0.0f;
  float HasRoughnessMap = 0.0f;
  float DisplacementScale = 0.0f;
  // PBR metallic-roughness workflow additions.
  float Metallic = 0.0f;
  float HasMetallicMap = 0.0f;
  float MatPad0 = 0.0f;
  float MatPad1 = 0.0f;
  DirectX::SimpleMath::Matrix TexTransform =
      DirectX::SimpleMath::Matrix::Identity;
};

struct Material {
  std::string Name;
  int MatCBIndex = -1;
  std::string DiffuseTexture;
  int DiffuseTextureIndex = -1;
  std::string NormalTexture;
  int NormalTextureIndex = -1;
  std::string DisplacementTexture;
  int DisplacementTextureIndex = -1;
  std::string RoughnessTexture;
  int RoughnessTextureIndex = -1;
  std::string MetallicTexture;
  int MetallicTextureIndex = -1;
  MaterialConstants Data;
};
