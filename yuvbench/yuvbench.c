#include "yuvbench.h"

int main(int argc, char** argv)
{
    // The source file should be raw YUV 4:2:0 pixel data packed with no padding.
    // File name should be [whatever]-<width>x<height>.yuv
    if (argc < 2) {
        printf("No file supplied\n");
        return 1;
    }

    // Decode with and height from file name
    int inp_w = 0;
    int inp_h = 0;
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
                inp_w *= 10;
                inp_w += (int)(c - '0');
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
                inp_h *= 10;
                inp_h += (int)(c - '0');
            } else if (c == '.') {
                break;
            } else {
                printf("Invalid file name. See comments in yuvbench.c\n");
                return 1;
            }
        }
    }
    printf("Input image is %dx%d YUV 4:2:0\n", inp_w, inp_h);
}
