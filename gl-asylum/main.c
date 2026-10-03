// ================================================================================================
// Procedurally generated 3D scene in OpenGL
//
// Changelog:
//     4/27/2026:  Initial release
//
// License:
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#define GLAD_GL_IMPLEMENTATION
#include "../_common/c/thirdparty/glad33/gl.h"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

#include <stddef.h>

#define COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))
#define UNUSED(var)  ((void)(var))

// ===========================================================================
// Data
// ===========================================================================

typedef struct Vertex3D Vertex3D;
struct Vertex3D
{
  float p[3];
  float n[3];
  float t[2];
  int   i;
};

typedef struct Mesh Mesh;
struct Mesh
{
  GLuint  vao;
  GLuint  vbo;
  GLuint  ibo;
  GLsizei icount;
};

#define BTN_FORWARD   (1 << 0)
#define BTN_BACKWARD  (1 << 1)
#define BTN_LEFT      (1 << 2)
#define BTN_RIGHT     (1 << 3)

#define BTNMASK_MOVE (BTN_FORWARD | BTN_BACKWARD | BTN_LEFT | BTN_RIGHT)

static struct
{
  SDL_Window* wnd;

  int    depth_dim;
  GLuint depth_fbo;
  GLuint depth_tex;

  GLuint world_prog;
  GLuint world_depth_prog;

  Mesh floor_mesh;
  Mesh box_mesh;

  GLuint floor_diffuse_tex;

  // Camera
  float cam_pos[3]; // x, y, z
  float cam_ang[2]; // yaw, pitch
  float cam_fov;    // degrees

  // Controls
  unsigned int btns;

  // Time
  float etime;
  float gtime;
  float dtime;

  // Global settings
  float eye_height;
  float fog_start_radius;
  float fog_end_radius;
  float fog_color[3];
} G = { 0 };

