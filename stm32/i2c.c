#include "jdstm.h"

#ifdef PIN_SDA

#define I2Cx I2C1
#define I2C_CLK LL_APB1_GRP1_PERIPH_I2C1

#ifndef I2C_FAST_MODE
#define I2C_FAST_MODE 1
#endif

#ifdef STM32F0
#if 1
// 22.4.10 I2C_TIMINGR register configuration examples
#if I2C_FAST_MODE
#define I2C_TIMING 0x00310309
#else
#define I2C_TIMING 0x00420F13
#endif
#else
// rise 100ns, fall 10ns
#if I2C_FAST_MODE
#define I2C_TIMING 0x0010020A
#else
#define I2C_TIMING 0x00201D2B
#endif
#endif
#endif

static void setup_pin(uint8_t pin) {
    pin_setup_output_af(pin, I2C_AF);
    LL_GPIO_SetPinOutputType(PIN_PORT(pin), PIN_MASK(pin), LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinPull(PIN_PORT(pin), PIN_MASK(pin), LL_GPIO_PULL_UP);
}

void i2c_init(void) {
    setup_pin(PIN_SDA);
    setup_pin(PIN_SCL);
    LL_RCC_SetI2CClockSource(LL_RCC_I2C1_CLKSOURCE_HSI);
    LL_APB1_GRP1_EnableClock(I2C_CLK);
    LL_I2C_Disable(I2Cx);
    LL_I2C_SetTiming(I2Cx, I2C_TIMING);
    LL_I2C_Enable(I2Cx);
}

#define CYCLES_PER_MS (77 * cpu_mhz)
static int wait_for_ack(void) {
    unsigned k = 10 * CYCLES_PER_MS;
    while (k--) {
        if (LL_I2C_IsActiveFlag_TXIS(I2Cx))
            return 0;
    }
    return -1;
}

int i2c_write_reg_buf(uint8_t addr, uint8_t reg, const void *src, unsigned len) {
    addr <<= 1;
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, len + 1, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    if (wait_for_ack() != 0)
        return -1;

    const uint8_t *p = src - 1;
    const uint8_t *end = (const uint8_t *)src + len;
    while (p < end) {
        if (LL_I2C_IsActiveFlag_TXIS(I2Cx)) {
            LL_I2C_TransmitData8(I2Cx, p == src - 1 ? reg : *p);
            p++;
        }
    }

    while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
        ;

    LL_I2C_ClearFlag_STOP(I2Cx);

    return 0;
}

int i2c_write_buf(uint8_t addr, const void *src, unsigned len) {
    const uint8_t *p = src;
    return i2c_write_reg_buf(addr, *p, p + 1, len - 1);
}

#define MAXREP 10000

int i2c_read_buf(uint8_t addr, uint8_t reg, void *dst, unsigned len) {
    addr <<= 1;
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, 1, LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);

    if (wait_for_ack() != 0)
        return -1;

    LL_I2C_TransmitData8(I2Cx, reg);

    while (!LL_I2C_IsActiveFlag_TC(I2Cx))
        ;

    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, len, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_RESTART_7BIT_READ);
    uint8_t *p = dst;
    uint8_t *end = p + len;
    while (p < end) {
        unsigned i;
        for (i = 0; i < MAXREP; ++i)
            if (LL_I2C_IsActiveFlag_RXNE(I2Cx))
                break;

        if (i >= MAXREP)
            return -1;

        *p++ = LL_I2C_ReceiveData8(I2Cx);
    }

    while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
        ;
    LL_I2C_ClearFlag_STOP(I2Cx);

    return 0;
}

int i2c_read_reg(uint8_t addr, uint8_t reg) {
    uint8_t r = 0;
    if (i2c_read_buf(addr, reg, &r, 1))
        return -1;
    return r;
}

int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    return i2c_write_reg_buf(addr, reg, &val, 1);
}

#endif