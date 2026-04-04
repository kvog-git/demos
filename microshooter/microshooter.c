#if !defined(CLIENT) && !defined(SERVER)
#   error Either CLIENT or SERVER must be defined
#endif

// ================================================================================
// Core headers
// ================================================================================

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// ================================================================================
// Core helpers
// ================================================================================

#define COUNTOF(_X) (sizeof(_X) / sizeof((_X)[0]))
#define UNUSED(_X) ((void)sizeof(_X))

#ifdef __GNUC__
#   define NORETURN __attribute__((noreturn))
#   define PRINTF_FORMAT(_X, _Y) __attribute__((format(printf, _X, _Y)))
#endif

// ================================================================================
// Game -> OS API
// ================================================================================

NORETURN
void OS_SpewError(const char *message);

void OS_SpewInfo(const char *message);

// ================================================================================
// OS -> Client API
// ================================================================================

typedef struct CL_InitParams CL_InitParams;
struct CL_InitParams 
{
    void *(*glproc)(const char *);
};

void CL_Init(const CL_InitParams *params);

void CL_Render(int vp_width, int vp_height);

// ================================================================================
// OS -> Server API
// ================================================================================

typedef struct SV_InitParams SV_InitParams;
struct SV_InitParams
{
    const char *hostname;
};

void SV_Init(const SV_InitParams *params);

// ================================================================================
// Shared utility code
// ================================================================================

#define HEAP_FMT(_STR, _FMT)                        \
    do {                                            \
        va_list va; va_start(va, _FMT);             \
        int len = vsnprintf(0, 0, _FMT, va) + 1;    \
        va_end(va);                                 \
        _STR = calloc(len, 1);                      \
        va_start(va, _FMT);                         \
        vsnprintf(_STR, len, _FMT, va);             \
        va_end(va);                                 \
    } while (0);

NORETURN PRINTF_FORMAT(1, 2)
void Error(const char *fmt, ...)
{
    char *str = 0;
    HEAP_FMT(str, fmt);
    OS_SpewError(str);
}

PRINTF_FORMAT(1, 2)
void Info(const char *fmt, ...)
{
    char *str = 0;
    HEAP_FMT(str, fmt);
    OS_SpewInfo(str);
    free(str);
}

#undef HEAP_FMT

// ================================================================================
// Math
// ================================================================================

typedef struct Mat4 Mat4;
struct Mat4
{
    float m[4 * 4];
};
#define MAT4(...) (Mat4){ .m = { __VA_ARGS__ }, }

#if 0
static inline Mat4 Mat4_Ident(void)
{
    return MAT4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    );
}
#endif

static inline Mat4 Mat4_Ortho(float l, float r, float t, float b, float n, float f)
{
    // ref: https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/orthographic-projection-matrix.html
    return MAT4(
        2.0f / (r - l),       0.0f,                 0.0f,                 0.0f,
        0.0f,                 2.0f / (t - b),       0.0f,                 0.0f,
        0.0f,                 0.0f,                 -2.0f / (f - n),      0.0f,
        -((r + l) / (r - l)), -((t + b) / (t - b)), -((f + n) / (f - n)), 1.0f,
    );
}

// ================================================================================
// OpenGL
// ================================================================================

// ref: https://registry.khronos.org/OpenGL/api/GL/glcorearb.h

#define GL_FALSE 0
#define GL_TRUE  1

#define GL_TRIANGLES 0x0004

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31

#define GL_UNSIGNED_SHORT   0x1403
#define GL_FLOAT            0x1406

#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893

#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100

#define GL_VENDOR   0x1F00
#define GL_RENDERER 0x1F01

#define GL_NO_ERROR             0
#define GL_INVALID_ENUM         0x0500
#define GL_INVALID_VALUE        0x0501
#define GL_INVALID_OPERATION    0x0502
#define GL_OUT_OF_MEMORY        0x0505

#define GL_STATIC_DRAW 0x88E4
#define GL_STREAM_DRAW 0x88E0

#define GL_COMPILE_STATUS   0x8B81
#define GL_LINK_STATUS      0x8B82

typedef unsigned int    GLenum;
typedef float           GLfloat;
typedef int             GLint;
typedef int             GLsizei;
typedef unsigned int    GLbitfield;
typedef unsigned int    GLuint;
typedef unsigned char   GLboolean;
typedef uint8_t         GLubyte;
typedef size_t          GLsizeiptr;
typedef intptr_t        GLintptr;

