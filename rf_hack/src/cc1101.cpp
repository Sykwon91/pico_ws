#include "cc1101.hpp"

#include <algorithm>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

// CC1101 SPI access flags
namespace
{
constexpr uint8_t READ_SINGLE = 0x80;
constexpr uint8_t READ_BURST  = 0xC0;
constexpr uint8_t WRITE_BURST = 0x40;

// Configuration registers
constexpr uint8_t IOCFG2   = 0x00;
constexpr uint8_t IOCFG0   = 0x02;
constexpr uint8_t FIFOTHR  = 0x03;
constexpr uint8_t SYNC1    = 0x04;
constexpr uint8_t SYNC0    = 0x05;
constexpr uint8_t PKTLEN   = 0x06;
constexpr uint8_t PKTCTRL1 = 0x07;
constexpr uint8_t PKTCTRL0 = 0x08;
constexpr uint8_t ADDR     = 0x09;
constexpr uint8_t CHANNR   = 0x0A;

constexpr uint8_t FSCTRL1  = 0x0B;
constexpr uint8_t FSCTRL0  = 0x0C;
constexpr uint8_t FREQ2    = 0x0D;
constexpr uint8_t FREQ1    = 0x0E;
constexpr uint8_t FREQ0    = 0x0F;

constexpr uint8_t MDMCFG4  = 0x10;
constexpr uint8_t MDMCFG3  = 0x11;
constexpr uint8_t MDMCFG2  = 0x12;
constexpr uint8_t MDMCFG1  = 0x13;
constexpr uint8_t MDMCFG0  = 0x14;
constexpr uint8_t DEVIATN  = 0x15;

constexpr uint8_t MCSM2    = 0x16;
constexpr uint8_t MCSM1    = 0x17;
constexpr uint8_t MCSM0    = 0x18;
constexpr uint8_t FOCCFG   = 0x19;
constexpr uint8_t BSCFG    = 0x1A;

constexpr uint8_t AGCCTRL2 = 0x1B;
constexpr uint8_t AGCCTRL1 = 0x1C;
constexpr uint8_t AGCCTRL0 = 0x1D;

constexpr uint8_t FREND1   = 0x21;
constexpr uint8_t FREND0   = 0x22;
constexpr uint8_t FSCAL3   = 0x23;
constexpr uint8_t FSCAL2   = 0x24;
constexpr uint8_t FSCAL1   = 0x25;
constexpr uint8_t FSCAL0   = 0x26;

constexpr uint8_t TEST2    = 0x2C;
constexpr uint8_t TEST1    = 0x2D;
constexpr uint8_t TEST0    = 0x2E;

// Command strobes
constexpr uint8_t SRES  = 0x30;
constexpr uint8_t SFSTXON = 0x31;
constexpr uint8_t SXOFF = 0x32;
constexpr uint8_t SCAL  = 0x33;
constexpr uint8_t SRX   = 0x34;
constexpr uint8_t STX   = 0x35;
constexpr uint8_t SIDLE = 0x36;
constexpr uint8_t SWOR  = 0x38;
constexpr uint8_t SPWD  = 0x39;
constexpr uint8_t SFRX  = 0x3A;
constexpr uint8_t SFTX  = 0x3B;
constexpr uint8_t SWORRST = 0x3C;
constexpr uint8_t SNOP  = 0x3D;

// Status registers
constexpr uint8_t PARTNUM  = 0x30;
constexpr uint8_t VERSION  = 0x31;
constexpr uint8_t RSSI     = 0x34;
constexpr uint8_t MARCSTATE = 0x35;
constexpr uint8_t RXBYTES  = 0x3B;
constexpr uint8_t TXBYTES  = 0x3A;

// FIFO
constexpr uint8_t FIFO = 0x3F;

// Typical CC1101 crystal frequency
constexpr uint32_t FXOSC_HZ = 26'000'000;
}

CC1101::CC1101(const Config& config)
    : config_(config)
{
}

bool CC1101::init()
{
    spi_init(config_.spi, config_.spiClockHz);

    gpio_set_function(config_.sckPin, GPIO_FUNC_SPI);
    gpio_set_function(config_.mosiPin, GPIO_FUNC_SPI);
    gpio_set_function(config_.misoPin, GPIO_FUNC_SPI);

    gpio_init(config_.csPin);
    gpio_set_dir(config_.csPin, GPIO_OUT);
    gpio_put(config_.csPin, 1);

    gpio_init(config_.gdo0Pin);
    gpio_set_dir(config_.gdo0Pin, GPIO_IN);

    gpio_init(config_.gdo2Pin);
    gpio_set_dir(config_.gdo2Pin, GPIO_IN);

    sleep_ms(5);

    if (!reset())
    {
        return false;
    }

    const uint8_t version = readVersion();

    // 불량 배선 시 흔히 0x00 또는 0xFF가 반환됨
    return version != 0x00 && version != 0xFF;
}

void CC1101::select()
{
    gpio_put(config_.csPin, 0);
}

void CC1101::deselect()
{
    gpio_put(config_.csPin, 1);
}

