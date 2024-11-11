#include <stdio.h>
#include <string.h>
#include <memory>

#include <intrinsics.h>
#include <wav_player.h>
#include <ff.h>
#include <cmsis_os2.h>
#include <FreeRTOS.h>
#include "task.h"
#include "queue.h"

extern I2S_HandleTypeDef hi2s2;

extern "C" void task_play_wav(void *arg)
{
    TaskPlayWav_t *play_wav = (TaskPlayWav_t*)arg;
    play_wav->task_play_wav_body();
}

TaskPlayWav_t::TaskPlayWav_t(size_t dac_buf_size, UBaseType_t prio, FILE *con)
    : m_audio_task_id(NULL)
    , m_dac_buf_size(dac_buf_size)
    , m_dac_buf_num_samples(dac_buf_size / sizeof(int16_t))
    , m_prio(prio)
    , m_console(con)
{
    m_req_h = xQueueCreate(16, sizeof(Request_t));
    m_rsp_q = xQueueCreate(16, sizeof(Response_t));
}

TaskPlayWav_t::~TaskPlayWav_t()
{
    f_close(&m_wav_file);
    vQueueDelete(m_req_h);
    vQueueDelete(m_rsp_q);
}

bool TaskPlayWav_t::start(const char *fname)
{
    int res = read_header(fname);
    if (res >= 0)
    {
        m_data_size = res;

        fprintf(m_console, "Creating a task to play wav file" ENDL);

        BaseType_t res = xTaskCreate(task_play_wav, "task_play_wav", 256, this, m_prio, &m_audio_task_id);
        return (res == pdPASS);
    }
    else
    {
        fprintf(m_console, "Failed to parse WAV header" ENDL);
        return false;
    }
}

void TaskPlayWav_t::stop()
{
    if (m_audio_task_id)
    {
        vTaskDelete(m_audio_task_id);
        signal_buff[0].reset();
        signal_buff[1].reset();
        m_audio_task_id = NULL;
    }
}

void TaskPlayWav_t::pause()
{
    vTaskSuspend(m_audio_task_id);
}

void TaskPlayWav_t::unpause()
{
    vTaskResume(m_audio_task_id);
}

void TaskPlayWav_t::send_response(QueueHandle_t rsp_q, ResponseId_t rsp_id, int arg)
{
    Response_t rsp;
    rsp.rsp_id = rsp_id; 
    rsp.arg = arg;
    xQueueSendToBack(rsp_q, &rsp, portMAX_DELAY);
}

int TaskPlayWav_t::read_header(const char *fname)
{
    fprintf(m_console, "Opening %s..." ENDL, fname);
    FRESULT res = f_open(&m_wav_file, fname, FA_READ);
    if(res != FR_OK) {
        fprintf(m_console, "f_open() failed, res = %d" ENDL, res);
        return -1;
    }

    fprintf(m_console, "File opened, reading..." ENDL);

    unsigned int bytesRead;
    uint8_t header[44+34];
    res = f_read(&m_wav_file, header, sizeof(header), &bytesRead);
    if(res != FR_OK) {
        fprintf(m_console, "f_read() failed, res = %d" ENDL, res);
        f_close(&m_wav_file);
        return -2;
    }

    if(memcmp((const char*)header, "RIFF", 4) != 0) {
        fprintf(m_console, "Wrong WAV signature at offset 0: "
                    "0x%02X 0x%02X 0x%02X 0x%02X" ENDL,
                    header[0], header[1], header[2], header[3]);
        f_close(&m_wav_file);
        return -3;
    }

    if(memcmp((const char*)header + 8, "WAVEfmt ", 8) != 0) {
        fprintf(m_console, "Wrong WAV signature at offset 8!" ENDL);
        f_close(&m_wav_file);
        return -4;
    }
    uint32_t data_ofs = 36;
    if(memcmp((const char*)header + data_ofs, "data", 4) != 0) {
        if (memcmp((const char*)header + data_ofs, "LIST", 4) == 0)
        {
            uint32_t listChunkSize = header[data_ofs + 4] | (header[data_ofs + 5] << 8) |
                        (header[data_ofs + 6] << 16) | (header[data_ofs + 7] << 24);
            data_ofs += 8 + listChunkSize;
        }
        else
        {
            fprintf(m_console, "Cannot find 'data' subchunk!" ENDL);
            f_close(&m_wav_file);
            return -5;
        }
    }

    uint32_t fileSize = 8 + (header[4] | (header[5] << 8) |
                        (header[6] << 16) | (header[7] << 24));
    uint32_t headerSizeLeft = header[16] | (header[17] << 8) |
                              (header[18] << 16) | (header[19] << 24);
    uint16_t compression = header[20] | (header[21] << 8);
    uint16_t channelsNum = header[22] | (header[23] << 8);
    uint32_t sampleRate = header[24] | (header[25] << 8) |
                          (header[26] << 16) | (header[27] << 24);
    uint32_t bytesPerSecond = header[28] | (header[29] << 8) |
                              (header[30] << 16) | (header[31] << 24);
    uint16_t bytesPerSample = header[32] | (header[33] << 8);
    uint16_t bitsPerSamplePerChannel = header[34] | (header[35] << 8);
    uint32_t dataSize = header[data_ofs + 4] | (header[data_ofs + 5] << 8) |
                        (header[data_ofs + 6] << 16) | (header[data_ofs + 7] << 24);

    fprintf(m_console, 
        "--- WAV header ---" ENDL
        "File size: %lu" ENDL
        "Header size left: %lu" ENDL
        "Compression (1 = no compression): %d" ENDL
        "Channels num: %d" ENDL
        "Sample rate: %ld" ENDL
        "Bytes per second: %ld" ENDL
        "Bytes per sample: %d" ENDL
        "Bits per sample per channel: %d" ENDL
        "Data size: %ld" ENDL
        "------------------" ENDL,
        fileSize, headerSizeLeft, compression, channelsNum,
        sampleRate, bytesPerSecond, bytesPerSample,
        bitsPerSamplePerChannel, dataSize);

    if(headerSizeLeft != 16) {
        fprintf(m_console, "Wrong `headerSizeLeft` value, 16 expected" ENDL);
        f_close(&m_wav_file);
        return -6;
    }

    if(compression != 1) {
        fprintf(m_console, "Wrong `compression` value, 1 expected" ENDL);
        f_close(&m_wav_file);
        return -7;
    }

    if(channelsNum != 2) {
        fprintf(m_console, "Wrong `channelsNum` value, 2 expected" ENDL);
        f_close(&m_wav_file);
        return -8;
    }

    if((sampleRate != 44100) || (bytesPerSample != 4) ||
       (bitsPerSamplePerChannel != 16) || (bytesPerSecond != 44100*2*2)
       || (dataSize < m_dac_buf_size + m_dac_buf_size)) {
        fprintf(m_console, "Wrong file format, 16 bit file with sample "
                    "rate 44100 expected" ENDL);
        f_close(&m_wav_file);
        return -9;
    }
    m_bytes_per_sec = bytesPerSecond;
    return dataSize;
}

