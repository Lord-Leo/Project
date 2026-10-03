#ifndef MODEL_H
#define MODEL_H

#include "ModelMesh.h"
#include "TextureManager.h"
#include <glm/glm.hpp>
#include <assimp/matrix4x4.h>
#include <memory>
#include <string>
#include <vector>

struct aiScene;
struct aiNode;
struct aiMesh;
struct aiMaterial;
struct aiTexture;

struct ModelLoadOptions
{
    // UV flipping is explicit per asset. It is intentionally false by
    // default so glTF/GLB assets are never blindly inverted.
    bool flipUVs = false;
    bool optimizeMeshes = true;
};

class Model
{
public:
    Model(const std::string& path, TextureManager& textureManager,
          const ModelLoadOptions& options = {});

    bool IsLoaded() const { return m_loaded; }
    const std::string& GetPath() const { return m_path; }
    const glm::vec3& GetLocalBoundsMin() const { return m_boundsMin; }
    const glm::vec3& GetLocalBoundsMax() const { return m_boundsMax; }

    void Draw(const Shader& shader, const glm::mat4& modelMatrix) const;
    void DrawInstanced(const Shader& shader, const std::vector<glm::mat4>& modelMatrices) const;

private:
    void Load(const ModelLoadOptions& options);
    void ProcessNode(aiNode* node, const aiScene* scene, const glm::mat4& parentTransform);
    ModelMesh ProcessMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& nodeTransform);
    Material ProcessMaterial(aiMaterial* material, const aiScene* scene, bool tangentSpaceValid);

    unsigned int LoadMaterialTexture(aiMaterial* material, int textureType,
                                     TextureManager::Semantic semantic,
                                     const aiScene* scene, bool& found);
    unsigned int LoadTextureReference(const std::string& reference,
                                      TextureManager::Semantic semantic,
                                      const aiScene* scene);

    static glm::mat4 ConvertMatrix(const aiMatrix4x4& assimpMatrix);
    static void WarnMissingModelOnce(const std::string& path, const std::string& detail);

    std::string m_path;
    std::string m_directory;
    TextureManager& m_textureManager;
    std::vector<ModelMesh> m_meshes;
    glm::vec3 m_boundsMin{0.0f};
    glm::vec3 m_boundsMax{0.0f};
    bool m_loaded = false;
    bool m_flipUVs = false;
};

#endif // MODEL_H