bool CC1101::waitForMisoLow(uint32_t timeoutUs)
{
    const uint64_t start = time_us_64();

    while (gpio_get(config_.misoPin))
    {
        if ((time_us_64() - start) >= timeoutUs)
        {
            return false;
        }
    }

    return true;
}

uint8_t CC1101::transfer(uint8_t value)
{
    uint8_t received = 0;
    spi_write_read_blocking(config_.spi, &value, &received, 1);
    return received;
}

bool CC1101::reset()
{
    deselect();
    sleep_us(5);

    select();
    sleep_us(10);

    deselect();
    sleep_us(40);

    select();

    if (!waitForMisoLow())
    {
        deselect();
        return false;
    }

    transfer(SRES);

    if (!waitForMisoLow())
    {
        deselect();
        return false;
    }

    deselect();
    sleep_ms(1);

    return true;
}

uint8_t CC1101::readRegister(uint8_t address)
{
    select();

    if (!waitForMisoLow())
    {
        deselect();
        return 0xFF;
    }

    // Status registers (0x30..0x3D) require the burst bit even for a
    // single-byte read. Configuration registers use the normal read bit.
    const uint8_t readCommand = address >= PARTNUM
        ? static_cast<uint8_t>(address | READ_BURST)
        : static_cast<uint8_t>(address | READ_SINGLE);
    transfer(readCommand);
    const uint8_t value = transfer(0x00);

    deselect();
    return value;
}

void CC1101::writeRegister(uint8_t address, uint8_t value)
{
    select();

    if (!waitForMisoLow())
    {
        deselect();
        return;
    }

    transfer(address);
    transfer(value);

    deselect();
}

void CC1101::writeBurst(uint8_t address,
                        const uint8_t* data,
                        std::size_t length)
{
    if (data == nullptr || length == 0)
    {
        return;
    }

    select();

    if (!waitForMisoLow())
    {
        deselect();
        return;
    }

    transfer(address | WRITE_BURST);
    spi_write_blocking(config_.spi, data, length);

    deselect();
}

void CC1101::readBurst(uint8_t address,
                       uint8_t* data,
                       std::size_t length)
{
    if (data == nullptr || length == 0)
    {
        return;
    }

    select();

    if (!waitForMisoLow())
    {
        deselect();
        return;
    }

    transfer(address | READ_BURST);
    spi_read_blocking(config_.spi, 0x00, data, length);

    deselect();
}

uint8_t CC1101::strobe(uint8_t command)
{
    select();

    if (!waitForMisoLow())
    {
        deselect();
        return 0xFF;
    }

    const uint8_t status = transfer(command);

    deselect();
    return status;
}

uint8_t CC1101::readPartNumber()
{
    return readRegister(PARTNUM);
}

uint8_t CC1101::readVersion()
{
    return readRegister(VERSION);
}

void CC1101::enterIdle()
{
    strobe(SIDLE);
}

void CC1101::enterReceive()
{
    strobe(SRX);
}

void CC1101::enterTransmit()
{
    strobe(STX);
}

void CC1101::flushRx()
{
    enterIdle();
    strobe(SFRX);
}

void CC1101::flushTx()
{
    enterIdle();
    strobe(SFTX);
}

uint8_t CC1101::readMarcState()
{
    return readRegister(MARCSTATE) & 0x1F;
}

int16_t CC1101::readRssiDbm()
{
    const int8_t raw = static_cast<int8_t>(readRegister(RSSI));
    return static_cast<int16_t>(raw / 2 - 74);
}

bool CC1101::setFrequencyHz(uint32_t frequencyHz)
{
    if (frequencyHz < 300'000'000 || frequencyHz > 928'000'000)
    {
        return false;
    }

    const uint32_t word = static_cast<uint32_t>(
        (static_cast<uint64_t>(frequencyHz) << 16) / FXOSC_HZ);

    writeRegister(FREQ2, static_cast<uint8_t>(word >> 16));
    writeRegister(FREQ1, static_cast<uint8_t>(word >> 8));
    writeRegister(FREQ0, static_cast<uint8_t>(word));
    return true;
}

