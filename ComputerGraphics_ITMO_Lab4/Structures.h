#pragma once

#include <SimpleMath.h>

#include <cstdint>
#include <vector>

struct Vertex {
  DirectX::SimpleMath::Vector3 Pos;
  DirectX::SimpleMath::Vector3 Normal;
  DirectX::SimpleMath::Vector2 TexC;  // текстурные координаты
  DirectX::SimpleMath::Vector4 Color;
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

struct ModelGeometry {
  std::vector<Vertex> Vertices;
  std::vector<uint32_t> Indices;
};
