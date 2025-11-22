// --------------------------------------------------------------------------------
// OpenGL shader sandbox
//
// SPDX-License-Identifier: 0BSD
// --------------------------------------------------------------------------------

#define GLAD_GL_IMPLEMENTATION
#include "../common/c_cpp/thirdparty/glad33/glad33.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "imgui.h"
#include "imgui_impl_opengl3.cpp"
#include "imgui_impl_sdl3.cpp"

#include <cctype>
#include <cmath>
#include <cstddef>

// --------------------------------------------------------------------------------
// Geometry
// --------------------------------------------------------------------------------

template <typename T>
static inline T Min(T lhs, T rhs)
{
  return (lhs < rhs) ? lhs : rhs;
}

template <typename T>
static inline T Max(T lhs, T rhs)
{
  return (lhs > rhs) ? lhs : rhs;
}

template <typename T>
static inline T Clamp(T val, T vmin, T vmax)
{
  return Min(Max(val, vmin), vmax);
}

static inline float Radians(float degrees)
{
  return degrees * M_PI / 180.0f;
}

class Vec2
{
public:
  float x = 0.0f;
  float y = 0.0f;
public:
  Vec2() = default;
  Vec2(float x, float y) : x(x), y(y) { }
};

class Vec3
{
public:
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
public:
  Vec3() = default;
  Vec3(float x, float y, float z) : x(x), y(y), z(z) { }

  inline Vec3 operator+(const Vec3& rhs) const
  {
    return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
  }

  inline Vec3 operator-(const Vec3& rhs) const
  {
    return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
  }

  inline Vec3 operator*(float scalar) const
  {
    return Vec3(x * scalar, y * scalar, z * scalar);
  }

  inline float Length() const
  {
    return std::sqrtf(x * x + y * y + z * z);
  }

  inline Vec3 Normalize() const
  {
    float l = Length();
    if (l == 0.0f) { l = 1.0f; }
    return Vec3(x / l, y / l, z / l);
  }

  inline float Dot(const Vec3& rhs) const
  {
    return x * rhs.x + y * rhs.y + z * rhs.z;
  }

  inline Vec3 Cross(const Vec3& rhs) const
  {
    return Vec3(
      y * rhs.z - z * rhs.y,
      z * rhs.x - x * rhs.z,
      x * rhs.y - y * rhs.x
    );
  }
public:
  static Vec3 FromEuler(Vec2 angles)
  {
    const float rx = Radians(angles.x);
    const float ry = Radians(angles.y);
    const float x = cosf(ry) * sinf(rx);
    const float y = sinf(ry);
    const float z = cosf(ry) * cosf(rx);
    return Vec3(x, y, z);
  }
};

class Vec4
{
public:
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 1.0f;
public:
  Vec4() = default;
  Vec4(float x, float y, float z) : x(x), y(y), z(z), w(1.0f) { }
  Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) { }
};

class Mat4x4
{
private:
  float m[4 * 4];
public:
  inline float& operator[](std::size_t s)       { return m[s]; }
  inline float  operator[](std::size_t s) const { return m[s]; }

  inline float* Base() { return m; }

  inline Mat4x4 operator*(const Mat4x4& rhs)
  {
    Mat4x4 r = { };
    for (int col = 0; col < 4; ++col) {
    for (int row = 0; row < 4; ++row) {
      r[col * 4 + row] =
        m[0 * 4 + row] * rhs[col * 4 + 0] +
        m[1 * 4 + row] * rhs[col * 4 + 1] +
        m[2 * 4 + row] * rhs[col * 4 + 2] +
        m[3 * 4 + row] * rhs[col * 4 + 3];
    }
    }
    return r;
  }
public:
  static Mat4x4 LookAt(const Vec3& eye, const Vec3& at, const Vec3& up)
  {
    Vec3 f = (at - eye).Normalize();
    Vec3 s = f.Cross(up).Normalize();
    Vec3 u = s.Cross(f);
    Vec3 t = Vec3(-s.Dot(eye), -u.Dot(eye), f.Dot(eye));

    Mat4x4 m;
    m[ 0] = s.x; m[ 1] = u.x; m[ 2] = -f.x; m[ 3] = 0.0f;
    m[ 4] = s.y; m[ 5] = u.y; m[ 6] = -f.y; m[ 7] = 0.0f;
    m[ 8] = s.z; m[ 9] = u.z; m[10] = -f.z; m[11] = 0.0f;
    m[12] = t.x; m[13] = t.y; m[14] =  t.z; m[15] = 1.0f;

    return m;
  }