// ===========================================================================
// Geometry
// ===========================================================================

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline unsigned int coord_hash(int x, int y)
{
  unsigned int xx = (unsigned int)x;
  unsigned int yy = (unsigned int)y;
  unsigned int h = xx * 374761393u + yy * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

// Return float in [0,1)
static inline float coord_hashf(int x, int y)
{
  return (coord_hash(x, y) & 0xFFFFFF) / 16777216.0f;
}


#if 0
static float degrees(float radians)
{
  return radians * 180.0f / SDL_PI_F;
}
#endif

static float radians(float degrees)
{
  return degrees * SDL_PI_F / 180.0f;
}

static float lensqv3(const float v[3])
{
  return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

static float lenv3(const float v[3])
{
  return SDL_sqrtf(lensqv3(v));
}

static void copyv3(float dst[3], const float src[3])
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
}

static void normv3(float dst[3], const float v[3])
{
  float len = lenv3(v);
  if (len == 0) { len = 1.0f; }
  dst[0] = v[0] / len;
  dst[1] = v[1] / len;
  dst[2] = v[2] / len;
}

static void addv3(float dst[3], const float left[3], const float right[3])
{
  dst[0] = left[0] + right[0];
  dst[1] = left[1] + right[1];
  dst[2] = left[2] + right[2];
}

static void subv3(float dst[3], const float left[3], const float right[3])
{
  dst[0] = left[0] - right[0];
  dst[1] = left[1] - right[1];
  dst[2] = left[2] - right[2];
}

static void mulv3(float dst[3], const float left[3], const float right[3])
{
  dst[0] = left[0] * right[0];
  dst[1] = left[1] * right[1];
  dst[2] = left[2] * right[2];
}

#if 0
static void negate3(float dst[3], const float src[3])
{
  dst[0] = -src[0];
  dst[1] = -src[1];
  dst[2] = -src[2];
}
#endif

static void crossv3(float dst[3], const float left[3], const float right[3])
{
  const float r[3] = {
    left[1] * right[2] - left[2] * right[1],
    left[2] * right[0] - left[0] * right[2],
    left[0] * right[1] - left[1] * right[0],
  };
  copyv3(dst, r);
}

static float dotv3(const float left[3], const float right[3])
{
  return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static void forward3(float dir[3], float yaw_deg, float pitch_deg)
{
  const float yaw   = radians(yaw_deg);
  const float pitch = radians(pitch_deg);
  // <0, 0> is towards <0, 0, -1>
  dir[0] = SDL_sinf(yaw) * SDL_cosf(pitch);
  dir[1] = SDL_sinf(pitch);
  dir[2] = -SDL_cosf(yaw) * SDL_cosf(pitch);
}

static void rotatev3(float dst[3], float inp[3], float around[3], float ang_rad)
{
  float axis[3]; copyv3(axis, around);
  if (lensqv3(axis) < 1e-12f) { copyv3(dst, inp); return; }
  normv3(axis, axis);

  float c = SDL_cosf(ang_rad);
  float s = SDL_sinf(ang_rad);

  // (axis × v)
  float axv[3]; crossv3(axv, axis, inp);

  // (axis · v)
  float ad = dotv3(axis, inp);

  // Rodrigues' formula:
  dst[0] = inp[0]*c + axv[0]*s + axis[0]*ad*(1.0f - c);
  dst[1] = inp[1]*c + axv[1]*s + axis[1]*ad*(1.0f - c);
  dst[2] = inp[2]*c + axv[2]*s + axis[2]*ad*(1.0f - c);
}

static void zero4(float m[16])
{
  m[ 0] = 0.0f; m[ 1] = 0.0f; m[ 2] = 0.0f; m[ 3] = 0.0f;
  m[ 4] = 0.0f; m[ 5] = 0.0f; m[ 6] = 0.0f; m[ 7] = 0.0f;
  m[ 8] = 0.0f; m[ 9] = 0.0f; m[10] = 0.0f; m[11] = 0.0f;
  m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 0.0f;
}

static void identity4(float m[16])
{
  m[ 0] = 1.0f; m[ 1] = 0.0f; m[ 2] = 0.0f; m[ 3] = 0.0f;
  m[ 4] = 0.0f; m[ 5] = 1.0f; m[ 6] = 0.0f; m[ 7] = 0.0f;
  m[ 8] = 0.0f; m[ 9] = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
  m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

static void copy4(float dst[16], const float src[16])
{
  for (int i = 0; i < 16; ++i) {
    dst[i] = src[i];
  }
}

static void mul4(float m[16], const float left[16], const float right[16])
{
  float r[16];
  for (int col = 0; col < 4; ++col) {
  for (int row = 0; row < 4; ++row) {
    r[col * 4 + row] =
      left[0 * 4 + row] * right[col * 4 + 0] +
      left[1 * 4 + row] * right[col * 4 + 1] +
      left[2 * 4 + row] * right[col * 4 + 2] +
      left[3 * 4 + row] * right[col * 4 + 3];
  }
  }
  copy4(m, r);
}

static void perspective4(float m[16], float fov_deg, float aspect,
                         float z_near, float z_far)
{
  const float fov_cot = 1.0f / SDL_tanf(radians(fov_deg) / 2.0f);

  zero4(m);

  m[0*4+0] = fov_cot / aspect;
  m[1*4+1] = fov_cot;
  m[2*4+3] = -1.0f;

  m[2*4+2] = (z_far + z_near) / (z_near - z_far);
  m[3*4+2] = (2.0f * z_near * z_far) / (z_near - z_far);
}

static void orthographic4(float m[16], float left, float right, float bottom, float top,
                          float z_near, float z_far)
{
  const float rl = right - left;
  const float tb = top   - bottom;
  const float fn = z_far - z_near;

  zero4(m);
  m[0*4+0] =  2.0f / rl;
  m[1*4+1] =  2.0f / tb;
  m[2*4+2] = -2.0f / fn;
  m[3*4+3] =  1.0f;

  m[3*4+0] = -(right + left)   / rl;
  m[3*4+1] = -(top   + bottom) / tb;
  m[3*4+2] = -(z_far + z_near) / fn;
}

static void look4(float m[16], const float eye[3], const float at[3], const float up[3])
{
  float f[3];
  subv3(f, at, eye);
  normv3(f, f);

  float s[3];
  crossv3(s, f, up);
  normv3(s, s);

  float u[3];
  crossv3(u, s, f);

  float t[3] = {
    -dotv3(s, eye),
    -dotv3(u, eye),
     dotv3(f, eye),
  };

  m[ 0] =  s[0]; m[ 1] =  u[0]; m[ 2] = -f[0]; m[ 3] = 0.0f;
  m[ 4] =  s[1]; m[ 5] =  u[1]; m[ 6] = -f[1]; m[ 7] = 0.0f;
  m[ 8] =  s[2]; m[ 9] =  u[2]; m[10] = -f[2]; m[11] = 0.0f;
  m[12] =  t[0]; m[13] =  t[1]; m[14] =  t[2]; m[15] = 1.0f;
}

static void translate4(float m[16], const float v[3])
{
  identity4(m);
  m[3*4+0] = v[0];
  m[3*4+1] = v[1];
  m[3*4+2] = v[2];
}

// ===========================================================================
// OpenGL helpers
// ===========================================================================

// Helper: Report errors via glGetError() after every OpenGL function call.
//         macOS does not support glDebugMessageCallback
static void gl_check(GLenum error, const char* expr, int line)
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
    SDL_Log("[Line #%d] %s caused %s\n", line, expr, error_string);
  }
}
#define GLCHECK(expr) expr; gl_check(glGetError(), #expr, __LINE__);

static void create_3d_mesh(Mesh* m, const Vertex3D* v, size_t vcount,
                           const Uint16* i, size_t icount)
{
  const size_t vsize = sizeof(*v) * vcount;
  const size_t isize = sizeof(*i) * icount;

  GLCHECK(glGenVertexArrays(1, &m->vao));
  GLCHECK(glBindVertexArray(m->vao));

  GLCHECK(glGenBuffers(1, &m->vbo))
  GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, m->vbo));
  GLCHECK(glBufferData(GL_ARRAY_BUFFER, vsize, v, GL_STATIC_DRAW));

  GLCHECK(glGenBuffers(1, &m->ibo))
  GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo));
  GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, isize, i, GL_STATIC_DRAW));

  GLCHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                                (void*)offsetof(Vertex3D, p)))
  GLCHECK(glEnableVertexAttribArray(0));
  GLCHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                                (void*)offsetof(Vertex3D, n)))
  GLCHECK(glEnableVertexAttribArray(1));
  GLCHECK(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D),
                                (void*)offsetof(Vertex3D, t)))
  GLCHECK(glEnableVertexAttribArray(2));
  GLCHECK(glVertexAttribIPointer(3, 1, GL_INT, sizeof(Vertex3D),
                                (void*)offsetof(Vertex3D, i)))
  GLCHECK(glEnableVertexAttribArray(3));

  m->icount = icount;
}

