#pragma once

#include <stdio.h>
#include <memory>
#include <ff.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <audio_volume_control.h>

//------------------------------------------------------------------------------
class WavPlayer_t
{
public:
    enum State_t
    {
        IDLE,
        READING_HEADER,
        PLAYING,
        PAUSED
    };

    WavPlayer_t(size_t dac_buf_size=4096, UBaseType_t prio=31, FILE *con=stdout);
    ~WavPlayer_t();

    void task_play_wav_body();
    bool start(const char *fname);
    void stop();
    void pause();
    void unpause();
    State_t get_state();
    int get_duration_ms();
    int get_time_pos_ms();
    int get_time_left_ms();
    void print_stat();

private:
    static const size_t NUM_CHANNELS = 2;
    static const size_t MONO_SAMPLE_SIZE = sizeof(int16_t);
    static const size_t STEREO_SAMPLE_SIZE = NUM_CHANNELS * MONO_SAMPLE_SIZE;

    int read_header(const char *fname);

    State_t m_state;
    std::unique_ptr<uint16_t> signal_buff[2];
    FIL m_wav_file;
    TaskHandle_t m_audio_task_id;
    size_t m_data_size;
    size_t m_data_size_left;
    size_t m_dac_buf_size;
    size_t m_dac_buf_num_samples;
    UBaseType_t m_prio;
    FILE *m_console;
    size_t m_bytes_per_sec;
};
