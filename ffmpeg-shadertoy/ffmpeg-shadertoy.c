// ================================================================================================
// Basic offline shadertoy rendererer. Textures are not supported.
//
// Usage:
//     $ ffmpeg-shadertoy <input.glsl> <output> -w <width> -h <height> -r <fps> -t <length>
//
//     For example, download https://www.shadertoy.com/view/Ms2SD1 as seascape.glsl and run:
//     $ ffmpeg-shadertoy ./seascape.glsl -w 1920 -h 1080 -r 60 seascape.mkv
//
// Changelog:
//     6/7/2026: Initial release
//
// License:
//     SPDX-License-Identifier: 0BSD
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <assert.h>
#include <stdlib.h>

#define GLAD_GL_IMPLEMENTATION
#include "../_common/c/thirdparty/glad33/gl.h"

#define CRF "20"

#define COUNTOF(ARR) (sizeof(ARR) / sizeof((ARR)[0]))

#define ASSERT assert

#define ASSERT_AV(RET, MESSAGE)                                 \
    do {                                                        \
        if ((RET) < 0) {                                        \
            printf("%s: %s\n", (MESSAGE), av_err2str(RET));     \
            ASSERT(0 && "FFmpeg function failed");              \
        }                                                       \
    } while (0);

#define ASSERT_SDL(RET, MESSAGE)                                \
    do {                                                        \
        if (!(RET)) {                                           \
            printf("%s: %s\n", (MESSAGE), SDL_GetError());      \
            ASSERT(0 && "SDL function failed");                 \
        }                                                       \
    } while (0);

static inline bool compile_shader(GLuint *shader, GLenum type, const char **srcs, size_t num_srcs);