static GLuint create_texture(int width, int height, void* bmp_rgb)
{
  GLuint tex;
  GLCHECK(glGenTextures(1, &tex));

  GLCHECK(glBindTexture(GL_TEXTURE_2D, tex));
  GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                       GL_UNSIGNED_BYTE, bmp_rgb));
  GLCHECK(glGenerateMipmap(GL_TEXTURE_2D));

  return tex;
}

static GLuint create_glshader(const char* path, GLenum type)
{
  GLuint shader = GLCHECK(glCreateShader(type));

  const char* src = SDL_LoadFile(path, NULL);
  if (!src) {
    SDL_Log("Failed to load shader %s: %s\n", path, SDL_GetError());
    SDL_assert(0);
  }
  GLCHECK(glShaderSource(shader, 1, &src, NULL));

  GLCHECK(glCompileShader(shader));

  GLint status;
  GLCHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
  if (status != GL_TRUE) {
    char error[1024] = { 0 };
    GLCHECK(glGetShaderInfoLog(shader, sizeof(error), NULL, error));
    SDL_Log("Error compiling %s: %s\n", path, error);
    SDL_assert(0);
  }

  SDL_free((void*)src);
  return shader;
}

static GLuint create_glprog(const char* vs_path, const char* fs_path)
{
  GLuint prog = GLCHECK(glCreateProgram());
  GLCHECK(glAttachShader(prog, create_glshader(vs_path, GL_VERTEX_SHADER)));
  GLCHECK(glAttachShader(prog, create_glshader(fs_path, GL_FRAGMENT_SHADER)));
  GLCHECK(glLinkProgram(prog));

  GLint status;
  GLCHECK(glGetProgramiv(prog, GL_LINK_STATUS, &status));
  if (status != GL_TRUE) {
    char error[1024] = { 0 };
    GLCHECK(glGetProgramInfoLog(prog, sizeof(error), NULL, error));
    SDL_Log("Error linking program: %s\n", error);
    SDL_assert(0);
  }

  SDL_Log("Compiled OpenGL program [%s, %s]\n", vs_path, fs_path);

  return prog;
}

