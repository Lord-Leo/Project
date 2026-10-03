#include "Shader.h"
#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string Shader::readFile(const std::string& path)
{
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try
    {
        file.open(path);
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();
        return stream.str();
    }
    catch (const std::ifstream::failure&)
    {
        std::cerr << "[Shader] ERROR: could not read shader file: " << path << std::endl;
        throw std::runtime_error("Failed to read shader file: " + path);
    }
}

unsigned int Shader::compile(unsigned int type, const std::string& source, const std::string& debugName)
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[2048];
        glGetShaderInfoLog(shader, 2048, nullptr, infoLog);
        std::cerr << "[Shader] ERROR: compilation failed (" << debugName << "):\n" << infoLog << std::endl;
        glDeleteShader(shader);
        throw std::runtime_error("Shader compilation failed: " + debugName);
    }
    return shader;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexCode = readFile(vertexPath);
    std::string fragmentCode = readFile(fragmentPath);

    unsigned int vertexShader = compile(GL_VERTEX_SHADER, vertexCode, vertexPath);
    unsigned int fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentCode, fragmentPath);

    ID = glCreateProgram();
    glAttachShader(ID, vertexShader);
    glAttachShader(ID, fragmentShader);
    glLinkProgram(ID);

    int success = 0;
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[2048];
        glGetProgramInfoLog(ID, 2048, nullptr, infoLog);
        std::cerr << "[Shader] ERROR: program linking failed (" << vertexPath << " / " << fragmentPath << "):\n"
                  << infoLog << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        throw std::runtime_error("Shader program link failed");
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    if (ID != 0)
        glDeleteProgram(ID);
}

void Shader::use() const
{
    glUseProgram(ID);
}

int Shader::getUniformLocation(const std::string& name) const
{
    return glGetUniformLocation(ID, name.c_str());
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(getUniformLocation(name), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setVec2(const std::string& name, float x, float y) const
{
    glUniform2f(getUniformLocation(name), x, y);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(getUniformLocation(name), x, y, z);
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(getUniformLocation(name), 1, &value[0]);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}
