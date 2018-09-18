#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/rtc.h>
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
    gpio_set_af( GPIOA, GPIO_AF1, GPIO2 | GPIO3 );
    gpio_set_output_options( GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO2 | GPIO3 );

    usart_set_baudrate( USART2, 115200 );
    usart_set_databits( USART2, 8 );
    usart_set_stopbits( USART2, USART_CR2_STOPBITS_1 );
    usart_set_mode( USART2, USART_MODE_TX_RX );
    usart_set_parity( USART2, USART_PARITY_NONE );
    usart_set_flow_control( USART2, USART_FLOWCONTROL_NONE );

    nvic_enable_irq(NVIC_USART2_IRQ);
    nvic_set_priority(NVIC_USART2_IRQ, 2);
    usart_enable(USART2);
    usart_enable_rx_interrupt(USART2);
}


static void encode_datetime(const struct tm* now, uint32_t * time_val, uint32_t * date_val)
{
    (*time_val)  = (((now->tm_hour / 10) & RTC_TR_HT_MASK)  << RTC_TR_HT_SHIFT)
                 | (((now->tm_hour % 10) & RTC_TR_HU_MASK)  << RTC_TR_HU_SHIFT)
                 | (((now->tm_min  / 10) & RTC_TR_MNT_MASK) << RTC_TR_MNT_SHIFT)
                 | (((now->tm_min  % 10) & RTC_TR_MNU_MASK) << RTC_TR_MNU_SHIFT)
                 | (((now->tm_sec  / 10) & RTC_TR_ST_MASK)  << RTC_TR_ST_SHIFT)
                 | (((now->tm_sec  % 10) & RTC_TR_SU_MASK)  << RTC_TR_SU_SHIFT);

    (*date_val) = (((now->tm_year / 10) & RTC_DR_YT_MASK)  << RTC_DR_YT_SHIFT)
                | (((now->tm_year % 10) & RTC_DR_YU_MASK)  << RTC_DR_YU_SHIFT)
                | (((now->tm_mon / 10)  & 0x01          )  << RTC_DR_MT_SHIFT)
                | (((now->tm_mon % 10)  & RTC_DR_MU_MASK)  << RTC_DR_MU_SHIFT)
                | (((now->tm_mday / 10) & RTC_DR_DT_MASK)  << RTC_DR_DT_SHIFT)
                | (((now->tm_mday % 10) & RTC_DR_DU_MASK)  << RTC_DR_DU_SHIFT)
                | (((now->tm_wday % 10) & RTC_DR_WDU_MASK) << RTC_DR_WDU_SHIFT);
}


static void decode_datetime(const uint32_t t, const uint32_t d, struct tm* result)
{
    result->tm_hour = (((t >> RTC_TR_HT_SHIFT) & RTC_TR_HT_MASK) * 10)
                     + ((t >> RTC_TR_HU_SHIFT) & RTC_TR_HU_MASK);

    result->tm_min = (((t >> RTC_TR_MNT_SHIFT) & RTC_TR_MNT_MASK) * 10)
                    + ((t >> RTC_TR_MNU_SHIFT) & RTC_TR_MNU_MASK);

    result->tm_sec = (((t >> RTC_TR_ST_SHIFT) & RTC_TR_ST_MASK) * 10)
                    + ((t >> RTC_TR_SU_SHIFT) & RTC_TR_SU_MASK);


    result->tm_year = (((d >> RTC_DR_YT_SHIFT) & RTC_DR_YT_MASK) * 10)
                     + ((d >> RTC_DR_YU_SHIFT) & RTC_DR_YU_MASK);

    result->tm_mon = (((d >> RTC_DR_MT_SHIFT) & 0x01) * 10)
                    + ((d >> RTC_DR_MU_SHIFT) & RTC_DR_MU_MASK);

    result->tm_mday = (((d >> RTC_DR_DT_SHIFT) & RTC_DR_DT_MASK) * 10)
                     + ((d >> RTC_DR_DU_SHIFT) & RTC_DR_DU_MASK);

    result->tm_wday = (((d >> RTC_DR_WDU_SHIFT) & 0x07 ) % 7) ;
}