static void set_gluniform_m4(GLuint shader, const char* name, float m[16])
{
  GLint p = GLCHECK(glGetUniformLocation(shader, name));
  SDL_assert(p >= 0 && "incorrect uniform name");
  GLCHECK(glUniformMatrix4fv(p, 1, GL_FALSE, m));
}

static void set_gluniform_3f(GLuint shader, const char* name, float v[3])
{
  GLint p = GLCHECK(glGetUniformLocation(shader, name));
  SDL_assert(p >= 0 && "incorrect uniform name");
  GLCHECK(glUniform3fv(p, 1, v));
}

static void set_gluniform_4f(GLuint shader, const char* name, float v[4])
{
  GLint p = GLCHECK(glGetUniformLocation(shader, name));
  SDL_assert(p >= 0 && "incorrect uniform name");
  GLCHECK(glUniform4fv(p, 1, v));
}

static void set_gluniform_1f(GLuint shader, const char* name, GLfloat f)
{
  GLint p = GLCHECK(glGetUniformLocation(shader, name));
  SDL_assert(p >= 0 && "incorrect uniform name");
  GLCHECK(glUniform1f(p, f));
}

static void set_gluniform_1i(GLuint shader, const char* name, GLint i)
{
  GLint p = GLCHECK(glGetUniformLocation(shader, name));
  SDL_assert(p >= 0 && "incorrect uniform name");
  GLCHECK(glUniform1i(p, i));
}

// ===========================================================================
// World generation
// ===========================================================================

static int cube(int x, int z)
{
  int y = 0;
  if (coord_hashf(x, z) <= 0.1f) {
    y = coord_hashf(z, x) * 10 + 2;
  }
  return y;
}

// ===========================================================================
// Rendering
// ===========================================================================

static void tile_diffusion(GLuint shader, int x, int z)
{
  const bool n  = cube(x+0, z-1) > 0;
  const bool ne = cube(x+1, z-1) > 0;
  const bool e  = cube(x+1, z+0) > 0;
  const bool se = cube(x+1, z+1) > 0;
  const bool s  = cube(x+0, z+1) > 0;
  const bool sw = cube(x-1, z+1) > 0;
  const bool w  = cube(x-1, z+0) > 0;
  const bool nw = cube(x-1, z-1) > 0;

  float d[4] = {
    (w || nw || n) ? 1.0f : 0.0f,
    (n || ne || e) ? 1.0f : 0.0f,
    (e || se || s) ? 1.0f : 0.0f,
    (s || sw || w) ? 1.0f : 0.0f,
  };

  set_gluniform_4f(shader, "u_diffusion", d);
}

static void cube_diffusion(GLuint shader, int x, int y, int z)
{
  // @@ not done, using a horrible hack in fragment shader
  UNUSED(x); UNUSED(y); UNUSED(z);
  // @@
  float d[4] = { 0.0f, 0.0f,0.0f, 0.0f };
  set_gluniform_4f(shader, "u_diffusion", d);
}

