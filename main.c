#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/pwr.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#define MAX_LINELEN 32


static inline void
log_msg(const char *s) {
    while(*s)
        usart_send_blocking(USART2, *s++);

    usart_send_blocking(USART2, '\n');
    usart_send_blocking(USART2, '\r');
}


void hard_fault_handler(void)
{
    log_msg("----big fat libopencm3 crash -----");
    while(true);
}


static void
uart_setup(void) {

    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_USART2);

    gpio_mode_setup( GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO2 | GPIO3 );
    gpio_set_af( GPIOA, GPIO_AF7, GPIO2 | GPIO3 );

    usart_set_baudrate( USART2, 102400 ); // 115200 / (72MHZ / 64MHZ)
    usart_set_databits( USART2, 8 );
    usart_set_stopbits( USART2, USART_CR2_STOPBITS_1 );
    usart_set_mode( USART2, USART_MODE_TX );
    usart_set_parity( USART2, USART_PARITY_NONE );
    usart_set_flow_control( USART2, USART_FLOWCONTROL_NONE );

    usart_enable(USART2);
}

static unsigned ticks = 0;

#define TICKS_PER_SECOND 1


void sys_tick_handler(void)
{
    log_msg("tick");

    if (ticks == 1000)
    {
        log_msg("@");
        ticks = 0;
    }

    ticks++;
    gpio_toggle(GPIOA, GPIO5);
}


static void systick_setup()
{
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    STK_CVR = 0;

    int xms = 1000 / TICKS_PER_SECOND;

    systick_set_reload(rcc_ahb_frequency / 8 / 1000 * xms);
    systick_counter_enable();
    systick_interrupt_enable();
}


int main(void) {
    //rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_HSI_64MHZ]);
    rcc_clock_setup_pll(&rcc_hse8mhz_configs[RCC_CLOCK_HSE8_72MHZ]);
    uart_setup();
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO5);
    log_msg("----start----");
    systick_setup();

    while(true);

    return 0;
}

