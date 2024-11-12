#pragma once

#include <cstdio>
#include <shell.h>
#include <ff.h>

class FileSystemUtils_t
{
public:
    static bool normalizePath(char *path);
    static bool joinPath(char *dst, const char *base, const char *ext);
    static bool relativePathToAbsolute(const char *rel_path, char *abs_path);
    static const char *getCurrentDir();
    static bool changeDir(const char *path);
    static void getFilesList();

    static bool shell_cmd_pwd(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_ls(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_cd(FILE *f, ShellCmd_t *cmd, const char *s);

private:
    static char m_current_folder[FF_MAX_LFN];
};

