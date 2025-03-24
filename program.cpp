#include "program.hpp"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <cstring>

Program::ProgramBuildException::ProgramBuildException(int programHandle)
{
    std::memset(message_, 0, (kBufflen + 1));
    glGetProgramInfoLog(programHandle, kBufflen, &len_, &message_[0]);
}

const char* Program::ProgramBuildException::what() const
{
    return message_;
}

const char* Program::ProgramBuildException::what(
    std::size_t* len /* [out] */) const
{
    return (*len = this->len_), message_;
}

Program::ShaderBuildException::ShaderBuildException(int shaderHandle)
{
    std::memset(message_, 0, (kBufflen + 1));
    glGetShaderInfoLog(shaderHandle, kBufflen, &len_, &message_[0]);
}

const char* Program::ShaderBuildException::what() const
{
    return message_;
}

const char* Program::ShaderBuildException::what(
    std::size_t* len /* [out] */) const
{
    return (*len = this->len_), message_;
}

Program::Program()
{
    programHandle_ = glCreateProgram();
}

void Program::use()
{
    glUseProgram(programHandle_);
}

void Program::link()
{
    // Link program
    glLinkProgram(programHandle_);

    // Check for compile errors
    int ret;
    glGetProgramiv(programHandle_, GL_LINK_STATUS, &ret);
    if (ret == GL_FALSE) {
        throw Program::ProgramBuildException(programHandle_);
    }
}

void Program::set_value(const char* name, const bool value)
{
    glUniform1i(glGetUniformLocation(programHandle_, name), value);
}

void Program::set_value(const char* name, const int value)
{
    glUniform1i(glGetUniformLocation(programHandle_, name), value);
}

void Program::set_value(const char* name, const float value)
{
    glUniform1f(glGetUniformLocation(programHandle_, name), value);
}

void Program::set_value_vec3(const char* name, const float* value)
{
    glUniform3fv(glGetUniformLocation(programHandle_, name), 1, value);
}

void Program::set_value_mat3x3(const char* name, const float* value)
{
    glUniformMatrix3fv(
        glGetUniformLocation(programHandle_, name), 1, GL_FALSE, value);
}

void Program::set_value_vec4(const char* name, const float* value)
{
    glUniform4fv(glGetUniformLocation(programHandle_, name), 1, value);
}

void Program::set_value_mat4x4(const char* name, const float* value)
{
    glUniformMatrix4fv(
        glGetUniformLocation(programHandle_, name), 1, GL_FALSE, value);
}

namespace {

    // Helper
    inline void create_shader(const int programHandle,
                              const char* src,
                              const int type)
    {
        // Build and compile shader program
        const int shaderHandle = glCreateShader(type);

        glShaderSource(shaderHandle, 1, &src, nullptr);
        glCompileShader(shaderHandle);

        // Check status
        int ret;
        glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &ret);
        if (ret == GL_FALSE) {
            throw Program::ShaderBuildException(shaderHandle);
        }

        glAttachShader(programHandle, shaderHandle);
        glDeleteShader(shaderHandle);
    }
} // namespace

void Program::create_shader(const fragment_shader& s) const
{
    ::create_shader(programHandle_, s.src, GL_FRAGMENT_SHADER);
}

void Program::create_shader(const vertex_shader& s) const
{
    ::create_shader(programHandle_, s.src, GL_VERTEX_SHADER);
}
