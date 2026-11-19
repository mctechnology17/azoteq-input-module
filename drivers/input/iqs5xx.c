#define DT_DRV_COMPAT azoteq_iqs5xx

#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include "iqs5xx.h"

LOG_MODULE_REGISTER(iqs5xx, CONFIG_INPUT_LOG_LEVEL);

static int iqs5xx_i2c_seq_read_16(const struct iqs5xx_config *cfg, uint16_t addr, uint8_t *buf,
                                  size_t len) {
    uint8_t addr_buf[2];
    addr_buf[0] = (uint8_t)((addr >> 8) & 0xFF);
    addr_buf[1] = (uint8_t)(addr & 0xFF);

    return i2c_write_read_dt(&cfg->i2c, addr_buf, 2, buf, len);
}

/* Longest register write of this driver, in bytes. */
#define IQS5XX_MAX_WRITE_LEN 16

static int iqs5xx_i2c_write_16(const struct iqs5xx_config *cfg, uint16_t addr, const uint8_t *data,
                               size_t len) {
    /* On the stack: ZMK builds without a kernel heap, so k_malloc() does not link. */
    uint8_t tx_buf[2 + IQS5XX_MAX_WRITE_LEN];

    if (len > IQS5XX_MAX_WRITE_LEN) {
        return -EINVAL;
    }
    tx_buf[0] = (uint8_t)((addr >> 8) & 0xFF);
    tx_buf[1] = (uint8_t)(addr & 0xFF);
    memcpy(&tx_buf[2], data, len);

    return i2c_write_dt(&cfg->i2c, tx_buf, len + 2);
}

static int iqs5xx_i2c_write_byte_16(const struct iqs5xx_config *cfg, uint16_t addr, uint8_t val) {
    return iqs5xx_i2c_write_16(cfg, addr, &val, 1);
}

int iqs5xx_set_sleep(const struct device *dev, bool enabled) {
    const struct iqs5xx_config *cfg = dev->config;

    /*
     * Pseudocode:
     *   - read a “system control” or “sleep” register
     *   - set or clear a bit
     *   - write it back
     */

    uint8_t sys_ctrl;
    int ret = iqs5xx_i2c_seq_read_16(cfg, 0x0432 /* e.g. SystemControl1_adr */, &sys_ctrl, 1);
    if (ret < 0) {
        LOG_ERR("Failed reading system control: %d", ret);
        return ret;
    }

    /* For example, suppose bit 0 = SUSPEND. */
    if (enabled) {
        sys_ctrl |= 0x01; /* Set SUSPEND bit */
    } else {
        sys_ctrl &= ~0x01; /* Clear SUSPEND bit */
    }

    ret = iqs5xx_i2c_write_byte_16(cfg, 0x0432, sys_ctrl);
    if (ret < 0) {
        LOG_ERR("Failed writing system control: %d", ret);
        return ret;
    }

    return 0;
}

static void iqs5xx_report_data(const struct device *dev) {
    const struct iqs5xx_config *cfg = dev->config;
    struct iqs5xx_data *data = dev->data;

    /* Example: read a status byte, relative X, Y, gesture. */
    uint8_t status_byte;
    int ret = iqs5xx_i2c_seq_read_16(cfg, IQS5XX_REG_STATUS, &status_byte, 1);
    if (ret < 0) {
        LOG_ERR("IQS5xx read status err=%d", ret);
        return;
    }

    /* If status indicates no data or not ready, bail out. */
    if ((status_byte & IQS5XX_STATUS_DRDY_BIT) == 0) {
        return;
    }

    uint8_t buffer[4];
    ret = iqs5xx_i2c_seq_read_16(cfg, IQS5XX_REG_REL_X, buffer, sizeof(buffer));
    if (ret < 0) {
        LOG_ERR("IQS5xx read XY/gest err=%d", ret);
        return;
    }

    /* Suppose 0:relX(8-bit), 1:relY(8-bit), 2:gesture, 3:some button bits. */
    int8_t rel_x = (int8_t)buffer[0];
    int8_t rel_y = (int8_t)buffer[1];
    uint8_t gest = buffer[2];
    uint8_t btn = buffer[3];

    /* Optionally invert X or Y */
    if (cfg->x_invert) {
        rel_x = -rel_x;
    }
    if (cfg->y_invert) {
        rel_y = -rel_y;
    }

    /*
     * button pressed bits for a tap or something
     */
    if (!cfg->no_taps && (btn || data->btn_cache)) {
        for (int i = 0; i < 3; i++) {
            bool is_pressed_now = (btn & BIT(i)) != 0;
            bool was_pressed = (data->btn_cache & BIT(i)) != 0;
            if (is_pressed_now != was_pressed) {
                input_report_key(dev, INPUT_BTN_0 + i, is_pressed_now ? 1 : 0, false, K_FOREVER);
            }
        }
    }
    data->btn_cache = btn;

    /*
     * Report relative motion as well.
     * The final "true" parameter indicates "sync = true",
     * i.e. finalize this input event.
     */
    input_report_rel(dev, INPUT_REL_X, rel_x, false, K_FOREVER);
    input_report_rel(dev, INPUT_REL_Y, rel_y, true, K_FOREVER);
}

