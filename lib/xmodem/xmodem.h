#pragma once

#include <cstdio>  // For FILE, fread, fwrite, fflush
#include <cstdint>
#include <cstring> // For memcpy
#include "main.h"  // For HAL_GetTick()

class XModem {
public:
    // Protocol Constants
    static constexpr uint8_t SOH = 0x01; // 128-byte block
    static constexpr uint8_t STX = 0x02; // 1024-byte block
    static constexpr uint8_t EOT = 0x04; // End of Transmission
    static constexpr uint8_t ACK = 0x06; // Acknowledge
    static constexpr uint8_t NAK = 0x15; // Not Acknowledge
    static constexpr uint8_t CAN = 0x18; // Cancel
    static constexpr uint8_t C   = 0x43; // 'C' for CRC handshake

    static inline uint8_t buffer[1030]; 

    // Constructor accepts a standard FILE* stream
    explicit XModem(FILE* stream) : stream_(stream) {}

    /**
     * @brief Starts the receiver. 
     * @param onData Callback function to handle incoming data chunks.
     * @return true if transfer completed successfully.
     */
    template <typename Callback>
    bool Receive(Callback onData) {
        uint8_t seq = 1;      
        uint32_t errors = 0;
        
        // 1. Handshake: Send 'C' to request CRC mode
        uint32_t start = HAL_GetTick();
        while (HAL_GetTick() - start < 3000) {
            writeByte(C);
            if (waitForData(100)) break; 
        }

        while (true) {
            // 2. Read first byte (Header)
            uint8_t header = 0;
            if (!readByte(header, 1000)) {
                if (++errors > 10) return false;
                writeByte(NAK);
                continue;
            }

            // 3. Handle Header
            if (header == EOT) {
                writeByte(ACK);
                return true; // Transfer complete
            } else if (header == CAN) {
                return false; // Host cancelled
            } else if (header != SOH && header != STX) {
                // Purge garbage until stream is quiet
                while(readByte(header, 10)); 
                writeByte(NAK);
                continue;
            }

            // 4. Determine Block Size
            size_t dataSize = (header == STX) ? 1024 : 128;

            // 5. Read the rest: Seq (1) + ~Seq (1) + Data (dataSize) + CRC (2)
            size_t remaining = 2 + dataSize + 2;
            if (!readBuffer(&buffer[1], remaining, 1000)) {
                writeByte(NAK);
                continue;
            }

            // 6. Validate Sequence Number (Packet: [0]=Seq, [1]=~Seq)
            uint8_t rcvSeq = buffer[1];
            uint8_t rcvSeqInv = buffer[2];

            if (rcvSeq != seq) {
                if (rcvSeq == seq - 1) {
                    writeByte(ACK); // Retransmission, ACK and ignore
                    continue;
                } else {
                    writeByte(CAN); // Fatal error
                    return false;
                }
            }
            if (rcvSeq != (uint8_t)~rcvSeqInv) {
                writeByte(NAK); 
                continue;
            }

            // 7. Validate CRC (Data starts at buffer[3])
            uint16_t rcvCRC = (buffer[remaining - 1] << 8) | buffer[remaining];
            uint16_t calcCRC = 0;
            
            for (size_t i = 0; i < dataSize; i++) {
                calcCRC = updateCRC(calcCRC, buffer[3 + i]);
            }

            if (calcCRC != rcvCRC) {
                writeByte(NAK); 
                continue;
            }

            // 8. Process Data
            if (!onData(&buffer[3], dataSize)) {
                writeByte(CAN); 
                return false;
            }

            // 9. Success
            writeByte(ACK);
            seq++;
        }
    }

    void RequestFile(const char* filename) {
        if (!stream_) return;
        // \x02 = STX, \x03 = ETX
        fprintf(stream_, "\x02REQ:%s\x03", filename);
        fflush(stream_);
    }

private:
    FILE* stream_;

    uint16_t updateCRC(uint16_t crc, uint8_t data) {
        crc = crc ^ ((uint16_t)data << 8);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
        return crc;
    }

    void writeByte(uint8_t byte) {
        if (!stream_) return;
        fputc(byte, stream_);
        fflush(stream_); // Ensure it hits the USB hardware immediately
    }

    // "Peeks" (actually just waits) to see if data becomes available
    bool waitForData(uint32_t timeoutMs) {
        uint32_t start = HAL_GetTick();
        while (HAL_GetTick() - start < timeoutMs) {
            // We can't easily peek standard FILE without reading, 
            // so we rely on readByte for actual retrieval.
            // This loop serves purely as a delay/timeout mechanism.
            // A smarter implementation might rely on ferror/feof checks if needed.
            return false; 
        }
        return false;
    }

    bool readByte(uint8_t& out, uint32_t timeoutMs) {
        if (!stream_) return false;
        
        uint32_t start = HAL_GetTick();
        while (HAL_GetTick() - start < timeoutMs) {
            // Try to read 1 byte
            unsigned char c;
            size_t n = fread(&c, 1, 1, stream_);
            
            if (n == 1) {
                out = (uint8_t)c;
                return true;
            }
            
            // If fread returns 0, it means no data is currently available 
            // in the USB buffer (non-blocking behavior from your glue code).
            // We clear any EOF/Error flags to ensure we can try reading again.
            clearerr(stream_); 
        }
        return false;
    }

    bool readBuffer(uint8_t* buf, size_t len, uint32_t timeoutMs) {
        size_t count = 0;
        uint32_t start = HAL_GetTick();
        
        while (count < len) {
            if (HAL_GetTick() - start > timeoutMs) return false;
            
            unsigned char c;
            size_t n = fread(&c, 1, 1, stream_);
            
            if (n == 1) {
                buf[count++] = (uint8_t)c;
                // Optional: Reset timeout on success?
                // start = HAL_GetTick(); 
            } else {
                clearerr(stream_);
            }
        }
        return true;
    }
};
