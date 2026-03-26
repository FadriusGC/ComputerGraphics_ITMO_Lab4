#pragma once

#include <SimpleMath.h>

#include <string>

struct MaterialConstants {
  DirectX::SimpleMath::Vector4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
  DirectX::SimpleMath::Vector3 FresnelR0 = {0.01f, 0.01f, 0.01f};
  float Roughness = 0.25f;
  float HasNormalMap = 0.0f;
  float HasDisplacementMap = 0.0f;
  float HasRoughnessMap = 0.0f;
  float DisplacementScale = 0.0f;
  DirectX::SimpleMath::Matrix TexTransform =
      DirectX::SimpleMath::Matrix::Identity;
};

struct Material {
  std::string Name;
  int MatCBIndex = -1;           // индекс в константном буфере материалов
  std::string DiffuseTexture;    // имя файла диффузной текстуры (из .mtl)
  int DiffuseTextureIndex = -1;  // индекс в массиве mTextures (после загрузки)
  std::string NormalTexture;
  int NormalTextureIndex = -1;
  std::string DisplacementTexture;
  int DisplacementTextureIndex = -1;
  std::string RoughnessTexture;
  int RoughnessTextureIndex = -1;
  MaterialConstants Data;
};
