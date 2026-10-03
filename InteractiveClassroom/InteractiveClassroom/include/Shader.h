#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <glm/glm.hpp>

// Compiles and links a vertex+fragment shader pair from disk, and provides
// convenience setters for the uniform types this project needs.
class Shader
{
public:
    unsigned int ID;

    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    static std::string readFile(const std::string& path);
    static unsigned int compile(unsigned int type, const std::string& source, const std::string& debugName);
    int getUniformLocation(const std::string& name) const;
};

#endif // SHADER_H
