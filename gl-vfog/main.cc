// OpenGL volumetric fog demo
//
// SPDX-License-Identifier: 0BSD
//
// Changelog:
// - 11/13/25: Initial release
//

#define GLAD_GL_IMPLEMENTATION
#include "../common/c_cpp/thirdparty/glad33/glad33.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "imgui.h"
#include "imgui_impl_opengl3.cpp"
#include "imgui_impl_sdl3.cpp"

#include "geometry.hh"

// Helper: Report errors via glGetError() after every OpenGL function call.
//         macOS does not support glDebugMessageCallback
static void assert_gl_error(GLenum error, const char* expr, int line)
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
    assert_gl_error(_glcode, #expr, __LINE__);                      \
  }

int main(int argc, char* argv[])
{
  (void)argc;
  (void)argv;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window* wnd = SDL_CreateWindow("gl-vfog", 1024, 768, SDL_WINDOW_OPENGL);
  if (!wnd) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  if (!SDL_GL_CreateContext(wnd)) {
    fprintf(stderr, "Failed to create OpenGL context: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }
  SDL_GL_SetSwapInterval(1);

  if (!gladLoadGL(SDL_GL_GetProcAddress)) {
    fprintf(stderr, "Failed to load OpenGL functions\n");
    return EXIT_FAILURE;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui_ImplSDL3_Init(wnd, 0, 0);
  ImGui_ImplOpenGL3_Init();
  ImGui::GetIO().IniFilename = NULL;

  //
  //        0            1
  //      +------------+
  //     /|           /|
  // 4  / |       5  / |
  //   +------------+  |
  //   |  |         |  |
  //   |  | 3       |  | 2
  //   |  +---------|--+
  //   | /          | /
  // 7 |/         6 |/
  //   +------------+
  //

  const float vdata[] = {
    0.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
  };

  const uint16_t idata[] = {
    1, 0, 3,
    1, 3, 2,
    5, 1, 2,
    5, 2, 6,
    4, 5, 6,
    4, 6, 7,
    0, 4, 7,
    0, 7, 3,
    0, 1, 5,
    0, 5, 4,
    7, 6, 2,
    7, 2, 3,
  };

  GLuint vao;
  GL(glGenVertexArrays(1, &vao))
  GL(glBindVertexArray(vao));

  GLuint vbo;
  GL(glGenBuffers(1, &vbo));
  GL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vdata), vdata, GL_STATIC_DRAW));

  GLuint ibo;
  GL(glGenBuffers(1, &ibo));
  GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo));
  GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idata), idata, GL_STATIC_DRAW));

  GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, NULL));
  GL(glEnableVertexAttribArray(0));

  struct {
    GLenum      type;
    const char* path;
  } shaders[] = {
    { .type = GL_VERTEX_SHADER,   .path = "vs.glsl" },
    { .type = GL_FRAGMENT_SHADER, .path = "fs.glsl" },
  };

  GLuint prog = GL(glCreateProgram());
  for (size_t i = 0; i < sizeof(shaders) / sizeof(*shaders); ++i) {
    const char* src = (const char*)SDL_LoadFile(shaders[i].path, NULL);
    if (!src) {
      fprintf(stderr, "Failed to load shader %s: %s\n", shaders[i].path, SDL_GetError());
      return EXIT_FAILURE;
    }

    GLuint shader = GL(glCreateShader(shaders[i].type));
    GL(glShaderSource(shader, 1, &src, NULL));
    GL(glCompileShader(shader));
    GLint compile_status = 0;
    GL(glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status));
    if (compile_status != GL_TRUE) {
      char error[1024] = { 0 };
      GL(glGetShaderInfoLog(shader, sizeof(error), NULL, error));
      SDL_Log("Error compiling shader %s: %s\n", shaders[i].path, error);
      return EXIT_FAILURE;
    }
    GL(glAttachShader(prog, shader));
  }
  GL(glLinkProgram(prog));

  GLint prog_status;
  GL(glGetProgramiv(prog, GL_LINK_STATUS, &prog_status));
  if (prog_status != GL_TRUE) {
    char error[1024] = { 0 };
    GL(glGetProgramInfoLog(prog, sizeof(error), NULL, error));
    SDL_Log("Error linking program: %s\n", error);
    return EXIT_FAILURE;
  }

  GL(glUseProgram(prog));

  GLuint u_view  = GL(glGetUniformLocation(prog, "u_view"));
  GLuint u_cam   = GL(glGetUniformLocation(prog, "u_cam"));
  GLuint u_time  = GL(glGetUniformLocation(prog, "u_time"));
  GLuint u_mode  = GL(glGetUniformLocation(prog, "u_mode"));
  GLuint u_sigma = GL(glGetUniformLocation(prog, "u_sigma"));

  bool running = true;
  while (running) {
    SDL_Event evt = { 0 };
    while (SDL_PollEvent(&evt)) {
      ImGui_ImplSDL3_ProcessEvent(&evt);
      switch (evt.type) {
      case SDL_EVENT_QUIT: {
        running = false;
      } break;
      }
    }

    int wnd_x = 0;
    int wnd_y = 0;
    SDL_GetWindowSizeInPixels(wnd, &wnd_x, &wnd_y);

    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    // Controls
    static int   c_mode  = 0;
    static float c_sigma = 4.0f;
    static float c_zoom  = 1.5f;
    ImGui::SetNextWindowPos(ImVec2(15, 15));
    ImGui::SetNextWindowSize(ImVec2(150, 768 - 30));
    if (ImGui::Begin("Controls", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
      ImGui::SeparatorText("Controls");
      ImGui::DragFloat("Zoom", &c_zoom, 0.05f, 0.1f, 3.0f);
      ImGui::SeparatorText("Settings");
      const char* modes[] = { "Basic" };
      if (ImGui::BeginCombo("Mode", modes[c_mode])) {
        for (int i = 0; i < IM_ARRAYSIZE(modes); ++i) {
          if (ImGui::Selectable(modes[c_mode], i == c_mode)) {
            c_mode = i;
          }
        }
        ImGui::EndCombo();
      }
      switch (c_mode) {
      case 0: {
        ImGui::DragFloat("Sigma", &c_sigma, 0.05);
      } break;
      }
      ImGui::End();
    }

    GL(glViewport(0, 0, wnd_x, wnd_y));
    GL(glEnable(GL_CULL_FACE));
    GL(glCullFace(GL_FRONT));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    const float t = SDL_GetTicks() / 1e3f;
    const float camt = t * 0.33f;

    Vec3 eye = Vec3(0.0f, 1.5f, 0.0f);
    Vec3 tgt = Vec3(0.5f, 0.5f, 0.5f);
    eye.x = tgt.x + SDL_sinf(camt) * c_zoom;
    eye.z = tgt.z + SDL_cosf(camt) * c_zoom;
    Vec3 up = Vec3(0.0f, 1.0f, 0.0f);

    Mat4x4 look = Mat4x4::LookAt(eye, tgt, up);
    Mat4x4 proj = Mat4x4::Perspective(70.0f, (float)wnd_x / (float)wnd_y, 0.1f, 10.0f);
    Mat4x4 view = proj * look;

    GL(glUniformMatrix4fv(u_view, 1, GL_FALSE, view.Base()))
    GL(glUniform3fv(u_cam, 1, eye.Base()));
    GL(glUniform1f(u_time, t));
    GL(glUniform1i(u_mode, c_mode));
    GL(glUniform1f(u_sigma, c_sigma));

    GL(glDrawElements(GL_TRIANGLES, 3 * 12, GL_UNSIGNED_SHORT, NULL));

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!SDL_GL_SwapWindow(wnd)) {
      fprintf(stderr, "Failed to present window: %s", SDL_GetError());
      return EXIT_FAILURE;
    }
  }

  SDL_DestroyWindow(wnd);

  return EXIT_SUCCESS;
}
