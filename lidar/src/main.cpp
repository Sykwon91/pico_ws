#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <cstdio>

#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

int main() {
    // USB stdio 초기화 (printf 출력용)
    stdio_init_all();
    sleep_ms(1500);  // 터미널 연결 대기

    // UART 초기화
    uart_init(UART_ID, BAUD_RATE);

    // 핀 설정
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // RX 안정화 (권장)
    gpio_pull_up(UART_RX_PIN);

    // UART 포맷: 8N1
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);

    printf("UART basic example start (9600 8N1)\n");

    while (true) {
        // UART → USB (수신)
        if (uart_is_readable(UART_ID)) {
            uint8_t ch = uart_getc(UART_ID);
            putchar_raw(ch);   // 그대로 출력
        }

        // USB → UART (송신)
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            uart_putc_raw(UART_ID, (char)c);
        }

        tight_loop_contents();
    }
}
