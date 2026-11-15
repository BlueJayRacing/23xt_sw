

#include "esp_log.h"

#include "ads131a04.hpp"

static const char* TAG = "ads131a04";

#define ADS_SPI_LOCK_TIMEOUT       10
#define ADS_SPI_CLOCK_SPEED_HZ     4 * 1000 * 1000
#define ADS_SPI_MODE               1
#define ADS_CS_EN_PRE_WAIT_CYCLES  2
#define ADS_CS_EN_POST_WAIT_CYCLES 0
#define ADS_SPI_INPUT_DELAY_NS     0


ADS131A04::ADS131A04() { memset(&regs_, 0x00, sizeof(ads131a04_regs_t)); }

/*******************************************************************************
 * @brief Initializes the ADS131A04.
 *
 * @param t_init_param - The initialization parameters for the ADS131A04.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::init(ads131a04_init_param_t t_init_param)
{
    // Set pins up
    drdy_pin_ = t_init_param.drdy_pin;

    gpio_set_direction(drdy_pin_, GPIO_MODE_INPUT);

    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(spi_device_interface_config_t));

    devcfg.clock_source     = SPI_CLK_SRC_DEFAULT;
    devcfg.mode             = ADS_SPI_MODE;
    devcfg.cs_ena_pretrans  = ADS_CS_EN_PRE_WAIT_CYCLES;
    devcfg.cs_ena_posttrans = ADS_CS_EN_POST_WAIT_CYCLES;
    devcfg.clock_speed_hz   = ADS_SPI_CLOCK_SPEED_HZ;
    devcfg.input_delay_ns   = ADS_SPI_INPUT_DELAY_NS;
    devcfg.spics_io_num     = t_init_param.cs_pin; // CS pin
    devcfg.flags            = 0;
    devcfg.queue_size       = 1;

    esp_err_t ret = spi_bus_add_device(t_init_param.spi_host, &devcfg, &spi_dev_);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(5);

    ret = reset();
    if (ret != ESP_OK) return ret;

    // Monitor READY signal
    while (!isAdcReady()) {
        vTaskDelay(5);
    }

    ret = unlock();
    if (ret != ESP_OK) return ret;

    vTaskDelay(5);

    ret = writeRegisterMasked(0x0F, ADS131A04_REG_MASK_ENA, ADS131A04_ADC_ENA_REG_ADDRESS);
    if (ret != ESP_OK) return ret;

    vTaskDelay(5);

    return wakeup();
}

/*******************************************************************************
 * @brief Configures the ADS131A04 with the proper registers. If a new register
 *        is equivalent to an old register, no spi transaction occurs.
 *
 * @param t_nregs   - The new registers to write to the ADC.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::configure(ads131a04_regs_t t_nregs)
{
    esp_err_t ret;

    ret = unlock();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unlock ADC");
        return ret;
    }

    // A_SYS_CFG
    if (t_nregs.vncp_enable != regs_.vncp_enable) {
        ret = writeRegisterMasked(t_nregs.vncp_enable << 7, ADS131A04_REG_MASK_VNCPEN, ADS131A04_A_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable negative charge pump");
            return ret;
        }

        ESP_LOGV(TAG, "Set negative charge pump w/: %d", t_nregs.vncp_enable);
        regs_.vncp_enable = t_nregs.vncp_enable;
    }

    if (t_nregs.hrm_enable != regs_.hrm_enable) {
        ret = writeRegisterMasked(t_nregs.hrm_enable << 6, ADS131A04_REG_MASK_HRM, ADS131A04_A_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable high resolution mode");
            return ret;
        }

        ESP_LOGV(TAG, "Set high resolution mode w/: %d", t_nregs.hrm_enable);
        regs_.hrm_enable = t_nregs.hrm_enable;
    }    

    if (t_nregs.volt_ref != regs_.volt_ref) {
        ret = writeRegisterMasked(t_nregs.volt_ref << 4, ADS131A04_REG_MASK_VREF_4V, ADS131A04_A_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set voltage reference");
            return ret;
        }

        ESP_LOGV(TAG, "Set voltage reference w/: %d", t_nregs.volt_ref);
        regs_.volt_ref = t_nregs.volt_ref;
    }

    if (t_nregs.internal_ref_enable != regs_.internal_ref_enable) {
        ret = writeRegisterMasked(t_nregs.internal_ref_enable << 3, ADS131A04_REG_MASK_INT_REFEN, ADS131A04_A_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable internal reference");
            return ret;
        }

        ESP_LOGV(TAG, "Set internal reference w/: %d", t_nregs.internal_ref_enable);
        regs_.internal_ref_enable = t_nregs.internal_ref_enable;
    }    

    if (t_nregs.comparator_thresh != regs_.comparator_thresh) {
        ret = writeRegisterMasked(t_nregs.comparator_thresh, ADS131A04_REG_MASK_COMP_TH, ADS131A04_A_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set comp_th (comparator threshold)");
            return ret;
        }

        ESP_LOGV(TAG, "Set comparator threshold w/: %d", t_nregs.comparator_thresh);
        regs_.comparator_thresh = t_nregs.comparator_thresh;
    }

    // D_SYS_CFG
    if (t_nregs.wdt_enable != regs_.wdt_enable) {
        ret = writeRegisterMasked(t_nregs.wdt_enable << 7, ADS131A04_REG_MASK_WDT_EN, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable watchdog timer");
            return ret;
        }

        ESP_LOGV(TAG, "Set watchdog timer w/: %d", t_nregs.wdt_enable);
        regs_.wdt_enable = t_nregs.wdt_enable;
    }    

    if (t_nregs.crc_mode != regs_.crc_mode) {
        ret = writeRegisterMasked(t_nregs.crc_mode << 6, ADS131A04_REG_MASK_CRC_MODE, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set CRC mode");
            return ret;
        }

        ESP_LOGV(TAG, "Set CRC mode w/: %d", t_nregs.crc_mode);
        regs_.crc_mode = t_nregs.crc_mode;
    }

    if (t_nregs.done_delay != regs_.done_delay) {
        ret = writeRegisterMasked(t_nregs.done_delay << 4, ADS131A04_REG_MASK_DNDLY, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set DONE delay");
            return ret;
        }

        ESP_LOGV(TAG, "Set DONE delay w/: %d", t_nregs.done_delay);
        regs_.done_delay = t_nregs.done_delay;
    }

    if (t_nregs.hi_z_delay != regs_.hi_z_delay) {
        ret = writeRegisterMasked(t_nregs.hi_z_delay << 2, ADS131A04_REG_MASK_HIZDLY, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set Hi-Z delay");
            return ret;
        }

        ESP_LOGV(TAG, "Set Hi-Z delay w/: %d", t_nregs.hi_z_delay);
        regs_.hi_z_delay = t_nregs.hi_z_delay;
    }

    if (t_nregs.word_size != regs_.word_size) {
        ret = writeRegisterMasked(t_nregs.word_size << 1, ADS131A04_REG_MASK_FIXED, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set fixed word size");
            return ret;
        }

        ESP_LOGV(TAG, "Set fixed word size w/: %d", t_nregs.word_size);
        regs_.word_size = t_nregs.word_size;
    }

    if (t_nregs.crc_enable != regs_.crc_enable) {
        ret = writeRegisterMasked(t_nregs.crc_enable, ADS131A04_REG_MASK_CRC_EN, ADS131A04_D_SYS_CFG_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable CRC mode");
            return ret;
        }

        ESP_LOGV(TAG, "Set CRC mode w/: %d", t_nregs.crc_enable);
        regs_.crc_enable = t_nregs.crc_enable;
    }    

    // CLK 1
    if (t_nregs.clk_source != regs_.clk_source) {
        ret = writeRegisterMasked(t_nregs.clk_source << 7, ADS131A04_REG_MASK_CLKSRC, ADS131A04_CLK1_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ADC clock source");
            return ret;
        }

        ESP_LOGV(TAG, "Set ADC clock source w/: %d", t_nregs.clk_source);
        regs_.clk_source = t_nregs.clk_source;
    }

    if (t_nregs.clkin_divider_ratio != regs_.clkin_divider_ratio) {
        ret = writeRegisterMasked(t_nregs.clkin_divider_ratio << 1, ADS131A04_REG_MASK_CLK_DIV, ADS131A04_CLK1_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set CLKIN divider ratio");
            return ret;
        }

        ESP_LOGV(TAG, "Set CLKIN divider ratio w/: %d", t_nregs.clkin_divider_ratio);
        regs_.clkin_divider_ratio = t_nregs.clkin_divider_ratio;
    }

    // CLK 2
    if (t_nregs.iclk_divider_ratio != regs_.iclk_divider_ratio) {
        ret = writeRegisterMasked(t_nregs.iclk_divider_ratio << 5, ADS131A04_REG_MASK_ICLK_DIV, ADS131A04_CLK2_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set ICLK divider ratio");
            return ret;
        }

        ESP_LOGV(TAG, "Set ICLK divider ratio w/: %d", t_nregs.iclk_divider_ratio);
        regs_.iclk_divider_ratio = t_nregs.iclk_divider_ratio;
    }

    if (t_nregs.oversampling_ratio != regs_.oversampling_ratio) {
        ret = writeRegisterMasked(t_nregs.oversampling_ratio, ADS131A04_REG_MASK_OSR, ADS131A04_CLK2_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set oversampling ratio");
            return ret;
        }

        ESP_LOGV(TAG, "Set oversampling ratio w/: %d", t_nregs.oversampling_ratio);
        regs_.oversampling_ratio = t_nregs.oversampling_ratio;
    }

    // ADC_ENA
    if (t_nregs.adc_enable != regs_.adc_enable) {
        ret = writeRegisterMasked(t_nregs.adc_enable, ADS131A04_REG_MASK_ENA, ADS131A04_ADC_ENA_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable/disable ADC channels");
            return ret;
        }

        ESP_LOGV(TAG, "Enabled/disabled ADC channels w/: %d", t_nregs.adc_enable);
        regs_.adc_enable = t_nregs.adc_enable;
    }

    // ADC channels
    if (t_nregs.gain_1 != regs_.gain_1) {
        ret = writeRegisterMasked(t_nregs.gain_1, ADS131A04_REG_MASK_GAINx, ADS131A04_ADC1_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set gain on ADC1");
            return ret;
        }

        ESP_LOGV(TAG, "Set gain on ADC1 w/: %d", t_nregs.gain_1);
        regs_.gain_1 = t_nregs.gain_1;
    }

    if (t_nregs.gain_2 != regs_.gain_2) {
        ret = writeRegisterMasked(t_nregs.gain_2, ADS131A04_REG_MASK_GAINx, ADS131A04_ADC2_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set gain on ADC2");
            return ret;
        }

        ESP_LOGV(TAG, "Set gain on ADC2 w/: %d", t_nregs.gain_2);
        regs_.gain_2 = t_nregs.gain_2;
    }

    if (t_nregs.gain_3 != regs_.gain_3) {
        ret = writeRegisterMasked(t_nregs.gain_3, ADS131A04_REG_MASK_GAINx, ADS131A04_ADC3_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set gain on ADC3");
            return ret;
        }

        ESP_LOGV(TAG, "Set gain on ADC3 w/: %d", t_nregs.gain_3);
        regs_.gain_3 = t_nregs.gain_3;
    }

    if (t_nregs.gain_4 != regs_.gain_4) {
        ret = writeRegisterMasked(t_nregs.gain_4, ADS131A04_REG_MASK_GAINx, ADS131A04_ADC4_REG_ADDRESS);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set gain on ADC4");
            return ret;
        }

        ESP_LOGV(TAG, "Set gain on ADC4 w/: %d", t_nregs.gain_4);
        regs_.gain_4 = t_nregs.gain_4;
    }

    // Lock ADC according to initialization flowchart, but doesn't seem necessary
    ret = lock();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to lock ADC");
        return ret;
    }

    return ESP_OK;
}

/*******************************************************************************
 * @brief Check for READY word from ADS131A04 at initialization. 
 *
 * @return Returns true if receives READY word from ADC, false if otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::isAdcReady() 
{
    bool isReady = false;
    spi_transaction_t t;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16;
    t.rxlength = 16;
    t.txdata[0] = (ADS131A04_CMD_NULL >> 8);
    t.txdata[1] = ADS131A04_CMD_NULL;

    ret = spi_device_queue_trans(spi_dev_, &t, portMAX_DELAY); // Transmit!
    if (ret != ESP_OK) {
        return ret;
    }

    if (t.rxdata[0] == OxFF && t.rxdata[1] == 0x04) return true;
    return false;
}

/*******************************************************************************
 * @brief Gets the current recorded registers of the ADS131A04 instance of the
 *        microcontroller (does not read the ADC).
 *
 * @param regs   - The pointer to where the current registers will be copied to.
 *******************************************************************************/