static void render_scene(GLuint shader, float view[16], bool depth_only)
{
  set_gluniform_m4(shader, "u_view", view);

  // Tiles
  {
    GLCHECK(glBindVertexArray(G.floor_mesh.vao));

    if (!depth_only) {
      GLCHECK(glActiveTexture(GL_TEXTURE0));
      GLCHECK(glBindTexture(GL_TEXTURE_2D, G.floor_diffuse_tex));
      set_gluniform_1i(shader, "u_diffuse", 0);
    }

    float model[16];
    const int r = (int)G.fog_end_radius + 1;
    const int cx = (int)G.cam_pos[0];
    const int cz = (int)G.cam_pos[2];
    for (int x = cx - r; x <= cx + r; ++x) {
    for (int z = cz - r; z <= cz + r; ++z) {
      const float t[3] = { x, 0.0f, z };

      float d[3]; subv3(d, t, G.cam_pos);
      if (lenv3(d) > G.fog_end_radius) {
        continue;
      }

      if (!depth_only) {
        tile_diffusion(shader, x, z);
      }

      translate4(model, t);
      set_gluniform_m4(shader, "u_model", model);
      GLCHECK(glDrawElements(GL_TRIANGLES, G.floor_mesh.icount, GL_UNSIGNED_SHORT, 0));
    }
    }
  }

  // Cubes
  {
    GLCHECK(glBindVertexArray(G.box_mesh.vao));

    if (!depth_only) {
      GLCHECK(glActiveTexture(GL_TEXTURE0));
      GLCHECK(glBindTexture(GL_TEXTURE_2D, G.floor_diffuse_tex));
      set_gluniform_1i(shader, "u_diffuse", 0);
    }

    float model[16];
    const int r = (int)G.fog_end_radius + 5;
    const int cx = (int)G.cam_pos[0];
    const int cz = (int)G.cam_pos[2];
    for (int x = cx - r; x <= cx + r; ++x) {
    for (int z = cz - r; z <= cz + r; ++z) {
      float t[3] = { x, 0.5f, z };

      const int height = cube(x, z);
      for (int y = 0; y < height; ++y) {
        if (!depth_only) {
          cube_diffusion(shader, x, y, z);
        }
        translate4(model, t);
        set_gluniform_m4(shader, "u_model", model);
        GLCHECK(glDrawElements(GL_TRIANGLES, G.box_mesh.icount,GL_UNSIGNED_SHORT, 0));
        t[1] += 1.0f;
      }
    }
    }
  }
}

