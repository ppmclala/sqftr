#include <stdio.h>
#include <unistd.h>

#define NOB_IMPLEMENTATION
#include "nob.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"

Cmd cmd = {0};

void generate_compile_commands(Nob_Cmd cmd) {
    FILE *f = fopen("compile_commands.json", "w");
    if (!f) return;

    String_Builder sb = {0};
    sb_append_cstr(&sb, "[\n");
    sb_append_cstr(&sb, "  {\n");
    sb_append_cstr(&sb, "    \"directory\": ");
    char cwd[1024];
    sb_append_cstr(&sb, temp_sprintf("\"%s\",\n", getcwd(cwd, sizeof(cwd))));
    sb_append_cstr(&sb, "    \"arguments\": [\n");

    for (size_t i = 0; i < cmd.count; ++i) {
        sb_append_cstr(&sb, temp_sprintf("      \"%s\"%s\n", cmd.items[i], (i == cmd.count - 1) ? "" : ","));
    }

    sb_append_cstr(&sb, "    ],\n");
    sb_append_cstr(&sb, "    \"file\": \"main.c\"\n");
    sb_append_cstr(&sb, "  }\n");
    sb_append_cstr(&sb, "]\n");

    String_View sv = sb_to_sv(sb);

    fprintf(f, "%s", temp_sv_to_cstr(sv));

    fclose(f);
}

int main(int argc, char **argv)
{
    GO_REBUILD_URSELF(argc, argv);

    bool run = false;
    flag_bool_var(&run, "run", false, "Run the program after compilation.");

    if (!flag_parse(argc, argv)) {
        flag_print_error(stderr);
        return 1;
    }

    cmd_append(&cmd, "clang", "-Wall", "-Wextra", "-ggdb");
    cmd_append(&cmd, "-I./raylib-5.5_macos/include/");
    cmd_append(&cmd, "-o", "./sqftr", "sqftr.c");
    cmd_append(&cmd, "-L./raylib-5.5_macos/lib/");
    cmd_append(&cmd, "-lraylib", "-Wl,-rpath,./raylib-5.5_macos/lib/");
    cmd_append(&cmd, "-lm");

    generate_compile_commands(cmd);

    if (!cmd_run(&cmd)) return 1;

    if (run) {
        cmd_append(&cmd, "./main");
        da_append_many(&cmd, argv, argc);
        if (!cmd_run(&cmd)) return 1;
    }

    return 0;
}
