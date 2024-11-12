#pragma once

#include <memory>
#include <wav_player.h>

class AudioPlayerController_t
{
public:
    enum State_t
    {
        IDLE,
        READING_HEADER,
        PLAYING,
        PAUSED,
        FINISHED
    };
    AudioPlayerController_t();
    ~AudioPlayerController_t();
    bool playSong(const char *wav_file_name);
    void playFolder();
    void pause();
    void stop();
    bool play();
    void shuffle();
    void nextSong();
    void prevSong();
    void reserveSongsListCapacity(int capacity);
    void clearSongsList();
    bool is_paused();
    State_t getState();

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
    std::unique_ptr<WavPlayer_t> m_task_play_wav;
};