// ===========================================================================
// SDL callbacks
// ===========================================================================

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
  UNUSED(appstate);
  UNUSED(argc);
  UNUSED(argv);

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  G.wnd = SDL_CreateWindow("demogame", 1024, 768, SDL_WINDOW_HIDDEN |
                           SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!G.wnd) {
    SDL_Log("Failed to create game window: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_GL_CreateContext(G.wnd)) {
    SDL_Log("Failed to create OpenGL context: %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_GL_SetSwapInterval(1);

  if (!gladLoadGL(SDL_GL_GetProcAddress)) {
    SDL_Log("gladLoadGL failed\n");
    return SDL_APP_FAILURE;
  }

  // Depth map framebuffer
  G.depth_dim = 8192;
  GLCHECK(glGenTextures(1, &G.depth_tex));
  GLCHECK(glBindTexture(GL_TEXTURE_2D, G.depth_tex));
  GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, G.depth_dim, G.depth_dim, 0,
                       GL_DEPTH_COMPONENT, GL_FLOAT, NULL));
  GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
  GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

  GLCHECK(glGenFramebuffers(1, &G.depth_fbo));
  GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, G.depth_fbo));
  GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                 G.depth_tex, 0));
  GLCHECK(glDrawBuffer(GL_NONE));
  GLCHECK(glReadBuffer(GL_NONE));
  GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

  // Floor mesh: Unit plane along <X, Z>
  {
    const Vertex3D v[] = {
      { { -0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, }, 0, },
      { {  0.5f, 0.0f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, }, 1, },
      { {  0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, }, 2, },
      { { -0.5f, 0.0f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, }, 3, },
    };
    const Uint16 i[] = {
      0, 1, 2,
      0, 2, 3,
    };
    create_3d_mesh(&G.floor_mesh, v, COUNTOF(v), i, COUNTOF(i));
  }

  // Box mesh: Cube with no bottom face
  {
    const Vertex3D v[] = {
      // +z
      { { -0.5f,  0.5f,  0.5f }, {  0.0f, 0.0f,  1.0f }, { 0.0f, 1.0f, }, 7 },
      { {  0.5f,  0.5f,  0.5f }, {  0.0f, 0.0f,  1.0f }, { 1.0f, 1.0f, }, 6 },
      { {  0.5f, -0.5f,  0.5f }, {  0.0f, 0.0f,  1.0f }, { 1.0f, 0.0f, }, 2 },
      { { -0.5f, -0.5f,  0.5f }, {  0.0f, 0.0f,  1.0f }, { 0.0f, 0.0f, }, 3 },
      // +x
      { { 0.5f,   0.5f,  0.5f }, {  1.0f, 0.0f,  0.0f }, { 0.0f, 1.0f, }, 6 },
      { { 0.5f,   0.5f, -0.5f }, {  1.0f, 0.0f,  0.0f }, { 1.0f, 1.0f, }, 5 },
      { { 0.5f,  -0.5f, -0.5f }, {  1.0f, 0.0f,  0.0f }, { 1.0f, 0.0f, }, 1 },
      { { 0.5f,  -0.5f,  0.5f }, {  1.0f, 0.0f,  0.0f }, { 0.0f, 0.0f, }, 2 },
      // -z
      { {  0.5f,  0.5f, -0.5f }, {  0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, }, 5 },
      { { -0.5f,  0.5f, -0.5f }, {  0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f, }, 4 },
      { { -0.5f, -0.5f, -0.5f }, {  0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, }, 0 },
      { {  0.5f, -0.5f, -0.5f }, {  0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, }, 1 },
      // -x
      { { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f,  0.0f }, { 0.0f, 1.0f, }, 4 },
      { { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f,  0.0f }, { 1.0f, 1.0f, }, 7 },
      { { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f,  0.0f }, { 1.0f, 0.0f, }, 3 },
      { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f,  0.0f }, { 0.0f, 0.0f, }, 0 },
      // +y
      { { -0.5f,  0.5f, -0.5f }, {  0.0f, 1.0f,  0.0f }, { 0.0f, 1.0f, }, 4 },
      { {  0.5f,  0.5f, -0.5f }, {  0.0f, 1.0f,  0.0f }, { 1.0f, 1.0f, }, 5 },
      { {  0.5f,  0.5f,  0.5f }, {  0.0f, 1.0f,  0.0f }, { 1.0f, 0.0f, }, 6 },
      { { -0.5f,  0.5f,  0.5f }, {  0.0f, 1.0f,  0.0f }, { 0.0f, 0.0f, }, 7 },
    };
    const Uint16 i[] = {
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
    };
    create_3d_mesh(&G.box_mesh, v, COUNTOF(v), i, COUNTOF(i));
  }

  // Floor texture
  {
    const int dim = 512;
    unsigned char* bmp = SDL_calloc(dim * dim, 3); SDL_assert(bmp);

    const int r = 4;
    for (int y = 0; y < dim; ++y) {
    for (int x = 0; x < dim; ++x) {
      const int dt = y;
      const int db = dim - y - 1;
      const int dl = x;
      const int dr = dim - x - 1;

      int d = dt;
      if (db < d) { d = db; }
      if (dl < d) { d = dl; }
      if (dr < d) { d = dr; }

      unsigned char bg = 0xEF;
      int block = 32;
      if (((x / block) + (y / block)) % 2 == 0 && x > block && y > block
          && x < dim - block && y < dim - block) {
        bg = 0xDF;
      }
      unsigned char val = (d >= r) ? bg : 0x7F;
      unsigned char* p = &bmp[(dim * 3) * y + x * 3];
      p[0] = val;
      p[1] = val;
      p[2] = val;
    }
    }

    G.floor_diffuse_tex = create_texture(dim, dim, bmp);

    SDL_free(bmp);
  }

  G.world_prog = create_glprog("world_vs.glsl", "world_fs.glsl");
  G.world_depth_prog = create_glprog("world_vs.glsl", "world_depth_fs.glsl");

  GLCHECK(glEnable(GL_MULTISAMPLE));
  GLCHECK(glEnable(GL_DEPTH_TEST));
  GLCHECK(glEnable(GL_CULL_FACE));
  GLCHECK(glCullFace(GL_FRONT));

  G.cam_ang[0] = 0.0f;
  G.cam_ang[1] = 0.0f;
  G.cam_fov = 90.0f;
  G.eye_height = 1.5f;
  G.fog_start_radius = 5.0f;
  G.fog_end_radius = 15.0f;
  G.fog_color[0] = G.fog_color[1] = G.fog_color[2] = 0.3f;

  // @@
  G.cam_pos[2] = 1.5f;

  SDL_ShowWindow(G.wnd);

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  UNUSED(appstate);

  int vp_w;
  int vp_h;
  SDL_GetWindowSizeInPixels(G.wnd, &vp_w, &vp_h);

  // ===== Simulate =====

  // Advance time
  const float now = SDL_GetTicks() / 1e3f;
  G.dtime = now - G.etime;
  G.etime = now;
  G.gtime += G.dtime;

  // Clamp player eye angles
  G.cam_ang[0] = SDL_fmodf(G.cam_ang[0], 360.0f);
  if (G.cam_ang[1] >  89.0f) { G.cam_ang[1] =  89.0f; }
  if (G.cam_ang[1] < -89.0f) { G.cam_ang[1] = -89.0f; }

  // Player movement
  if (G.btns & BTNMASK_MOVE) {
    float movedir_f[3]; forward3(movedir_f, G.cam_ang[0], G.cam_ang[1]);
    movedir_f[1] = 0.0f;
    normv3(movedir_f, movedir_f);
    
    float up[3] = { 0.0f, 1.0f, 0.0f };
    float movedir_r[3]; crossv3(movedir_r, movedir_f, up);

    float move[3] = { 0 };
    if (G.btns & BTN_FORWARD)  { addv3(move, move, movedir_f); }
    if (G.btns & BTN_BACKWARD) { subv3(move, move, movedir_f); }
    if (G.btns & BTN_LEFT)     { subv3(move, move, movedir_r); }
    if (G.btns & BTN_RIGHT)    { addv3(move, move, movedir_r); }
    normv3(move, move);

    float speed = 4.0f * G.dtime;
    float scale[3] = { speed, speed, speed };
    mulv3(move, move, scale);

    addv3(G.cam_pos, G.cam_pos, move);
  }

  // Collisions
  for (int i = 0; i < 8; ++i) {
    const int xoff[] = {  0,  1, 1, 1, 0, -1, -1, -1 };
    const int zoff[] = { -1, -1, 0, 1, 1,  1,  0, -1 };
    const int wall_x = (int)G.cam_pos[0] + xoff[i];
    const int wall_z = (int)G.cam_pos[2] + zoff[i];
    if (cube(wall_x, wall_z) < 1) {
      continue;
    }
  }

  // FOV mod
  static float fov_mod = 0;
  float fov_tgt = 0;
  if (G.btns & BTN_FORWARD) {
    fov_tgt += 15.0f;
  }
  if (G.btns & BTN_BACKWARD) {
    fov_tgt -= 5.0f;
  }
  fov_mod = lerp(fov_mod, fov_tgt, G.dtime * 5.0f);

  // Roll
  static float cam_roll = 0;
  float roll_tgt = 0;
  if (G.btns & BTN_RIGHT) {
    roll_tgt += 5.0f;
  }
  if (G.btns & BTN_LEFT) {
    roll_tgt -= 5.0f;
  }
  cam_roll = lerp(cam_roll, roll_tgt, G.dtime * 5.0f);

  // Update camera
  G.cam_pos[1] = G.eye_height;
  G.cam_fov = 75.0f + fov_mod;

  // Light space matrix needed in both shaders
  float lsm[16];
  float sun_off[3] = { 5.214f, 20.0f, 8.143f };;

  // Render pass: sun shadow map
  GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, G.depth_fbo));
  {
    GLCHECK(glViewport(0, 0, G.depth_dim, G.depth_dim));
    GLCHECK(glClear(GL_DEPTH_BUFFER_BIT));
    GLCHECK(glUseProgram(G.world_depth_prog));

    float proj[16]; orthographic4(proj, -25, 25, -25, 25, 5.0f, 40.0f);

    float cam_up[3] = { 0.0f, 1.0f, 0.0f };

    float sun[3]; 
    addv3(sun, G.cam_pos, sun_off);
    float look[16]; look4(look, sun, G.cam_pos, cam_up);

    float view[16]; mul4(view, proj, look);

    copy4(lsm, view);

    render_scene(G.world_depth_prog, view, true);
  }

  // Render pass: First-person view
  GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  {
    GLCHECK(glViewport(0, 0, vp_w, vp_h));
    GLCHECK(glClearColor(G.fog_color[0], G.fog_color[1], G.fog_color[2], 1.0f));
    GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GLCHECK(glUseProgram(G.world_prog));

    set_gluniform_3f(G.world_prog, "u_cam",       G.cam_pos);
    set_gluniform_1f(G.world_prog, "u_fog_start", G.fog_start_radius);
    set_gluniform_1f(G.world_prog, "u_fog_end",   G.fog_end_radius);
    set_gluniform_3f(G.world_prog, "u_fog_color", G.fog_color);

    GLCHECK(glActiveTexture(GL_TEXTURE1));
    GLCHECK(glBindTexture(GL_TEXTURE_2D, G.depth_tex));
    set_gluniform_1i(G.world_prog, "u_shadow_depth", 1);

    set_gluniform_3f(G.world_prog, "u_sun", sun_off);
    set_gluniform_m4(G.world_prog, "u_lightspace", lsm);

    const float aspect = (float)vp_w / (float)vp_h;
    float proj[16]; perspective4(proj, G.cam_fov, aspect, 0.1f, 100.0f);

    float cam_fwd[3];
    forward3(cam_fwd, G.cam_ang[0], G.cam_ang[1]);

    float cam_tgt[3];
    addv3(cam_tgt, G.cam_pos, cam_fwd);

    float cam_up[3] = { 0.0f, 1.0f, 0.0f };
    float cam_up_rot[3];
    rotatev3(cam_up_rot, cam_up, cam_fwd, radians(cam_roll));

    float look[16]; look4(look, G.cam_pos, cam_tgt, cam_up_rot);

    float view[16]; mul4(view, proj, look);

    render_scene(G.world_prog, view, false);
  }

  SDL_GL_SwapWindow(G.wnd);

  return SDL_APP_CONTINUE;
}

