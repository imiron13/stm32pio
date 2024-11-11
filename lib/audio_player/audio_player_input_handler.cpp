#include <audio_player_input_handler.h>

AudioPlayerInputHandler_t::AudioPlayerInputHandler_t(AudioPlayerController_t *controller, const AudioPlayerInputs_t &inputs)
    : m_controller(controller)
    , m_inp(inputs)
{
}

void AudioPlayerInputHandler_t::handle_inputs()
{
    if (m_inp.button_pause->is_pressed_event())
    {
        if (m_controller->is_paused())
        {
            m_controller->play();
        }
        else
        {
            m_controller->pause();
        }
        m_controller->printStat();
    }

    if (m_inp.button_next->is_pressed_event())
    {
        m_controller->nextSong();
    }
}
