#include "stm32f4xx.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================
   FIX FOR SYSTICK ERROR
   ============================================================ */
volatile uint32_t msTicks = 0;

/* ============================================================
   CONFIGS
   ============================================================ */
#define LDR_THRESHOLD     2000
#define HOLD_TIME_MS      3000
#define NORMAL_BEEP_MS    200
#define FAST_BEEP_MS      50

volatile bool motion_active = false;
volatile uint16_t latest_adc = 0;
volatile bool need_oled_update = true;
volatile uint8_t pir_count = 0;

/* OLED I2C Address */
#define SSD1306_ADDR 0x3C

/* ============================================================
   FORWARD DECLARATIONS
   ============================================================ */

static void gpio_all_init(void);
static void adc1_init(void);
static uint16_t adc_read_blocking(void);
static void exti0_init(void);
static void tim2_init(void);
static void tim4_init(void);

static void tim2_start_reset(void);
static void tim4_start_reset(void);

static void i2c1_init_pb8_pb9(void);
static int  i2c1_write_bytes(uint8_t addr7, uint8_t *data, uint32_t len);

static void ssd1306_init(void);
static void ssd1306_command(uint8_t cmd);
static void ssd1306_data(uint8_t *data, uint32_t len);

static void oled_clear(void);
static void oled_set_cursor(uint8_t page, uint8_t col);
static void oled_print_adc(uint16_t val);
static void oled_print_motion_flag(bool on);

static void oled_put_digit(uint8_t digit);
static void oled_put_char_motion(char c);

static void nvic_init(void);

/* ============================================================
   FONT TABLES
   ============================================================ */

static const uint8_t font_digits[10][5] = {
    {0x7E,0x81,0x81,0x81,0x7E},
    {0x00,0x82,0xFF,0x80,0x00},
    {0xE2,0x91,0x89,0x89,0x86},
    {0x42,0x81,0x89,0x89,0x76},
    {0x18,0x14,0x12,0xFF,0x10},
    {0x4F,0x89,0x89,0x89,0x71},
    {0x7E,0x89,0x89,0x89,0x72},
    {0x01,0xE1,0x11,0x09,0x07},
    {0x76,0x89,0x89,0x89,0x76},
    {0x46,0x89,0x89,0x89,0x7E}
};

static const uint8_t font_M[5] = {0xFF,0x06,0x18,0x06,0xFF};
static const uint8_t font_O[5] = {0x7E,0x81,0x81,0x81,0x7E};
static const uint8_t font_T[5] = {0x01,0x01,0xFF,0x01,0x01};
static const uint8_t font_I[5] = {0x00,0x81,0xFF,0x81,0x00};
static const uint8_t font_N[5] = {0xFF,0x06,0x18,0x60,0xFF};

/* ============================================================
   I2C INITIALIZATION
   ============================================================ */
static void i2c1_init_pb8_pb9(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    GPIOB->MODER &= ~((3<<(8*2)) | (3<<(9*2)));
    GPIOB->MODER |=  ((2<<(8*2)) | (2<<(9*2)));

    GPIOB->OTYPER |= (1<<8) | (1<<9);
    GPIOB->OSPEEDR |= (3<<(8*2)) | (3<<(9*2));
    GPIOB->PUPDR &= ~((3<<(8*2)) | (3<<(9*2)));

    GPIOB->AFR[1] &= ~((0xF<<0) | (0xF<<4));
    GPIOB->AFR[1] |=  ((4<<0) | (4<<4));

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 = 16;
    I2C1->CCR = 80;
    I2C1->TRISE = 17;

    I2C1->CR1 |= I2C_CR1_PE;
}

