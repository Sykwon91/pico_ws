#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/spi.h"

class CC1101
{
public:
    struct Config
    {
        spi_inst_t* spi = spi0;

        uint sckPin  = 18;
        uint mosiPin = 19;
        uint misoPin = 16;
        uint csPin   = 17;
        uint gdo0Pin = 20;
        uint gdo2Pin = 21;

        uint32_t spiClockHz = 4'000'000;
    };

    explicit CC1101(const Config& config);

    bool init();
    bool reset();

    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);

    void writeBurst(uint8_t address,
                    const uint8_t* data,
                    std::size_t length);

    void readBurst(uint8_t address,
                   uint8_t* data,
                   std::size_t length);

    uint8_t strobe(uint8_t command);

    uint8_t readPartNumber();
    uint8_t readVersion();

    bool configurePacketMode433();
    bool configurePassiveOok433();

    bool setFrequencyHz(uint32_t frequencyHz);

    void enterIdle();
    void enterReceive();
    void enterTransmit();

    void flushRx();
    void flushTx();

    int16_t readRssiDbm();
    uint8_t readMarcState();

    bool sendPacket(const uint8_t* data,
                    std::size_t length,
                    uint32_t timeoutMs = 100);

    int receivePacket(uint8_t* data,
                      std::size_t capacity,
                      int16_t& rssiDbm,
                      uint8_t& lqi,
                      bool& crcOk);

    uint gdo0Pin() const
    {
        return config_.gdo0Pin;
    }

    uint gdo2Pin() const
    {
        return config_.gdo2Pin;
    }

private:
    Config config_;

    void select();
    void deselect();
    bool waitForMisoLow(uint32_t timeoutUs = 1000);
    uint8_t transfer(uint8_t value);
};