static unsigned int keybind(SDL_Keycode key)
{
  switch (key) {
    case SDLK_W: return BTN_FORWARD;
    case SDLK_S: return BTN_BACKWARD;
    case SDLK_A: return BTN_LEFT;
    case SDLK_D: return BTN_RIGHT;
    default:     return 0;
  };
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* evt)
{
  UNUSED(appstate);

  switch (evt->type) {
    case SDL_EVENT_KEY_DOWN: {
      if (evt->key.key == SDLK_ESCAPE) {
        int ww = 0;
        int wh = 0;
        SDL_GetWindowSize(G.wnd, &ww, &wh);
        SDL_WarpMouseInWindow(G.wnd, (float)ww / 2, (float)wh / 2);
        SDL_SetWindowRelativeMouseMode(G.wnd, false);
      }

      G.btns |= keybind(evt->key.key);
    } break;
    case SDL_EVENT_KEY_UP: {
      G.btns &= ~keybind(evt->key.key);
    } break;
    case SDL_EVENT_MOUSE_MOTION: {
      if (SDL_GetWindowRelativeMouseMode(G.wnd)) {
        const float sens = 0.1f;
        G.cam_ang[0] += evt->motion.xrel * sens;
        G.cam_ang[1] -= evt->motion.yrel * sens;
      }
    } break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      if (!SDL_GetWindowRelativeMouseMode(G.wnd)) {
        SDL_SetWindowRelativeMouseMode(G.wnd, true);
      }
    } break;
    case SDL_EVENT_WINDOW_FOCUS_LOST: {
      SDL_SetWindowRelativeMouseMode(G.wnd, false);
    } break;
  };

  return (evt->type == SDL_EVENT_QUIT) ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  UNUSED(appstate);
  UNUSED(result);

  SDL_DestroyWindow(G.wnd);
}

