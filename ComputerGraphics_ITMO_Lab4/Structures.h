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

struct GpuLight {
  // xyz: position in NDC, w: range (for point/spot)
  DirectX::SimpleMath::Vector4 PositionNdcAndRange;
  // xyz: direction in world-space, w: type (0-point, 1-directional, 2-spot)
  DirectX::SimpleMath::Vector4 DirectionAndType;
  // rgb: light color, a: intensity
  DirectX::SimpleMath::Vector4 ColorAndIntensity;
  // x: spotInnerCos, y: spotOuterCos
  DirectX::SimpleMath::Vector4 Params;
};

struct ComposeConstants {
  static constexpr int kMaxLights = 8;

  DirectX::SimpleMath::Matrix InvViewProj;
  DirectX::SimpleMath::Vector4 CameraPosition;
  DirectX::SimpleMath::Vector4 ScreenSize;
  DirectX::SimpleMath::Vector4 LightCount;
  GpuLight Lights[kMaxLights];
};
