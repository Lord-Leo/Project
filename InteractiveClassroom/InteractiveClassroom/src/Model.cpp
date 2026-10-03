#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/pbrmaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <unordered_set>

namespace fs = std::filesystem;

namespace
{
std::unordered_set<std::string> g_modelWarnings;

bool getColour(aiMaterial* material, const char* key, unsigned int type,
               unsigned int index, glm::vec4& out)
{
    aiColor4D colour;
    if (aiGetMaterialColor(material, key, type, index, &colour) == AI_SUCCESS)
    {
        out = glm::vec4(colour.r, colour.g, colour.b, colour.a);
        return true;
    }
    return false;
}

float getFloatOr(aiMaterial* material, const char* key, unsigned int type,
                 unsigned int index, float fallback)
{
    float value = fallback;
    unsigned int count = 1;
    if (aiGetMaterialFloatArray(material, key, type, index, &value, &count) == AI_SUCCESS)
        return value;
    return fallback;
}
}

Model::Model(const std::string& path, TextureManager& textureManager,
             const ModelLoadOptions& options)
    : m_path(path), m_textureManager(textureManager)
{
    Load(options);
}

void Model::WarnMissingModelOnce(const std::string& path, const std::string& detail)
{
    if (g_modelWarnings.insert(path + detail).second)
        std::cerr << "[Model] " << detail << " '" << path << "'. Procedural fallback remains active.\n";
}

glm::mat4 Model::ConvertMatrix(const aiMatrix4x4& m)
{
    glm::mat4 result(1.0f);
    result[0][0] = m.a1; result[1][0] = m.a2; result[2][0] = m.a3; result[3][0] = m.a4;
    result[0][1] = m.b1; result[1][1] = m.b2; result[2][1] = m.b3; result[3][1] = m.b4;
    result[0][2] = m.c1; result[1][2] = m.c2; result[2][2] = m.c3; result[3][2] = m.c4;
    result[0][3] = m.d1; result[1][3] = m.d2; result[2][3] = m.d3; result[3][3] = m.d4;
    return result;
}

void Model::Load(const ModelLoadOptions& options)
{
    m_flipUVs = options.flipUVs;
    m_directory = fs::path(m_path).parent_path().generic_string();

    if (!fs::exists(m_path))
    {
        WarnMissingModelOnce(m_path, "Missing model");
        return;
    }

    unsigned int flags = aiProcess_Triangulate |
                         aiProcess_GenSmoothNormals |
                         aiProcess_CalcTangentSpace |
                         aiProcess_JoinIdenticalVertices |
                         aiProcess_ImproveCacheLocality |
                         aiProcess_SortByPType |
                         aiProcess_ValidateDataStructure;
    if (options.optimizeMeshes)
        flags |= aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph;
    if (options.flipUVs)
        flags |= aiProcess_FlipUVs;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(m_path, flags);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
    {
        WarnMissingModelOnce(m_path, std::string("Assimp could not load model: ") + importer.GetErrorString());
        return;
    }

    m_boundsMin = glm::vec3(std::numeric_limits<float>::max());
    m_boundsMax = glm::vec3(std::numeric_limits<float>::lowest());
    ProcessNode(scene->mRootNode, scene, glm::mat4(1.0f));

    m_loaded = !m_meshes.empty();
    if (!m_loaded)
        WarnMissingModelOnce(m_path, "Model contained no renderable triangle meshes");
}

void Model::ProcessNode(aiNode* node, const aiScene* scene,
                        const glm::mat4& parentTransform)
{
    const glm::mat4 nodeTransform = parentTransform * ConvertMatrix(node->mTransformation);
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.emplace_back(ProcessMesh(mesh, scene, nodeTransform));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        ProcessNode(node->mChildren[i], scene, nodeTransform);
}

ModelMesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene,
                             const glm::mat4& nodeTransform)
{
    std::vector<ModelVertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(mesh->mNumVertices);
    indices.reserve(mesh->mNumFaces * 3u);

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(nodeTransform));
    const bool hasTangents = mesh->HasTangentsAndBitangents();

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        ModelVertex vertex;
        const aiVector3D& p = mesh->mVertices[i];
        vertex.position = glm::vec3(nodeTransform * glm::vec4(p.x, p.y, p.z, 1.0f));

        if (mesh->HasNormals())
        {
            const aiVector3D& n = mesh->mNormals[i];
            vertex.normal = glm::normalize(normalMatrix * glm::vec3(n.x, n.y, n.z));
        }

        if (mesh->mTextureCoords[0])
            vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x,
                                         mesh->mTextureCoords[0][i].y);

        if (hasTangents)
        {
            const aiVector3D& t = mesh->mTangents[i];
            const aiVector3D& b = mesh->mBitangents[i];
            vertex.tangent = glm::normalize(normalMatrix * glm::vec3(t.x, t.y, t.z));
            vertex.bitangent = glm::normalize(normalMatrix * glm::vec3(b.x, b.y, b.z));
        }

        m_boundsMin = glm::min(m_boundsMin, vertex.position);
        m_boundsMax = glm::max(m_boundsMax, vertex.position);
        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            indices.push_back(face.mIndices[j]);
    }

    Material material;
    if (mesh->mMaterialIndex < scene->mNumMaterials)
        material = ProcessMaterial(scene->mMaterials[mesh->mMaterialIndex], scene, hasTangents);
    material.tangentSpaceValid = hasTangents;
    return ModelMesh(std::move(vertices), std::move(indices), material);
}

