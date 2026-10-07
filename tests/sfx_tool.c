/* sfx_tool.c - command-line harness around sfx_build (for tests). */
#include "../core/sfx_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prog(void *ctx, double f) { (void)ctx; (void)f; return 0; }

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "usage: %s <stub> <out> <settings_file|-> <level> <password|-> [file:arcname ...]\n", argv[0]);
        return 2;
    }
    const char *stub = argv[1];
    const char *out = argv[2];
    const char *settings_file = argv[3];
    int level = atoi(argv[4]);
    const char *password = strcmp(argv[5], "-") == 0 ? NULL : argv[5];

    char *settings = (char *)malloc(1);
    settings[0] = 0;
    size_t slen = 0;
    if (strcmp(settings_file, "-") != 0) {
        FILE *f = fopen(settings_file, "rb");
        if (!f) { perror("settings"); return 1; }
        fseek(f, 0, SEEK_END);
        slen = ftell(f);
        fseek(f, 0, SEEK_SET);
        settings = (char *)malloc(slen + 1);
        if (fread(settings, 1, slen, f) != slen) { fclose(f); return 1; }
        settings[slen] = 0;
        fclose(f);
    }

    int n = argc - 6;
    SfxInput *inputs = (SfxInput *)calloc(n ? n : 1, sizeof(SfxInput));
    for (int i = 0; i < n; ++i) {
        char *spec = strdup(argv[6 + i]);
        char *colon = strrchr(spec, ':');
        if (colon) { *colon = 0; inputs[i].disk_path = spec; inputs[i].arc_name = colon + 1; }
        else { inputs[i].disk_path = spec; inputs[i].arc_name = spec; }
    }

    char err[256] = {0};
    int rc = sfx_build(stub, out, inputs, n, settings, slen, level, password,
                       prog, NULL, err, sizeof(err));
    if (rc != 0) {
        fprintf(stderr, "sfx_build failed: %s\n", err);
        return 1;
    }
    printf("ok: wrote %s\n", out);
    return 0;
}
