#include "usb_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if __has_include("usbd_cdc_if.h")
#define USB_LOG_ENABLED 1
#include "main.h"
#include "usbd_cdc_if.h"
extern USBD_HandleTypeDef hUsbDeviceFS;
#else
#define USB_LOG_ENABLED 0
#endif

#define USB_LOG_CHUNK      256U
#define USB_LOG_TIMEOUT_MS 20U

int usb_log_available(void)
{
    return USB_LOG_ENABLED;
}

#if USB_LOG_ENABLED

int usb_log_connected(void)
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

// CDC_Transmit_FS keeps using the buffer until the transfer completes, so alternate between
// two: a transmit from one only starts (stops returning BUSY) once the other has finished.
static uint8_t buf[2][USB_LOG_CHUNK];
static uint8_t which;

static void send_chunk(const char *s, uint16_t len)
{
    uint8_t *b = buf[which];
    which ^= 1U;
    memcpy(b, s, len);

    uint32_t start = HAL_GetTick();
    while (CDC_Transmit_FS(b, len) == USBD_BUSY)
    {
        if (HAL_GetTick() - start >= USB_LOG_TIMEOUT_MS)
        {
            return; // host not reading; drop rather than stall the caller
        }
    }
}

void usb_log_write(const char *s)
{
    if (!usb_log_connected())
    {
        return;
    }
    size_t len = strlen(s);
    while (len > 0U)
    {
        uint16_t n = (uint16_t)(len > USB_LOG_CHUNK ? USB_LOG_CHUNK : len);
        send_chunk(s, n);
        s += n;
        len -= n;
    }
}

#else

int usb_log_connected(void)
{
    return 0;
}

void usb_log_write(const char *s)
{
    (void)s;
}

#endif

void usb_log_printf(const char *fmt, ...)
{
    if (!usb_log_connected())
    {
        return;
    }
    char line[USB_LOG_CHUNK];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    usb_log_write(line);
}
