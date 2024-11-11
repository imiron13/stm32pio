#pragma once

#include <stdio.h>
#include <memory>
#include <ff.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <audio_volume_control.h>

//------------------------------------------------------------------------------
class TaskPlayWav_t
{
public:
    struct Args_t
    {
        FIL *file;
        FILE *console;
        int data_size;
        QueueHandle_t req_q;
        QueueHandle_t rsp_q;
    };

    enum RequestId_t
    {
        TASK_PLAY_WAV_REQ_ID_START,
        TASK_PLAY_WAV_REQ_ID_STOP
    };

    enum ResponseId_t
    {
        TASK_PLAY_WAV_RSP_ID_TERMINATE_WITH_ERROR,
        TASK_PLAY_WAV_RSP_ID_TERMINATE_NORMALLY
    };

    struct Request_t
    {
        RequestId_t req_id;
        int arg;
    };

    struct Response_t
    {
        ResponseId_t rsp_id;
        int arg;
    };

    TaskPlayWav_t(size_t dac_buf_size=4096, UBaseType_t prio=31, FILE *con=stdout);
    ~TaskPlayWav_t();

    void task_play_wav_body();
    bool start(const char *fname);
    void stop();
    void pause();
    void unpause();
    int get_duration_ms();
    int get_time_pos_ms();
    int get_time_left_ms();
    void print_stat();

private:
    static const size_t NUM_CHANNELS = 2;
    static const size_t MONO_SAMPLE_SIZE = sizeof(int16_t);
    static const size_t STEREO_SAMPLE_SIZE = NUM_CHANNELS * MONO_SAMPLE_SIZE;

    int read_header(const char *fname);
    void send_response(QueueHandle_t rsp_q, ResponseId_t rsp_id, int arg);

    FIL m_wav_file;
    Args_t m_args;
    QueueHandle_t m_req_h;
    QueueHandle_t m_rsp_q;
    TaskHandle_t m_audio_task_id;
    size_t m_data_size;
    size_t m_data_size_left;
    size_t m_dac_buf_size;
    size_t m_dac_buf_num_samples;
    UBaseType_t m_prio;
    FILE *m_console;
    size_t m_bytes_per_sec;
};