static int i2c1_write_bytes(uint8_t addr7, uint8_t *data, uint32_t len)
{
    while(I2C1->SR2 & I2C_SR2_BUSY);

    I2C1->CR1 |= I2C_CR1_START;
    while(!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = addr7 << 1;
    while(!(I2C1->SR1 & I2C_SR1_ADDR));
    (void) I2C1->SR1;
    (void) I2C1->SR2;

    for(uint32_t i=0;i<len;i++)
    {
        while(!(I2C1->SR1 & I2C_SR1_TXE));
        I2C1->DR = data[i];
    }

    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

/* ============================================================
   SSD1306 COMMAND/DATA FUNCTIONS
   ============================================================ */

static void ssd1306_command(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    i2c1_write_bytes(SSD1306_ADDR, buf, 2);
}

static void ssd1306_data(uint8_t *data, uint32_t len)
{
    uint8_t temp[17];
    uint32_t sent = 0;

    while(sent < len)
    {
        uint32_t chunk = (len - sent);
        if(chunk > 16) chunk = 16;

        temp[0] = 0x40;
        for(uint32_t i=0;i<chunk;i++)
            temp[i+1] = data[sent+i];

        i2c1_write_bytes(SSD1306_ADDR, temp, chunk+1);
        sent += chunk;
    }
}

static void ssd1306_init(void)
{
    ssd1306_command(0xAE);
    ssd1306_command(0x20); ssd1306_command(0x00);
    ssd1306_command(0xB0);
    ssd1306_command(0xC8);
    ssd1306_command(0x00);
    ssd1306_command(0x10);
    ssd1306_command(0x40);
    ssd1306_command(0x81); ssd1306_command(0x7F);
    ssd1306_command(0xA1);
    ssd1306_command(0xA6);
    ssd1306_command(0xA8); ssd1306_command(0x3F);
    ssd1306_command(0xA4);
    ssd1306_command(0xD3); ssd1306_command(0x00);
    ssd1306_command(0xD5); ssd1306_command(0x80);
    ssd1306_command(0xD9); ssd1306_command(0xF1);
    ssd1306_command(0xDA); ssd1306_command(0x12);
    ssd1306_command(0xDB); ssd1306_command(0x40);
    ssd1306_command(0x8D); ssd1306_command(0x14);
    ssd1306_command(0xAF);

    oled_clear();
}

/* ============================================================
   OLED Helpers
   ============================================================ */

static void oled_clear(void)
{
    uint8_t zero[128];
    memset(zero,0,128);

    for(uint8_t p=0;p<8;p++)
    {
        ssd1306_command(0xB0|p);
        ssd1306_command(0x00);
        ssd1306_command(0x10);
        ssd1306_data(zero,128);
    }
}

static void oled_set_cursor(uint8_t page, uint8_t col)
{
    ssd1306_command(0xB0 | page);
    ssd1306_command(0x00 | (col & 0x0F));
    ssd1306_command(0x10 | (col >> 4));
}

static void oled_put_digit(uint8_t d)
{
    if(d > 9) d = 0;

    uint8_t buf[6];
    for(int i=0;i<5;i++) buf[i] = font_digits[d][i];
    buf[5] = 0;

    ssd1306_data(buf,6);
}

static void oled_put_char_motion(char c)
{
    const uint8_t *p = NULL;

    if(c=='M') p=font_M;
    else if(c=='O') p=font_O;
    else if(c=='T') p=font_T;
    else if(c=='I') p=font_I;
    else if(c=='N') p=font_N;
    else
    {
        uint8_t blank[6]={0};
        ssd1306_data(blank,6);
        return;
    }

    uint8_t buf[6];
    for(int i=0;i<5;i++) buf[i] = p[i];
    buf[5]=0;
    ssd1306_data(buf,6);
}

static void oled_print_adc(uint16_t v)
{
    char d[4]={'0','0','0','0'};
    for(int i=3;i>=0;i--)
    {
        d[i] = '0' + (v % 10);
        v /= 10;
    }

    oled_set_cursor(0,40);
    for(int i=0;i<4;i++) oled_put_digit(d[i] - '0');
}

static void oled_print_motion_flag(bool on)
{
    oled_set_cursor(1,0);

    if(on)
    {
        const char *s = "MOTION";
        for(int i=0;s[i];i++) oled_put_char_motion(s[i]);
    }
    else
    {
        uint8_t z[36];
        memset(z,0,sizeof(z));
        ssd1306_data(z,sizeof(z));
    }
}

/* ============================================================
   GPIO / ADC
   ============================================================ */

static void gpio_all_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    GPIOA->PUPDR &= ~(3<<(0*2));
    GPIOA->PUPDR |=  (2<<(0*2));

    GPIOA->MODER &= ~(3<<(5*2));
    GPIOA->MODER |= (1<<(5*2));

    GPIOA->MODER &= ~(3<<(6*2));
    GPIOA->MODER |= (1<<(6*2));

    GPIOA->MODER |= (3<<(1*2));
}

static void adc1_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    ADC1->CR2 &= ~ADC_CR2_ADON;
    ADC1->SMPR2 |= (7<<3);
    ADC1->CR2 |= ADC_CR2_ADON;
}

