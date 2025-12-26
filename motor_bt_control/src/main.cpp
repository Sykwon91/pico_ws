#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <cstdio>
#include "hardware/pwm.h"


#define UART_ID     uart1
#define BAUD_RATE   9600          // HC-06 기본 9600 (바꿨으면 여기도 맞추기)
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#define LED_PIN     25
#define MotorPin 15   // PWM 출력 핀


int main() {
    // (선택) PC USB 디버그 출력
    stdio_init_all();
    sleep_ms(1500);
    //printf("HC-06 RX LED control start (baud=%d)\n", BAUD_RATE);

    // LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);



    // UART 설정
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // RX 라인 안정화(떠있으면 깨짐)
    gpio_pull_up(UART_RX_PIN);

    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_fifo_enabled(UART_ID, true);


    // GPIO를 PWM 기능으로 설정
    gpio_set_function(MotorPin, GPIO_FUNC_PWM);

    // 해당 GPIO가 속한 PWM 슬라이스 번호 가져오기
    uint slice_num = pwm_gpio_to_slice_num(MotorPin);

    // PWM 설정 구조체 가져오기
    pwm_config config = pwm_get_default_config();

    // PWM 클럭 분주 (125MHz / 125 = 1MHz)
    pwm_config_set_clkdiv(&config, 125.0f);

    // PWM 카운터 TOP 값 (1MHz / 1000 = 1kHz)
    pwm_config_set_wrap(&config, 1000);

    // PWM 설정 적용 및 시작
    pwm_init(slice_num, &config, true);


    while (true) {
        // HC-06 -> Pico (UART 수신)
        while (uart_is_readable(UART_ID)) {
            uint8_t ch = uart_getc(UART_ID);

            // (선택) 디버그: PC로 수신 문자 출력
            printf("%c",ch);

            if (ch == 'a') {
                gpio_put(LED_PIN, 1);   // LED ON
                uart_puts(UART_ID,"Led on\n");
                pwm_set_gpio_level(MotorPin, 100);

            } else if (ch == 'b') {
                gpio_put(LED_PIN, 0);   // LED OFF
                uart_puts(UART_ID,"Led off\n");
                pwm_set_gpio_level(MotorPin, 0);

            } else if (ch == 't') {
                // (옵션) 토글
                uart_puts(UART_ID,"toggle\n");
                gpio_put(LED_PIN, !gpio_get(LED_PIN));
            }
        }

        tight_loop_contents();
    }
}
