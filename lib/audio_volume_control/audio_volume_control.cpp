#include <audio_volume_control.h>
#include <intrinsics.h>
#include <cmath>
#include <cstring>
#include <shell.h>

AudioVolumeControl_t g_volume;
const int AudioVolumeControl_t::volume_table[] = { 100, 80, 60, 40, 20, 10, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0 };

AudioVolumeControl_t::AudioVolumeControl_t()
    : m_volume_shift(DEFAULT_SHIFT_VALUE)
{
}

void AudioVolumeControl_t::increase()
{
    if (m_volume_shift >= VOLUME_SHIFT_MUTE) m_volume_shift = VOLUME_SHIFT_MAX;
    else if (m_volume_shift > 0) m_volume_shift--;
}

void AudioVolumeControl_t::decrease()
{
    if (m_volume_shift < VOLUME_SHIFT_MAX) m_volume_shift++;
    else m_volume_shift = VOLUME_SHIFT_MUTE;
}

void AudioVolumeControl_t::mute()
{
    m_volume_shift = VOLUME_SHIFT_MUTE;
}

void AudioVolumeControl_t::max()
{
    m_volume_shift = 0;
}

void AudioVolumeControl_t::min()
{
    m_volume_shift = VOLUME_SHIFT_MAX;
}

void AudioVolumeControl_t::set(size_t volume_level)
{
    int volume_div = 0;
    int min_dif = INT32_MAX;
    for (int i = VOLUME_SHIFT_MUTE; i >= 0; i--)
    {
        if (abs(volume_table[i] - volume_level) < min_dif)
        {
            min_dif = abs(volume_table[i] - volume_level);
            volume_div = i;
        }
    }
    m_volume_shift = volume_div;
}

size_t AudioVolumeControl_t::get()
{
    return volume_table[m_volume_shift];
}

size_t AudioVolumeControl_t::get_divider()
{
    return m_volume_shift;
}

bool shell_cmd_set_volume(FILE *f, ShellCmd_t *cmd, const char *s)
{
    if (strcmp(cmd->get_str_arg(s, 1), "+") == 0)
    {
        g_volume.increase();
    }
    else if (strcmp(cmd->get_str_arg(s, 1), "-") == 0)
    {
        g_volume.decrease();
    }
    else if (strcmp(cmd->get_str_arg(s, 1), "max") == 0)
    {
        g_volume.max();
    }
    else if (strcmp(cmd->get_str_arg(s, 1), "min") == 0)
    {
        g_volume.min();
    }
    else if (strcmp(cmd->get_str_arg(s, 1), "mute") == 0)
    {
        g_volume.mute();
    }    
    else
    {
        int volume  = cmd->get_int_arg(s, 1);
        g_volume.set(volume);
    }
    fprintf(f, "Volume now is %d%% (volume divider is %d)" ENDL, g_volume.get(), g_volume.get_divider());
    return true;
}