  static Mat4x4 Perspective(float fov_deg, float aspect, float z_near, float z_far)
  {
    const float fov_cot = 1.0f / std::tanf(Radians(fov_deg) / 2.0f);

    Mat4x4 m = { };

    m[0*4+0] = fov_cot / aspect;
    m[1*4+1] = fov_cot;
    m[2*4+3] = -1.0f;

    m[2*4+2] = (z_far + z_near) / (z_near - z_far);
    m[3*4+2] = (2.0f * z_near * z_far) / (z_near - z_far);

    return m;
  }
};

// --------------------------------------------------------------------------------
// Shader metadata parser
// --------------------------------------------------------------------------------

enum : int
{
  SHADER_MODEL_TYPE_INVALID = 0,
  SHADER_MODEL_TYPE_QUAD,
  SHADER_MODEL_TYPE_CUBE,
};

enum : int
{
  SHADER_UNIFORM_TYPE_INVALID = 0,
  SHADER_UNIFORM_TYPE_INT,
  SHADER_UNIFORM_TYPE_FLOAT,
  SHADER_UNIFORM_TYPE_COLOR,
};

struct ShaderUniform
{
  char name[32];
  int  type;
  int   val_i1;
  float val_fl;
  Vec4  val_v4;
  int min_i;
  int max_i;
};

struct ShaderMetadata
{
  int                     model;
  ImVector<ShaderUniform> uniforms;
};

static bool ReadToken(const char** src, const char** out, int* len)
{
  while (isspace(**src)) {
    ++*src;
  }
  if (!**src) {
    return false;
  }

  *len = 0;
  *out = *src;
  while (**src && !isspace(**src)) {
    ++*src;
    ++*len;
  }

  return true;
}

static bool TokenEq(const char* tok, int len, const char* str)
{
  return (int)strlen(str) == len && !strncmp(tok, str, len);
}

static int TokenInt(const char* tok, int len)
{
  char tmp[64] = { };
  strncpy(tmp, tok, Min((int)sizeof(tmp), len));
  return atoi(tmp);
}

static float TokenFloat(const char* tok, int len)
{
  char tmp[64] = { };
  strncpy(tmp, tok, Min((int)sizeof(tmp), len));
  return strtof(tmp, NULL);
}

