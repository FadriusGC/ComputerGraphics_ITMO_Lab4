#pragma once

#include <DirectXCollision.h>
#include <SimpleMath.h>

#include <array>
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
  static constexpr UINT kLodCount = 3;
  UINT MaterialIndex = 0;       // индекс в списке материалов
  UINT IndexCount = 0;          // количество индексов в этой сабмеши
  UINT StartIndexLocation = 0;  // смещение в общем буфере индексов
  std::array<UINT, kLodCount> LodIndexCount = {0, 0, 0};
  std::array<UINT, kLodCount> LodStartIndexLocation = {0, 0, 0};
  DirectX::BoundingBox Bounds;
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
  DirectX::SimpleMath::Vector4 WaveParams;  // первый аргумент это ампилтуда,
                                            // второе частота, третье скорость
};

struct SceneObject {
  UINT SubmeshStart = 0;
  UINT SubmeshCount = 0;
  DirectX::SimpleMath::Matrix World = DirectX::SimpleMath::Matrix::Identity;
  DirectX::BoundingBox LocalBounds;
  DirectX::BoundingBox WorldBounds;
  DirectX::SimpleMath::Vector4 TessellationParams =
      DirectX::SimpleMath::Vector4(25.0f, 350.0f, 12.0f, 1.0f);
  DirectX::SimpleMath::Vector4 LodDistances =
      DirectX::SimpleMath::Vector4(60.0f, 140.0f, 0.0f, 0.0f);
  DirectX::SimpleMath::Vector4 WaveParams =
      DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
};

struct SubmeshInstance {
  UINT ObjectIndex = 0;
  UINT SubmeshIndex = 0;
  DirectX::BoundingBox LocalBounds;
  DirectX::BoundingBox WorldBounds;
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