#define GLFUNCS                                                 \
    X(glAttachShader, void, GLuint, GLuint)                     \
    X(glBindBuffer, void, GLenum, GLuint)                       \
    X(glBindVertexArray, void, GLuint)                          \
    X(glBufferData, void, GLenum, GLsizeiptr, const void *, GLenum) \
    X(glBufferSubData, void, GLenum, GLintptr, GLsizeiptr, const void *) \
    X(glClear, void, GLbitfield)                                \
    X(glClearColor, void, GLfloat, GLfloat, GLfloat, GLfloat)   \
    X(glCompileShader, void, GLuint)                            \
    X(glCreateProgram, GLuint, void)                            \
    X(glCreateShader, GLuint, GLenum)                           \
    X(glDrawElements, void, GLenum, GLsizei, GLenum, const void *) \
    X(glEnableVertexAttribArray, void, GLuint)                  \
    X(glGenBuffers, void, GLsizei, GLuint *)                    \
    X(glGenVertexArrays, void, GLsizei, GLuint *)               \
    X(glGetError, GLenum, void)                                 \
    X(glGetProgramInfoLog, void, GLuint, GLsizei, GLsizei *, char *) \
    X(glGetProgramiv, void, GLuint, GLenum, GLint *)            \
    X(glGetShaderInfoLog, void, GLuint, GLsizei, GLsizei *, char *) \
    X(glGetShaderiv, void, GLuint, GLenum, GLint *)             \
    X(glGetString, const GLubyte *, GLenum)                     \
    X(glGetUniformLocation, GLint, GLuint, const char *)        \
    X(glLinkProgram, void, GLuint)                              \
    X(glShaderSource, void, GLuint, GLsizei, const char *const *, const GLint *) \
    X(glUniformMatrix4fv, void, GLint, GLsizei, GLboolean, const GLfloat *) \
    X(glUseProgram, void, GLuint)                               \
    X(glVertexAttribPointer, void, GLuint, GLint, GLenum, GLboolean, GLsizei, const void *) \
    X(glViewport, void, GLint, GLint, GLsizei, GLsizei)

#define X(_NAME, _RET, ...) static _RET (*_NAME)(__VA_ARGS__) = 0;
GLFUNCS
#undef X

void LoadOpenGLFunctions(void *(*glproc)(const char *))
{
    #define X(_NAME, _RET, ...)                                             \
        _NAME = (_RET(*)(__VA_ARGS__))glproc(#_NAME);                       \
        if (!_NAME) {Error("Failed to load OpenGL function %s", #_NAME); }
    GLFUNCS
    #undef X
}

#undef GLFUNCS

void CheckGLError(unsigned int line, const char *code)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        const char *error_str = "unknown error";
        switch (error)
        {
#define BIND_ERROR(_ERR) case _ERR: error_str = #_ERR; break;
            BIND_ERROR(GL_INVALID_ENUM);
            BIND_ERROR(GL_INVALID_VALUE);
            BIND_ERROR(GL_INVALID_OPERATION);
            BIND_ERROR(GL_OUT_OF_MEMORY);
#undef BIND_ERROR
        };
        Error("%s:%d: %s generated %s", __FILE__, line, code, error_str);
    }
}

#define GL(_CODE) _CODE; CheckGLError(__LINE__, #_CODE)

// ================================================================================
// Batched quad renderer
// ================================================================================

#define MAX_QUADS 256

typedef struct R_Quad R_Quad;
struct R_Quad
{
    float p[2];
    float t[2];
};

typedef struct R_QuadRenderer R_QuadRenderer;
struct R_QuadRenderer
{
    GLuint vao;
    GLuint vbo;
    GLuint ibo;

    GLuint shader;
    
    R_Quad vbuf[MAX_QUADS * 4];
    size_t head;
};

void R_InitQuads(R_QuadRenderer *qr, GLuint shader)
{
    qr->shader = shader;

    GL(glGenVertexArrays(1, &qr->vao));
    GL(glBindVertexArray(qr->vao));

    GL(glGenBuffers(1, &qr->vbo));
    GL(glBindBuffer(GL_ARRAY_BUFFER, qr->vbo));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(qr->vbuf), qr->vbuf, GL_STREAM_DRAW));

    uint16_t ibuf[MAX_QUADS * 6];
    for (int i = 0; i < MAX_QUADS; ++i)
    {
        ibuf[6*i+0] = 4*i+0;
        ibuf[6*i+1] = 4*i+1;
        ibuf[6*i+2] = 4*i+2;
        ibuf[6*i+3] = 4*i+0;
        ibuf[6*i+4] = 4*i+2;
        ibuf[6*i+5] = 4*i+3;
    }

    GL(glGenBuffers(1, &qr->ibo));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, qr->ibo));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ibuf), ibuf, GL_STATIC_DRAW));

    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(R_Quad), (const void *)offsetof(R_Quad, p)));
    GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R_Quad), (const void *)offsetof(R_Quad, t)));
    GL(glEnableVertexAttribArray(0));
    GL(glEnableVertexAttribArray(1));
}

