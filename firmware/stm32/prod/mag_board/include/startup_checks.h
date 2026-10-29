#pragma once
#ifndef _STARTUP_CHECKS_H_
#define _STARTUP_CHECKS_H_

#include <stdint.h>

// Expected configuration (must match mag_board.ioc)
#define STARTUP_SYSCLK_HZ   48000000U
#define STARTUP_PCLK1_HZ    48000000U
#define STARTUP_SPI_SCK_HZ  6000000U
#define STARTUP_CAN_BAUD    125000U

// One bit per check, set in the returned mask if that check failed
typedef enum
{
    STARTUP_FAIL_SYSCLK_SRC   = 1U << 0,  // SYSCLK not running from HSI48
    STARTUP_FAIL_CLOCK_FREQ   = 1U << 1,  // SYSCLK/HCLK/PCLK1/SystemCoreClock not 48 MHz
    STARTUP_FAIL_FLASH_LAT    = 1U << 2,  // flash latency not 1 wait state (required above 24 MHz)
    STARTUP_FAIL_SYSTICK      = 1U << 3,  // HAL tick not advancing
    STARTUP_FAIL_CS_N         = 1U << 4,  // PA15 not output or not idling high
    STARTUP_FAIL_CAN_STM      = 1U << 5,  // PB1 not output or not idling low
    STARTUP_FAIL_ALERT_N_PIN  = 1U << 6,  // PA0 not input / no pull
    STARTUP_FAIL_ALERT_N_EXTI = 1U << 7,  // EXTI0 not routed to PA0, falling edge, unmasked, NVIC enabled
    STARTUP_FAIL_SPI_PINS     = 1U << 8,  // PA5/6/7 not AF0 (SPI1)
    STARTUP_FAIL_SPI_CONFIG   = 1U << 9,  // SPI1 not master, mode 0, 8 bit, MSB first, soft NSS
    STARTUP_FAIL_SPI_SPEED    = 1U << 10, // SCK not 6 MHz
    STARTUP_FAIL_CAN_PINS     = 1U << 11, // PB8/9 not AF4 (CAN)
    STARTUP_FAIL_CAN_CONFIG   = 1U << 12, // CAN handle not ready / not in init mode
    STARTUP_FAIL_CAN_BAUD     = 1U << 13, // CAN bit timing not 125 kbit/s
    STARTUP_FAIL_USB          = 1U << 14, // USB not clocked from HSI48 or handle not ready
} startup_fail_t;

// Run after all MX_*_Init() calls. Returns 0 if everything passed,
// otherwise an OR of startup_fail_t bits.
uint32_t startup_checks_run(void);

#endif
