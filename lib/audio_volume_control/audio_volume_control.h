#pragma once

#include <intrinsics.h>
#include <cstdio>
#include <shell.h>

class AudioVolumeControl_t
{
public:
    AudioVolumeControl_t();

    void increase();
    void decrease();
    void mute();
    void max();
    void min();
    void set(size_t volume_level);
    size_t get();
    size_t get_divider();

private:
    static const int VOLUME_SHIFT_MAX = 9;
    static const int VOLUME_SHIFT_MUTE = 16;
    static const int DEFAULT_SHIFT_VALUE = 1;
    static const int volume_table[];
    size_t m_volume_shift;
};

extern AudioVolumeControl_t g_volume;

bool shell_cmd_set_volume(FILE *f, ShellCmd_t *cmd, const char *s);
