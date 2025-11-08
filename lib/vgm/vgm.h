#include <cstdint>
#include <cstdio>

class Apu2A03;

using namespace std;

class VgmPlayer {
public:
    enum class Status { IDLE, FINISHED, PLAYING, QUIT, ST_ERROR, NEXT, PREV };
    VgmPlayer() = default;
    Status play(const uint8_t* data, size_t size, Apu2A03& apu, FILE* console);

private:
    size_t dataOffset = 0;
};