void ADS131A04::getRegs(ads131a04_regs_t* regs)
{
    if (regs == NULL) {
        ESP_LOGE(TAG, "Given register pointer is NULL");
        return;
    }
    memcpy(regs, &regs_, sizeof(ads131a04_regs_t));
}

/*******************************************************************************
 * @brief Checks if the data on the ADC is ready to be read.
 *
 * @return Returns true if ready, and false if not.
 *******************************************************************************/
bool ADS131A04::isDataReady() const { return !gpio_get_level(drdy_pin_); }

/*******************************************************************************
 * @brief Reads the data value from the ADC and stores it in t_data.
 *
 * @param t_data A pointer to where the ADC value is stored.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::readADC(int16_t* t_data)
{
    esp_err_t ret;

    while (!isDataReady()){
        vTaskDelay(pdMS_TO_TICKS(500));
    };

    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));

    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    memset(t.tx_data, ADS1120_SPI_MASTER_DUMMY, 2);

    t.length   = 2 * 8; // 2 bytes
    t.rxlength = 2 * 8; // 2 bytes

    ESP_LOGI(TAG, "drdy before queue trans: %d", gpio_get_level(drdy_pin_));

    ret = spi_device_queue_trans(spi_dev_, &t, portMAX_DELAY); // Transmit!
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "drdy after queue trans: %d", gpio_get_level(drdy_pin_));

    spi_transaction_t* trans_result;

    ret = spi_device_get_trans_result(spi_dev_, &trans_result, portMAX_DELAY);

    ESP_LOGI(TAG, "drdy after trans result: %d", gpio_get_level(drdy_pin_));

    *t_data = trans_result->rx_data[0];
    *t_data = (*t_data << 8) | trans_result->rx_data[1];

    return ESP_OK;
}

/*******************************************************************************
 * @brief Sends a command to the ADS131A04.
 *
 * @param t_command  - 16-bit command to be sent.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::sendCommand(uint16_t t_command)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));

    t.flags      = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = t_command >> 8;
    t.tx_data[1] = t_command;
    t.length     = 1 * 16;

    return spi_device_polling_transmit(spi_dev_, &t); // Transmit!
}

/*******************************************************************************
 * @brief writes a register onto the ADS131A04.
 *
 * @param t_addr  - The register address.
 * @param t_value - The register value.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::writeRegister(uint8_t t_addr, uint8_t t_value)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));

    t.flags      = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = (ADS131A04_CMD_WREG >> 8 | t_addr);
    t.tx_data[1] = t_value;
    t.length     = 2 * 8; // 2 bytes

    return spi_device_polling_transmit(spi_dev_, &t); // Transmit!
}

/*******************************************************************************
 * @brief Reads a register from ADS131A04.
 *
 * @param t_addr  - The register address.
 * @param t_value - A pointer to where the read data will be stored.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::readRegister(uint8_t t_addr, uint8_t* t_data) const
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(spi_transaction_t));

    t.flags      = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length     = 2 * 8; // 2 bytes
    t.rxlength   = 2 * 8; // 2 bytes
    t.tx_data[0] = (ADS131A04_CMD_RREG >> 8 | t_addr);
    t.tx_data[1] = ADS131A04_SPI_MASTER_DUMMY;

    ret = spi_device_polling_transmit(spi_dev_, &t); // Transmit!
    if (ret != ESP_OK) {
        return ret;
    }

    *t_data = t.rx_data[1];

    return ESP_OK;
}

/*******************************************************************************
 * @brief Writes a masked register onto the ADS131A04_CMD_RESET.
 *
 * @param t_value    - The new register value.
 * @param t_mask     - The register mask.
 * @param t_address  - The register address.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::writeRegisterMasked(uint8_t t_value, uint8_t t_mask, uint8_t t_address)
{
    uint8_t register_contents;
    esp_err_t ret = readRegister(t_address, &register_contents);
    if (ret != ESP_OK) {
        return ret;
    }

    register_contents = register_contents & ~t_mask;

    register_contents = register_contents | t_value;

    return writeRegister(t_address, register_contents);
}

/*******************************************************************************
 * @brief Resets the ADS131A04.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::reset() { return sendCommand(ADS131A04_CMD_RESET); }

/*******************************************************************************
 * @brief Locks the ADS131A04.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::lock() { return sendCommand(ADS131A04_CMD_LOCK); }


/*******************************************************************************
 * @brief Unlocks the ADS131A04.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::unlock() { return sendCommand(ADS131A04_CMD_UNLOCK); }

/*******************************************************************************
 * @brief Wakes the ADS131A04.
 *
 * @return Returns ESP_OK for success or a non-zero value otherwise.
 *******************************************************************************/
esp_err_t ADS131A04::wakeup() { return sendCommand(ADS131A04_CMD_WAKEUP); }


