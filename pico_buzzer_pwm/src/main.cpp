#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define BUZZER_PIN 15

// 템포/해상도
#define BPM 140
#define TICKS_PER_BEAT 8   // 8비트 느낌 내기 좋은 분해능(16도 가능)
#define GATE_PERCENT 80    // 음 길이(%) : 80이면 약간 끊김감(8bit 느낌)

// 아르페지오(화음 흉내) 스위칭 주기(ms) - 작을수록 "동시에 난다" 느낌
#define ARP_STEP_MS 12

typedef enum {
    N_REST = -1,
    N_C4, N_CS4, N_D4, N_DS4, N_E4, N_F4, N_FS4, N_G4, N_GS4, N_A4, N_AS4, N_B4,
    N_C5, N_CS5, N_D5, N_DS5, N_E5, N_F5, N_FS5, N_G5, N_GS5, N_A5, N_AS5, N_B5,
} Note;

static float note_freq(Note n) {
    if (n == N_REST) return 0.0f;
    // MIDI 기준으로 계산: C4=60
    int midi = 60 + (int)n; // N_C4=0
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

static uint g_slice, g_chan;

static void pwm_tone(float freq_hz) {
    if (freq_hz <= 0.0f) {
        pwm_set_enabled(g_slice, false);
        gpio_put(BUZZER_PIN, 0);
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
    pwm_set_chan_level(g_slice, g_chan, (top + 1) / 2); // 50% duty (사각파)
    pwm_set_enabled(g_slice, true);
}

static void buzzer_init(void) {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    g_chan  = pwm_gpio_to_channel(BUZZER_PIN);
    pwm_set_enabled(g_slice, false);
}

// 한 “틱”의 시간(ms)
static int tick_ms(void) {
    // 1 beat = 60/BPM 초
    // 1 tick = beat / TICKS_PER_BEAT
    float ms = (60.0f * 1000.0f / (float)BPM) / (float)TICKS_PER_BEAT;
    return (int)(ms + 0.5f);
}

typedef struct {
    Note n1, n2, n3;   // 1채널 멜로디(n1) or 3음 화음(아르페지오)
    uint8_t ticks;     // 길이(틱 단위)
    uint8_t arp;       // 0=단음, 1=아르페지오(화음)
} Step;

// 간단한 8비트풍 패턴 (원하면 곡을 더 길게/다양하게 만들어 줄 수 있음)
static const Step song[] = {
    // Intro riff (단음)
    {N_E4, N_REST, N_REST, 4, 0}, {N_G4, N_REST, N_REST, 4, 0}, {N_A4, N_REST, N_REST, 4, 0}, {N_G4, N_REST, N_REST, 4, 0},
    {N_E4, N_REST, N_REST, 4, 0}, {N_D4, N_REST, N_REST, 4, 0}, {N_C4, N_REST, N_REST, 8, 0}, {N_REST, N_REST, N_REST, 4, 0},

    // Chord section (아르페지오로 화음 흉내): C - Am - F - G
    {N_C4, N_E4, N_G4, 8, 1},
    {N_A5 /* 아래 enum엔 없음이라 대신 A4-12로 계산하려면 확장 필요 */, N_REST, N_REST, 0, 0},
};

static const Step song2[] = {
    // C major arpeggio loop (C-E-G-C5)
    {N_C4, N_REST, N_REST, 4, 0},
    {N_E4, N_REST, N_REST, 4, 0},
    {N_G4, N_REST, N_REST, 4, 0},
    {N_C5, N_REST, N_REST, 8, 0},

    // C chord (arp)
    {N_C4, N_E4, N_G4, 8, 1},
    // F chord (arp)
    {N_F4, N_A4, N_C5, 8, 1},
    // G chord (arp)
    {N_G4, N_B4, N_D5, 8, 1},
    // Am chord (arp)
    {N_A4, N_C5, N_E5, 8, 1},
};

static void play_step(const Step* s) {
    int tms = tick_ms();
    int total_ms = s->ticks * tms;
    int gate_ms  = (total_ms * GATE_PERCENT) / 100;
    int rest_ms  = total_ms - gate_ms;

    if (!s->arp) {
        float f = note_freq(s->n1);
        pwm_tone(f);
        sleep_ms(gate_ms);
        pwm_tone(0);
        sleep_ms(rest_ms);
        return;
    }

    // 아르페지오: gate 동안 n1,n2,n3를 ARP_STEP_MS 단위로 순환
    absolute_time_t start = get_absolute_time();
    int idx = 0;
    while (absolute_time_diff_us(start, get_absolute_time()) < (int64_t)gate_ms * 1000) {
        Note nn = (idx % 3 == 0) ? s->n1 : (idx % 3 == 1) ? s->n2 : s->n3;
        pwm_tone(note_freq(nn));
        sleep_ms(ARP_STEP_MS);
        idx++;
    }
    pwm_tone(0);
    sleep_ms(rest_ms);
}

int main() {
    stdio_init_all();
    sleep_ms(500);

    buzzer_init();

    printf("8-bit buzzer player start. BPM=%d tick=%dms\n", BPM, tick_ms());

    while (true) {
        for (size_t i = 0; i < sizeof(song2)/sizeof(song2[0]); ++i) {
            play_step(&song2[i]);
        }
    }
}
