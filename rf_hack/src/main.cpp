#include <cstdio>
#include <cstring>

#include "cc1101.hpp"
#include "pico/stdlib.h"
#include "pulse_capture.hpp"
#include "rf_test.hpp"

enum class OperatingMode
{
    PassiveKeyMonitor,
    DirectAsk315Monitor,
    TestTransmitter,
    TestResponder
};

constexpr OperatingMode MODE =
    OperatingMode::DirectAsk315Monitor;

// 315 MHz receiver DATA input. The receiver is powered from 5 V, so connect
// DATA through a 10 kOhm / 20 kOhm voltage divider before this Pico pin.
constexpr uint DIRECT_ASK_DATA_PIN = 22;

int main()
{
    stdio_init_all();
    sleep_ms(1500);

    printf("\nRF Tester\n");

    if (MODE == OperatingMode::DirectAsk315Monitor)
    {
        PulseCapture capture;
        runDirectAskMonitor(capture, DIRECT_ASK_DATA_PIN);
        return 0;
    }

    CC1101::Config config;
    config.spi = spi0;
    config.sckPin = 18;
    config.mosiPin = 19;
    config.misoPin = 16;
    config.csPin = 17;
    config.gdo0Pin = 20;
    config.gdo2Pin = 21;
    config.spiClockHz = 4'000'000;

    CC1101 radio(config);

    if (!radio.init())
    {
        printf("CC1101 initialization failed\n");

        while (true)
        {
            sleep_ms(1000);
        }
    }

    printf("PARTNUM = 0x%02X\n",
           radio.readPartNumber());

    printf("VERSION = 0x%02X\n",
           radio.readVersion());

    switch (MODE)
    {
        case OperatingMode::PassiveKeyMonitor:
        {
            PulseCapture capture;
            runPassiveKeyMonitor(radio, capture);
            break;
        }

        case OperatingMode::DirectAsk315Monitor:
            break;

        case OperatingMode::TestTransmitter:
        {
            radio.configurePacketMode433();

            uint32_t sent = 0;
            uint32_t success = 0;

            while (true)
            {
                ++sent;

                if (runPingTest(radio, sent, 100))
                {
                    ++success;
                }

                const double successRate =
                    sent == 0
                    ? 0.0
                    : 100.0 *
                      static_cast<double>(success) /
                      static_cast<double>(sent);

                printf(
                    "Sent=%lu Success=%lu Rate=%.2f%%\n",
                    static_cast<unsigned long>(sent),
                    static_cast<unsigned long>(success),
                    successRate);

                sleep_ms(500);
            }

            break;
        }

        case OperatingMode::TestResponder:
        {
            radio.configurePacketMode433();
            runResponder(radio);
            break;
        }
    }

    return 0;
}
