#pragma once

#include <memory>
#include <wav_player.h>

class AudioPlayerController_t
{
public:
    enum State_t
    {
        STOPPED,
        PAUSED,
        PLAYING
    };
    AudioPlayerController_t();
    ~AudioPlayerController_t();
    void playSong(const char *wav_file_name);
    void playFolder();
    void pause();
    void stop();
    void play();
    void shuffle();
    void nextSong();
    void prevSong();
    void reserveSongsListCapacity(int capacity);
    void clearSongsList();
    bool is_paused();

    void setVolume();
    void getVolume();
    void printStat();

    const char *getCurrentSongName();
    const char *getCurrentSongFileName();
    int getTimestampMsec();
    void setTimestampMsec();

private:
    const char **m_songs_list;
    int m_num_songs;
    int m_songs_list_capacity;
    int m_cur_song_idx;
    int addSong(const char *wav_file_name);
    State_t m_state;
    std::unique_ptr<TaskPlayWav_t> m_task_play_wav;
};