static void ParseShaderMetadata(ShaderMetadata* sm, const char* src)
{
  *sm = ShaderMetadata();

  while ((src = strchr(src, '@'))) {
    // @model <quad|cube>
    if (!strncmp(src, "@model", 6)) {
      src += 6;

      const char* tok;
      int         len;
      if (!ReadToken(&src, &tok, &len)) {
        SDL_Log("@model: Missing parameter");
        continue;
      }

      if (TokenEq(tok, len, "quad")) {
        sm->model = SHADER_MODEL_TYPE_QUAD;
      } else if (TokenEq(tok, len, "cube")) {
        sm->model = SHADER_MODEL_TYPE_CUBE;
      } else {
        SDL_Log("@model: Invalid parameter: %.*s", len, tok);
      }

      continue;
    }

    // @uniform <type> <name> <val>
    else if (!strncmp(src, "@uniform", 8)) {
      src += 8;

      ShaderUniform u = { };

      const char* tok;
      int         len;
      if (!ReadToken(&src, &tok, &len)) {
        SDL_Log("@uniform: Missing parameter");
        continue;
      }
      const char* type_tok = tok;
      int         type_len = len;

      if (!ReadToken(&src, &tok, &len)) {
        SDL_Log("@uniform: Missing parameter");
        continue;
      }
      strncpy(u.name, tok, Min((int)sizeof(u.name), len));

      // i min max
      if (TokenEq(type_tok, type_len, "int")) {
        u.type = SHADER_UNIFORM_TYPE_INT;

        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_i1 = TokenInt(tok, len);
        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.min_i = TokenInt(tok, len);
        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.max_i = TokenInt(tok, len);
      }
      // f
      else if (TokenEq(type_tok, type_len, "float")) {
        u.type = SHADER_UNIFORM_TYPE_FLOAT;

        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_fl = TokenFloat(tok, len);
      }
      // f f f f
      else if (TokenEq(type_tok, type_len, "color")) {
        u.type = SHADER_UNIFORM_TYPE_COLOR;

        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_v4.x = TokenFloat(tok, len);
        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_v4.y = TokenFloat(tok, len);
        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_v4.z = TokenFloat(tok, len);
        if (!ReadToken(&src, &tok, &len)) {
          SDL_Log("@uniform: No value");
          continue;
        }
        u.val_v4.w = TokenFloat(tok, len);
      } else {
        SDL_Log("@uniform: Unsupported type: %.*s", type_len, type_tok);
        continue;
      }

      sm->uniforms.push_back(u);

      continue;
    }
  }
}

// --------------------------------------------------------------------------------
// Application
// --------------------------------------------------------------------------------

static struct
{
  SDL_Window*     wnd;
  ShaderMetadata  sm;
  bool            ui_hidden;

  GLuint shader;

  float cam_dist = 3.5f;
  Vec2  cam_ang  = Vec2(0, 0);

  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  int    ibo_len;
} G = { };

static inline bool Is3D()
{
  return G.sm.model == SHADER_MODEL_TYPE_CUBE;
}

