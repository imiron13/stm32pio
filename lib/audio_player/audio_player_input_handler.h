#pragma once

#include <audio_player_controller.h>
#include <button.h>

struct AudioPlayerInputs_t
{
    Button_t *button_pause;
    Button_t *button_next;
};

class AudioPlayerInputHandler_t
{
    AudioPlayerController_t *m_controller;
    AudioPlayerInputs_t m_inp;
public:
    AudioPlayerInputHandler_t(AudioPlayerController_t *controller, const AudioPlayerInputs_t &inputs);

    void handle_inputs();
};
