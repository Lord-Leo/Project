#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <vector>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent{1.0f, 0.0f, 0.0f};
    glm::vec3 Bitangent{0.0f, 1.0f, 0.0f};
};

// Shared "one box, many uses" render/collision unit. Every renderable thing
// in the classroom (walls, desks, monitors, the door, ...) is one of these,
// drawn with the single shared unit-box Mesh and positioned via its own
// model matrix. `position` is the box's bottom-center in world space,
// `scale` is (width, height, depth) in meters.
struct BoxInstance
{
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 color{0.8f, 0.8f, 0.8f};
    glm::vec3 emissive{0.0f};
    float rotationY = 0.0f; // radians, rotation about the box's own vertical axis
    bool useTexture = false;
    unsigned int textureID = 0;
    glm::vec2 uvTiling{1.0f, 1.0f}; // how many times the texture repeats across this box's face
    float shininess = 16.0f;

    // Phase 7-8 rendering metadata. Collision ownership is unchanged even
    // when render=false because an imported visual model replaces this box.
    bool render = true;
    bool castsShadow = true;
    float metallic = 0.0f;
    float roughness = 0.65f;
    float ambientOcclusion = 1.0f;
    unsigned int normalTextureID = 0;
    unsigned int metallicTextureID = 0;
    unsigned int roughnessTextureID = 0;
    unsigned int aoTextureID = 0;
    unsigned int emissiveTextureID = 0;

    // Phase 3 material properties. `color` already served as the diffuse
    // term (kept as-is for compatibility); these add the remaining two
    // classic Phong components so materials can be tuned per object
    // instead of using one hardcoded global ambient/specular constant.
    float ambient = 0.04f;                        // ambient reflectance factor, multiplies diffuse color
    glm::vec3 specular{0.25f, 0.25f, 0.25f};      // specular tint/intensity, multiplies light color

    glm::mat4 GetModelMatrix() const;

    // World-space AABB. Correctly accounts for rotationY by transforming
    // all 8 corners and taking the min/max (a "conservative" AABB), so
    // collision stays correct even for the rotating door.
    glm::vec3 GetWorldMin() const;
    glm::vec3 GetWorldMax() const;
};


// A minimal mesh wrapper around a VAO/VBO/EBO. Phases 1-2 use procedural
// primitives only, so in practice a single unit-cube Mesh is created once
// and reused (scaled/positioned per object via the model matrix) for every
// piece of furniture and every wall/floor/ceiling panel in the scene.
class Mesh
{
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void Draw() const;

    // Builds a unit cube centered on X/Z (spanning -0.5..0.5) but resting on
    // Y=0..1, so that scaling + translating places the box's base exactly
    // at the given world position (matches how furniture/walls are placed
    // in Classroom.cpp).
    static Mesh* CreateUnitBox();

    // Builds a simple flat quad on the XY plane (-0.5..0.5), useful for the
    // whiteboard projection surface and screen-space UI.
    static Mesh* CreateQuad();

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    unsigned int indexCount = 0;
};

#endif // MESH_H
