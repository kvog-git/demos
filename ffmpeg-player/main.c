// Barebones FFmpeg + SDL3 video playback
//
// SPDX-License-Identifier: GPL-3.0
//
// - Assumes video and audio streams are both present
// - Assumes both streams start at 0
//
// ref: https://github.com/libsdl-org/SDL/blob/main/test/testffmpeg.c
// ref: https://code.ffmpeg.org/FFmpeg/FFmpeg/src/branch/master/doc/examples/demux_decode.c
//
// Changelog:
// - 9/17/25: Initial release
// - 9/18/25: libswscale, flush decoders
//

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef struct Decoder Decoder;
struct Decoder
{
  const AVStream*           stream;
  const AVCodecParameters*  cpar;
  const AVCodec*            codec;
  AVCodecContext*           cctx;
};

static bool create_decoder(Decoder* dec, AVFormatContext* avfc, enum AVMediaType type)
{
  int ret = 0;

  if ((ret = av_find_best_stream(avfc, type, -1, -1, NULL, 0)) < 0) {
    fprintf(stderr, "Failed to find stream: %s\n", av_err2str(ret));
    return false;
  }

  dec->stream = avfc->streams[ret];
  dec->cpar   = dec->stream->codecpar;

  if (!(dec->codec = avcodec_find_decoder(dec->stream->codecpar->codec_id))) {
    fprintf(stderr, "Failed to open decoder\n");
    return false;
  }

  if (!(dec->cctx = avcodec_alloc_context3(dec->codec))) {
    fprintf(stderr, "Failed to allocate decoder context\n");
    return false;
  }

  if ((ret = avcodec_parameters_to_context(dec->cctx, dec->stream->codecpar)) < 0) {
    fprintf(stderr, "Failed to set decoder parameters: %s\n", av_err2str(ret));
    return false;
  }

  if ((ret = avcodec_open2(dec->cctx, dec->codec, NULL)) < 0) {
    fprintf(stderr, "Failed to open decoder: %s\n", av_err2str(ret));
    return false;
  }

  return true;
}

