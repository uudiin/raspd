#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

static int gpio = 25;
module_param(gpio, int, S_IRUGO);

static int irq_number;

static irqreturn_t gpio_reset_interrupt(int irq, void *dev_id)
{
    printk(KERN_ERR "gpio %d IRQ %d event, irq = %d\n", gpio, irq_number, irq);
    return IRQ_HANDLED;
}

static int gpio_init(void)
{
    int err;

    printk(KERN_NOTICE "enter\n");

    if (gpio < 0 || gpio > 54) {
        printk(KERN_ERR "GPIO_RESET: invalid gpio number, gpio = %d\n", gpio);
        return -EINVAL;
    }

    if (err = gpio_request(gpio, "test-gpio")) {
        printk(KERN_ERR "GPIO: request failure %d, err = %d\n", gpio, err);
        return -EFAULT;
    }

    irq_number = gpio_to_irq(gpio);
    if (irq_number < 0) {
        printk(KERN_ERR "GPIO: gpio_to_irq(%d), irq_number = %d\n", gpio, irq_number);
        return -EPERM;
    } else {
        printk(KERN_NOTICE "Mapped gpio %d to %d\n", gpio, irq_number);
    }

    err = request_irq(irq_number, gpio_reset_interrupt,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "gpio_reset", NULL);
    if (err) {
        printk(KERN_ERR "GPIO: trouble requesting IRQ %d\n", irq_number);
        return -EIO;
    } else {
        printk(KERN_ERR "GPIO: requesting IRQ %d -> fine\n", irq_number);
    }

    return 0;
}

static void gpio_exit(void)
{
    free_irq(irq_number, NULL);
    gpio_free(gpio);
    printk("gpio_reset module unloaded\n");
    return;
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_LICENSE("GPL");
