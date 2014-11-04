#include <linux/module.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

static irq_number;

static irqreturn_t gpio_reset_interrupt(int irq, void *dev_id)
{
    printk(KERN_ERR "gpio0 IRQ %d event\n", irq_number);
    return IRQ_HANDLED;
}

static int __init gpio_init(void)
{
    int err;

    irq_number = gpio_to_irq(25);
    err = request_irq(irq_number, gpio_reset_interrupt,
            IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "gpio_reset", NULL);
    if (err) {
        printk(KERN_ERR "GPIO_RESET: trouble requesting IRQ %d\n", irq_number);
        return -EIO;
    } else {
        printk(KERN_ERR "GPIO_RESET: requesting IRQ %d -> fine\n", irq_number);
    }

    return 0;
}

static void __exit gpio_exit(void)
{
    free_irq(irq_number, NULL);
    printk("gpio_reset module unloaded\n");
    return;
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_LICENSE("GPL");