int main(int argc, char* argv[])
{
  if (argc <= 1) {
    fprintf(stderr, "Usage: %s <file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  int ret = 0;

  av_log_set_level(AV_LOG_VERBOSE);

  AVFormatContext* avfc = NULL;
  if ((ret = avformat_open_input(&avfc, argv[1], NULL, NULL)) < 0) {
    fprintf(stderr, "Failed to open input: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  if ((ret = avformat_find_stream_info(avfc, NULL)) < 0) {
    fprintf(stderr, "Failed to find stream info: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  Decoder vdec = { 0 };
  if (!create_decoder(&vdec, avfc, AVMEDIA_TYPE_VIDEO)) {
    return EXIT_FAILURE;
  }

  Decoder adec = { 0 };
  if (!create_decoder(&adec, avfc, AVMEDIA_TYPE_AUDIO)) {
    return EXIT_FAILURE;
  }

  SDL_Window* wnd = SDL_CreateWindow("ffmpeg-player", vdec.cpar->width, vdec.cpar->height, SDL_WINDOW_HIDDEN);
  if (!wnd) {
    fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  int ww = vdec.cpar->width;
  int wh = vdec.cpar->height;
  SDL_Rect drect;
  if (SDL_GetDisplayBounds(SDL_GetDisplayForWindow(wnd), &drect)) {
    while (ww >= drect.w || wh >= drect.h) {
      ww /= 2;
      wh /= 2;
    }
  }
  SDL_SetWindowSize(wnd, ww, wh);
  SDL_ShowWindow(wnd);

  SDL_Renderer* rnd = SDL_CreateRenderer(wnd, NULL);
  if (!rnd) {
    fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_SetRenderVSync(rnd, 1);

  AVPacket* avp      = av_packet_alloc();
  AVFrame*  avf      = av_frame_alloc();
  AVFrame*  avf_sdl  = av_frame_alloc();
  SDL_assert(avp && avf && avf_sdl);

  // Use a YUV texture when blitting to the screen. Most video files are decoded to
  // AV_PIX_FMT_YUV420P frames anyway, but convert anyway to use the same code path.
  SDL_Texture* tex = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                       vdec.cpar->width, vdec.cpar->height);
  if (!tex) {
    fprintf(stderr, "Failed to create texture: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  avf_sdl->format = AV_PIX_FMT_YUV420P;
  avf_sdl->width  = vdec.cpar->width;
  avf_sdl->height = vdec.cpar->height;
  if ((ret = av_frame_get_buffer(avf_sdl, 32)) < 0) {
    fprintf(stderr, "Failed to allocate frame: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }
  if ((ret = av_frame_make_writable(avf_sdl)) < 0) {
    fprintf(stderr, "Failed to make frame writable: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  struct SwsContext* sws = sws_getContext(vdec.cpar->width, vdec.cpar->height, vdec.cpar->format,
                                          vdec.cpar->width, vdec.cpar->height, AV_PIX_FMT_YUV420P,
                                          0, NULL, NULL, NULL);
  if (!sws) {
    fprintf(stderr, "Failed to create libswscale context\n");
    return EXIT_FAILURE;
  }

  // libswresample is like libswscale but for audio. This barely works because I don't
  // really understand audio programming.
  SDL_AudioSpec aspec = { 0 };
  aspec.format   = SDL_AUDIO_F32;
  aspec.channels = adec.cctx->ch_layout.nb_channels;
  aspec.freq     = adec.cctx->sample_rate;
  SDL_AudioStream* astream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &aspec, NULL, NULL);
  if (!astream) {
    fprintf(stderr, "Failed to create audio device stream: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }
  SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(astream));

  SwrContext* swr = swr_alloc(); SDL_assert(swr);
  ret = swr_alloc_set_opts2(&swr,
    &adec.cctx->ch_layout, AV_SAMPLE_FMT_FLT,     adec.cctx->sample_rate,
    &adec.cctx->ch_layout, adec.cctx->sample_fmt, adec.cctx->sample_rate,
    0, NULL);
  if (ret < 0) {
    fprintf(stderr, "Failed to set swresample parameters: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }
  if ((ret = swr_init(swr)) < 0) {
    fprintf(stderr, "Failed to initialize swresample: %s\n", av_err2str(ret));
    return EXIT_FAILURE;
  }

  bool eof     = false;
  bool flushed = false;
  bool running = true;
  while (running) {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_EVENT_QUIT) {
        running = false;
      }
      if (evt.type == SDL_EVENT_KEY_DOWN) {
        if (evt.key.key == SDLK_ESCAPE || evt.key.key == SDLK_Q) {
          running = false;
        }
      }
    }

    // Read a packet from the file
    if (!eof) {
      ret = av_read_frame(avfc, avp);
      if (ret < 0) {
        if (ret == AVERROR_EOF) {
          eof = true;
        } else {
          fprintf(stderr, "Error reading frame: %s\n", av_err2str(ret));
          return EXIT_FAILURE;
        }
      }

      if (eof) {
        if (!flushed) {
          // Send NULL to flush the decoders
          avcodec_send_packet(vdec.cctx, NULL);
          avcodec_send_packet(adec.cctx, NULL);
          flushed = true;
        }
        // End of file, no need to continue feeding the decoders
        continue;
      }

      // Send encoded packet to decoder.
      // Decoding isn't a 1:1 process. One or more packets produce one or more frames.
      if (avp->stream_index == vdec.stream->index) {
        if ((ret = avcodec_send_packet(vdec.cctx, avp)) < 0) {
          fprintf(stderr, "Failed to send video packet to decoder: %s\n", av_err2str(ret));
          return EXIT_FAILURE;
        }
      }
      if (avp->stream_index == adec.stream->index) {
        if ((ret = avcodec_send_packet(adec.cctx, avp)) < 0) {
          fprintf(stderr, "Failed to send audio packet to decoder: %s\n", av_err2str(ret));
          return EXIT_FAILURE;
        }
      }

      av_packet_unref(avp);
    }

    static double start = -1.0f;
    if (start < 0.0f) {
      start = SDL_GetTicks() / 1e3f;
    }

    // debug
    static double d_vpts = 0.0f;
    static double d_apts = 0.0f;

    // Receive zero or more video frames from the video decoder
    while (avcodec_receive_frame(vdec.cctx, avf) >= 0) {
      // This sync really sucks. I might fix it some day.
      const double now = SDL_GetTicks() / 1e3f;
      const double pts = avf->best_effort_timestamp * av_q2d(vdec.stream->time_base);
      d_vpts = pts;
      if (now - start < pts) {
        SDL_DelayPrecise((pts - now + start) * SDL_NS_PER_SECOND);
      }

      sws_scale(sws, (const uint8_t* const*)avf->data, avf->linesize, 0, avf->height,
                avf_sdl->data, avf_sdl->linesize);

      bool updated = SDL_UpdateYUVTexture(tex, NULL, avf_sdl->data[0], avf_sdl->linesize[0],
                                                     avf_sdl->data[1], avf_sdl->linesize[1],
                                                     avf_sdl->data[2], avf_sdl->linesize[2]);
      if (!updated) {
        fprintf(stderr, "Failed to update YUV texture: %s\n", SDL_GetError());
        return EXIT_FAILURE;
      }

      SDL_SetRenderDrawColor(rnd, 0x00, 0x00, 0x00, 0xFF);
      SDL_RenderClear(rnd);
      SDL_SetRenderDrawColor(rnd, 0xFF, 0xFF, 0xFF, 0xFF);
      SDL_RenderTexture(rnd, tex, NULL, NULL);
      SDL_RenderDebugTextFormat(rnd, 10, 10, "now    %.2f\n", now);
      SDL_RenderDebugTextFormat(rnd, 10, 20, "v pts  %.2f\n", d_vpts);
      SDL_RenderDebugTextFormat(rnd, 10, 30, "a pts  %.2f\n", d_apts);

      SDL_RenderPresent(rnd);

      av_frame_unref(avf);
    }

    // Receive one or more audio frames from the audio decoder
    while (avcodec_receive_frame(adec.cctx, avf) >= 0) {
      d_apts = avf->best_effort_timestamp * av_q2d(adec.stream->time_base);

      int max_out = swr_get_out_samples(swr, avf->nb_samples);
      if (max_out > 0) {
        uint8_t* out_data     = NULL;
        int      out_linesize = 0;

        av_samples_alloc(&out_data, &out_linesize, avf->ch_layout.nb_channels,
                         max_out, AV_SAMPLE_FMT_FLT, 0);

        const uint8_t** in = (const uint8_t**)avf->extended_data;
        ret = swr_convert(swr, &out_data, max_out, in, avf->nb_samples);
        if (ret > 0) {
          size_t out_bytes = (size_t)ret * avf->ch_layout.nb_channels * 
            av_get_bytes_per_sample(AV_SAMPLE_FMT_FLT);
          SDL_PutAudioStreamData(astream, out_data, out_bytes);
        }

        av_freep(&out_data);
      }

      av_frame_unref(avf);
    }
  }

  SDL_DestroyWindow(wnd);

  return EXIT_SUCCESS;
}
