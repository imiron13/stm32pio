#pragma once

#include <audio_player_controller.h>
#include <audio_player_input_handler.h>

class AudioPlayer_t
{
public:
    AudioPlayerController_t m_controller;
    AudioPlayerInputHandler_t m_input_handler;

    AudioPlayer_t(const AudioPlayerInputs_t &inputs);
};
