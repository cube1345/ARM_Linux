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

#define DTSLED_NAME "dtsled"
#define LEDOFF 0
#define LEDON  1

struct dtsled_dev {
	dev_t devid;
	struct cdev cdev;
	struct class *class;
	struct device *device;
	int gpio;
	bool active_low;
};

static int dtsled_open(struct inode *inode, struct file *filp)
{
	struct dtsled_dev *led;
	led = container_of(inode->i_cdev, struct dtsled_dev, cdev);
	filp->private_data = led;
	return 0;
}

static ssize_t dtsled_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct dtsled_dev *led = filp->private_data;
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

static int dtsled_probe(struct platform_device *pdev)
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
