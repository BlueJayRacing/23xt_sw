#include "startup_checks.h"
#include "main.h"

// Peripheral handles from main.c
extern CAN_HandleTypeDef hcan;
extern SPI_HandleTypeDef hspi1;
extern PCD_HandleTypeDef hpcd_USB_FS;

#define GPIO_MODE_BITS_INPUT  0U
#define GPIO_MODE_BITS_OUTPUT 1U
#define GPIO_MODE_BITS_AF     2U

static uint32_t pin_index(uint16_t pin)
{
    return (uint32_t)__builtin_ctz(pin);
}

static uint32_t pin_mode(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->MODER >> (pin_index(pin) * 2U)) & 0x3U;
}

static uint32_t pin_pull(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->PUPDR >> (pin_index(pin) * 2U)) & 0x3U;
}

static uint32_t pin_af(GPIO_TypeDef *port, uint16_t pin)
{
    uint32_t idx = pin_index(pin);
    return (port->AFR[idx >> 3U] >> ((idx & 0x7U) * 4U)) & 0xFU;
}

static int pin_is_af(GPIO_TypeDef *port, uint16_t pin, uint32_t af)
{
    return pin_mode(port, pin) == GPIO_MODE_BITS_AF && pin_af(port, pin) == af;
}

static uint32_t check_clocks(void)
{
    uint32_t fail = 0;

    if ((RCC->CR2 & RCC_CR2_HSI48RDY) == 0U ||
        __HAL_RCC_GET_SYSCLK_SOURCE() != RCC_SYSCLKSOURCE_STATUS_HSI48)
    {
        fail |= STARTUP_FAIL_SYSCLK_SRC;
    }

    if (HAL_RCC_GetSysClockFreq() != STARTUP_SYSCLK_HZ ||
        HAL_RCC_GetHCLKFreq() != STARTUP_SYSCLK_HZ ||
        HAL_RCC_GetPCLK1Freq() != STARTUP_PCLK1_HZ ||
        SystemCoreClock != STARTUP_SYSCLK_HZ)
    {
        fail |= STARTUP_FAIL_CLOCK_FREQ;
    }

    if (__HAL_FLASH_GET_LATENCY() != FLASH_LATENCY_1)
    {
        fail |= STARTUP_FAIL_FLASH_LAT;
    }

    // 1 ms is ~48k cycles, so this bound is several ms worth of spinning
    uint32_t t0 = HAL_GetTick();
    uint32_t spins = 0;
    while (HAL_GetTick() == t0 && spins < 1000000U)
    {
        spins++;
    }
    if (HAL_GetTick() == t0)
    {
        fail |= STARTUP_FAIL_SYSTICK;
    }

    return fail;
}

static uint32_t check_gpio(void)
{
    uint32_t fail = 0;

    if (!__HAL_RCC_GPIOA_IS_CLK_ENABLED() || !__HAL_RCC_GPIOB_IS_CLK_ENABLED())
    {
        return STARTUP_FAIL_CS_N | STARTUP_FAIL_CAN_STM | STARTUP_FAIL_ALERT_N_PIN;
    }

    // reads IDR as well as ODR, so a pin shorted to the wrong rail fails too
    if (pin_mode(CS_N_GPIO_Port, CS_N_Pin) != GPIO_MODE_BITS_OUTPUT ||
        (CS_N_GPIO_Port->ODR & CS_N_Pin) == 0U ||
        HAL_GPIO_ReadPin(CS_N_GPIO_Port, CS_N_Pin) != GPIO_PIN_SET)
    {
        fail |= STARTUP_FAIL_CS_N;
    }

    if (pin_mode(CAN_STM_GPIO_Port, CAN_STM_Pin) != GPIO_MODE_BITS_OUTPUT ||
        (CAN_STM_GPIO_Port->ODR & CAN_STM_Pin) != 0U ||
        HAL_GPIO_ReadPin(CAN_STM_GPIO_Port, CAN_STM_Pin) != GPIO_PIN_RESET)
    {
        fail |= STARTUP_FAIL_CAN_STM;
    }

    // pull-up is external (R8), so no internal pull expected.
    // pin level is not checked since the sensor may legitimately assert at boot
    if (pin_mode(ALERT_N_GPIO_Port, ALERT_N_Pin) != GPIO_MODE_BITS_INPUT ||
        pin_pull(ALERT_N_GPIO_Port, ALERT_N_Pin) != GPIO_NOPULL)
    {
        fail |= STARTUP_FAIL_ALERT_N_PIN;
    }

    // SYSCFG reads as 0 (= port A) when unclocked, so check the clock first
    if (!__HAL_RCC_SYSCFG_IS_CLK_ENABLED() ||
        (SYSCFG->EXTICR[0] & SYSCFG_EXTICR1_EXTI0) != SYSCFG_EXTICR1_EXTI0_PA ||
        (EXTI->IMR & ALERT_N_Pin) == 0U ||
        (EXTI->FTSR & ALERT_N_Pin) == 0U ||
        (EXTI->RTSR & ALERT_N_Pin) != 0U ||
        NVIC_GetEnableIRQ(EXTI0_1_IRQn) == 0U)
    {
        fail |= STARTUP_FAIL_ALERT_N_EXTI;
    }

    return fail;
}

