#include "ModelLoader.h"

#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <windows.h>

#include <assimp/Importer.hpp>

bool ModelLoader::LoadModel(const std::string& filePath,
                            ModelGeometry& outModelGeometry) {
  outModelGeometry.Vertices.clear();
  outModelGeometry.Indices.clear();

  Assimp::Importer importer;

  const aiScene* scene = importer.ReadFile(
      filePath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                    aiProcess_FlipUVs | aiProcess_GenNormals |
                    aiProcess_OptimizeMeshes);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    const char* errorStr = importer.GetErrorString();
    OutputDebugStringA("Assimp error: ");
    OutputDebugStringA(errorStr);
    OutputDebugStringA("\n");
    MessageBoxA(nullptr, errorStr, "Assimp Load Error", MB_OK | MB_ICONERROR);
    return false;
  }

  unsigned int totalVertices = 0;

  for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
    aiMesh* mesh = scene->mMeshes[meshIdx];

    for (unsigned int vertIdx = 0; vertIdx < mesh->mNumVertices; ++vertIdx) {
      Vertex vertex;

      // Позиция
      vertex.Pos.x = mesh->mVertices[vertIdx].x;
      vertex.Pos.y = mesh->mVertices[vertIdx].y;
      vertex.Pos.z = mesh->mVertices[vertIdx].z;

      // Нормаль
      if (mesh->HasNormals()) {
        vertex.Normal.x = mesh->mNormals[vertIdx].x;
        vertex.Normal.y = mesh->mNormals[vertIdx].y;
        vertex.Normal.z = mesh->mNormals[vertIdx].z;
      } else {
        vertex.Normal = {0.0f, 1.0f, 0.0f};
      }

      // Текстурные координаты
      if (mesh->HasTextureCoords(0)) {
        vertex.TexC.x = mesh->mTextureCoords[0][vertIdx].x;
        vertex.TexC.y = mesh->mTextureCoords[0][vertIdx].y;
      } else {
        vertex.TexC = {0.0f, 0.0f};
      }

      // Цвет
      if (mesh->HasVertexColors(0)) {
        vertex.Color.x = mesh->mColors[0][vertIdx].r;
        vertex.Color.y = mesh->mColors[0][vertIdx].g;
        vertex.Color.z = mesh->mColors[0][vertIdx].b;
        vertex.Color.w = mesh->mColors[0][vertIdx].a;
      } else {
        vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};
      }

      outModelGeometry.Vertices.push_back(vertex);
    }

    // Индексы
    for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx) {
      aiFace face = mesh->mFaces[faceIdx];
      for (unsigned int indexIdx = 0; indexIdx < face.mNumIndices; ++indexIdx) {
        outModelGeometry.Indices.push_back(
            static_cast<uint32_t>(face.mIndices[indexIdx] + totalVertices));
      }
    }

    totalVertices += mesh->mNumVertices;
  }

  return !outModelGeometry.Vertices.empty();
}
