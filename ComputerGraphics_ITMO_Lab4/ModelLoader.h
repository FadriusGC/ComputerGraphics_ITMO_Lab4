#pragma once

#include <string>

#include "Structures.h"

class ModelLoader {
 public:
  static bool LoadModel(const std::string& filePath,
                        ModelGeometry& outModelGeometry);
};
