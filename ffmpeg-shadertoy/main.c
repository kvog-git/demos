// ShaderToy video renderer
//
// SPDX-License-Identifier: 0BSD
//
// Usage: ffmpeg-shadertoy -w <width> -h <height> -t <seconds> -fps <fps> -crf <crf> <input.glsl> <output.mp4>
//
// - No texture support
//
// ref: https://code.ffmpeg.org/FFmpeg/FFmpeg/src/branch/master/doc/examples/mux.c
// ref: https://code.ffmpeg.org/FFmpeg/FFmpeg/src/branch/master/doc/examples/encode_video.c
//

#define GLAD_GL_IMPLEMENTATION
#include "../common/c_cpp/thirdparty/glad33/glad33.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>

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
  int         a_w   = 1280;
  int         a_h   = 720;
  int         a_t   = 10;
  int         a_fps = 30;
  int         a_crf = 20;
  const char* a_src = NULL;
  const char* a_dst = NULL;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (!SDL_strcasecmp(arg, "-w") && i + 1 < argc) {
      a_w = SDL_atoi(argv[i + 1]);
      i += 1;
    } 
    else if (!SDL_strcasecmp(arg, "-h") && i + 1 < argc) {
      a_h = SDL_atoi(argv[i + 1]);
      i += 1;
    }
    else if (!SDL_strcasecmp(arg, "-t") && i + 1 < argc) {
      a_t = SDL_atoi(argv[i + 1]);
      i += 1;
    }
    else if (!SDL_strcasecmp(arg, "-fps") && i + 1 < argc) {
      a_fps = SDL_atoi(argv[i + 1]);
      i += 1;
    }
    else if (!SDL_strcasecmp(arg, "-crf") && i + 1 < argc) {
      a_crf = SDL_atoi(argv[i + 1]);
      i += 1;
    }
    else {
      printf("a=%s\n", arg);
      if (!a_src) {
        a_src = argv[i];
      }
      else if (!a_dst) {
        a_dst = argv[i];
      }
      else {
        fprintf(stderr, "Unknown argument %s\n", arg);
        return EXIT_FAILURE;
      }
    }
  }

  if (!a_src) {
    fprintf(stderr, "No input file specified\n");
    return EXIT_FAILURE;
  }

  if (!a_dst) {
    fprintf(stderr, "No output file specified\n");
    return EXIT_FAILURE;
  }

  printf("Rendering %s w=%d h=%d t=%d fps=%d to %s\n", a_src, a_w, a_h, a_t, a_fps, a_dst);

  int ret = 0;

  AVFormatContext* avfc = NULL;
  if ((ret = avformat_alloc_output_context2(&avfc, NULL, NULL, a_dst)) < 0) {
    fprintf(stderr, "Failed to allocate output context: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  // Always encode H.264 mp4, no matter what
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) {
    fprintf(stderr, "No H.264 encoder is available\n");
    return EXIT_FAILURE;
  }

  AVStream* avs = avformat_new_stream(avfc, codec);
  if (!avs) {
    fprintf(stderr, "Failed to create video stream\n");
    return EXIT_FAILURE;
  }

  AVCodecContext* avcc = avcodec_alloc_context3(codec);
  if (!avcc) {
    fprintf(stderr, "Failed to allocate codec context\n");
    return EXIT_FAILURE;
  }

  avs->time_base.num = 1;
  avs->time_base.den = a_fps;

  avcc->codec_id      = codec->id;
  avcc->width         = a_w;
  avcc->height        = a_h;
  avcc->time_base     = avs->time_base;
  avcc->gop_size      = 2 * a_fps;
  avcc->max_b_frames  = 2;
  avcc->pix_fmt       = AV_PIX_FMT_YUV420P;

  char crf_str[32];
  snprintf(crf_str, sizeof(crf_str), "%d", a_crf);

  av_opt_set(avcc->priv_data, "preset", "fast",       0);
  av_opt_set(avcc->priv_data, "tune",   "animation",  0);
  av_opt_set(avcc->priv_data, "crf",    crf_str,      0);

  if (avfc->oformat->flags & AVFMT_GLOBALHEADER) {
    avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  if ((ret = avcodec_open2(avcc, codec, NULL)) < 0) {
    fprintf(stderr, "Failed to open encoder: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  if ((ret = avcodec_parameters_from_context(avs->codecpar, avcc)) < 0) {
    fprintf(stderr, "Failed to set stream codec parameters: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  if ((ret = avio_open(&avfc->pb, a_dst, AVIO_FLAG_WRITE)) < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", a_dst, av_err2str(ret));
    return EXIT_FAILURE;
  }

  if ((ret = avformat_write_header(avfc, NULL)) < 0) {
    fprintf(stderr, "Failed to write file header: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  AVPacket* pkt = av_packet_alloc();
  if (!pkt) {
    fprintf(stderr, "Failed to allocate packet\n");
    return EXIT_FAILURE;
  }

  AVFrame* frame_src = av_frame_alloc();
  AVFrame* frame_dst = av_frame_alloc();
  if (!frame_src || !frame_dst) {
    fprintf(stderr, "Failed to allocate frame\n");
    return EXIT_FAILURE;
  }

  frame_src->format = AV_PIX_FMT_RGBA;
  frame_src->width  = a_w;
  frame_src->height = a_h;
  if ((ret = av_frame_get_buffer(frame_src, 32)) < 0) {
    fprintf(stderr, "Failed to allocate src frame buffer: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }
  if ((ret = av_frame_make_writable(frame_src)) < 0) {
    fprintf(stderr, "Failed to make src frame writable: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  frame_dst->format = AV_PIX_FMT_YUV420P;
  frame_dst->width  = a_w;
  frame_dst->height = a_h;
  if ((ret = av_frame_get_buffer(frame_dst, 32)) < 0) {
    fprintf(stderr, "Failed to allocate dst frame buffer: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }
  if ((ret = av_frame_make_writable(frame_dst)) < 0) {
    fprintf(stderr, "Failed to make dst frame writable: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  struct SwsContext* sws = sws_getContext(frame_src->width, frame_src->height, frame_src->format,
                                          frame_src->width, frame_src->height, frame_dst->format,
                                          0, NULL, NULL, NULL);
  if (!sws) {
    fprintf(stderr, "Failed to get libswscale context\n");
    return EXIT_FAILURE;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  SDL_Window* wnd = SDL_CreateWindow("ffmpeg-shadertoy", a_w, a_h, SDL_WINDOW_OPENGL);
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

  const float vdata[] = {
    -1.0f,  1.0f, -1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, -1.0f, 1.0f, 1.0f,
     1.0f, -1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, -1.0f, 0.0f, 0.0f,
  };

  const uint16_t idata[] = {
    0, 1, 2,
    0, 2, 3,
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

  GL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 0)));
  GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3)));
  GL(glEnableVertexAttribArray(0));
  GL(glEnableVertexAttribArray(1));

  const char* vs_src = SDL_LoadFile("vs.glsl", NULL);
  if (!vs_src) {
    fprintf(stderr, "Failed to load vertex shader: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  GLuint vs = GL(glCreateShader(GL_VERTEX_SHADER));
  GL(glShaderSource(vs, 1, &vs_src, NULL));
  GL(glCompileShader(vs));
  GLint vs_status = 0;
  GL(glGetShaderiv(vs, GL_COMPILE_STATUS, &vs_status));
  if (vs_status != GL_TRUE) {
    char error[1024] = { 0 };
    GL(glGetShaderInfoLog(vs, sizeof(error), NULL, error));
    SDL_Log("Error compiling vertex shader: %s\n", error);
    return EXIT_FAILURE;
  }

  const char* fs_src1 = SDL_LoadFile("fs1.glsl", NULL);
  if (!fs_src1) {
    fprintf(stderr, "Failed to load fragment shader: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  const char* fs_src2 = SDL_LoadFile(a_src, NULL);
  if (!fs_src2) {
    fprintf(stderr, "Failed to load source shader: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  const char* fs_src3 = SDL_LoadFile("fs2.glsl", NULL);
  if (!fs_src3) {
    fprintf(stderr, "Failed to load fragment shader: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  const char* fs_src[] = {
    fs_src1,
    fs_src2,
    fs_src3,
  };

  GLuint fs = GL(glCreateShader(GL_FRAGMENT_SHADER));
  GL(glShaderSource(fs, 3, fs_src, NULL));
  GL(glCompileShader(fs));
  GLint fs_status = 0;
  GL(glGetShaderiv(fs, GL_COMPILE_STATUS, &fs_status));
  if (fs_status != GL_TRUE) {
    char error[1024] = { 0 };
    GL(glGetShaderInfoLog(fs, sizeof(error), NULL, error));
    SDL_Log("Error compiling fragment shader: %s\n", error);
    return EXIT_FAILURE;
  }

  GLuint prog = GL(glCreateProgram());
  GL(glAttachShader(prog, vs));
  GL(glAttachShader(prog, fs));
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

  GL(glViewport(0, 0, a_w, a_h));

  // Constant uniforms
  GLint upos = -1;
  if ((upos = glGetUniformLocation(prog, "iResolution")) >= 0) {
    GL(glUniform3f(upos, (float)a_w, (float)a_h, 0.0f));
  }

  int64_t frame_pts = 0;

  for (float t = 0; t <= (float)a_t; t += 1.0f / (float)a_fps) {
    printf("t=%.2f/%.2f\n", t, (float)a_t);

    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
      bool quit = false;
      switch (evt.type) {
      case SDL_EVENT_QUIT: {
        quit = true;
      } break;
      case SDL_EVENT_KEY_DOWN: {
        if (evt.key.key == SDLK_ESCAPE || evt.key.key == SDLK_Q) {
          quit = true;
        }
      } break;
      if (quit) {
        exit(1);
      }
    }
    }

    // Verying uniforms
    if ((upos = glGetUniformLocation(prog, "iTime")) >= 0) {
      GL(glUniform1f(upos, t));
    }

    GL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL));

    GL(glReadPixels(0, 0, a_w, a_h, GL_RGBA, GL_UNSIGNED_BYTE, frame_src->data[0]));

    // Invert frame. Could do this in vertex shader but that would break the preview.
    const uint8_t* src_data0 = frame_src->data[0] + (a_h - 1) * frame_src->linesize[0];
    const uint8_t* src_data[4]  = { src_data0,               NULL, NULL, NULL };
    int            src_lines[4] = { -frame_src->linesize[0], 0,    0,    0    };

    sws_scale(sws, src_data, src_lines, 0, frame_src->height,
                frame_dst->data, frame_dst->linesize);

    frame_dst->pts = frame_pts++;

    if ((ret = avcodec_send_frame(avcc, frame_dst)) < 0) {
      fprintf(stderr, "Failed to send video frame to encoder: %s\n", av_err2str(ret));
      return EXIT_FAILURE;
    }

    while (ret >= 0) {
      if ((ret = avcodec_receive_packet(avcc, pkt)) < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
          break;
        }
        else {
          fprintf(stderr, "Error receiving packet from encoder: %s\n", av_err2str(ret));
          return EXIT_FAILURE;
        }
      }

      av_packet_rescale_ts(pkt, avcc->time_base, avs->time_base);

      if ((ret = av_interleaved_write_frame(avfc, pkt)) < 0) {
        fprintf(stderr, "Error writing packet: %s\n", av_err2str(ret));
        return EXIT_FAILURE;
      }
    }

    SDL_GL_SwapWindow(wnd);
  }

  if ((ret = av_write_trailer(avfc)) < 0) {
    fprintf(stderr, "Failed to write file trailer: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  avio_closep(&avfc->pb);

  SDL_DestroyWindow(wnd);

  return EXIT_SUCCESS;
}