// Helper: Report errors via glGetError() after every OpenGL function call.
//         macOS does not support glDebugMessageCallback
static void AssertGL(GLenum error, const char* expr, int line)
{
  if (error != GL_NO_ERROR) {
    const char* error_string;
    switch (error) {
#define BIND_ERROR(name) case name: { error_string = #name; }; break;
      BIND_ERROR(GL_INVALID_ENUM);
      BIND_ERROR(GL_INVALID_VALUE);
      BIND_ERROR(GL_INVALID_OPERATION);
      BIND_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
      BIND_ERROR(GL_OUT_OF_MEMORY);
#undef BIND_ERROR
      default: SDL_assert(0 && "Invalid GL_ERROR enum");
    }
    SDL_Log("[Line #%u] %s caused %s\n", line, expr, error_string);
  }
}
#define GL(expr)                                                    \
  expr;                                                             \
  for (GLenum _glcode; (_glcode = glGetError()) != GL_NO_ERROR; ) { \
    AssertGL(_glcode, #expr, __LINE__);                             \
  }

SDL_AppResult SDLCALL SDL_AppInit(void**, int argc, char* argv[])
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  const char* path = (argc > 1) ? argv[1] : NULL;
  if (!path) {
    SDL_Log("No shader file supplied");
    return SDL_APP_FAILURE;
  }

  char* src = (char*)SDL_LoadFile(path, NULL);
  if (!src) {
    SDL_Log("Failed to load %s: %s", path, SDL_GetError());
    return SDL_APP_FAILURE;
  }

  ParseShaderMetadata(&G.sm, src);

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  char title[64]; snprintf(title, sizeof(title), "shaders: %s", path);
  Uint32 wflags = SDL_WINDOW_OPENGL             |
                  SDL_WINDOW_HIGH_PIXEL_DENSITY |
                  SDL_WINDOW_RESIZABLE;
  G.wnd = SDL_CreateWindow(title, 1024, 768, wflags);
  if (!G.wnd) {
    SDL_Log("Failed to create window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_GL_CreateContext(G.wnd)) {
    SDL_Log("Failed to create OpenGL context: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_GL_SetSwapInterval(1);

  if (!gladLoadGL(SDL_GL_GetProcAddress)) {
    SDL_Log("Failed to load OpenGL functions");
    return SDL_APP_FAILURE;
  }

  GL(glGenVertexArrays(1, &G.vao));
  GL(glBindVertexArray(G.vao));

  GL(glGenBuffers(1, &G.vbo));
  GL(glBindBuffer(GL_ARRAY_BUFFER, G.vbo));

  GL(glGenBuffers(1, &G.ibo));
  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, G.ibo));

  switch (G.sm.model) {
  case SHADER_MODEL_TYPE_QUAD: {
    //
    //   0            1
    // +------------+
    // |            |
    // |            |
    // |            |
    // |            |
    // | 3          | 2
    // +------------+
    //
    struct V { Vec2 p; Vec2 t; };
    const V vdata[] = {
      { Vec2(-1.0f,  1.0f), Vec2(0, 1) },
      { Vec2( 1.0f,  1.0f), Vec2(1, 1) },
      { Vec2( 1.0f, -1.0f), Vec2(1, 0) },
      { Vec2(-1.0f, -1.0f), Vec2(0, 0) },
    };
    const uint16_t idata[] = {
      0, 1, 2,
      0, 2, 3,
    };
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vdata), vdata, GL_STATIC_DRAW));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idata), idata, GL_STATIC_DRAW));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, p)));
    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, t)));
    GL(glEnableVertexAttribArray(1));
    G.ibo_len = sizeof(idata) / sizeof(*idata);
  } break;
  case SHADER_MODEL_TYPE_CUBE: {
    //
    //      +------------+
    //     /|           /|
    //    / |          / |
    //   +------------+  |
    //   |  |         |  |
    //   |  |         |  |
    //   |  +---------|--+
    //   | /          | /
    //   |/           |/
    //   +------------+
    //
    struct V { Vec3 p; Vec2 t; Vec3 n; };
    const V vdata[] = {
      // Front
      { Vec3(-1,  1,  1), Vec2(0, 1), Vec3(0, 0, 1) },
      { Vec3( 1,  1,  1), Vec2(1, 1), Vec3(0, 0, 1) },
      { Vec3( 1, -1,  1), Vec2(1, 0), Vec3(0, 0, 1) },
      { Vec3(-1, -1,  1), Vec2(0, 0), Vec3(0, 0, 1) },
      // Right
      { Vec3( 1,  1,  1), Vec2(0, 1), Vec3(1, 0, 0) },
      { Vec3( 1,  1, -1), Vec2(1, 1), Vec3(1, 0, 0) },
      { Vec3( 1, -1, -1), Vec2(1, 0), Vec3(1, 0, 0) },
      { Vec3( 1, -1,  1), Vec2(0, 0), Vec3(1, 0, 0) },
      // Back
      { Vec3( 1,  1, -1), Vec2(0, 1), Vec3(0, 0, -1) },
      { Vec3(-1,  1, -1), Vec2(1, 1), Vec3(0, 0, -1) },
      { Vec3(-1, -1, -1), Vec2(1, 0), Vec3(0, 0, -1) },
      { Vec3( 1, -1, -1), Vec2(0, 0), Vec3(0, 0, -1) },
      // Left
      { Vec3(-1,  1, -1), Vec2(0, 1), Vec3(-1, 0, 0) },
      { Vec3(-1,  1,  1), Vec2(1, 1), Vec3(-1, 0, 0) },
      { Vec3(-1, -1,  1), Vec2(1, 0), Vec3(-1, 0, 0) },
      { Vec3(-1, -1, -1), Vec2(0, 0), Vec3(-1, 0, 0) },
      // Top
      { Vec3(-1,  1, -1), Vec2(0, 1), Vec3(0, 1, 0) },
      { Vec3( 1,  1, -1), Vec2(1, 1), Vec3(0, 1, 0) },
      { Vec3( 1,  1,  1), Vec2(1, 0), Vec3(0, 1, 0) },
      { Vec3(-1,  1,  1), Vec2(0, 0), Vec3(0, 1, 0) },
      // Bottom
      { Vec3(-1, -1,  1), Vec2(0, 1), Vec3(0, -1, 0) },
      { Vec3( 1, -1,  1), Vec2(1, 1), Vec3(0, -1, 0) },
      { Vec3( 1, -1, -1), Vec2(1, 0), Vec3(0, -1, 0) },
      { Vec3(-1, -1, -1), Vec2(0, 0), Vec3(0, -1, 0) },
    };
    const uint16_t idata[] = {
       0,  1,  2,
       0,  2,  3,
       4,  5,  6,
       4,  6,  7,
       8,  9, 10,
       8, 10, 11,
      12, 13, 14,
      12, 14, 15,
      16, 17, 18,
      16, 18, 19,
      20, 21, 22,
      20, 22, 23,
    };
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vdata), vdata, GL_STATIC_DRAW));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idata), idata, GL_STATIC_DRAW));
    GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, p)));
    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, t)));
    GL(glEnableVertexAttribArray(1));
    GL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n)));
    GL(glEnableVertexAttribArray(2));
    G.ibo_len = sizeof(idata) / sizeof(*idata);
  } break;
  default: return SDL_APP_FAILURE;
  }

  const char* vs_table[] = {
    NULL,
    "vert_quad.glsl",
    "vert_cube.glsl",
  };
  char* vs_src = (char*)SDL_LoadFile(vs_table[G.sm.model], NULL);
  if (!vs_src) {
    SDL_Log("Failed to load vertex shader %s: %s", vs_table[G.sm.model], SDL_GetError());
    return SDL_APP_FAILURE;
  }
  char* fs_src = src;

  struct {
    GLenum      type;
    const char* src;
  } shaders[] = {
    { .type = GL_VERTEX_SHADER,   .src = vs_src },
    { .type = GL_FRAGMENT_SHADER, .src = fs_src },
  };
  G.shader = GL(glCreateProgram());
  for (size_t i = 0; i < sizeof(shaders) / sizeof(*shaders); ++i) {
    GLuint shader = GL(glCreateShader(shaders[i].type));
    GL(glShaderSource(shader, 1, &shaders[i].src, NULL));
    GL(glCompileShader(shader));
    GLint compile_status = 0;
    GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status));
    if (compile_status != GL_TRUE) {
      char error[1024] = { 0 };
      GL(glGetShaderInfoLog(shader, sizeof(error), NULL, error));
      SDL_Log("Error compiling shader %s", error);
      return SDL_APP_FAILURE;
    }
    GL(glAttachShader(G.shader, shader));
  }
  GL(glLinkProgram(G.shader));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplSDL3_Init(G.wnd, 0, 0);
  ImGui_ImplOpenGL3_Init();
  ImGui::GetIO().IniFilename = NULL;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppIterate(void*)
{
  int wnd_x = 0;
  int wnd_y = 0;
  SDL_GetWindowSizeInPixels(G.wnd, &wnd_x, &wnd_y);

  GL(glViewport(0, 0, wnd_x, wnd_y));
  GL(glEnable(GL_CULL_FACE));
  GL(glCullFace(GL_FRONT));
  GL(glEnable(GL_DEPTH_TEST));
  GL(glEnable(GL_BLEND));
  GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  GL(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
  GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  GL(glUseProgram(G.shader));
  GL(glBindVertexArray(G.vao));

  GLint u_id = -1;
  if ((u_id = glGetUniformLocation(G.shader, "u_res")) != -1) {
    GL(glUniform2f(u_id, (float)wnd_x, (float)wnd_y));
  }
  if ((u_id = glGetUniformLocation(G.shader, "u_time")) != -1) {
    GL(glUniform1f(u_id, SDL_GetTicks() / 1e3f));
  }

  if (Is3D()) {
    const Vec3  cam = Vec3::FromEuler(G.cam_ang) * G.cam_dist;
    const Vec3  tgt = Vec3(0, 0, 0);
    const float fov = 75.0f;

    Mat4x4 m_look = Mat4x4::LookAt(cam, tgt, Vec3(0, 1, 0));
    Mat4x4 m_proj = Mat4x4::Perspective(fov, (float)wnd_x / (float)wnd_y, 0.1f, 100.0f);
    Mat4x4 m_view = m_proj * m_look;

    if ((u_id = glGetUniformLocation(G.shader, "u_vmat")) == -1) {
      SDL_Log("Warning: u_vmat uniform not found");
    } else {
      GL(glUniformMatrix4fv(u_id, 1, GL_FALSE, m_view.Base()));
    }

    if ((u_id = glGetUniformLocation(G.shader, "u_cam")) != -1) {
      GL(glUniform3f(u_id, cam.x, cam.y, cam.z));
    }
  }

  for (auto& u : G.sm.uniforms) {
    if ((u_id = glGetUniformLocation(G.shader, u.name)) == -1) {
      SDL_Log("Warning: Couldn't find a uniform with the name %s", u.name);
      continue;
    }
    switch (u.type) {
    case SHADER_UNIFORM_TYPE_INT: {
      GL(glUniform1i(u_id, u.val_i1));
    } break;
    case SHADER_UNIFORM_TYPE_FLOAT: {
      GL(glUniform1f(u_id, u.val_fl));
    } break;
    case SHADER_UNIFORM_TYPE_COLOR: {
      GL(glUniform4f(u_id, u.val_v4.x, u.val_v4.y, u.val_v4.z, u.val_v4.w));
    } break;
    }
  }

  GL(glDrawElements(GL_TRIANGLES, G.ibo_len, GL_UNSIGNED_SHORT, NULL));

  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  int wnd_height = 0;
  SDL_GetWindowSize(G.wnd, NULL, &wnd_height);
  if (!G.ui_hidden) {
    ImGui::SetNextWindowPos(ImVec2(15.0f, 15.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(200.0f, wnd_height - 30.0f), ImGuiCond_Once);
    ImGuiWindowFlags flags = 
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoMove     |
      ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("Test", NULL, flags)) {
      ImGui::SeparatorText("Controls");
      ImGui::Text("Tab: Toggle UI");
      if (G.sm.uniforms.size() > 0) {
        ImGui::SeparatorText("Uniforms");
        for (auto& u : G.sm.uniforms) {
          switch (u.type) {
          case SHADER_UNIFORM_TYPE_INT: {
            ImGui::SliderInt(u.name, &u.val_i1, u.min_i, u.max_i);
          } break;
          case SHADER_UNIFORM_TYPE_FLOAT: {
            ImGui::DragFloat(u.name, &u.val_fl, 0.01f);
          } break;
          case SHADER_UNIFORM_TYPE_COLOR: {
            ImGui::ColorEdit4(u.name, &u.val_v4.x);
          } break;
          }
        }
      }
    }
    ImGui::End();
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  if (!SDL_GL_SwapWindow(G.wnd)) {
    SDL_Log("Failed to present window: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDLCALL SDL_AppEvent(void*, SDL_Event* event)
{
  ImGui_ImplSDL3_ProcessEvent(event);
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    if (event->key.key == SDLK_TAB) {
      G.ui_hidden = !G.ui_hidden;
    }
  } break;
  case SDL_EVENT_MOUSE_MOTION: {
    if (Is3D() && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) {
      const float sens = 0.4f;
      G.cam_ang.x -= event->motion.xrel * sens;
      G.cam_ang.y += event->motion.yrel * sens;
      G.cam_ang.y = Clamp(G.cam_ang.y, -89.0f, 89.0f);
    }
  } break;
  case SDL_EVENT_MOUSE_WHEEL: {
    if (Is3D()) {
      G.cam_dist -= event->wheel.y * 0.1f;
      G.cam_dist = Max(G.cam_dist, 1.0f);
    }
  } break;
  case SDL_EVENT_QUIT: {
    return SDL_APP_SUCCESS;
  } break;
  }
  return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void*, SDL_AppResult)
{
  SDL_DestroyWindow(G.wnd);
}
