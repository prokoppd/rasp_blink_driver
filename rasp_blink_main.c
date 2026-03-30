#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/timer.h>

#define DRIVER_NAME "rasp-blink"
#define DRIVER_DESC "A raspberry pi 3 blink module"

#define GPIO_BASE  0x3F200000UL // Base address for GPIO on Raspberry Pi 2 and 3
#define IRQ_NUMBER (1)

#define BCM_BASE    (512)
#define GPIO_LED    (BCM_BASE + 17)
#define GPIO_BUTTON (BCM_BASE + 27)

#define DEBOUNCE_TIME_MS (250)

typedef struct context_t
{
    int               irq_number;
    struct gpio_desc *gpio_led;
    struct gpio_desc *gpio_button;
} context_t;

static context_t ctx;

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    static int           state        = 0;
    static unsigned long last_time    = 0;
    unsigned long        current_time = jiffies;

    int btn_val = gpio_get_value(GPIO_BUTTON);
    if (!btn_val && time_after(current_time, last_time + msecs_to_jiffies(DEBOUNCE_TIME_MS)))
    {
        state = !state;
        gpio_set_value(GPIO_LED, state);
        printk(KERN_INFO "BUTTON pressed\n");
    }
    return IRQ_HANDLED;
}

static int led_button_probe(struct platform_device *pdev)
{
    int ret;

    if (!gpio_is_valid(GPIO_LED) || !gpio_is_valid(GPIO_BUTTON))
    {
        dev_err(&pdev->dev, "Failed to get LED gpio\n");
        return PTR_ERR(ctx.gpio_led);
    }

    ret = gpio_request(GPIO_LED, "sysfs");
    if (ret)
    {
        dev_err(&pdev->dev, "%s: Failed to request GPIO %d\n", DRIVER_NAME, GPIO_LED);
        return ret;
    }

    ret = gpio_direction_output(GPIO_LED, 1);
    if (ret)
    {
        dev_err(&pdev->dev, "%s: Failed to set GPIO direction\n", DRIVER_NAME);
        gpio_free(GPIO_LED);
        return ret;
    }

    ret = gpio_request(GPIO_BUTTON, "sysfs");
    if (ret)
    {
        dev_err(&pdev->dev, "%s: Failed to request GPIO %d\n", DRIVER_NAME, GPIO_BUTTON);
        return ret;
    }
    dev_info(&pdev->dev, "%s: LED (GPIO17) initialized\n", DRIVER_NAME);

    ret = gpio_direction_input(GPIO_BUTTON);
    if (ret)
    {
        dev_err(&pdev->dev, "%s: Failed to set GPIO direction\n", DRIVER_NAME);
        gpio_free(GPIO_LED);
        gpio_free(GPIO_BUTTON);
        return ret;
    }
    dev_info(&pdev->dev, "%s: BUTTON (GPIO27) initialized\n", DRIVER_NAME);

    ctx.irq_number = gpio_to_irq(GPIO_BUTTON);
    if (ctx.irq_number < 0)
    {
        dev_err(&pdev->dev, "%s: Failed to request gpio irq\n", DRIVER_NAME);
        gpio_free(GPIO_LED);
        gpio_free(GPIO_BUTTON);
        return ctx.irq_number;
    }
    dev_info(&pdev->dev, "%s: irq requested=%d", DRIVER_NAME, ctx.irq_number);

    ret = devm_request_irq(&pdev->dev,
                           ctx.irq_number,
                           button_irq_handler,
                           IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                           "button_irq",
                           NULL);
    if (ret)
    {
        dev_err(&pdev->dev, "%s: Failed to request gpio irq\n", DRIVER_NAME);
        gpio_free(GPIO_LED);
        gpio_free(GPIO_BUTTON);
        return ret;
    }
    dev_info(&pdev->dev, "LED (GPIO17) and Button (GPIO27) initialized\n");
    return 0;
}

static int led_button_remove(struct platform_device *pdev)
{
    gpiod_set_value(ctx.gpio_led, 1); // turn of led
    gpio_free(GPIO_LED);
    gpio_free(GPIO_BUTTON);
    dev_info(&pdev->dev, "Module Removed\n");
    return 0;
}

static const struct platform_device_id led_button_id_table[] = {
    {DRIVER_NAME, 0},
    {},
};
MODULE_DEVICE_TABLE(platform, led_button_id_table);

static struct platform_driver led_button_driver = {
    .probe  = led_button_probe,
    .remove = led_button_remove,
    // clang-format off
    .driver = {
        .name  = DRIVER_NAME,
        // .of_match_table = led_button_of_match,
        .owner = THIS_MODULE,
    },
    // clang-format on
};

static struct platform_device *led_button_pdev;

static int __init rpi_blink_init(void)
{
    int ret;

    led_button_pdev = platform_device_register_simple(DRIVER_NAME, -1, NULL, 0);

    if (IS_ERR(led_button_pdev))
    {
        pr_err("%s: failed to register platform device\n", DRIVER_NAME);
        return PTR_ERR(led_button_pdev);
    }

    ret = platform_driver_register(&led_button_driver);
    if (ret)
    {
        pr_err("%s: failed to register platform driver\n", DRIVER_NAME);
        platform_device_unregister(led_button_pdev);
        return ret;
    }
    return 0;
}

static void __exit rpi_blink_exit(void)
{
    pr_info("%s: module exit\n", DRIVER_NAME);
    platform_driver_unregister(&led_button_driver);
    platform_device_unregister(led_button_pdev);
}

module_init(rpi_blink_init);
module_exit(rpi_blink_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Daniel P. Prokopp <daniel.prokopp@gbtsolutions.pt>");
MODULE_DESCRIPTION(DRIVER_DESC);