bool CC1101::configurePacketMode433()
{
    enterIdle();

    writeRegister(IOCFG2, 0x06);   // sync sent/received
    writeRegister(IOCFG0, 0x06);
    writeRegister(FIFOTHR, 0x47);
    writeRegister(SYNC1, 0xD3);
    writeRegister(SYNC0, 0x91);
    writeRegister(PKTLEN, 61);
    writeRegister(PKTCTRL1, 0x04); // append RSSI/LQI status bytes
    writeRegister(PKTCTRL0, 0x05); // variable length, CRC enabled
    writeRegister(ADDR, 0x00);
    writeRegister(CHANNR, 0x00);
    writeRegister(FSCTRL1, 0x06);
    writeRegister(FSCTRL0, 0x00);
    setFrequencyHz(433'920'000);
    writeRegister(MDMCFG4, 0xCA);  // ~38.4 kbaud, ~101 kHz filter BW
    writeRegister(MDMCFG3, 0x83);
    writeRegister(MDMCFG2, 0x13);  // GFSK, 30/32 sync bits
    writeRegister(MDMCFG1, 0x22);  // 4 preamble bytes
    writeRegister(MDMCFG0, 0xF8);
    writeRegister(DEVIATN, 0x35);  // ~20.6 kHz deviation
    writeRegister(MCSM2, 0x07);
    writeRegister(MCSM1, 0x3F);    // return to RX after TX/RX
    writeRegister(MCSM0, 0x18);
    writeRegister(FOCCFG, 0x16);
    writeRegister(BSCFG, 0x6C);
    writeRegister(AGCCTRL2, 0x43);
    writeRegister(AGCCTRL1, 0x40);
    writeRegister(AGCCTRL0, 0x91);
    writeRegister(FREND1, 0x56);
    writeRegister(FREND0, 0x10);
    writeRegister(FSCAL3, 0xE9);
    writeRegister(FSCAL2, 0x2A);
    writeRegister(FSCAL1, 0x00);
    writeRegister(FSCAL0, 0x1F);
    writeRegister(TEST2, 0x81);
    writeRegister(TEST1, 0x35);
    writeRegister(TEST0, 0x09);

    flushRx();
    flushTx();
    enterReceive();
    return true;
}

bool CC1101::configurePassiveOok433()
{
    enterIdle();

    writeRegister(IOCFG0, 0x0D);   // asynchronous serial data
    writeRegister(IOCFG2, 0x0E);   // carrier sense
    writeRegister(PKTCTRL0, 0x32); // asynchronous, infinite packet mode
    writeRegister(FSCTRL1, 0x06);
    writeRegister(FSCTRL0, 0x00);
    setFrequencyHz(433'920'000);
    writeRegister(MDMCFG4, 0xF5);  // wide receive bandwidth
    writeRegister(MDMCFG3, 0x83);
    writeRegister(MDMCFG2, 0x30);  // ASK/OOK, no sync
    writeRegister(MDMCFG1, 0x22);
    writeRegister(MDMCFG0, 0xF8);
    writeRegister(AGCCTRL2, 0x04);
    writeRegister(AGCCTRL1, 0x00);
    writeRegister(AGCCTRL0, 0x92);
    writeRegister(FREND1, 0x56);
    writeRegister(FREND0, 0x11);
    writeRegister(FSCAL3, 0xE9);
    writeRegister(FSCAL2, 0x2A);
    writeRegister(FSCAL1, 0x00);
    writeRegister(FSCAL0, 0x1F);

    flushRx();
    enterReceive();
    return true;
}

bool CC1101::sendPacket(const uint8_t* data,
                        std::size_t length,
                        uint32_t timeoutMs)
{
    if (data == nullptr || length == 0 || length > 61)
    {
        return false;
    }

    flushTx();
    const uint8_t packetLength = static_cast<uint8_t>(length);
    writeBurst(FIFO, &packetLength, 1);
    writeBurst(FIFO, data, length);
    enterTransmit();

    const uint64_t deadline = time_us_64() +
        static_cast<uint64_t>(timeoutMs) * 1000ULL;
    bool transmissionStarted = false;

    while (time_us_64() < deadline)
    {
        const uint8_t state = readMarcState();
        if (state == 0x13 || state == 0x14 || state == 0x15)
        {
            transmissionStarted = true;
        }
        else if (transmissionStarted && (state == 0x0D || state == 0x01))
        {
            return true;
        }
        if (state == 0x16) // TX FIFO underflow
        {
            flushTx();
            return false;
        }
        sleep_us(100);
    }

    flushTx();
    return false;
}

int CC1101::receivePacket(uint8_t* data,
                          std::size_t capacity,
                          int16_t& rssiDbm,
                          uint8_t& lqi,
                          bool& crcOk)
{
    if (data == nullptr || capacity == 0)
    {
        return 0;
    }

    const uint8_t rxBytes = readRegister(RXBYTES);
    if ((rxBytes & 0x80) != 0)
    {
        flushRx();
        enterReceive();
        return -1;
    }
    if ((rxBytes & 0x7F) < 1)
    {
        return 0;
    }

    uint8_t length = 0;
    readBurst(FIFO, &length, 1);
    if (length == 0 || length > 61 || length > capacity)
    {
        flushRx();
        enterReceive();
        return -1;
    }

    // Wait briefly until payload plus the two appended status bytes arrive.
    const uint64_t deadline = time_us_64() + 5'000;
    while ((readRegister(RXBYTES) & 0x7F) < length + 2)
    {
        if (time_us_64() >= deadline)
        {
            flushRx();
            enterReceive();
            return -1;
        }
    }

    readBurst(FIFO, data, length);
    uint8_t status[2] = {};
    readBurst(FIFO, status, sizeof(status));

    const int8_t rawRssi = static_cast<int8_t>(status[0]);
    rssiDbm = static_cast<int16_t>(rawRssi / 2 - 74);
    lqi = status[1] & 0x7F;
    crcOk = (status[1] & 0x80) != 0;
    return length;
}