static void iqs5xx_work_cb(struct k_work *work) {
    struct iqs5xx_data *data = CONTAINER_OF(work, struct iqs5xx_data, work);
    data->in_int = false;

    /* Poll the device for data and send input reports. */
    iqs5xx_report_data(data->dev);
}

static void iqs5xx_gpio_cb(const struct device *port, struct gpio_callback *cb, uint32_t pins) {
    struct iqs5xx_data *data = CONTAINER_OF(cb, struct iqs5xx_data, gpio_cb);

    data->in_int = true;
    k_work_submit(&data->work);
}

/*
 * Device power-management callback (optional).
 * For example, disable the DR interrupt on SUSPEND, re-enable on RESUME.
 */
#if IS_ENABLED(CONFIG_PM_DEVICE)
static int iqs5xx_pm_action(const struct device *dev, enum pm_device_action action) {
    const struct iqs5xx_config *cfg = dev->config;

    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        gpio_pin_interrupt_configure_dt(&cfg->dr, GPIO_INT_DISABLE);
        return 0;
    case PM_DEVICE_ACTION_RESUME:
        gpio_pin_interrupt_configure_dt(&cfg->dr, GPIO_INT_EDGE_TO_ACTIVE);
        return 0;
    default:
        return -ENOTSUP;
    }
}
#endif /* CONFIG_PM_DEVICE */

/*
 * Driver init: configure the device, set up GPIO callback, etc.
 */
static int iqs5xx_init(const struct device *dev) {
    struct iqs5xx_data *data = dev->data;
    const struct iqs5xx_config *cfg = dev->config;
    int ret;

    /* Check that the I2C bus is ready. */
    if (!device_is_ready(cfg->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    /* Set up interrupt pin. */
    if (!device_is_ready(cfg->dr.port)) {
        LOG_ERR("IQS5xx DR port not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&cfg->dr, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to configure DR pin: %d", ret);
        return ret;
    }

    gpio_init_callback(&data->gpio_cb, iqs5xx_gpio_cb, BIT(cfg->dr.pin));
    ret = gpio_add_callback(cfg->dr.port, &data->gpio_cb);
    if (ret < 0) {
        LOG_ERR("Failed to set DR callback: %d", ret);
        return ret;
    }

    /* Prepare the work item used to fetch data after each interrupt. */
    k_work_init(&data->work, iqs5xx_work_cb);

    /*
     * Perform any “basic init” steps for the IQS5xx:
     *   - Reset?
     *   - Write configuration registers (refresh rates, gesture enables, etc.)
     *   - Clear status
     *   - Possibly run initial “ATI calibration” if required
     */
    LOG_INF("IQS5xx: doing basic configuration...");
    /* Example: write active refresh rate or end window, etc. */
    uint8_t dummy[2] = {0, 8};
    iqs5xx_i2c_write_16(cfg, 0x057A /* ActiveRR_adr */, dummy, 2); /* error checking omitted */

    /* Write END_WINDOW to close transaction (some IQS devices require that). */
    uint8_t dummy_byte = 0;
    iqs5xx_i2c_write_16(cfg, IQS5XX_REG_END_WINDOW, &dummy_byte, 1);

    /* Finally, enable the interrupt. If the device triggers on edge,
     * use GPIO_INT_EDGE_TO_ACTIVE or similar.
     */
    ret = gpio_pin_interrupt_configure_dt(&cfg->dr, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to enable DR interrupt: %d", ret);
        return ret;
    }

    data->dev = dev;
    data->btn_cache = 0;
    data->in_int = false;

    LOG_INF("IQS5xx driver init complete");
    return 0;
}

#if IS_ENABLED(CONFIG_PM_DEVICE)
#define IQS5XX_PM_INIT(n) PM_DEVICE_DT_INST_DEFINE(n, iqs5xx_pm_action);
#define IQS5XX_PM_REF(n) PM_DEVICE_DT_INST_GET(n)
#else
#define IQS5XX_PM_INIT(n)
#define IQS5XX_PM_REF(n) NULL
#endif

#define IQS5XX_INST(n)                                                                             \
    static struct iqs5xx_data iqs5xx_data_##n;                                                     \
    static const struct iqs5xx_config iqs5xx_config_##n = {                                        \
        .i2c = I2C_DT_SPEC_INST_GET(n),                                                            \
        .dr = GPIO_DT_SPEC_INST_GET(n, dr_gpios),                                                  \
        .no_taps = DT_INST_PROP(n, no_taps),                                                       \
        .x_invert = DT_INST_PROP(n, x_invert),                                                     \
        .y_invert = DT_INST_PROP(n, y_invert),                                                     \
    };                                                                                             \
    IQS5XX_PM_INIT(n)                                                                              \
    DEVICE_DT_INST_DEFINE(n, iqs5xx_init, IQS5XX_PM_REF(n), &iqs5xx_data_##n, &iqs5xx_config_##n,  \
                          POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY,                                 \
                          NULL /* No API struct, input drivers pass NULL */);

DT_INST_FOREACH_STATUS_OKAY(IQS5XX_INST)