static uint32_t check_spi(void)
{
    uint32_t fail = 0;

    if (!pin_is_af(SCLK_GPIO_Port, SCLK_Pin, GPIO_AF0_SPI1) ||
        !pin_is_af(MISO_GPIO_Port, MISO_Pin, GPIO_AF0_SPI1) ||
        !pin_is_af(MOSI_GPIO_Port, MOSI_Pin, GPIO_AF0_SPI1))
    {
        fail |= STARTUP_FAIL_SPI_PINS;
    }

    if (!__HAL_RCC_SPI1_IS_CLK_ENABLED() || hspi1.State != HAL_SPI_STATE_READY)
    {
        return fail | STARTUP_FAIL_SPI_CONFIG | STARTUP_FAIL_SPI_SPEED;
    }

    uint32_t cr1 = SPI1->CR1;
    uint32_t cr2 = SPI1->CR2;
    if ((cr1 & SPI_CR1_MSTR) == 0U ||
        (cr1 & SPI_CR1_CPOL) != 0U ||       // CPOL = 0
        (cr1 & SPI_CR1_CPHA) == 0U ||       // CPHA = 1 (second edge)
        (cr1 & SPI_CR1_LSBFIRST) != 0U ||
        (cr1 & SPI_CR1_BIDIMODE) != 0U ||
        (cr1 & (SPI_CR1_SSM | SPI_CR1_SSI)) != (SPI_CR1_SSM | SPI_CR1_SSI) ||
        (cr2 & SPI_CR2_DS) != SPI_DATASIZE_8BIT)
    {
        fail |= STARTUP_FAIL_SPI_CONFIG;
    }

    uint32_t br = (cr1 & SPI_CR1_BR) >> SPI_CR1_BR_Pos;
    uint32_t sck = HAL_RCC_GetPCLK1Freq() >> (br + 1U);
    if (sck != STARTUP_SPI_SCK_HZ)
    {
        fail |= STARTUP_FAIL_SPI_SPEED;
    }

    return fail;
}

static uint32_t check_can(void)
{
    uint32_t fail = 0;

    if (!pin_is_af(GPIOB, GPIO_PIN_8, GPIO_AF4_CAN) ||
        !pin_is_af(GPIOB, GPIO_PIN_9, GPIO_AF4_CAN))
    {
        fail |= STARTUP_FAIL_CAN_PINS;
    }

    // __HAL_RCC_CAN1_IS_CLK_ENABLED() is broken on F072 (uses RCC_APB1ENR_CAN1EN, which isn't defined)
    if ((RCC->APB1ENR & RCC_APB1ENR_CANEN) == 0U || hcan.State != HAL_CAN_STATE_READY)
    {
        return fail | STARTUP_FAIL_CAN_CONFIG | STARTUP_FAIL_CAN_BAUD;
    }

    // HAL_CAN_Init leaves the peripheral in init mode until HAL_CAN_Start()
    if ((CAN->MSR & CAN_MSR_INAK) == 0U)
    {
        fail |= STARTUP_FAIL_CAN_CONFIG;
    }

    // bit time = (1 sync + TS1 + TS2) quanta, register fields are value - 1
    uint32_t btr = CAN->BTR;
    uint32_t brp = ((btr & CAN_BTR_BRP) >> CAN_BTR_BRP_Pos) + 1U;
    uint32_t ts1 = ((btr & CAN_BTR_TS1) >> CAN_BTR_TS1_Pos) + 1U;
    uint32_t ts2 = ((btr & CAN_BTR_TS2) >> CAN_BTR_TS2_Pos) + 1U;
    uint32_t quanta = 1U + ts1 + ts2;
    if (HAL_RCC_GetPCLK1Freq() != STARTUP_CAN_BAUD * brp * quanta)
    {
        fail |= STARTUP_FAIL_CAN_BAUD;
    }

    return fail;
}

static uint32_t check_usb(void)
{
    if (!__HAL_RCC_USB_IS_CLK_ENABLED() ||
        __HAL_RCC_GET_USB_SOURCE() != RCC_USBCLKSOURCE_HSI48 ||
        hpcd_USB_FS.State != HAL_PCD_STATE_READY)
    {
        return STARTUP_FAIL_USB;
    }
    return 0;
}

uint32_t startup_checks_run(void)
{
    uint32_t fail = 0;
    fail |= check_clocks();
    fail |= check_gpio();
    fail |= check_spi();
    fail |= check_can();
    fail |= check_usb();
    return fail;
}
