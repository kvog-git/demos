// SPDX-License-Identifier: 0BSD

#ifndef _SHADERS_HH_
#define _SHADERS_HH_

#include "../common/c_cpp/thirdparty/glad33/glad33.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>

#define COUNTOF(arr) (sizeof(arr) / sizeof(*(arr)))

// Model type
enum
{
  MODEL_QUAD = 0,
  MODEL_CUBE,
  _MODEL_COUNT
};

struct Shader
{
  const char* path;
  int model;
  void(*ui_fn)(void);
  void(*uf_fn)(Shader* prog);

  // Private
  bool ready = false;
  GLuint program;

  inline GLint get_required_uniform(const char* name)
  {
    GLint id = glGetUniformLocation(program, name);
    if (id == -1) {
      SDL_Log("Shader %s missing required uniform %s", path, name);
    }
    return id;
  }
};

std::vector<Shader*>& GetShaders();

void AssertGL(GLenum error, const char* expr, int line);
#define GL(expr)                                                    \
  expr;                                                             \
  for (GLenum _glcode; (_glcode = glGetError()) != GL_NO_ERROR; ) { \
    AssertGL(_glcode, #expr, __LINE__);                             \
  }


class ShaderRegisterHelper
{
public:
  ShaderRegisterHelper(Shader* shader)
  {
    std::vector<Shader*>& shaders = GetShaders();
    shaders.push_back(shader);
    std::sort(shaders.begin(), shaders.end(), [](const Shader* a, const Shader* b) {
      return std::strcmp(a->path, b->path) < 0;
    });
  }
};
#define ENABLE_SHADER(shader) static ShaderRegisterHelper _shader_reg = ShaderRegisterHelper(&shader);

#endif // _SHADERS_HH_
