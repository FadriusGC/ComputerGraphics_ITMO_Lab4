#include "ModelLoader.h"
#define NOMINMAX
#include <DirectXMath.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <windows.h>

#include <assimp/Importer.hpp>

using namespace DirectX;

bool ModelLoader::LoadModel(const std::string& filePath,
                            ModelGeometry& outModelGeometry) {
  outModelGeometry.Vertices.clear();
  outModelGeometry.Indices.clear();
  outModelGeometry.Submeshes.clear();
  outModelGeometry.Materials.clear();

  Assimp::Importer importer;

  const aiScene* scene = importer.ReadFile(
      filePath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                    aiProcess_FlipUVs | aiProcess_GenNormals |
                    aiProcess_OptimizeMeshes | aiProcess_CalcTangentSpace);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    const char* errorStr = importer.GetErrorString();
    OutputDebugStringA("Assimp error: ");
    OutputDebugStringA(errorStr);
    OutputDebugStringA("\n");
    MessageBoxA(nullptr, errorStr, "Assimp Load Error", MB_OK | MB_ICONERROR);
    return false;
  }

  // Загрузка материалов
  if (scene->HasMaterials()) {
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
      aiMaterial* aiMat = scene->mMaterials[i];
      Material material;

      // Имя материала
      aiString name;
      aiMat->Get(AI_MATKEY_NAME, name);
      material.Name = name.C_Str();

      // Диффузный цвет
      aiColor3D diffuse(1.0f, 1.0f, 1.0f);
      aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
      material.Data.DiffuseAlbedo =
          XMFLOAT4(diffuse.r, diffuse.g, diffuse.b, 1.0f);

      // Зеркальный цвет (используем как FresnelR0)
      aiColor3D specular(0.01f, 0.01f, 0.01f);
      aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
      material.Data.FresnelR0 = XMFLOAT3(specular.r, specular.g, specular.b);

      // Блеск -> Roughness
      float shininess = 0.0f;
      aiMat->Get(AI_MATKEY_SHININESS, shininess);
      if (shininess > 0.0f) {
        float normalized = std::min(shininess / 1000.0f, 1.0f);
        material.Data.Roughness = 1.0f - normalized;
      } else {
        material.Data.Roughness = 0.5f;
      }

      outModelGeometry.Materials.push_back(material);
    }
  } else {
    // Материал по умолчанию
    Material defaultMat;
    defaultMat.Name = "Default";
    defaultMat.Data.DiffuseAlbedo = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    defaultMat.Data.FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
    defaultMat.Data.Roughness = 0.25f;
    outModelGeometry.Materials.push_back(defaultMat);
  }

  unsigned int totalVertices = 0;
  unsigned int totalIndices = 0;

  for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx) {
    aiMesh* mesh = scene->mMeshes[meshIdx];

    Submesh submesh;
    submesh.MaterialIndex = mesh->mMaterialIndex;
    submesh.StartIndexLocation = totalIndices;

    // Вершины
    for (unsigned int vertIdx = 0; vertIdx < mesh->mNumVertices; ++vertIdx) {
      Vertex vertex;

      vertex.Pos.x = mesh->mVertices[vertIdx].x;
      vertex.Pos.y = mesh->mVertices[vertIdx].y;
      vertex.Pos.z = mesh->mVertices[vertIdx].z;

      if (mesh->HasNormals()) {
        vertex.Normal.x = mesh->mNormals[vertIdx].x;
        vertex.Normal.y = mesh->mNormals[vertIdx].y;
        vertex.Normal.z = mesh->mNormals[vertIdx].z;
      } else {
        vertex.Normal = {0.0f, 1.0f, 0.0f};
      }

      if (mesh->HasTextureCoords(0)) {
        vertex.TexC.x = mesh->mTextureCoords[0][vertIdx].x;
        vertex.TexC.y = mesh->mTextureCoords[0][vertIdx].y;
      } else {
        vertex.TexC = {0.0f, 0.0f};
      }

      // Цвет вершины пока не используем, ставим белый
      vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};

      outModelGeometry.Vertices.push_back(vertex);
    }

    // Индексы (с коррекцией на общее количество вершин)
    for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx) {
      aiFace face = mesh->mFaces[faceIdx];
      for (unsigned int indexIdx = 0; indexIdx < face.mNumIndices; ++indexIdx) {
        outModelGeometry.Indices.push_back(face.mIndices[indexIdx] +
                                           totalVertices);
      }
    }

    submesh.IndexCount = mesh->mNumFaces * 3;  // после триангуляции
    outModelGeometry.Submeshes.push_back(submesh);

    totalVertices += mesh->mNumVertices;
    totalIndices += submesh.IndexCount;
  }

  return !outModelGeometry.Vertices.empty();
}