void TaskPlayWav_t::task_play_wav_body()
{
    unsigned int bytesRead;

    signal_buff[0].reset(new uint16_t[m_dac_buf_size / 2]);
    signal_buff[1].reset(new uint16_t[m_dac_buf_size / 2]);

    FRESULT res = f_read(&m_wav_file, (uint8_t*)signal_buff[0].get(), m_dac_buf_size,
                 &bytesRead);
    if(res != FR_OK) {
        fprintf(m_console, "f_read() failed, res = %d" ENDL, res);
        f_close(&m_wav_file);
        send_response(m_rsp_q, TASK_PLAY_WAV_RSP_ID_TERMINATE_WITH_ERROR, -10);
    }
    m_data_size_left = m_data_size;

    vTaskDelay(10);
    bool end = false;
    uint32_t play_buf_idx = 0;
    size_t delay_msec = 1000 * (uint64_t)m_dac_buf_size / m_bytes_per_sec;
    if (delay_msec >= 3) delay_msec -= 2;

    while (m_data_size_left >= m_dac_buf_size && !end) {
        vTaskSuspendAll();
        res = f_read(&m_wav_file, (uint8_t*)signal_buff[!play_buf_idx].get(),
                     m_dac_buf_size, &bytesRead);
        xTaskResumeAll();
        int16_t *p = (int16_t*)signal_buff[!play_buf_idx].get();

        int volume_divider = g_volume.get_divider();
        for (size_t i = 0; i < m_dac_buf_num_samples; i++)
        {
            p[i] >>= volume_divider;
        }
        if(res != FR_OK) {
            fprintf(m_console, "f_read() failed, res = %d" ENDL, res);
            f_close(&m_wav_file);
            send_response(m_rsp_q, TASK_PLAY_WAV_RSP_ID_TERMINATE_WITH_ERROR, -10);
        }

        while (HAL_I2S_GetState(&hi2s2) == HAL_I2S_STATE_BUSY_TX) { };
        HAL_StatusTypeDef hal_res = HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t*)signal_buff[!play_buf_idx].get(),
                                  m_dac_buf_num_samples);
        vTaskDelay(delay_msec);

        play_buf_idx = !play_buf_idx;  
        m_data_size_left -= m_dac_buf_size;
    }

    res = f_close(&m_wav_file);
    if(res != FR_OK) {
        fprintf(m_console, "f_close() failed, res = %d" ENDL, res);
        send_response(m_rsp_q, TASK_PLAY_WAV_RSP_ID_TERMINATE_WITH_ERROR, -14);

    }
    send_response(m_rsp_q, TASK_PLAY_WAV_RSP_ID_TERMINATE_NORMALLY, 0);
    vTaskDelay(portMAX_DELAY);
    for (;;);
}

int TaskPlayWav_t::get_duration_ms()
{
    return 1000 * (uint64_t)m_data_size / m_bytes_per_sec;
}

int TaskPlayWav_t::get_time_pos_ms()
{
    size_t total_duration = get_duration_ms();
    size_t left = get_time_left_ms();
    return total_duration - left;
}

int TaskPlayWav_t::get_time_left_ms()
{
    vTaskSuspendAll();
    size_t data_size_left = m_data_size_left;
    xTaskResumeAll();
    return 1000 * (uint64_t)data_size_left / m_bytes_per_sec;
}

void TaskPlayWav_t::print_stat()
{
    size_t total = get_duration_ms() / 1000;
    size_t cur = get_time_pos_ms() / 1000;
    size_t left = total - cur;

    fprintf(m_console, "total_time: %d s, cur: %d s, left s: %d" ENDL, total, cur, left);
}
