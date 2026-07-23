#include "rf_test.hpp"

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"

bool runPingTest(CC1101& radio,
                 uint32_t sequence,
                 uint32_t timeoutMs)
{
    TestPacket packet;
    packet.sequence = sequence;
    packet.timestampUs =
        static_cast<uint32_t>(time_us_64());

    for (std::size_t i = 0; i < sizeof(packet.payload); ++i)
    {
        packet.payload[i] =
            static_cast<uint8_t>(sequence + i);
    }

    const uint64_t txTime = time_us_64();

    if (!radio.sendPacket(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet)))
    {
        printf("TX failed: seq=%lu\n",
               static_cast<unsigned long>(sequence));

        radio.enterReceive();
        return false;
    }

    radio.flushRx();
    radio.enterReceive();

    const uint64_t deadline =
        time_us_64() +
        static_cast<uint64_t>(timeoutMs) * 1000ULL;

    while (time_us_64() < deadline)
    {
        uint8_t buffer[64] = {};

        int16_t localRssi = 0;
        uint8_t lqi = 0;
        bool crcOk = false;

        const int received =
            radio.receivePacket(buffer,
                                sizeof(buffer),
                                localRssi,
                                lqi,
                                crcOk);

        if (received == sizeof(AckPacket) && crcOk)
        {
            AckPacket ack;
            std::memcpy(&ack, buffer, sizeof(ack));

            if (ack.magic == 0xAA55 &&
                ack.type == MessageType::Ack &&
                ack.sequence == sequence)
            {
                const uint64_t rttUs =
                    time_us_64() - txTime;

                printf(
                    "SEQ=%lu RTT=%llu us "
                    "localRSSI=%d remoteRSSI=%d "
                    "LQI=%u\n",
                    static_cast<unsigned long>(sequence),
                    static_cast<unsigned long long>(rttUs),
                    localRssi,
                    ack.receiverRssiDbm,
                    ack.receiverLqi);

                return true;
            }
        }

        sleep_us(100);
    }

    printf("Timeout: seq=%lu\n",
           static_cast<unsigned long>(sequence));

    return false;
}

void runResponder(CC1101& radio)
{
    radio.flushRx();
    radio.enterReceive();
    printf("Responder ready\n");

    while (true)
    {
        uint8_t buffer[64] = {};
        int16_t rssi = 0;
        uint8_t lqi = 0;
        bool crcOk = false;
        const int received = radio.receivePacket(
            buffer, sizeof(buffer), rssi, lqi, crcOk);

        if (received == static_cast<int>(sizeof(TestPacket)) && crcOk)
        {
            TestPacket request;
            std::memcpy(&request, buffer, sizeof(request));

            if (request.magic == 0xAA55 &&
                request.version == 1 &&
                request.type == MessageType::Test)
            {
                AckPacket ack;
                ack.sequence = request.sequence;
                ack.originalTimestampUs = request.timestampUs;
                ack.receiverRssiDbm = rssi;
                ack.receiverLqi = lqi;

                if (!radio.sendPacket(
                        reinterpret_cast<const uint8_t*>(&ack),
                        sizeof(ack)))
                {
                    printf("ACK TX failed: seq=%lu\n",
                           static_cast<unsigned long>(request.sequence));
                }
                radio.enterReceive();
            }
        }

        sleep_us(100);
    }
}

void runPassiveKeyMonitor(CC1101& radio, PulseCapture& capture)
{
    if (!radio.configurePassiveOok433() ||
        !capture.init(radio.gdo0Pin(), radio.gdo2Pin()))
    {
        printf("Passive monitor initialization failed\n");
        return;
    }

    static PulseSample samples[PulseCapture::MaxSamples];
    capture.start();
    printf("Passive OOK monitor ready at 433.92 MHz\n");

    while (true)
    {
        if (capture.burstReady())
        {
            capture.stop();
            const std::size_t count = capture.copySamples(
                samples, PulseCapture::MaxSamples);

            printf("BURST edges=%lu RSSI=%d dBm\n",
                   static_cast<unsigned long>(count),
                   radio.readRssiDbm());

            for (std::size_t i = 0; i < count; ++i)
            {
                printf("%c%lu%s",
                       samples[i].level ? 'H' : 'L',
                       static_cast<unsigned long>(samples[i].durationUs),
                       (i + 1) % 12 == 0 ? "\n" : " ");
            }
            if (count % 12 != 0)
            {
                printf("\n");
            }

            capture.start();
        }
        else if (!capture.active())
        {
            printf("Capture buffer full; restarting\n");
            capture.start();
        }

        sleep_ms(1);
    }
}

void runDirectAskMonitor(PulseCapture& capture, uint dataPin)
{
    if (!capture.init(dataPin))
    {
        printf("Direct ASK receiver initialization failed\n");
        return;
    }

    static PulseSample samples[PulseCapture::MaxSamples];
    capture.start();

    printf("315 MHz ASK receiver monitor ready on GPIO %u\n", dataPin);
    printf("Press a remote button; idle receiver noise is normal\n");

    while (true)
    {
        if (capture.burstReady())
        {
            capture.stop();
            const std::size_t count = capture.copySamples(
                samples, PulseCapture::MaxSamples);

            printf("BURST edges=%lu\n",
                   static_cast<unsigned long>(count));

            for (std::size_t i = 0; i < count; ++i)
            {
                printf("%c%lu%s",
                       samples[i].level ? 'H' : 'L',
                       static_cast<unsigned long>(samples[i].durationUs),
                       (i + 1) % 12 == 0 ? "\n" : " ");
            }
            if (count % 12 != 0)
            {
                printf("\n");
            }

            capture.start();
        }
        else if (!capture.active())
        {
            printf("Capture buffer full; restarting\n");
            capture.start();
        }

        sleep_ms(1);
    }
}