static uint16_t adc_read_blocking(void)
{
    ADC1->SQR3 = 1;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while(!(ADC1->SR & ADC_SR_EOC));
    return ADC1->DR;
}

/* ============================================================
   EXTI + Timers
   ============================================================ */

static void exti0_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    SYSCFG->EXTICR[0] = 0;

    EXTI->IMR |= EXTI_IMR_MR0;
    EXTI->RTSR |= EXTI_RTSR_TR0;

    NVIC_EnableIRQ(EXTI0_IRQn);
}

static void tim2_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC = 16000 - 1;
    TIM2->ARR = HOLD_TIME_MS;
    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_EnableIRQ(TIM2_IRQn);
}

static void tim4_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    TIM4->PSC = 16000 - 1;
    TIM4->ARR = NORMAL_BEEP_MS;
    TIM4->DIER |= TIM_DIER_UIE;

    NVIC_EnableIRQ(TIM4_IRQn);
}

static void tim2_start_reset(void)
{
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void tim4_start_reset(void)
{
    TIM4->CNT = 0;
    TIM4->CR1 |= TIM_CR1_CEN;
}

/* ============================================================
   NVIC INIT  (FIXED)
   ============================================================ */
static void nvic_init(void)
{
    NVIC_SetPriority(EXTI0_IRQn,2);
    NVIC_SetPriority(TIM2_IRQn,3);
    NVIC_SetPriority(TIM4_IRQn,3);

    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_EnableIRQ(TIM4_IRQn);
}

/* ============================================================
   INTERRUPT HANDLERS
   ============================================================ */

void EXTI0_IRQHandler(void)
{
    if(EXTI->PR & EXTI_PR_PR0)
    {
        EXTI->PR = EXTI_PR_PR0;

        pir_count++;

        latest_adc = adc_read_blocking();
        need_oled_update = true;

        if(latest_adc < LDR_THRESHOLD)
        {
            motion_active = true;
            GPIOA->BSRR = (1<<5);

            tim4_start_reset();
            tim2_start_reset();
        }
    }
}

void TIM4_IRQHandler(void)
{
    if(TIM4->SR & TIM_SR_UIF)
    {
        TIM4->SR &= ~TIM_SR_UIF;

        if(pir_count > 2)
            TIM4->ARR = FAST_BEEP_MS;
        else
            TIM4->ARR = NORMAL_BEEP_MS;

        GPIOA->ODR ^= (1<<6);
    }
}

void TIM2_IRQHandler(void)
{
    if(TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR &= ~TIM_SR_UIF;

        motion_active = false;
        pir_count = 0;

        GPIOA->BSRR = (1<<(5+16));
        GPIOA->BSRR = (1<<(6+16));

        TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM2->CR1 &= ~TIM_CR1_CEN;

        need_oled_update = true;
    }
}

/* ============================================================
   MAIN
   ============================================================ */

int main(void)
{
    gpio_all_init();
    adc1_init();
    i2c1_init_pb8_pb9();
    ssd1306_init();

    exti0_init();
    tim2_init();
    tim4_init();
    nvic_init();

    oled_clear();

    while(1)
    {
        if(need_oled_update)
        {
            need_oled_update = false;
            oled_print_adc(latest_adc);
            oled_print_motion_flag(motion_active);
        }
        __WFI();
    }
}
