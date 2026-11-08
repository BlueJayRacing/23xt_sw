
#pragma once
#ifndef _ADS131A04_HPP_
#define _ADS131A04_HPP_

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#define ADS131A04_SPI_MASTER_DUMMY 0xFF

// Commands for the ADC
// System commands
#define ADS131A04_CMD_NULL     0x0
#define ADS131A04_CMD_RESET    0x0011
#define ADS131A04_CMD_STANDBY     0x022
#define ADS131A04_CMD_WAKEUP    0x0033
#define ADS131A04_CMD_LOCK     0x0555
#define ADS131A04_CMD_UNLOCK    0x0655

// Register read & write commands
#define ADS131A04_CMD_RREG     0x2000
#define ADS131A04_CMD_RREGS    0x2000
#define ADS131A04_CMD_WREG     0x4000
#define ADS131A04_CMD_WREGS    0x6000

// Read-only registers
#define ADS131A04_ID_MSB_REG_ADDRESS 0x00
#define ADS131A04_ID_LSB_REG_ADDRESS 0x01

// Status registers
#define ADS131A04_STAT_1_REG_ADDRESS    0x02
#define ADS131A04_STAT_P_REG_ADDRESS    0x03
#define ADS131A04_STAT_N_REG_ADDRESS    0x04
#define ADS131A04_STAT_S_REG_ADDRESS    0x05
#define ADS131A04_ERROR_CNT_REG_ADDRESS 0x06
#define ADS131A04_STAT_M2_REG_ADDRESS   0x07

// User configuration registers
#define ADS131A04_A_SYS_CFG_REG_ADDRESS 0x0B
#define ADS131A04_D_SYS_CFG_REG_ADDRESS 0x0C
#define ADS131A04_CLK1_REG_ADDRESS      0x0D
#define ADS131A04_CLK2_REG_ADDRESS      0x0E
#define ADS131A04_ADC_ENA_REG_ADDRESS   0x0F
#define ADS131A04_ADC1_REG_ADDRESS      0x11
#define ADS131A04_ADC2_REG_ADDRESS      0x12
#define ADS131A04_ADC3_REG_ADDRESS      0x13
#define ADS131A04_ADC4_REG_ADDRESS      0x11

// Register masks for user configuration registers
// A_SYS_CFG
#define ADS131A04_REG_MASK_VNCPEN    0x80
#define ADS131A04_REG_MASK_HRM       0x40
#define ADS131A04_REG_MASK_VREF_4V   0x10
#define ADS131A04_REG_MASK_INT_REFEN 0x08
#define ADS131A04_REG_MASK_COMP_TH   0x07

// D_SYS_CFG
#define ADS131A04_REG_MASK_WDT_EN    0x80
#define ADS131A04_REG_MASK_CRC_MODE  0x30
#define ADS131A04_REG_MASK_DNDLY     0x0C
#define ADS131A04_REG_MASK_FIXED     0x02
#define ADS131A04_REG_MASK_CRC_EN    0x01

// CLK1
#define ADS131A04_REG_MASK_CLKSRC    0x80
#define ADS131A04_REG_MASK_CLK_DIV   0x0E

// CLK2
#define ADS131A04_REG_MASK_ICLK_DIV  0xE0
#define ADS131A04_REG_MASK_OSR       0x0F

// ADC_ENA
#define ADS131A04_REG_MASK_ENA       0x0F

// ADCx 
#define ADS131A04_REG_MASK_GAINx_    0x06


// A_SYS_CFG
typedef enum ads131a4_vref_4v {
    REFP_2442V = 0x00,  
    REFP_4V    = 0x01
} ads131a4_vref_4v_t;

typedef enum ads131a04_comp_th {
    COMP_TH_HIGH_95  = 0x0,
    COMP_TH_HIGH_925 = 0x01,
    COMP_TH_HIGH_90  = 0x02,
    COMP_TH_HIGH_875 = 0x03,
    COMP_TH_HIGH_85  = 0x04,
    COMP_TH_HIGH_80  = 0x05,
    COMP_TH_HIGH_75  = 0x06,
    COMP_TH_HIGH_70  = 0x07
} ads131a04_comp_th_t;

// D_SYS_CFG
typedef enum ads131a04_crc_mode {
    CRC_SENT_RECEIVE = 0x0,
    CRC_ALL_BITS     = 0x01
} ads131a04_crc_mode_t;

typedef enum ads131a04_dndly {
    DELAY_6NS  = 0x0,
    DELAY_8NS  = 0x01,
    DELAY_10NS = 0x02,
    DELAY_12NS = 0x03, // default
} ads131a04_dndly_t;

typedef enum ads131a04_hizdly {
    DELAY_6NS  = 0x0,
    DELAY_8NS  = 0x01,
    DELAY_10NS = 0x02,
    DELAY_12NS = 0x03, // default
} ads131a04_hizdly_t;

