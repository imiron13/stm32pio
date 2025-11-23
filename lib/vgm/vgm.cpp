#include "vgm.h"
#include "intrinsics.h"
#include <nes_apu.h>
#include <cstdio>
#include <cstdint>
#include "hal_wrapper_stm32.h"

VgmPlayer::Status VgmPlayer::play(const uint8_t* data, size_t size, Apu2A03& apu, FILE* console)
{
   // Check VGM signature
    if (size < 0x40 || data[0] != 'V' || data[1] != 'g' || data[2] != 'm' || data[3] != ' ') {
        fprintf(console, "Invalid VGM header" ENDL);
        return Status::ST_ERROR;
    }

    // Data offset (relative to 0x34 + value)
    uint32_t dataOffsetField = *reinterpret_cast<const uint32_t*>(&data[0x34]);
    dataOffset = dataOffsetField ? (0x34 + dataOffsetField) : 0x40;

    if (dataOffset >= size) {
        fprintf(console, "Invalid data offset" ENDL);
        return Status::ST_ERROR;
    }
    
    size_t pos = dataOffset;
    const size_t end = size;
    const double samplesPerCpuCycle = (1789773.0 / 44100.0/2); // ≈0.0246 cycles/sample
    while (pos < end) {
        while (apu.isBufferFull() == false) {
            uint8_t cmd = data[pos++];

            switch (cmd) {
            case 0x66: // End of sound data
                fprintf(console, "End of VGM stream" ENDL);
                return Status::FINISHED;

            case 0xB4: { // NES APU write
                if (pos + 2 > end) return Status::ST_ERROR;
                uint8_t addr = data[pos++];
                uint8_t val = data[pos++];
                apu.cpuWrite(0x4000 + addr, val);
                break;
            }

            case 0x61: { // wait n samples
                if (pos + 2 > end) return Status::ST_ERROR;
                uint16_t n = data[pos] | (data[pos + 1] << 8);
                pos += 2;
                uint32_t cycles = static_cast<uint32_t>(n * samplesPerCpuCycle);
                apu.clock(cycles);
                break;
            }

            case 0x62: { // wait 735 samples (60 Hz)
                apu.clock(static_cast<uint32_t>(735 * samplesPerCpuCycle));
                break;
            }

            case 0x63: { // wait 882 samples (50 Hz)
                apu.clock(static_cast<uint32_t>(882 * samplesPerCpuCycle));
                break;
            }

            case 0x67: // Data block
                if (pos + 3 > end) return Status::ST_ERROR;
                {
                    uint8_t extra_cmd = data[pos++];
                    uint8_t type = data[pos++];
                    uint32_t size = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24);
                    pos += 4;
                    if (pos + size > end) return Status::ST_ERROR;
                    // Skip data block for now
                    (void)extra_cmd;
                    (void)type;
                    pos += size;
                }
                break;
            default:
                if (cmd >= 0x70 && cmd <= 0x7F) {
                    // wait (n+1) samples
                    uint8_t n = (cmd & 0x0F) + 1;
                    uint32_t cycles = static_cast<uint32_t>(n * samplesPerCpuCycle);
                    apu.clock(cycles);
                } else {
                    // Unhandled command, skip or stop
                    fprintf(console, "Unknown VGM command: 0x%X" ENDL, (int)cmd);
                    return Status::ST_ERROR;
                }
                break;
            }
        }

        int ch = fgetc(console);
        if (ch == 27 || ch == 'q' || ch == 'Q') { // ESC key
            return Status::QUIT;
        } else if (ch == 'n' || ch == 'N')  {
            return Status::NEXT;
        } else if (ch == 'p' || ch == 'P') {
            return Status::PREV;
        } else if (ch != -1) {
            printf("Key pressed: %d\n", ch);
        }

    }
    return Status::FINISHED;
}

