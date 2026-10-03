#include "Mesh.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <limits>

glm::mat4 BoxInstance::GetModelMatrix() const
{
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, rotationY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);
    return model;
}

static void computeCorners(const BoxInstance& box, glm::vec3& lo, glm::vec3& hi)
{
    glm::mat4 model = box.GetModelMatrix();
    lo = glm::vec3(std::numeric_limits<float>::max());
    hi = glm::vec3(std::numeric_limits<float>::lowest());
    const float xs[2] = { -0.5f, 0.5f };
    const float ys[2] = { 0.0f, 1.0f };
    const float zs[2] = { -0.5f, 0.5f };
    for (float x : xs) for (float y : ys) for (float z : zs)
    {
        glm::vec3 world = glm::vec3(model * glm::vec4(x, y, z, 1.0f));
        lo = glm::min(lo, world);
        hi = glm::max(hi, world);
    }
}

glm::vec3 BoxInstance::GetWorldMin() const
{
    glm::vec3 lo, hi;
    computeCorners(*this, lo, hi);
    return lo;
}

glm::vec3 BoxInstance::GetWorldMax() const
{
    glm::vec3 lo, hi;
    computeCorners(*this, lo, hi);
    return hi;
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
    indexCount = (unsigned int)indices.size();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    // texcoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
    // tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
    // bitangent
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void Mesh::Draw() const
{
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

Mesh* Mesh::CreateUnitBox()
{
    const glm::vec3 tx(1.0f, 0.0f, 0.0f);
    const glm::vec3 ntx(-1.0f, 0.0f, 0.0f);
    const glm::vec3 ty(0.0f, 1.0f, 0.0f);
    const glm::vec3 tz(0.0f, 0.0f, 1.0f);
    const glm::vec3 ntz(0.0f, 0.0f, -1.0f);

    // 24 vertices: 4 unique per face so normals, UVs and tangent space are
    // correct per face. Box spans X/Z [-0.5,0.5], Y [0,1].
    std::vector<Vertex> v = {
        // -Z face
        {{-0.5f,0.0f,-0.5f}, {0,0,-1}, {0,0}, tx, ty},
        {{ 0.5f,0.0f,-0.5f}, {0,0,-1}, {1,0}, tx, ty},
        {{ 0.5f,1.0f,-0.5f}, {0,0,-1}, {1,1}, tx, ty},
        {{-0.5f,1.0f,-0.5f}, {0,0,-1}, {0,1}, tx, ty},
        // +Z face
        {{ 0.5f,0.0f, 0.5f}, {0,0,1}, {0,0}, ntx, ty},
        {{-0.5f,0.0f, 0.5f}, {0,0,1}, {1,0}, ntx, ty},
        {{-0.5f,1.0f, 0.5f}, {0,0,1}, {1,1}, ntx, ty},
        {{ 0.5f,1.0f, 0.5f}, {0,0,1}, {0,1}, ntx, ty},
        // -X face
        {{-0.5f,0.0f, 0.5f}, {-1,0,0}, {0,0}, ntz, ty},
        {{-0.5f,0.0f,-0.5f}, {-1,0,0}, {1,0}, ntz, ty},
        {{-0.5f,1.0f,-0.5f}, {-1,0,0}, {1,1}, ntz, ty},
        {{-0.5f,1.0f, 0.5f}, {-1,0,0}, {0,1}, ntz, ty},
        // +X face
        {{ 0.5f,0.0f,-0.5f}, {1,0,0}, {0,0}, tz, ty},
        {{ 0.5f,0.0f, 0.5f}, {1,0,0}, {1,0}, tz, ty},
        {{ 0.5f,1.0f, 0.5f}, {1,0,0}, {1,1}, tz, ty},
        {{ 0.5f,1.0f,-0.5f}, {1,0,0}, {0,1}, tz, ty},
        // +Y face
        {{-0.5f,1.0f,-0.5f}, {0,1,0}, {0,0}, tx, tz},
        {{ 0.5f,1.0f,-0.5f}, {0,1,0}, {1,0}, tx, tz},
        {{ 0.5f,1.0f, 0.5f}, {0,1,0}, {1,1}, tx, tz},
        {{-0.5f,1.0f, 0.5f}, {0,1,0}, {0,1}, tx, tz},
        // -Y face
        {{-0.5f,0.0f, 0.5f}, {0,-1,0}, {0,0}, tx, ntz},
        {{ 0.5f,0.0f, 0.5f}, {0,-1,0}, {1,0}, tx, ntz},
        {{ 0.5f,0.0f,-0.5f}, {0,-1,0}, {1,1}, tx, ntz},
        {{-0.5f,0.0f,-0.5f}, {0,-1,0}, {0,1}, tx, ntz},
    };

    std::vector<unsigned int> idx;
    idx.reserve(36);
    for (unsigned int face = 0; face < 6; ++face)
    {
        unsigned int base = face * 4;
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 2); idx.push_back(base + 3); idx.push_back(base + 0);
    }
    return new Mesh(v, idx);
}

Mesh* Mesh::CreateQuad()
{
    const glm::vec3 tangent(1.0f, 0.0f, 0.0f);
    const glm::vec3 bitangent(0.0f, 1.0f, 0.0f);
    std::vector<Vertex> v = {
        {{-0.5f,-0.5f,0.0f}, {0,0,1}, {0,0}, tangent, bitangent},
        {{ 0.5f,-0.5f,0.0f}, {0,0,1}, {1,0}, tangent, bitangent},
        {{ 0.5f, 0.5f,0.0f}, {0,0,1}, {1,1}, tangent, bitangent},
        {{-0.5f, 0.5f,0.0f}, {0,0,1}, {0,1}, tangent, bitangent},
    };
    std::vector<unsigned int> idx = {0,1,2, 2,3,0};
    return new Mesh(v, idx);
}
