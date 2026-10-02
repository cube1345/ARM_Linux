#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define DTSLED_CNT 1
#define DTSLED_NAME "dtsled"
#define LEDOFF 0
#define LEDON  1

struct dtsled_dev {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int major;
	int minor;
	struct device_node *nd;
	bool active_low;
};

struct dtsled_dev dtsled;

/*
 * @description		: LED打开/关闭
 * @param - sta 	: LEDON(0) 打开LED，LEDOFF(1) 关闭LED
 * @return 			: 无
 */
void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);	
		writel(val, GPIO1_DR);
	}else if(sta == LEDOFF) {
		val = readl(GPIO1_DR);
		val|= (1 << 3);	
		writel(val, GPIO1_DR);
	}	
}

static int dtsled_open(struct inode *inode, struct file *filp)
{
	dtsled = container_of(inode->i_cdev, struct dtsled_dev, cdev);
	filp->private_data = dtsled;
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t dtsled_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}


static ssize_t dtsled_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	
	u8 value;

	if (count == 0)
		return 0;
	if (count != sizeof(value))
		return -EINVAL;
	if (copy_from_user(&value, buf, sizeof(value)) != 0)
		return -EFAULT;
	if (value != LEDON && value != LEDOFF)
		return -EINVAL;

	gpio_set_value_cansleep(led->gpio,
				value == LEDON ? !led->active_low : led->active_low);
	return sizeof(value);
}

static const struct file_operations dtsled_fops = {
	.owner = THIS_MODULE,
	.open = dtsled_open,
	.write = dtsled_write,
};

static int __init dtsled_init(struct platform_device *pdev)
{
	struct dtsled_dev *led;
	enum of_gpio_flags flags;
	unsigned long init_flags;
	int ret;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;
	led->gpio = of_get_gpio_flags(pdev->dev.of_node, 0, &flags);
	if (!gpio_is_valid(led->gpio))
		return led->gpio;
	led->active_low = !!(flags & OF_GPIO_ACTIVE_LOW);
	init_flags = led->active_low ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW;
	ret = devm_gpio_request_one(&pdev->dev, led->gpio, init_flags,
				    DTSLED_NAME);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&led->devid, 0, 1, DTSLED_NAME);
	if (ret)
		return ret;
	cdev_init(&led->cdev, &dtsled_fops);
	led->cdev.owner = THIS_MODULE;
	ret = cdev_add(&led->cdev, led->devid, 1);
	if (ret)
		goto err_unregister;
	led->class = class_create(THIS_MODULE, DTSLED_NAME);
	if (IS_ERR(led->class)) {
		ret = PTR_ERR(led->class);
		goto err_cdev;
	}
	led->device = device_create(led->class, &pdev->dev, led->devid, NULL,
				    DTSLED_NAME);
	if (IS_ERR(led->device)) {
		ret = PTR_ERR(led->device);
		goto err_class;
	}
	platform_set_drvdata(pdev, led);
	dev_info(&pdev->dev, "registered /dev/%s major=%d minor=%d gpio=%d active_low=%d\n",
		 DTSLED_NAME, MAJOR(led->devid), MINOR(led->devid), led->gpio,
		 led->active_low);
	return 0;

err_class:
	class_destroy(led->class);
err_cdev:
	cdev_del(&led->cdev);
err_unregister:
	unregister_chrdev_region(led->devid, 1);
	return ret;
}

static int dtsled_remove(struct platform_device *pdev)
{
	struct dtsled_dev *led = platform_get_drvdata(pdev);
	device_destroy(led->class, led->devid);
	class_destroy(led->class);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devid, 1);
	return 0;
}

static const struct of_device_id dtsled_of_match[] = {
	{ .compatible = "cube,imx6ull-dts-led" },
	{ }
};
MODULE_DEVICE_TABLE(of, dtsled_of_match);

static struct platform_driver dtsled_driver = {
	.probe = dtsled_probe,
	.remove = dtsled_remove,
	.driver = {
		.name = DTSLED_NAME,
		.of_match_table = dtsled_of_match,
	},
};

module_platform_driver(dtsled_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cube");
MODULE_DESCRIPTION("i.MX6ULL Device Tree GPIO LED character driver");
