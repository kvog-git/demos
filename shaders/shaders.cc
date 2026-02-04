// SPDX-License-Identifier: 0BSD

#define GLAD_GL_IMPLEMENTATION
#include "shaders.hh"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "imgui_impl_opengl3.cpp"
#include "imgui_impl_sdl3.cpp"

#include <cctype>
#include <cmath>
#include <cstddef>

// --------------------------------------------------------------------------------
// Geometry
// --------------------------------------------------------------------------------

constexpr float Pi = 3.1415926535f;

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
  return degrees * Pi / 180.0f;
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
// Application
// --------------------------------------------------------------------------------

typedef struct Model Model;
struct Model
{
  GLuint vao;
  GLuint vbo;
  GLuint ibo;
  GLuint vs;
  int    ibo_len;
  bool   is_3d;
};

static struct
{
  SDL_Window* wnd;
  bool        ui_hidden;
  bool        pause;

  int   shader_index = 0;

  float cam_dist = 3.5f;
  Vec2  cam_ang  = Vec2(0, 0);

  Model models[_MODEL_COUNT];
} G = { };

// Helper: Report errors via glGetError() after every OpenGL function call.
//         macOS does not support glDebugMessageCallback
void AssertGL(GLenum error, const char* expr, int line)
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

std::vector<Shader*>& GetShaders()
{
  static std::vector<Shader*> s_shaders = { };
  return s_shaders;
}

