#include <audio_player.h>

AudioPlayer_t::AudioPlayer_t(const AudioPlayerInputs_t &inputs)
    : m_controller()
    , m_input_handler(&m_controller, inputs)
{

}
