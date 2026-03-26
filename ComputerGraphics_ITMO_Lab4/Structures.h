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
  DirectX::SimpleMath::Vector3 Tangent = {1.0f, 0.0f, 0.0f};
  DirectX::SimpleMath::Vector3 Bitangent = {0.0f, 1.0f, 0.0f};
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
  DirectX::SimpleMath::Vector4 CameraPosition;
  DirectX::SimpleMath::Vector4 TessellationParams;
};

struct SceneObject {
  UINT SubmeshStart = 0;
  UINT SubmeshCount = 0;
  DirectX::SimpleMath::Matrix World = DirectX::SimpleMath::Matrix::Identity;
};

struct LightConstants {
  DirectX::SimpleMath::Vector4 LightPosition;
  DirectX::SimpleMath::Vector4 LightColor;
  DirectX::SimpleMath::Vector4 CameraPosition;
};

struct GpuLight {
  // xyz: мировая позиция, w: дальность для (point/spot)
  DirectX::SimpleMath::Vector4 PositionWorldAndRange;
  // xyz: направление в мире, w: тип (0-point, 1-directional, 2-spot)
  DirectX::SimpleMath::Vector4 DirectionAndType;
  // rgb: цвет , a: интенсиность
  DirectX::SimpleMath::Vector4 ColorAndIntensity;
  // x: spotInnerCos, y: spotOuterCos
  DirectX::SimpleMath::Vector4 Params;
};

struct ComposeConstants {
  static constexpr int kMaxLights = 100;

  DirectX::SimpleMath::Matrix InvViewProj;
  DirectX::SimpleMath::Vector4 CameraPosition;
  DirectX::SimpleMath::Vector4 ScreenSize;
  DirectX::SimpleMath::Vector4 LightCount;
  GpuLight Lights[kMaxLights];
};