SDL_AppResult SDLCALL SDL_AppInit(void**, int, char**)
{
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  Uint32 wflags = SDL_WINDOW_OPENGL             |
                  SDL_WINDOW_HIGH_PIXEL_DENSITY |
                  SDL_WINDOW_RESIZABLE;
  G.wnd = SDL_CreateWindow("shaders", 1024, 768, wflags);
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

  // Quad mesh
  //   0            1
  // +------------+
  // |            |
  // |            |
  // |            |
  // |            |
  // | 3          | 2
  // +------------+
  //
  {
    Model* m = &G.models[MODEL_QUAD];

    GL(glGenVertexArrays(1, &m->vao));
    GL(glBindVertexArray(m->vao));

    GL(glGenBuffers(1, &m->vbo));
    GL(glBindBuffer(GL_ARRAY_BUFFER, m->vbo));

    GL(glGenBuffers(1, &m->ibo));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo));

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
    m->ibo_len = COUNTOF(idata);
    m->is_3d = false;
  }

  // Cube mesh
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
  {
    Model* m = &G.models[MODEL_CUBE];

    GL(glGenVertexArrays(1, &m->vao));
    GL(glBindVertexArray(m->vao));

    GL(glGenBuffers(1, &m->vbo));
    GL(glBindBuffer(GL_ARRAY_BUFFER, m->vbo));

    GL(glGenBuffers(1, &m->ibo));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo));

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
    m->ibo_len = COUNTOF(idata);
    m->is_3d = true;
  }

  const char* vshaders[] = {
    "vert_quad.glsl",
    "vert_cube.glsl"
  };
  static_assert(COUNTOF(vshaders) == _MODEL_COUNT);
  for (size_t i = 0; i < COUNTOF(vshaders); ++i) {
    char* vs_src = (char*)SDL_LoadFile(vshaders[i], NULL);
    if (!vs_src) {
      SDL_Log("Failed to load vertex shader %s: %s", vshaders[i], SDL_GetError());
      return SDL_APP_FAILURE;
    }

    GLuint shader = GL(glCreateShader(GL_VERTEX_SHADER));
    GL(glShaderSource(shader, 1, &vs_src, NULL));
    GL(glCompileShader(shader));

    GLint compile_status = 0;
    GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status));
    if (compile_status != GL_TRUE) {
      char error[1024] = { 0 };
      GL(glGetShaderInfoLog(shader, sizeof(error), NULL, error));
      SDL_Log("Error compiling shader %s: %s", vshaders[i], error);
      return SDL_APP_FAILURE;
    }

    G.models[i].vs = shader;
    SDL_Log("Compiled vertex shader %s", vshaders[i]);

    SDL_free(vs_src);
  }

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

  Shader* shader = GetShaders()[G.shader_index];
  Model* model = &G.models[shader->model];

  if (!shader->ready) {
    shader->program = GL(glCreateProgram());

    char* fs_src = (char*)SDL_LoadFile(shader->path, NULL);
    if (!fs_src) {
      SDL_Log("Failed to load fragment shader %s: %s", shader->path, SDL_GetError());
      return SDL_APP_FAILURE;
    }

    GLuint fs = GL(glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(fs, 1, &fs_src, NULL));
    GL(glCompileShader(fs));

    GLint compile_status = 0;
    GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &compile_status));
    if (compile_status != GL_TRUE) {
      char error[1024] = { 0 };
      GL(glGetShaderInfoLog(fs, sizeof(error), NULL, error));
      SDL_Log("Error compiling shader %s: %s", shader->path, error);
      return SDL_APP_FAILURE;
    }

    SDL_Log("Compiled fragment shader %s", shader->path);

    SDL_free(fs_src);

    GL(glAttachShader(shader->program, model->vs));
    GL(glAttachShader(shader->program, fs));
    GL(glLinkProgram(shader->program));

    GLint link_status = 0;
    GL(glGetProgramiv(shader->program, GL_LINK_STATUS, &link_status));
    if (link_status != GL_TRUE) {
      char error[1024] = { 0 };
      GL(glGetProgramInfoLog(shader->program, sizeof(error), NULL, error));
      SDL_Log("Error linking program %s: %s", shader->path, error);
      return SDL_APP_FAILURE;
    }

    SDL_Log("Linked program");

    shader->ready = true;
  }

  GL(glBindVertexArray(model->vao));
  GL(glUseProgram(shader->program));

  GLint u_id = -1;
  if ((u_id = glGetUniformLocation(shader->program, "u_res")) != -1) {
    GL(glUniform2f(u_id, (float)wnd_x, (float)wnd_y));
  }
  if ((u_id = glGetUniformLocation(shader->program, "u_time")) != -1) {
    if (!G.pause) {
      GL(glUniform1f(u_id, SDL_GetTicks() / 1e3f));
    }
  }

  if (model->is_3d) {
    const Vec3  cam = Vec3::FromEuler(G.cam_ang) * G.cam_dist;
    const Vec3  tgt = Vec3(0, 0, 0);
    const float fov = 75.0f;

    Mat4x4 m_look = Mat4x4::LookAt(cam, tgt, Vec3(0, 1, 0));
    Mat4x4 m_proj = Mat4x4::Perspective(fov, (float)wnd_x / (float)wnd_y, 0.1f, 100.0f);
    Mat4x4 m_view = m_proj * m_look;

    if ((u_id = glGetUniformLocation(shader->program, "u_vmat")) == -1) {
      SDL_Log("Warning: u_vmat uniform not found");
    } else {
      GL(glUniformMatrix4fv(u_id, 1, GL_FALSE, m_view.Base()));
    }

    if ((u_id = glGetUniformLocation(shader->program, "u_cam")) != -1) {
      GL(glUniform3f(u_id, cam.x, cam.y, cam.z));
    }
  }

  if (shader->uf_fn) {
    shader->uf_fn(shader);
  }

  GL(glDrawElements(GL_TRIANGLES, model->ibo_len, GL_UNSIGNED_SHORT, NULL));

  ImGui_ImplSDL3_NewFrame();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  int wnd_height = 0;
  SDL_GetWindowSize(G.wnd, NULL, &wnd_height);
  if (!G.ui_hidden) {
    ImGui::SetNextWindowPos(ImVec2(15.0f, 15.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300.0f, wnd_height - 30.0f), ImGuiCond_Once);
    ImGuiWindowFlags flags = 
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoMove     |
      ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("ControlsWindow", NULL, flags)) {
      if (ImGui::BeginCombo("Shader", GetShaders()[G.shader_index]->path)) {
        for (int i = 0; i < (int)GetShaders().size(); ++i) {
          if (ImGui::Selectable(GetShaders()[i]->path, i == G.shader_index)) {
            G.shader_index = i;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SeparatorText("Controls");
      ImGui::Text("Tab: Toggle UI");
      ImGui::Checkbox("Pause time", &G.pause);

      if (shader->ui_fn) {
        ImGui::SeparatorText("Uniforms");
        shader->ui_fn();
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
  bool is_3d = G.models[GetShaders()[G.shader_index]->model].is_3d;

  ImGui_ImplSDL3_ProcessEvent(event);
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    if (event->key.key == SDLK_TAB) {
      G.ui_hidden = !G.ui_hidden;
    }
  } break;
  case SDL_EVENT_MOUSE_MOTION: {
    if (is_3d && (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) {
      const float sens = 0.4f;
      G.cam_ang.x -= event->motion.xrel * sens;
      G.cam_ang.y += event->motion.yrel * sens;
      G.cam_ang.y = Clamp(G.cam_ang.y, -89.0f, 89.0f);
    }
  } break;
  case SDL_EVENT_MOUSE_WHEEL: {
    if (is_3d) {
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
