#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LED_MAJOR 200
#define LED_NAME "led"

#define LEDOFF 0
#define LEDON 1

#define CCM_CCGR1_BASE (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE (0X020E02F4)
#define GPIO1_DR_BASE (0X0209C000)
#define GPIO1_GDIR_BASE (0X0209C004)

static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

static void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LEDON) {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    }
    else if(sta == LEDOFF) {
        val = readl(GPIO1_DR);
        val|= (1 << 3);
        writel(val, GPIO1_DR);
    }   
}

static int led_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf,size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    unsigned long not_copied;
    unsigned char databuf[1];

    if (cnt == 0)
        return 0;
    if (cnt != sizeof(databuf))
        return -EINVAL;

    not_copied = copy_from_user(databuf, buf, sizeof(databuf));
    if (not_copied != 0)
        return -EFAULT;

    if (databuf[0] == LEDON)
        led_switch(LEDON);
    else if (databuf[0] == LEDOFF)
        led_switch(LEDOFF);
    else
        return -EINVAL;

    return sizeof(databuf);
}

static int led_release(struct inode *inode,struct file *filp)
{
    return 0;
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};
static int __init led_init(void)
{
    int retvalue;
    u32 val = 0;
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

    if (!IMX6U_CCM_CCGR1 || !SW_MUX_GPIO1_IO03 || !SW_PAD_GPIO1_IO03 ||
        !GPIO1_DR || !GPIO1_GDIR) {
        retvalue = -ENOMEM;
        goto err_unmap;
    }

    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26); /* 清除以前的设置 */
    val |= (3 << 26); /* 设置新值 */
    writel(val, IMX6U_CCM_CCGR1);    
    writel(5,SW_MUX_GPIO1_IO03);
    writel(0X10B0,SW_PAD_GPIO1_IO03);

    /* 4、设置 GPIO1_IO03 为输出功能 */
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3); /* 清除以前的设置 */
    val |= (1 << 3); /* 设置为输出 */
    writel(val, GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);    

    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    
    if (retvalue < 0) {
        printk(KERN_ERR "led: register_chrdev failed: %d\n", retvalue);
        goto err_unmap;
    }

    return 0;

err_unmap:
    if (IMX6U_CCM_CCGR1) iounmap(IMX6U_CCM_CCGR1);
    if (SW_MUX_GPIO1_IO03) iounmap(SW_MUX_GPIO1_IO03);
    if (SW_PAD_GPIO1_IO03) iounmap(SW_PAD_GPIO1_IO03);
    if (GPIO1_DR) iounmap(GPIO1_DR);
    if (GPIO1_GDIR) iounmap(GPIO1_GDIR);
    return retvalue;
}

static void __exit led_exit(void)
{
    /* 取消映射 */
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);
    /* 注销字符设备驱动 */
    unregister_chrdev(LED_MAJOR, LED_NAME);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cube");