void R_FlushQuads(R_QuadRenderer *qr)
{
    if (qr->head > 0)
    {
        GL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(R_Quad) * 4 * qr->head, qr->vbuf));
        GL(glDrawElements(GL_TRIANGLES, qr->head * 6, GL_UNSIGNED_SHORT, 0));
        qr->head = 0;
    }
}

void R_PushQuad(R_QuadRenderer *qr, const R_Quad *quad4)
{
    qr->vbuf[qr->head*4+0] = quad4[0];
    qr->vbuf[qr->head*4+1] = quad4[1];
    qr->vbuf[qr->head*4+2] = quad4[2];
    qr->vbuf[qr->head*4+3] = quad4[3];
    ++qr->head;
    if (qr->head >= MAX_QUADS)
    {
        R_FlushQuads(qr);
    }
}

void R_BeginQuads(R_QuadRenderer *qr, int vp_w, int vp_h)
{
    GL(glBindVertexArray(qr->vao));
    GL(glBindBuffer(GL_ARRAY_BUFFER, qr->vbo));
    GL(glUseProgram(qr->shader));

    Mat4 proj = Mat4_Ortho(0, vp_w, 0, vp_h, 0, 10);
    GL(glUniformMatrix4fv(glGetUniformLocation(qr->shader, "u_proj"), 1, GL_FALSE, proj.m));
}

void R_EndQuads(R_QuadRenderer *qr)
{
    R_FlushQuads(qr);
}

// ================================================================================
// Client code
// ================================================================================

static struct
{
    R_QuadRenderer qr;
    int width;
    int height;
} cl = { 0 };

