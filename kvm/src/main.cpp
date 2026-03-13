#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#define U0  uart0
#define TX0 0
#define RX0 1

#define U1  uart1
#define TX1 4
#define RX1 5

#define SW1_PIN 2
#define SW2_PIN 3
#define LED_PIN 25

#define OUT1_A 10
#define OUT1_B 11
#define OUT2_A 12
#define OUT2_B 13

static void init_uart()
{
    uart_init(U0, 19200);
    gpio_set_function(TX0, GPIO_FUNC_UART);
    gpio_set_function(RX0, GPIO_FUNC_UART);
    uart_set_format(U0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(U0, true);

    uart_init(U1, 19200);
    gpio_set_function(TX1, GPIO_FUNC_UART);
    gpio_set_function(RX1, GPIO_FUNC_UART);
    uart_set_format(U1, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(U1, true);
}

static void init_gpio()
{
    gpio_init(SW1_PIN);
    gpio_set_dir(SW1_PIN, GPIO_IN);
    gpio_pull_up(SW1_PIN);

    gpio_init(SW2_PIN);
    gpio_set_dir(SW2_PIN, GPIO_IN);
    gpio_pull_up(SW2_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(OUT1_A);
    gpio_set_dir(OUT1_A, GPIO_OUT);

    gpio_init(OUT1_B);
    gpio_set_dir(OUT1_B, GPIO_OUT);

    gpio_init(OUT2_A);
    gpio_set_dir(OUT2_A, GPIO_OUT);

    gpio_init(OUT2_B);
    gpio_set_dir(OUT2_B, GPIO_OUT);

    // 초기 상태
    gpio_put(LED_PIN, 0);

    gpio_put(OUT1_A, 0);
    gpio_put(OUT1_B, 1);

    gpio_put(OUT2_A, 0);
    gpio_put(OUT2_B, 1);
}

int main()
{
    stdio_init_all();
    init_uart();
    init_gpio();

    bool switch1_state = false;   // false = i02, true = i01
    bool switch2_state = false;

    bool prev_sw1 = true;         // pull-up이므로 기본값 HIGH(true)
    bool prev_sw2 = true;

    while (1)
    {
        bool curr_sw1 = gpio_get(SW1_PIN);
        bool curr_sw2 = gpio_get(SW2_PIN);

        // SW1: falling edge 검출 (안 눌림 -> 눌림)
        if (prev_sw1 == true && curr_sw1 == false)
        {
            sleep_ms(20); // debounce
            if (gpio_get(SW1_PIN) == false)
            {
                switch1_state = !switch1_state;

                if (switch1_state)
                {
                    uart_puts(U0, "sw i01\r");
                    gpio_put(OUT1_A, 1);
                    gpio_put(OUT1_B, 0);
                }
                else
                {
                    uart_puts(U0, "sw i02\r");
                    gpio_put(OUT1_A, 0);
                    gpio_put(OUT1_B, 1);
                }
            }
        }

        // SW2: falling edge 검출
        if (prev_sw2 == true && curr_sw2 == false)
        {
            sleep_ms(20); // debounce
            if (gpio_get(SW2_PIN) == false)
            {
                switch2_state = !switch2_state;

                if (switch2_state)
                {
                    uart_puts(U1, "sw i01\r");
                    gpio_put(OUT2_A, 1);
                    gpio_put(OUT2_B, 0);
                }
                else
                {
                    uart_puts(U1, "sw i02\r");
                    gpio_put(OUT2_A, 0);
                    gpio_put(OUT2_B, 1);
                }
            }
        }

        // LED는 둘 중 하나라도 눌리면 ON
        if (!curr_sw1 || !curr_sw2)
            gpio_put(LED_PIN, 1);
        else
            gpio_put(LED_PIN, 0);

        prev_sw1 = curr_sw1;
        prev_sw2 = curr_sw2;

        sleep_ms(10);
    }

    return 0;
}