#include "pulse_capture.hpp"

#include <algorithm>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

PulseCapture* PulseCapture::instance_ = nullptr;

bool PulseCapture::init(uint dataPin, uint carrierSensePin)
{
    dataPin_ = dataPin;
    carrierSensePin_ = carrierSensePin;

    gpio_init(dataPin_);
    gpio_set_dir(dataPin_, GPIO_IN);
    gpio_pull_down(dataPin_);

    gpio_init(carrierSensePin_);
    gpio_set_dir(carrierSensePin_, GPIO_IN);
    gpio_pull_down(carrierSensePin_);

    instance_ = this;

    gpio_set_irq_enabled_with_callback(
        dataPin_,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        false,
        &PulseCapture::gpioCallback);

    return true;
}

void PulseCapture::start()
{
    clear();

    running_ = true;

    gpio_set_irq_enabled(
        dataPin_,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true);
}

void PulseCapture::stop()
{
    gpio_set_irq_enabled(
        dataPin_,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        false);

    running_ = false;
}

void PulseCapture::clear()
{
    const uint32_t irqState =
        save_and_disable_interrupts();

    sampleCount_ = 0;
    previousEdgeUs_ = 0;
    burstStartUs_ = 0;
    lastEdgeUs_ = 0;

    restore_interrupts(irqState);
}

void PulseCapture::gpioCallback(uint gpio, uint32_t events)
{
    if (instance_ != nullptr)
    {
        instance_->onEdge(gpio, events);
    }
}

void PulseCapture::onEdge(uint gpio, uint32_t events)
{
    if (!running_ || gpio != dataPin_)
    {
        return;
    }

    const uint64_t now = time_us_64();

    if (previousEdgeUs_ == 0)
    {
        previousEdgeUs_ = now;
        burstStartUs_ = now;
        lastEdgeUs_ = now;
        return;
    }

    const uint64_t duration64 = now - previousEdgeUs_;

    previousEdgeUs_ = now;
    lastEdgeUs_ = now;

    if (sampleCount_ >= MaxSamples)
    {
        running_ = false;
        return;
    }

    const bool previousLevel =
        (events & GPIO_IRQ_EDGE_RISE) != 0
        ? false
        : true;

    samples_[sampleCount_].durationUs =
        static_cast<uint32_t>(
            std::min<uint64_t>(duration64, UINT32_MAX));

    samples_[sampleCount_].level = previousLevel;

    ++sampleCount_;
}

bool PulseCapture::active() const
{
    return running_;
}

bool PulseCapture::burstReady(uint32_t silenceUs) const
{
    if (sampleCount_ < 4 || lastEdgeUs_ == 0)
    {
        return false;
    }

    return time_us_64() - lastEdgeUs_ >= silenceUs;
}

std::size_t PulseCapture::copySamples(
    PulseSample* destination,
    std::size_t capacity) const
{
    if (destination == nullptr || capacity == 0)
    {
        return 0;
    }

    const uint32_t irqState =
        save_and_disable_interrupts();

    const std::size_t capturedCount = sampleCount_;
    const std::size_t count =
        std::min(capturedCount, capacity);

    for (std::size_t i = 0; i < count; ++i)
    {
        destination[i] = samples_[i];
    }

    restore_interrupts(irqState);

    return count;
}

uint64_t PulseCapture::burstStartUs() const
{
    return burstStartUs_;
}

uint64_t PulseCapture::lastEdgeUs() const
{
    return lastEdgeUs_;
}
