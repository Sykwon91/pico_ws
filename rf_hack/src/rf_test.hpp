#pragma once

#include <cstdint>

#include "cc1101.hpp"
#include "pulse_capture.hpp"

#pragma pack(push, 1)

enum class MessageType : uint8_t
{
    Test = 1,
    Ack  = 2
};

struct TestPacket
{
    uint16_t magic = 0xAA55;
    uint8_t version = 1;
    MessageType type = MessageType::Test;

    uint32_t sequence = 0;
    uint32_t timestampUs = 0;

    uint8_t payload[16] = {};
};

struct AckPacket
{
    uint16_t magic = 0xAA55;
    uint8_t version = 1;
    MessageType type = MessageType::Ack;

    uint32_t sequence = 0;
    uint32_t originalTimestampUs = 0;

    int16_t receiverRssiDbm = 0;
    uint8_t receiverLqi = 0;
};

#pragma pack(pop)

bool runPingTest(CC1101& radio,
                 uint32_t sequence,
                 uint32_t timeoutMs = 100);

void runResponder(CC1101& radio);
void runPassiveKeyMonitor(CC1101& radio, PulseCapture& capture);
void runDirectAskMonitor(PulseCapture& capture, uint dataPin);
