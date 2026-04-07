#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

int main() {
    stdio_init_all();  // 표준 입출력 초기화

    for(int pincnt = 0; pincnt < 16 ; pincnt++) {
        gpio_init(pincnt);
        gpio_set_dir(pincnt, GPIO_IN);
        gpio_pull_up(pincnt);
    }
    gpio_init(25);
    gpio_set_dir(25,GPIO_OUT);

    gpio_init(16);
    gpio_set_dir(16,GPIO_OUT);

    gpio_init(13);
    gpio_set_dir(13,GPIO_OUT);
    printf("GPIO settings initialized\n");

    int ledcnt = 0;
    int heartcnt = 0;

    while (true) 
    {
        ledcnt = 0;
        heartcnt++;
        for(int pincnt = 0; pincnt < 13; pincnt++) 
        {
            if (!gpio_get(pincnt)) {
                printf("GPIO %d on\n", pincnt);
                ledcnt++;
            }
            else
            {
                printf("GPIO %d off\n", pincnt);
            }
        }

        if(ledcnt > 11)
        {
            gpio_put(13,1);
            
        }
        else{gpio_put(13,0);}
        
        gpio_put(25,heartcnt%2);
        gpio_put(16,heartcnt%2);

        sleep_ms(500);  // CPU 사용률을 줄이기 위해 약간의 지연을 추가
        
    }

    return 0;
}
