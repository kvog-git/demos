#include "yuvbench.h"

#define KBENCH_IMPLEMENTATION
#include "kbench.h"

#ifdef YUVBENCH_ACCELERATE
Backend yuvbench_accelerate(void);
#endif
#ifdef YUVBENCH_BAD
Backend yuvbench_bad(void);
#endif
#ifdef YUVBENCH_LIBYUV
Backend yuvbench_libyuv(void);
#endif
#ifdef YUVBENCH_SWSCALE
Backend yuvbench_swscale(void);
#endif

static struct
{
    uint32_t inp_w;
    uint32_t inp_h;
    void* inp_buf;
    size_t inp_len;
    void* out_buf;
    size_t out_len;
    bool show;
} G = { 0 };

static void run_backend(Backend b)
{
    Ctx ctx = { 0 };
    ctx.inp_w = G.inp_w;
    ctx.inp_h = G.inp_h;
    ctx.inp_buf = G.inp_buf;
    ctx.inp_len = G.inp_len;
    ctx.out_buf = G.out_buf;
    ctx.out_len = G.out_len;

    if (b.init_fn && !b.init_fn(&ctx)) {
        printf("Init failed\n");
        return;
    }

    if (!b.convert_fn) {
        printf("No convert function");
        return;
    }

    printf("    running warnups...\n");
    int warmup = 100;
    for (int i = 0; i < warmup; ++i) {
        b.convert_fn(&ctx);
    }

    printf("    testing...\n");
    int tests = 2500;
    double* tests_table = calloc(tests, sizeof(double)); assert(tests_table);
    for (int i = 0; i < tests; ++i) {
        uintptr_t t0 = KBenchTS();
        if (!b.convert_fn(&ctx)) {
            printf("Conversion failed\n");
            return;
        }
        uintptr_t t1 = KBenchTS();
        tests_table[i] = KBenchElapsedTime(t0, t1);
    }

    if (b.deinit_fn) {
        b.deinit_fn(&ctx);
    }

    double ts_min = -1.0f;
    double ts_max = -1.0f;
    double ts_avg = 0.0f;
    for (int i = 0; i < tests; ++i) {
        if (ts_min < 0 || tests_table[i] < ts_min) {
            ts_min = tests_table[i];
        }
        if (ts_max < 0 || tests_table[i] > ts_max) {
            ts_max = tests_table[i];
        }
        ts_avg += (tests_table[i] / (double)tests);
    }
    printf("    min result: %fms\n", ts_min * 1000.0f);
    printf("    max result: %fms\n", ts_max * 1000.0f);
    printf("    avg result: %fms\n", ts_avg * 1000.0f);


    if (G.show) {
        // Display last result
#if 1 && (defined(__APPLE__) || defined(__linux__))
        char cmd[512] = { 0 };
        snprintf(cmd, sizeof(cmd), "ffplay -hide_banner -f rawvideo -pixel_format rgb24 -video_size %dx%d -", G.inp_w, G.inp_h);
        FILE* pipe_fp = popen(cmd, "w");
        if (!pipe_fp) {
            printf("Failed to open ffplay: %s\n", strerror(errno));
            abort();
        }
        fwrite(G.out_buf, 1, G.out_len, pipe_fp);
        fflush(pipe_fp);
        if (pclose(pipe_fp) == -1) {
            printf("Failed to close ffplay: %s\n", strerror(errno));
            abort();
        }
#endif
    }
}

int main(int argc, char** argv)
{
    // The source file should be raw YUV 4:2:0 pixel data packed with no padding.
    // File name should be [whatever]-<width>x<height>.yuv
    if (argc < 2) {
        printf("No file supplied\n");
        return 1;
    }

    // Decode with and height from file name
    {
        // Scan for last "-"
        const char* suffix = 0;
        for (const char* s = argv[1]; *s; ++s) {
            if (*s == '-') {
                suffix = s + 1;
            }
        }
        if (!suffix) {
            printf("Invalid file name. See comments in yuvbench.c\n");
            return 1;
        }
        // Decode width and height
        for (; *suffix; ++suffix) {
            const char c = *suffix;
            if (c >= '0' && c <= '9') {
                G.inp_w *= 10;
                G.inp_w += (int)(c - '0');
            } else if (c == 'x') {
                ++suffix;
                break;
            } else {
                printf("Invalid file name. See comments in yuvbench.c\n");
                return 1;
            }
        }
        for (; *suffix; ++suffix) {
            const char c = *suffix;
            if (c >= '0' && c <= '9') {
                G.inp_h *= 10;
                G.inp_h += (int)(c - '0');
            } else if (c == '.') {
                break;
            } else {
                printf("Invalid file name. See comments in yuvbench.c\n");
                return 1;
            }
        }
        // Read from disk
        FILE* f = fopen(argv[1], "rb");
        if (!f) {
            printf("Failed to open file: %s\n", strerror(errno));
            return 1;
        }
        fseek(f, 0, SEEK_END);
        G.inp_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        assert(G.inp_len && "Zero-length file");
        G.inp_buf = calloc(1, G.inp_len);
        fread(G.inp_buf, 1, G.inp_len, f);
        fclose(f);
    }

    // Parse other CLI
    for (int i = 2; i < argc; ++i) {
        const char* s = argv[i];
        if (!strcmp(s, "show")) {
            G.show = true;
        }
    }
    printf("Input image is %dx%d YUV 4:2:0\n", G.inp_w, G.inp_h);
    printf("Input buffer size: %lu\n", G.inp_len);
    // 1x full res luminance plane + 2x half res chroma planes
    const size_t expected_len = (G.inp_w * G.inp_h) + ((G.inp_w / 2 * G.inp_h / 2) * 2);
    if (G.inp_len != expected_len) {
        printf("Expected file length %lu but got %lu. Allowing.\n", expected_len, G.inp_len);
    }

    G.out_len = G.inp_w * G.inp_h * 3; // RGB888
    G.out_buf = calloc(1, G.out_len);

#ifdef YUVBENCH_ACCELERATE
    printf("YUVBENCH_ACCELERATE\n");
    run_backend(yuvbench_accelerate());
#endif
#ifdef YUVBENCH_BAD
    printf("YUVBENCH_BAD\n");
    run_backend(yuvbench_bad());
#endif
#ifdef YUVBENCH_LIBYUV
    printf("YUVBENCH_LIBYUV\n");
    run_backend(yuvbench_libyuv());
#endif
#ifdef YUVBENCH_SWSCALE
    printf("YUVBENCH_SWSCALE\n");
    run_backend(yuvbench_swscale());
#endif
}