unsigned int Model::LoadTextureReference(const std::string& reference,
                                         TextureManager::Semantic semantic,
                                         const aiScene* scene)
{
    if (!reference.empty() && reference[0] == '*')
    {
        const aiTexture* texture = scene->GetEmbeddedTexture(reference.c_str());
        if (!texture)
            return m_textureManager.GetFallback(semantic);

        const std::string key = m_path + "::" + reference;
        if (texture->mHeight == 0)
        {
            return m_textureManager.LoadEncodedMemory(
                key, reinterpret_cast<const unsigned char*>(texture->pcData),
                static_cast<int>(texture->mWidth), semantic, m_flipUVs);
        }

        return m_textureManager.LoadRawBGRA(
            key, reinterpret_cast<const unsigned char*>(texture->pcData),
            static_cast<int>(texture->mWidth), static_cast<int>(texture->mHeight), semantic);
    }

    fs::path texturePath(reference);
    if (texturePath.is_relative())
        texturePath = fs::path(m_directory) / texturePath;
    return m_textureManager.Load(texturePath.generic_string(), semantic, m_flipUVs);
}

unsigned int Model::LoadMaterialTexture(aiMaterial* material, int textureTypeValue,
                                        TextureManager::Semantic semantic,
                                        const aiScene* scene, bool& found)
{
    const aiTextureType textureType = static_cast<aiTextureType>(textureTypeValue);
    if (material->GetTextureCount(textureType) == 0)
    {
        found = false;
        return m_textureManager.GetFallback(semantic);
    }

    aiString texturePath;
    if (material->GetTexture(textureType, 0, &texturePath) != AI_SUCCESS)
    {
        found = false;
        return m_textureManager.GetFallback(semantic);
    }

    found = true;
    return LoadTextureReference(texturePath.C_Str(), semantic, scene);
}

Material Model::ProcessMaterial(aiMaterial* material, const aiScene* scene,
                                bool tangentSpaceValid)
{
    Material result;
    glm::vec4 colour(1.0f);
    if (!getColour(material, AI_MATKEY_BASE_COLOR, colour))
        getColour(material, AI_MATKEY_COLOR_DIFFUSE, colour);
    result.baseColour = colour;

    aiColor3D emissive(0.0f, 0.0f, 0.0f);
    if (material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
        result.emissiveColour = glm::vec3(emissive.r, emissive.g, emissive.b);

    result.metallic = std::clamp(getFloatOr(material, AI_MATKEY_METALLIC_FACTOR, 0.0f), 0.0f, 1.0f);
    result.roughness = std::clamp(getFloatOr(material, AI_MATKEY_ROUGHNESS_FACTOR, 0.65f), 0.045f, 1.0f);
    result.ambientOcclusion = 1.0f;
    result.tangentSpaceValid = tangentSpaceValid;

    bool found = false;
    result.baseColourMap = LoadMaterialTexture(material, aiTextureType_BASE_COLOR,
        TextureManager::Semantic::BaseColour, scene, found);
    result.hasBaseColourMap = found;
    if (!found)
    {
        result.baseColourMap = LoadMaterialTexture(material, aiTextureType_DIFFUSE,
            TextureManager::Semantic::BaseColour, scene, found);
        result.hasBaseColourMap = found;
    }

    result.normalMap = LoadMaterialTexture(material, aiTextureType_NORMALS,
        TextureManager::Semantic::Normal, scene, found);
    result.hasNormalMap = found && tangentSpaceValid;
    if (!found)
    {
        result.normalMap = LoadMaterialTexture(material, aiTextureType_HEIGHT,
            TextureManager::Semantic::Normal, scene, found);
        result.hasNormalMap = found && tangentSpaceValid;
    }

    result.metallicMap = LoadMaterialTexture(material, aiTextureType_METALNESS,
        TextureManager::Semantic::Metallic, scene, found);
    result.hasMetallicMap = found;

    result.roughnessMap = LoadMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS,
        TextureManager::Semantic::Roughness, scene, found);
    result.hasRoughnessMap = found;

    result.aoMap = LoadMaterialTexture(material, aiTextureType_AMBIENT_OCCLUSION,
        TextureManager::Semantic::AmbientOcclusion, scene, found);
    result.hasAOMap = found;
    if (!found)
    {
        result.aoMap = LoadMaterialTexture(material, aiTextureType_LIGHTMAP,
            TextureManager::Semantic::AmbientOcclusion, scene, found);
        result.hasAOMap = found;
    }

    result.emissiveMap = LoadMaterialTexture(material, aiTextureType_EMISSIVE,
        TextureManager::Semantic::Emissive, scene, found);
    result.hasEmissiveMap = found;
    if (!found)
    {
        result.emissiveMap = LoadMaterialTexture(material, aiTextureType_EMISSION_COLOR,
            TextureManager::Semantic::Emissive, scene, found);
        result.hasEmissiveMap = found;
    }

    result.specularMap = LoadMaterialTexture(material, aiTextureType_SPECULAR,
        TextureManager::Semantic::Specular, scene, found);
    result.hasSpecularMap = found;

    return result;
}

void Model::Draw(const Shader& shader, const glm::mat4& modelMatrix) const
{
    if (!m_loaded)
        return;

    shader.setBool("useInstancing", false);
    shader.setMat4("model", modelMatrix);
    for (const ModelMesh& mesh : m_meshes)
        mesh.Draw(shader);
}

void Model::DrawInstanced(const Shader& shader,
                          const std::vector<glm::mat4>& modelMatrices) const
{
    if (!m_loaded || modelMatrices.empty())
        return;

    // The non-instanced model uniform remains identity. Vertex shaders select
    // the per-instance matrix from attributes 5..8 when useInstancing is true.
    shader.setMat4("model", glm::mat4(1.0f));
    for (const ModelMesh& mesh : m_meshes)
        mesh.DrawInstanced(shader, modelMatrices);
}
