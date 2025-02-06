#pragma once

#include <shell.h>
#include <audio_player_controller.h>

class AudioPlayerShelUi_t
{
public:
    static bool shell_cmd_auplayer_pause(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_resume(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_play(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_stop(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_next(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_prev(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_shuffle(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_list(FILE *f, ShellCmd_t *cmd, const char *s);
    static bool shell_cmd_auplayer_set_pos(FILE *f, ShellCmd_t *cmd, const char *s);

private:
    AudioPlayerController_t s_controller;
};

