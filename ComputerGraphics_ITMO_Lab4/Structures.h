#pragma once

#include <SimpleMath.h>

// Структуры вершин и констант
struct Vertex {
  DirectX::SimpleMath::Vector3 Pos;
  DirectX::SimpleMath::Vector3 Normal;  // Нормали для корректного освещения
  DirectX::SimpleMath::Vector4 Color;
};

struct ObjectConstants {
  DirectX::SimpleMath::Matrix World;          // Матрица мира для трансформации
  DirectX::SimpleMath::Matrix WorldViewProj;  // Матрица проекции
  float Time;                                 // Время для анимации
  float ScaleFactor;                          // Коэффициент масштабирования
  DirectX::SimpleMath::Vector2 Padding;       // Выравнивание
};

// Структура для параметров света
struct LightConstants {
  DirectX::SimpleMath::Vector4
      LightPosition;                        // Позиция света (x, y, z, w=1.0f)
  DirectX::SimpleMath::Vector4 LightColor;  // Цвет света (r, g, b, a)
  DirectX::SimpleMath::Vector4
      CameraPosition;  // Позиция камеры для вычисления viewDir
};
