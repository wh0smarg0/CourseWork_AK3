#include <stdint.h>

extern uint32_t _estack;
void Reset_Handler(void);

// Vector table
__attribute__((used, section(".isr_vector")))
void (* const vector_table[75])(void) = {
    [0] = (void (*)(void))(&_estack),
    [1] = Reset_Handler
};

// Регістри RCC
#define RCC_AHB1ENR    (*(volatile uint32_t *)0x40023830)
#define RCC_APB1ENR    (*(volatile uint32_t *)0x40023840)

// GPIO Регістри
#define GPIOA_MODER    (*(volatile uint32_t *)0x40020000)
#define GPIOA_AFRL     (*(volatile uint32_t *)0x40020020)
#define GPIOB_MODER    (*(volatile uint32_t *)0x40020400)
#define GPIOB_AFRL     (*(volatile uint32_t *)0x40020420)
#define GPIOB_AFRH     (*(volatile uint32_t *)0x40020424)

// Таймер 2 (Зелений: D6/PB10)
#define TIM2_CR1       (*(volatile uint32_t *)0x40000000)
#define TIM2_CCMR2     (*(volatile uint32_t *)0x4000001C)
#define TIM2_CCER      (*(volatile uint32_t *)0x40000020)
#define TIM2_PSC       (*(volatile uint32_t *)0x40000028)
#define TIM2_ARR       (*(volatile uint32_t *)0x4000002C)
#define TIM2_CCR3      (*(volatile uint32_t *)0x4000003C)

// Таймер 3 (Червоний: D5/PB4, Синій: A3/PB0)
#define TIM3_CR1       (*(volatile uint32_t *)0x40000400)
#define TIM3_CCMR1     (*(volatile uint32_t *)0x40000418)
#define TIM3_CCMR2     (*(volatile uint32_t *)0x4000041C)
#define TIM3_CCER      (*(volatile uint32_t *)0x40000420)
#define TIM3_PSC       (*(volatile uint32_t *)0x40000428)
#define TIM3_ARR       (*(volatile uint32_t *)0x4000042C)
#define TIM3_CCR1      (*(volatile uint32_t *)0x40000434) // Red
#define TIM3_CCR3      (*(volatile uint32_t *)0x4000043C) // Blue

// USART2
#define USART2_SR      (*(volatile uint32_t *)0x40004400)
#define USART2_DR      (*(volatile uint32_t *)0x40004404)
#define USART2_BRR     (*(volatile uint32_t *)0x40004408)
#define USART2_CR1     (*(volatile uint32_t *)0x4000440C)

void soft_delay(volatile uint32_t count) {
    while(count--) __asm("nop");
}

void set_rgb(int r, int g, int b) {
    // Обмеження значень для безпеки (0-255)
    if(r > 255) r = 255; if(g > 255) g = 255; if(b > 255) b = 255;
    TIM3_CCR1 = (r * 1000) / 255;
    TIM2_CCR3 = (g * 1000) / 255;
    TIM3_CCR3 = (b * 1000) / 255;
}

// Покращений парсер (шукає ключ у всьому буфері)
int parse_color(char *buf, char key) {
    for (int i = 0; i < 28; i++) {
        if (buf[i] == key) {
            int val = 0;
            int j = i + 1;
            if (buf[j] < '0' || buf[j] > '9') continue;
            while (buf[j] >= '0' && buf[j] <= '9') {
                val = val * 10 + (buf[j] - '0');
                j++;
            }
            return val;
        }
        if (buf[i] == '\0') break;
    }
    return -1;
}

