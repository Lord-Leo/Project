#include "ModelMesh.h"
#include <glad/glad.h>
#include <cstddef>
#include <utility>

ModelMesh::ModelMesh(std::vector<ModelVertex> vertices,
                     std::vector<unsigned int> indices,
                     const Material& material)
    : m_vertices(std::move(vertices)),
      m_indices(std::move(indices)),
      m_material(material)
{
    SetupMesh();
}

ModelMesh::~ModelMesh()
{
    Release();
}

ModelMesh::ModelMesh(ModelMesh&& other) noexcept
{
    *this = std::move(other);
}

ModelMesh& ModelMesh::operator=(ModelMesh&& other) noexcept
{
    if (this == &other)
        return *this;

    Release();
    m_vertices = std::move(other.m_vertices);
    m_indices = std::move(other.m_indices);
    m_material = other.m_material;
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_instanceVBO = other.m_instanceVBO;
    m_instanceCapacity = other.m_instanceCapacity;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_instanceVBO = 0;
    other.m_instanceCapacity = 0;
    return *this;
}

void ModelMesh::Release()
{
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_instanceVBO = 0;
    m_ebo = 0;
    m_vbo = 0;
    m_vao = 0;
    m_instanceCapacity = 0;
}

void ModelMesh::SetupMesh()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_vertices.size() * sizeof(ModelVertex)),
                 m_vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_indices.size() * sizeof(unsigned int)),
                 m_indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<void*>(offsetof(ModelVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<void*>(offsetof(ModelVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<void*>(offsetof(ModelVertex, texCoords)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<void*>(offsetof(ModelVertex, tangent)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex),
                          reinterpret_cast<void*>(offsetof(ModelVertex, bitangent)));

    glBindVertexArray(0);
}

void ModelMesh::EnsureInstanceBuffer(size_t matrixCount) const
{
    if (!m_instanceVBO)
        glGenBuffers(1, &m_instanceVBO);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    if (matrixCount > m_instanceCapacity)
    {
        m_instanceCapacity = matrixCount;
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(m_instanceCapacity * sizeof(glm::mat4)),
                     nullptr, GL_DYNAMIC_DRAW);
    }

    const size_t vec4Size = sizeof(glm::vec4);
    for (unsigned int column = 0; column < 4; ++column)
    {
        const unsigned int attribute = 5 + column;
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              reinterpret_cast<void*>(column * vec4Size));
        glVertexAttribDivisor(attribute, 1);
    }
    glBindVertexArray(0);
}

void ModelMesh::Draw(const Shader& shader) const
{
    m_material.Apply(shader);
    shader.setBool("useInstancing", false);
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void ModelMesh::DrawInstanced(const Shader& shader,
                              const std::vector<glm::mat4>& modelMatrices) const
{
    if (modelMatrices.empty())
        return;

    EnsureInstanceBuffer(modelMatrices.size());
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(modelMatrices.size() * sizeof(glm::mat4)),
                    modelMatrices.data());

    m_material.Apply(shader);
    shader.setBool("useInstancing", true);
    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                            GL_UNSIGNED_INT, nullptr,
                            static_cast<GLsizei>(modelMatrices.size()));
    glBindVertexArray(0);
    shader.setBool("useInstancing", false);
}