int main(int argc, const char **argv)
{
    // Parse command-line arguments
    int a_w = 1280;
    int a_h = 720;
    int a_t = 10;
    int a_r = 30;
    const char *a_src = 0;
    const char *a_dst = 0;
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (i + 1 < argc) {
                 if (!SDL_strcasecmp(a, "-w")) { a_w = SDL_atoi(argv[++i]); continue; }
            else if (!SDL_strcasecmp(a, "-h")) { a_h = SDL_atoi(argv[++i]); continue; }
            else if (!SDL_strcasecmp(a, "-t")) { a_t = SDL_atoi(argv[++i]); continue; }
            else if (!SDL_strcasecmp(a, "-r")) { a_r = SDL_atoi(argv[++i]); continue; }
        }
        if (!a_src) {
            a_src = a;
        }
        else if (!a_dst) {
            a_dst = a;
        } else {
            printf("Unknown argument %s\n", a);
            return EXIT_FAILURE;
        }
    }

    if (!a_src) {
        printf("No input file supplied\n");
        return EXIT_FAILURE;
    }
    if (!a_dst) {
        printf("No output file specified\n");
        return EXIT_FAILURE;
    }

    printf("Rendering %s (%dx%d@%d) %ds (crf %s) to %s\n", a_src, a_w, a_h, a_r, a_t, CRF, a_dst);

    // Open output format context
    AVFormatContext *avfc;
    int ret = avformat_alloc_output_context2(&avfc, 0, 0, a_dst);
    ASSERT_AV(ret, "Failed to allocate output context");

    // Encode video as H.264
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    ASSERT(codec && "No H.264 encoder available");

    AVCodecContext *avcc = avcodec_alloc_context3(codec);
    ASSERT(avcc && "Failed to create H.264 encoder context");

    AVStream *avs = avformat_new_stream(avfc, codec);
    ASSERT(avs && "Failed to create video stream");

    // Stream options
    avs->time_base.num = 1;
    avs->time_base.den = a_r;

    // Encoder options
    avcc->codec_id     = codec->id;
    avcc->width        = a_w;
    avcc->height       = a_h;
    avcc->time_base    = avs->time_base;
    avcc->gop_size     = 2 * a_r;
    avcc->max_b_frames = 2;
    avcc->pix_fmt      = AV_PIX_FMT_YUV420P;
    av_opt_set(avcc->priv_data, "preset", "fast",      0);
    av_opt_set(avcc->priv_data, "tune",   "animation", 0);
    av_opt_set(avcc->priv_data, "crf",    CRF,         0);

    // Not completely sure what this flag does, but every FFmpeg demo ever has it.
    if (avfc->oformat->flags & AVFMT_GLOBALHEADER) {
        avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    ret = avcodec_open2(avcc, codec, 0);
    ASSERT_AV(ret, "Failed to open H.264 encoder");

    // Set stream settings from encoder parameters
    ret = avcodec_parameters_from_context(avs->codecpar, avcc);
    ASSERT_AV(ret, "Failed to copy encoder parameters");

    ret = avio_open(&avfc->pb, a_dst, AVIO_FLAG_WRITE);
    ASSERT_AV(ret, "Failed to open output file");

    ret = avformat_write_header(avfc, 0);
    ASSERT_AV(ret, "Failed to write file header");

    // Use one packet for encode
    AVPacket *pkt = av_packet_alloc();
    ASSERT(pkt);

    // Use two frames for encode - one RGBA and one YUV420P
    AVFrame *src_frame = av_frame_alloc();
    AVFrame *dst_frame = av_frame_alloc();
    ASSERT(src_frame && dst_frame);

    src_frame->format = AV_PIX_FMT_RGBA;
    src_frame->width  = a_w;
    src_frame->height = a_h;
    ret = av_frame_get_buffer(src_frame, 32);
    ASSERT_AV(ret, "Failed to allocate src frame buffer");
    ret = av_frame_make_writable(src_frame);
    ASSERT_AV(ret, "Failed to make src frame writable");

    dst_frame->format = AV_PIX_FMT_YUV420P;
    dst_frame->width  = src_frame->width;
    dst_frame->height = src_frame->height;
    ret = av_frame_get_buffer(dst_frame, 32);
    ASSERT_AV(ret, "Failed to allocate dst frame buffer");
    ret = av_frame_make_writable(dst_frame);
    ASSERT_AV(ret, "Failed to make dst frame writable");

    // RGBA to YUV420P conversion context.
    // This coule be replaced with a simple GLSL shader, but this is less code.
    struct SwsContext *sws = sws_getContext(src_frame->width, src_frame->height, src_frame->format,
                                            dst_frame->width, dst_frame->height, dst_frame->format,
                                            0, 0, 0, 0);
    ASSERT(sws && "Failed to create libswscale context for RGBA -> YUV420P");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        ASSERT_SDL(0, "Failed to initialize SDL");
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *wnd = SDL_CreateWindow("ffmpeg-shadertoy", a_w, a_h, SDL_WINDOW_OPENGL);
    if (!wnd) {
        ASSERT_SDL(0, "Failed to create window");
    }

    if (!SDL_GL_CreateContext(wnd)) {
        ASSERT_SDL(0, "Failed to create OpenGL context");
    }

    if (!gladLoadGL(SDL_GL_GetProcAddress)) {
        ASSERT(0 && "Failed to load OpenGL functions");
    }

    static float vdata[] = {
        -1.0f,  1.0f,
         1.0f,  1.0f,
         1.0f, -1.0f,
        -1.0f, -1.0f
    };
    static uint16_t idata[] = {
        0, 1, 2,
        0, 2, 3
    };

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vdata), vdata, GL_STATIC_DRAW);

    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idata), idata, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
    glEnableVertexAttribArray(0);

    const char *vs_src = 
        "#version 330 core                              \n"
        "                                               \n"
        "layout (location = 0) in vec2 p;               \n"
        "                                               \n"
        "void main()                                    \n"
        "{                                              \n"
        "    gl_Position = vec4(p.x, p.y, 0.0f, 1.0f);  \n"
        "}                                              \n";
    const char *fs_src1 =
        "#version 330 core                              \n"
        "                                               \n"
        "uniform vec4  iMouse;                          \n"
        "uniform vec3  iResolution;                     \n"
        "uniform float iTime;                           \n"
        "                                               \n"
        "out vec4 _color;                               \n"
        "                                               \n"
        "void mainImage(out vec4, vec2);                \n"
        "                                               \n"
        "void main()                                    \n"
        "{                                              \n"
        "    mainImage(_color, gl_FragCoord.xy);        \n"
        "}                                              \n";

    const char *fs_src2 = SDL_LoadFile(a_src, 0);
    if (!fs_src2) {
        printf("Failed to read %s: %s\n", a_src, SDL_GetError());
        return EXIT_FAILURE;
    }

    GLuint vs;
    if (!compile_shader(&vs, GL_VERTEX_SHADER, &vs_src, 1)) {
        printf("Failed to compile vertex shader\n");
        return EXIT_FAILURE;
    }
    GLuint fs;
    const char *fs_srcs[] = {
        fs_src1,
        fs_src2
    };
    if (!compile_shader(&fs, GL_FRAGMENT_SHADER, fs_srcs, COUNTOF(fs_srcs))) {
        printf("Failed to compile fragment shader\n");
        return EXIT_FAILURE;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint status;
    glGetProgramiv(prog, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char error[1024] = { 0 };
        glGetProgramInfoLog(prog, sizeof(error), 0, error);
        printf("Shader link error: %s\n", error);
        return EXIT_FAILURE;
    }

    glUseProgram(prog);
    glViewport(0, 0, a_w, a_h);

    glUniform3f(glGetUniformLocation(prog, "iResolution"), (float)a_w, (float)a_h, 0.0f);

    bool quit = false;
    int64_t frame_pts = 0;
    for (double t = 0; !quit && t <= (double)a_t; t += 1.0f / (double)a_r) {
        printf("t=%.2f/%.2f\n", t, (double)a_t);

        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            quit |= (evt.type == SDL_EVENT_QUIT);
            quit |= (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_ESCAPE);
            quit |= (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_Q);
        }

        glUniform1f(glGetUniformLocation(prog, "iTime"), t);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        SDL_GL_SwapWindow(wnd);

        // Read pixels from screen into src_frame
        glReadPixels(0, 0, a_w, a_h, GL_RGBA, GL_UNSIGNED_BYTE, src_frame->data[0]);

        // Invert frame. This could be done in the vertex shader, but it would break the preview.
        const uint8_t *src_data0    = src_frame->data[0] + (a_h - 1) * src_frame->linesize[0];
        const uint8_t *src_data[4]  = { src_data0,               0, 0, 0 };
        int            src_lines[4] = { -src_frame->linesize[0], 0, 0, 0 };
        sws_scale(sws, src_data, src_lines, 0, src_frame->height, dst_frame->data,
                  dst_frame->linesize);

        dst_frame->pts = frame_pts++;

        // Send frame to encoder
        ret = avcodec_send_frame(avcc, dst_frame);
        ASSERT_AV(ret, "Failed to send frame to encoder");

        // After the last frame, flush the encoder
        if (quit || t + 1.0f / (double)a_r > (double)a_t) {
            ret = avcodec_send_frame(avcc, 0);
            ASSERT_AV(ret, "Failed to flush encoder");
        }

        // Receive 0 or more packets from encoder
        while (ret >= 0) {
            ret = avcodec_receive_packet(avcc, pkt);
            if (ret < 0) {
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    // Need more
                    break;
                }
                ASSERT_AV(ret, "Failed to receive packet from encoder");
            }

            av_packet_rescale_ts(pkt, avcc->time_base, avs->time_base);

            ret = av_interleaved_write_frame(avfc, pkt);
            ASSERT_AV(ret, "Failed to write packet to file");
        }
    }

    ret = av_write_trailer(avfc);
    ASSERT_AV(ret, "Failed to write file trailer");

    avio_closep(&avfc->pb);

    SDL_DestroyWindow(wnd);

    return EXIT_SUCCESS;
}

static inline bool compile_shader(GLuint *shader, GLenum type, const char **srcs, size_t num_srcs)
{
    *shader = glCreateShader(type);
    glShaderSource(*shader, num_srcs, srcs, 0);
    glCompileShader(*shader);
    GLint status;
    glGetShaderiv(*shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char error[1024] = { 0 };
        glGetShaderInfoLog(*shader, sizeof(error), 0, error);
        printf("Shader compile error: %s\n", error);
        return false;
    }
    return true;
}

