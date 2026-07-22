#pragma once

#include <cstddef>
#include <cstdint>

#include "pico/types.h"

struct PulseSample
{
    uint32_t durationUs;
    bool level;
};

class PulseCapture
{
public:
    static constexpr std::size_t MaxSamples = 4096;

    bool init(uint dataPin, uint carrierSensePin);

    void start();
    void stop();
    void clear();

    bool active() const;
    bool burstReady(uint32_t silenceUs = 15'000) const;

    std::size_t copySamples(PulseSample* destination,
                            std::size_t capacity) const;

    uint64_t burstStartUs() const;
    uint64_t lastEdgeUs() const;

private:
    static void gpioCallback(uint gpio, uint32_t events);
    void onEdge(uint gpio, uint32_t events);

    static PulseCapture* instance_;

    uint dataPin_ = 0;
    uint carrierSensePin_ = 0;

    volatile bool running_ = false;
    volatile std::size_t sampleCount_ = 0;

    volatile uint64_t previousEdgeUs_ = 0;
    volatile uint64_t burstStartUs_ = 0;
    volatile uint64_t lastEdgeUs_ = 0;

    PulseSample samples_[MaxSamples] = {};
};