void CL_Init(const CL_InitParams *params)
{
    cl.width = -1;
    cl.height = -1;

    LoadOpenGLFunctions(params->glproc);
    Info("Loaded OpenGL: GL_VENDOR = %s, GL_RENDERER = %s", glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    GLuint prog_quad;

#define GLSL(_X) "#version 330 core\n" #_X
    const char* quad_vs = GLSL(
        layout (location = 0) in vec2 p;
        layout (location = 1) in vec2 t;

        uniform mat4 u_proj;

        void main()
        {
            gl_Position = u_proj * vec4(p.x, p.y, 0.0, 1.0);
        }
    );
    const char* quad_fs = GLSL(
        out vec4 out_color;

        void main()
        {
            out_color = vec4(1.0, 0.0, 0.0, 1.0);
        }
    );
#undef GLSL

    struct { GLuint* prog; const char *vsrc; const char *fsrc; } reg[] =
    {
        { .prog = &prog_quad, .vsrc = quad_vs, .fsrc = quad_fs, },
    };
    for (size_t i = 0; i < COUNTOF(reg); ++i)
    {
        GLuint prog = GL(glCreateProgram());

        GLuint vs = GL(glCreateShader(GL_VERTEX_SHADER));
        GL(glShaderSource(vs, 1, &reg[i].vsrc, 0));

        GLuint fs = GL(glCreateShader(GL_FRAGMENT_SHADER));
        GL(glShaderSource(fs, 1, &reg[i].fsrc, 0));

        GLuint shaders[] = { vs, fs };
        for (size_t j = 0; j < COUNTOF(shaders); ++j)
        {
            GL(glCompileShader(shaders[j]));

            GLint status = GL_TRUE;
            GL(glGetShaderiv(shaders[j], GL_COMPILE_STATUS, &status));
            if (status != GL_TRUE)
            {
                char shader_log[1024];
                GL(glGetShaderInfoLog(shaders[j], sizeof(shader_log), 0, shader_log));
                Error("Failed to compile shader: %s", shader_log);
            }

            GL(glAttachShader(prog, shaders[j]));
        }

        GL(glLinkProgram(prog));

        GLint status = GL_TRUE;
        GL(glGetProgramiv(prog, GL_LINK_STATUS, &status));
        if (status != GL_TRUE)
        {
            char shader_log[1024];
            GL(glGetProgramInfoLog(prog, sizeof(shader_log), 0, shader_log));
            Error("Failed to link shader: %s", shader_log);
        }

        *reg[i].prog = prog;
    }

    R_InitQuads(&cl.qr, prog_quad);
}

void CL_Render(int vp_width, int vp_height)
{
    if (cl.width != vp_width || cl.height != vp_height)
    {
        cl.width = vp_width;
        cl.height = vp_height;
        Info("Resized to %dx%d", cl.width, cl.height);
    }

    GL(glViewport(0, 0, vp_width, vp_height));
    GL(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    R_BeginQuads(&cl.qr, cl.width, cl.height);
        R_Quad rect[4] =
        {
            (R_Quad){ .p = { 0.0, 0.0 }, .t = { 0, 0 } },
            (R_Quad){ .p = { 100, 0.0 }, .t = { 0, 0 } },
            (R_Quad){ .p = { 100, 100 }, .t = { 0, 0 } },
            (R_Quad){ .p = { 0.0, 100 }, .t = { 0, 0 } },
        };
        R_PushQuad(&cl.qr, rect);
    R_EndQuads(&cl.qr);
}

// ================================================================================
// Server code
// ================================================================================

void SV_Init(const SV_InitParams *params)
{
    Info("Starting server %s", params->hostname);
}

// ================================================================================
// SDL3 shared
// ================================================================================

#ifdef CLIENT
#   define SDL_MAIN_USE_CALLBACKS
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

void OS_SpewError(const char *message)
{
    printf("Error: %s\n", message);
#ifdef CLIENT
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MicroShooter: Error!", message, 0);
#endif
    exit(1);
}

void OS_SpewInfo(const char *message)
{
    printf("%s\n", message);
}

// ================================================================================
// SDL3 client
// ================================================================================

#ifdef CLIENT

typedef struct AppState AppState;
struct AppState
{
    SDL_Window *wnd;
    SDL_GLContext glctx;
};

void RenderFrame(AppState *app)
{
    int vp_width = 0;
    int vp_height = 0;
    SDL_GetWindowSizeInPixels(app->wnd, &vp_width, &vp_height);
    CL_Render(vp_width, vp_height);
    SDL_GL_SwapWindow(app->wnd);
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    *appstate = calloc(1, sizeof(AppState));
    AppState *app = *appstate;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        Error("Failed to initialize SDL: %s", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    app->wnd = SDL_CreateWindow("MicroShooter", 800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!app->wnd)
    {
        Error("Failed to create window: %s", SDL_GetError());
    }

    app->glctx = SDL_GL_CreateContext(app->wnd);
    if (!app->glctx)
    {
        Error("Failed to create OpenGL context: %s", SDL_GetError());
    }

    CL_InitParams init = (CL_InitParams)
    {
        .glproc = (void *(*)(const char *))SDL_GL_GetProcAddress,
    };
    CL_Init(&init);

    // Render one frame before showing the window
    RenderFrame(app);

    SDL_ShowWindow(app->wnd);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    UNUSED(appstate);

    SDL_AppResult result = SDL_APP_CONTINUE;

    switch (event->type)
    {
        case SDL_EVENT_QUIT:
        {
            result = SDL_APP_SUCCESS;
            break;
        }
    }

    return result;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *app = appstate;

    RenderFrame(app);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    UNUSED(result);

    AppState *app = appstate;

    SDL_DestroyWindow(app->wnd);
    SDL_Quit();
}

#endif // CLIENT

// ================================================================================
// SDL3 server
// ================================================================================

#ifdef SERVER

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    printf("<server>\n");
}

#endif // SERVER