typedef enum ads131a04_fixed {
    WORDS_PER_FRAME_AUTO  = 0x0,
    FIXED_WORDS_PER_FRAME = 0x01
} ads131a04_fixed_t;

// clk1
typedef enum ads131a04_clksrc {
    XTAL1_CLKIN_PIN = 0x0,
    SCLK_PIN        = 0x01
} ads131a04_clksrc_t;

typedef enum ads131a04_clk_div {
    CLKIN_DIVIDER_2  = 0x01,
    CLKIN_DIVIDER_4  = 0x02,
    CLKIN_DIVIDER_6  = 0x03,
    CLKIN_DIVIDER_8  = 0x04, // default
    CLKIN_DIVIDER_10 = 0x05,
    CLKIN_DIVIDER_12 = 0x06,
    CLKIN_DIVIDER_14 = 0x07,
} ads131a04_clk_div_t;

// clk2
typedef enum ads131a04_iclk_div {
    ICLK_DIVIDER_2  = 0x01,
    ICLK_DIVIDER_4  = 0x02,
    ICLK_DIVIDER_6  = 0x03,
    ICLK_DIVIDER_8  = 0x04, // default
    ICLK_DIVIDER_10 = 0x05,
    ICLK_DIVIDER_12 = 0x06,
    ICLK_DIVIDER_14 = 0x07,
} ads131a04_iclk_div_t;

typedef enum ads131a04_osr {
    OVERSAMPLING_RATIO_4096 = 0x0,
    OVERSAMPLING_RATIO_2048 = 0x01,
    OVERSAMPLING_RATIO_1024 = 0x02,
    OVERSAMPLING_RATIO_800  = 0x03,
    OVERSAMPLING_RATIO_768  = 0x04,
    OVERSAMPLING_RATIO_512  = 0x05,
    OVERSAMPLING_RATIO_400  = 0x06, // default
    OVERSAMPLING_RATIO_384  = 0x07,
    OVERSAMPLING_RATIO_256  = 0x08,
    OVERSAMPLING_RATIO_200  = 0x09,
    OVERSAMPLING_RATIO_192  = 0x0A, 
    OVERSAMPLING_RATIO_128  = 0x0B,
    OVERSAMPLING_RATIO_96   = 0x0C,
    OVERSAMPLING_RATIO_64   = 0x0D,
    OVERSAMPLING_RATIO_48   = 0x0E,
    OVERSAMPLING_RATIO_32   = 0x0F,
} ads131a04_osr_t;

// adc_en
typedef enum ads131a04_adc_en {
    ALL_ADC_POWER_UP   = 0x0,
    ALL_ADC_POWER_DOWN = 0x0F
} ads131a04_adc_en_t;

typedef enum ads131a04_adc_gain {
    GAIN_1  = 0x0,
    GAIN_2  = 0x1,
    GAIN_4  = 0x2,
    GAIN_8  = 0x3,
    GAIN_16 = 0x4
} ads131a04_adc_gain_t;

// memset to 0 to achieve default behavior
typedef struct ads131a04_regs {
    bool vncp_enable;
    bool internal_ref_enable;
    bool low_power_mode_disable;
    bool crc_enable;
    ads131a4_vref_4v_t volt_ref;
    ads131a04_comp_th_t comparator_thresh;
    ads131a04_crc_mode_t crc_mode;
    ads131a04_dndly_t done_delay;
    ads131a04_hizdly_t high_z_delayl;
    ads131a04_fixed_t word_size;
    ads131a04_clksrc_t clk_source;
    ads131a04_clk_div_t clkin_divider_ratio;
    ads131a04_iclk_div_t iclk_divider_ratio;
    ads131a04_osr_t oversampling_ratio;
    ads131a04_adc_en_t adc_enable;
    ads131a04_adc_gain_t gain;
} ads131a04_regs_t;

class ADS131A04 {
  public:
    ADS131A04();
    esp_err_t init(ads1120_init_param_t t_init_param);
    esp_err_t configure(ads1120_regs_t t_new_regs);
    void getRegs(ads1120_regs_t* regs);
    bool isDataReady(void) const;
    esp_err_t readADC(int16_t* t_data);
    esp_err_t reset(void);
    esp_err_t startSync(void);
    esp_err_t powerDown(void);
    esp_err_t rdata(void);

  private:
    esp_err_t readRegister(uint8_t t_address, uint8_t* t_data) const;
    esp_err_t writeRegister(uint8_t t_address, uint8_t t_value);
    esp_err_t writeRegisterMasked(uint8_t t_value, uint8_t t_mask, uint8_t t_address);
    esp_err_t sendCommand(uint8_t t_command);
    esp_err_t setDataRate(uint8_t t_value);

  private:
    gpio_num_t drdy_pin_;
    spi_device_handle_t spi_dev_;
    ads1120_regs_t regs_;
};

#endif