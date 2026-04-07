#include <stdio.h>
#include "pico/stdlib.h"

#define INPUT_PIN 15

volatile bool signal_detected = false;

// 인터럽트 콜백 함수
void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == INPUT_PIN) {
        if (events & GPIO_IRQ_EDGE_RISE) {
            signal_detected = true;
            
        }
    }
}

int main()
{
    stdio_init_all();

    gpio_init(INPUT_PIN);
    gpio_set_dir(INPUT_PIN, GPIO_IN);
    gpio_pull_down(INPUT_PIN);  // 필요에 따라 pull-up 또는 pull-down 설정

    // GPIO 인터럽트 등록
    gpio_set_irq_enabled_with_callback(
        INPUT_PIN,
        GPIO_IRQ_EDGE_RISE,   // 상승 에지 감지
        true,
        &gpio_callback
    );

    while (1) {
        if (signal_detected) {
            signal_detected = false;
            printf("a\n");
        }
        tight_loop_contents();
        //sleep_ms(10);
    }
}