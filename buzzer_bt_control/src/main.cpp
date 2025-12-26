#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

/* =========================
 *  USER CONFIG
 * ========================= */
#define BUZZER_PIN      15

// HC-06 보통 기본 9600 (AT로 바꿨으면 여기 맞추기)
#define UART_ID         uart0
#define UART_BAUD       9600
#define UART_TX_PIN     0   // Pico -> HC-06 RXD (선택)
#define UART_RX_PIN     1   // HC-06 TXD -> Pico

// 메시지 버퍼
#define RX_BUF_SIZE     64

// 개행 없이 보내는 앱을 위해 "타임아웃으로 메시지 완성" 처리 (ms)
#define MSG_TIMEOUT_MS  80

// 노트 재생 길이 (ms)
#define PLAY_MS         250
#define GAP_MS          30

/* =========================
 *  PWM BUZZER
 * ========================= */
static uint g_slice;
static uint g_chan;

static void buzzer_init(void) {
    printf("[INIT] Buzzer pin=%d\n", BUZZER_PIN);
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    g_chan  = pwm_gpio_to_channel(BUZZER_PIN);
    pwm_set_enabled(g_slice, false);
    gpio_put(BUZZER_PIN, 0);
}

static void pwm_set_tone(float freq_hz) {
    if (freq_hz <= 0.0f) {
        pwm_set_enabled(g_slice, false);
        gpio_put(BUZZER_PIN, 0);
        printf("[PWM] OFF\n");
        return;
    }

    uint32_t clk = clock_get_hz(clk_sys);
    float div = 1.0f;
    uint32_t top = (uint32_t)((clk / (div * freq_hz)) - 1.0f);

    while (top > 65535) {
        div *= 2.0f;
        top = (uint32_t)((clk / (div * freq_hz)) - 1.0f);
        if (div > 256.0f) { top = 65535; break; }
    }

    pwm_set_clkdiv(g_slice, div);
    pwm_set_wrap(g_slice, top);
    pwm_set_chan_level(g_slice, g_chan, (top + 1) / 2); // 50%
    pwm_set_enabled(g_slice, true);

    printf("[PWM] ON freq=%.2fHz div=%.1f top=%u\n", freq_hz, div, top);
}

/* =========================
 *  UART
 * ========================= */
static void uart_init_hc06(void) {
    printf("[INIT] UART0 baud=%d TX=%d RX=%d\n", UART_BAUD, UART_TX_PIN, UART_RX_PIN);
    uart_init(UART_ID, UART_BAUD);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    // RX 플로팅 방지 (배선/GND 문제 시 증상이 완화될 수 있음)
    gpio_pull_up(UART_RX_PIN);
}

/* =========================
 *  Utils
 * ========================= */
static int streq_nocase(const char* a, const char* b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static void trim_inplace(char* s) {
    // trim front
    while (isspace((unsigned char)s[0])) memmove(s, s+1, strlen(s));
    // trim back
    for (int i = (int)strlen(s)-1; i >= 0; --i) {
        if (isspace((unsigned char)s[i])) s[i] = '\0';
        else break;
    }
}

/*
  입력 예:
    "C4", "D#4", "Eb4", "F#3", "Bb5", "A4"
    "REST" 또는 "R"
  반환:
    freq Hz (0.0f는 쉼표), 실패면 -1.0f
*/
static float note_to_freq_debug(const char* in_raw) {
    printf("[PARSE] raw='%s'\n", in_raw);

    char s[32] = {0};
    strncpy(s, in_raw, sizeof(s)-1);
    trim_inplace(s);

    printf("[PARSE] trimmed='%s'\n", s);

    if (streq_nocase(s, "REST") || streq_nocase(s, "R")) {
        printf("[PARSE] REST\n");
        return 0.0f;
    }

    char n = toupper((unsigned char)s[0]);
    int base;
    switch (n) {
        case 'C': base = 0; break;
        case 'D': base = 2; break;
        case 'E': base = 4; break;
        case 'F': base = 5; break;
        case 'G': base = 7; break;
        case 'A': base = 9; break;
        case 'B': base = 11; break;
        default:
            printf("[PARSE][ERR] invalid note letter '%c'\n", n);
            return -1.0f;
    }
    printf("[PARSE] note=%c base=%d\n", n, base);

    int idx = 1;
    int accidental = 0;
    if (s[idx] == '#') { accidental = +1; idx++; printf("[PARSE] accidental=#\n"); }
    else if (s[idx] == 'b' || s[idx] == 'B') { accidental = -1; idx++; printf("[PARSE] accidental=b\n"); }
    else { printf("[PARSE] accidental=none\n"); }

    if (s[idx] == '\0') {
        printf("[PARSE][ERR] octave missing\n");
        return -1.0f;
    }
    // octave can be negative? allow '-' too
    if (!isdigit((unsigned char)s[idx]) && s[idx] != '-') {
        printf("[PARSE][ERR] octave invalid start char '%c'\n", s[idx]);
        return -1.0f;
    }

    int octave = atoi(&s[idx]);
    printf("[PARSE] octave=%d\n", octave);

    int midi = (octave + 1) * 12 + base + accidental;
    printf("[PARSE] midi=%d\n", midi);

    float freq = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    printf("[PARSE] freq=%.2fHz\n", freq);
    return freq;
}

static void play_note(float freq_hz) {
    if (freq_hz <= 0.0f) {
        printf("[PLAY] REST\n");
        pwm_set_tone(0.0f);
        sleep_ms(PLAY_MS);
        return;
    }
    printf("[PLAY] %.2fHz for %dms\n", freq_hz, PLAY_MS);
    pwm_set_tone(freq_hz);
    sleep_ms(PLAY_MS);
    pwm_set_tone(0.0f);
    sleep_ms(GAP_MS);
}

/* =========================
 *  MAIN LOOP (1-byte RX)
 * ========================= */
int main() {
    stdio_init_all();
    sleep_ms(800);

    printf("\n===== HC-06 UART NOTE PLAYER START =====\n");
    printf("[INFO] Expect messages like: C4, D#4, Eb4, A4, REST\n");
    printf("[INFO] HC-06 default baud is often 9600. Current UART_BAUD=%d\n", UART_BAUD);

    buzzer_init();
    uart_init_hc06();

    char rx_buf[RX_BUF_SIZE] = {0};
    size_t rx_len = 0;

    absolute_time_t last_rx_time = get_absolute_time();
    absolute_time_t last_alive   = get_absolute_time();

    while (true) {
        // alive
        if (absolute_time_diff_us(last_alive, get_absolute_time()) > 1000 * 1000) {
            printf("[ALIVE] running rx_len=%u\n", (unsigned)rx_len);
            last_alive = get_absolute_time();
        }

        // 1-byte RX
        if (uart_is_readable(UART_ID)) {
            uint8_t c = uart_getc(UART_ID);
            //printf("sibal :%c", c);
            if(c == 'A'){play_note(100.f);}
            else if(c == 'B'){play_note(200.f);}
            else if(c == 'C'){play_note(300.f);}
            else if(c == 'D'){play_note(400.f);}
            else if(c == 'E'){play_note(500.f);}
            else{play_note(0.f);}
        }
        sleep_ms(1);
    }
}
