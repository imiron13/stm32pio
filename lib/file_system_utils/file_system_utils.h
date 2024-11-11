#pragma once

#include <cstdio>
#include <shell.h>

class FileSystemUtils_t
{
public:
    static void relativePathToAbsolute(const char *rel_path);
    static const char *getCurrentDir();
    static void changeDir(const char *path);
    static void getFilesList();
};

bool shell_cmd_ls(FILE *f, ShellCmd_t *cmd, const char *s);
bool shell_cmd_pwd(FILE *f, ShellCmd_t *cmd, const char *s);
bool shell_cmd_cd(FILE *f, ShellCmd_t *cmd, const char *s);