static void get_example_stamp(struct tm* now)
{
    now->tm_year = 18; /* Year - 1900 */
    now->tm_mon = 9 - 1; /* Month (0-11) */
    now->tm_mday = 17;   /* Day of the month (1-31) */
    now->tm_wday = 1;    /* Day of the week (0-6, Sunday = 0) */
    now->tm_hour = 19;  /* Seconds (0-60) */
    now->tm_min = 30;   /* Minutes (0-59) */
    now->tm_sec = 15;    /* Seconds (0-60) */
}


static void rtc_setup()
{
    rcc_periph_clock_enable(RCC_PWR);
    rcc_periph_clock_enable(RCC_RTC);

    /* Enable RTC to change */
    pwr_disable_backup_domain_write_protect();
    rcc_periph_reset_pulse(RST_BACKUPDOMAIN); // Wipe RTC settings

    rcc_osc_on(RCC_LSE);
    rcc_wait_for_osc_ready(RCC_LSE);
    rcc_set_rtc_clock_source(RCC_LSE);

    rcc_enable_rtc_clock();

    /* Step 1 : Disable the RTC registers write protection */
    rtc_unlock();

    /* Step 2 : Enter Initialization mode */
    RTC_ISR |= RTC_ISR_INIT;
    /* Step 3 : Wait for the confirmation of Initialization mode */
    RTC_ISR &= ~RTC_ISR_INITF;
    while(!(RTC_ISR & RTC_ISR_INITF));
    /* Step 4 : Program the prescaler values */
    rtc_set_prescaler(255, 127);
    rtc_set_prescaler(255, 127);


    /* Step 5 : Load time and date values in the shadow registers */
    struct tm test_time;
    get_example_stamp(&test_time);
    uint32_t time_reg, date_reg;
    encode_datetime(&test_time, &time_reg, &date_reg);
    RTC_TR = time_reg;
    RTC_DR = date_reg;

    /* Step 6 : Configure the time format */
    RTC_CR &= ~RTC_CR_FMT; // 24 clock

    /* Step 7 : Exit Initialization mode */
    RTC_ISR &= ~RTC_ISR_INIT;

    /* Step 8 : Enable the RTC Registers Write Protection */
    rtc_lock();

    rtc_wait_for_synchro();
    pwr_enable_backup_domain_write_protect();
}


static unsigned seconds = 0;
static unsigned ticks = 0;

#define TICKS_PER_SECOND 10


void sys_tick_handler(void) {

    char buffer[64];
    uint32_t time_reg = RTC_TR;
    uint32_t date_reg = RTC_DR;
    snprintf(buffer, sizeof(buffer), "%"PRIu32" %"PRIu32" %"PRIu32, date_reg, time_reg, RTC_SSR);
    log_msg(buffer);

    struct tm date;
    decode_datetime(time_reg, date_reg, &date);
    snprintf(buffer, sizeof(buffer), "RTC %i/%i/%i (%i) %i:%i:%i\n",
        date.tm_year,
        date.tm_mon+1,
        date.tm_mday,
        date.tm_wday,
        date.tm_hour,
        date.tm_min,
        date.tm_sec);
    log_msg(buffer);

    if (!(ticks % TICKS_PER_SECOND)) {
        snprintf(buffer, sizeof(buffer), "--second-- %u", seconds);
        log_msg(buffer);
        seconds++;
        ticks = 0;
    }
    ticks++;

}


static void systick_setup()
{
    systick_set_clocksource(STK_CSR_CLKSOURCE_EXT);
    STK_CVR = 0;

    int xms = 1000 / TICKS_PER_SECOND;

    systick_set_reload(rcc_ahb_frequency / 8 / 1000 * xms);
    systick_counter_enable();
}


int main(void) {
    rcc_clock_setup_in_hsi_out_48mhz();
    uart_setup();
    rtc_setup();
    rcc_periph_clock_enable(RCC_GPIOA);
    systick_setup();

    log_msg("----start----");
    systick_interrupt_enable();

    while(true);

    return 0;
}