void Reset_Handler(void) {
    // Вмикаємо тактування: GPIOA, GPIOB, TIM2, TIM3, USART2
    RCC_AHB1ENR |= (1 << 0) | (1 << 1);
    RCC_APB1ENR |= (1 << 0) | (1 << 1) | (1 << 17);

    // Налаштування UART (PA2, PA3)
    GPIOA_MODER &= ~((3 << 4) | (3 << 6));
    GPIOA_MODER |= (2 << 4) | (2 << 6);
    GPIOA_AFRL  &= ~((0xF << 8) | (0xF << 12));
    GPIOA_AFRL  |= (7 << 8) | (7 << 12);

    // Налаштування робочих пінів ПОРТУ B: PB0(A3), PB4(D5), PB10(D6)
    GPIOB_MODER &= ~((3 << 0) | (3 << 8) | (3 << 20));
    GPIOB_MODER |=  ((2 << 0) | (2 << 8) | (2 << 20));
    GPIOB_AFRL  &= ~((0xF << 0) | (0xF << 16));
    GPIOB_AFRL  |=  ((2 << 0) | (2 << 16)); // PB0, PB4 -> AF2
    GPIOB_AFRH  &= ~(0xF << 8);
    GPIOB_AFRH  |=  (1 << 8);              // PB10 -> AF1

    // Ініціалізація Таймерів (PWM Mode 1)
    TIM2_PSC = 16-1; TIM2_ARR = 1000; TIM2_CCMR2 = (6 << 4); TIM2_CCER = (1 << 8); TIM2_CR1 = 1;
    TIM3_PSC = 16-1; TIM3_ARR = 1000; TIM3_CCMR1 = (6 << 4); TIM3_CCMR2 = (6 << 4);
    TIM3_CCER = (1 << 0) | (1 << 8); TIM3_CR1 = 1;

    USART2_BRR = 0x0683;
    USART2_CR1 |= 0x200C;

    // Авто-тест при старті
    set_rgb(500, 0, 0); soft_delay(1000000);
    set_rgb(0, 500, 0); soft_delay(1000000);
    set_rgb(0, 0, 500); soft_delay(1000000);
    set_rgb(0, 0, 0);

    int br = 150;
    char mode = '0';
    uint32_t step = 0;
    
    char rx_buffer[32];
    int rx_idx = 0;
    
    int cur_r = 0; 
    int cur_g = 0; 
    int cur_b = 0;
    
    while (1) {
        if (USART2_SR & 0x0F) {
            volatile uint32_t dummy = USART2_DR;
            rx_idx = 0;
        }
    
        if (USART2_SR & (1 << 5)) {
            char c = USART2_DR;
            if (rx_idx < 31) rx_buffer[rx_idx++] = c;

            if (c == '\n' || c == '\r') {
                rx_buffer[rx_idx] = '\0';
                
                if (rx_buffer[0] == 'R') {
                    cur_r = parse_color(rx_buffer, 'R');
                    cur_g = parse_color(rx_buffer, 'G');
                    cur_b = parse_color(rx_buffer, 'B');
                    mode = 's';
                } 
                // Перевірка на яскравість
                else if (rx_buffer[0] == 'w') { if(br <= 245) br += 20; }
                else if (rx_buffer[0] == 's') { if(br >= 10)  br -= 20; }
                else {
                    mode = rx_buffer[0];
                    step = 0;
                }
                rx_idx = 0;
            }
        }

        step++;

        switch (mode) {
            case 's': set_rgb(cur_r, cur_g, cur_b); break;
            
            case '1': set_rgb(br, 0, 0); break;
            case '2': set_rgb(0, br, 0); break;
            case '3': set_rgb(0, 0, br); break;
            case '4': set_rgb(br, br, br); break;
            case '0': set_rgb(0, 0, 0); break;

            case 'r': // Rainbow
                {
                    int h = (step / 100) % 360;
                    int x = (br * (h % 60)) / 60;
                    int inv_x = br - x;
                    if (h < 60)       set_rgb(br, x, 0);
                    else if (h < 120) set_rgb(inv_x, br, 0);
                    else if (h < 180) set_rgb(0, br, x);
                    else if (h < 240) set_rgb(0, inv_x, br);
                    else if (h < 300) set_rgb(x, 0, br);
                    else              set_rgb(br, 0, inv_x);
                }
                break;
                
            case 'c' : //Cosmos
                {
                    int p = (step / 50) % 200;
                    int it = (p < 100) ? p : 200 - p;
                    
                    set_rgb((br * it) / 100, 0, br);
                }
                break;

            case 'b': // Breathing
                {
                    uint32_t angle = (step / 30) % 360;
                    int p = (angle < 180) ? (angle * (180 - angle)) / 32 : 0;
                
                    set_rgb(0, 0, (p * br) / 255);
                }
                break;

            case 'l': // Lightning
                if (step % 2000 == 0) {
                    set_rgb(255, 255, 255); soft_delay(10000);
                    set_rgb(0, 0, 0);
                } else {
                    set_rgb(br/20, br/20, br/10);
                }
                break;

            case 'x': // SOS
                {
                    uint32_t s = (step / 2000) % 20;
                    if ((s < 6 || s > 13) && (s % 2 == 0)) set_rgb(br, 0, 0);
                    else if (s >= 7 && s <= 12 && (s % 2 == 1)) set_rgb(br, 0, 0);
                    else set_rgb(0, 0, 0);
                }
                break;
        }
        soft_delay(100);
    }
}
