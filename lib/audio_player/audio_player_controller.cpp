#include <audio_player_controller.h>
#include <cstring>
#include <memory>

AudioPlayerController_t::AudioPlayerController_t()
    : m_songs_list(nullptr)
    , m_num_songs(0)
    , m_songs_list_capacity(0)
    , m_cur_song_idx(0)
    , m_state(IDLE)
{

}

AudioPlayerController_t::~AudioPlayerController_t()
{
    clearSongsList();
    delete m_songs_list;
}

void AudioPlayerController_t::printStat()
{
    if (m_task_play_wav)
    {
        m_task_play_wav->print_stat();
    }
}

void AudioPlayerController_t::clearSongsList()
{
    for (int i = 0; i < m_num_songs; i++)
    {
        delete m_songs_list[i];
    }
    m_num_songs = 0;
}

void AudioPlayerController_t::reserveSongsListCapacity(int capacity)
{
    if (m_songs_list_capacity <= capacity)
    {
        const char **new_songs_list = new const char*[capacity];
        for (int i = 0; i < m_songs_list_capacity; i++)
        {
            new_songs_list[i] = m_songs_list[i];
        }
        if (m_songs_list) delete m_songs_list;
        m_songs_list = new_songs_list;
        m_songs_list_capacity = capacity;
    }
}

bool AudioPlayerController_t::is_paused()
{
    return m_state == PAUSED;
}

void AudioPlayerController_t::nextSong()
{

}

bool AudioPlayerController_t::playSong(const char *wav_file_name)
{
    clearSongsList();
    reserveSongsListCapacity(1);
    addSong(wav_file_name);
    return play();
}

int AudioPlayerController_t::addSong(const char *wav_file_name)
{
    int len = strlen(wav_file_name);
    char *p = new char[len];
    if (p)
    {
        strcpy(p, wav_file_name);
        m_songs_list[m_num_songs] = p;
        int song_id = m_num_songs;
        m_num_songs++;
        return song_id;
    }
    else
    {
        printf("Memory allocation failure" ENDL);
        return INT32_MAX;
    }
}

void AudioPlayerController_t::shuffle()
{

}

void AudioPlayerController_t::pause()
{
    if (m_state == PLAYING)
    {
        m_task_play_wav->pause();
        printf("pause" ENDL);
        m_state = PAUSED;
    }
}

bool AudioPlayerController_t::play()
{
    if (m_state == PAUSED)
    {
        m_task_play_wav->unpause();
        m_state = PLAYING;
        return true;
    }
    else if (m_state == PLAYING)
    {
        stop();
    }

    if (m_state == IDLE)
    {
        printf("play" ENDL);
        
        m_task_play_wav = std::unique_ptr<WavPlayer_t>(new WavPlayer_t);
        bool ok = m_task_play_wav->start(getCurrentSongFileName());

        if (ok)
        {
            m_state = PLAYING;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

void AudioPlayerController_t::stop()
{
    if (m_task_play_wav)
    {
        m_task_play_wav->stop();
        m_task_play_wav.reset();
        m_state = IDLE;
    }
}

int AudioPlayerController_t::getTimestampMsec()
{
    return 0;
}

void AudioPlayerController_t::setTimestampMsec()
{

}

const char *AudioPlayerController_t::getCurrentSongFileName()
{
    return m_songs_list[m_cur_song_idx];
}

AudioPlayerController_t::State_t AudioPlayerController_t::getState()
{
    return m_state;
    /*if (m_task_play_wav)
    {
        return WavPlayer_t::IDLE;
    }
    else
    {
        return m_task_play_wav->get_state();
    }*/
}
