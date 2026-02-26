#pragma once

#include <SimpleMath.h>

#include <cstdint>
#include <vector>

#include "Material.h"

struct Vertex {
  DirectX::SimpleMath::Vector3 Pos;
  DirectX::SimpleMath::Vector3 Normal;
  DirectX::SimpleMath::Vector2 TexC;  // текстурные координаты
  DirectX::SimpleMath::Vector4 Color;
};

struct Submesh {
  UINT MaterialIndex = 0;       // индекс в списке материалов
  UINT IndexCount = 0;          // количество индексов в этой сабмеши
  UINT StartIndexLocation = 0;  // смещение в общем буфере индексов
};

struct ModelGeometry {
  std::vector<Vertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<Submesh> Submeshes;
  std::vector<Material> Materials;
};

struct ObjectConstants {
  DirectX::SimpleMath::Matrix World;
  DirectX::SimpleMath::Matrix WorldViewProj;
};

struct LightConstants {
  DirectX::SimpleMath::Vector4 LightPosition;
  DirectX::SimpleMath::Vector4 LightColor;
  DirectX::SimpleMath::Vector4 CameraPosition;
};
