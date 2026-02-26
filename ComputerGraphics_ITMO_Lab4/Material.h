#pragma once

#include <SimpleMath.h>

#include <string>

struct MaterialConstants {
  DirectX::SimpleMath::Vector4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
  DirectX::SimpleMath::Vector3 FresnelR0 = {0.01f, 0.01f, 0.01f};
  float Roughness = 0.25f;
};

struct Material {
  std::string Name;
  int MatCBIndex = -1;           // индекс в константном буфере материалов
  std::string DiffuseTexture;    // имя файла диффузной текстуры (из .mtl)
  int DiffuseTextureIndex = -1;  // индекс в массиве mTextures (после загрузки)
  MaterialConstants Data;
};
