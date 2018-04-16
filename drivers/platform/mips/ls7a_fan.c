#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <asm/io.h>

#include <boot_param.h>
#include <loongson-pch.h>
#include <loongson_hwmon.h>

#define LS7A_PWM_REG_BASE              (void *)TO_UNCAC(LS7A_MISC_REG_BASE + 0x20000)

#define LS7A_PWM0_LOW                   0x004
#define LS7A_PWM0_FULL                  0x008
#define LS7A_PWM0_CTRL                  0x00c

#define LS7A_PWM1_LOW                   0x104
#define LS7A_PWM1_FULL                  0x108
#define LS7A_PWM1_CTRL                  0x10c

#define LS7A_PWM2_LOW                   0x204
#define LS7A_PWM2_FULL                  0x208
#define LS7A_PWM2_CTRL                  0x20c

#define LS7A_PWM3_LOW                   0x304
#define LS7A_PWM3_FULL                  0x308
#define LS7A_PWM3_CTRL                  0x30c

#define MAX_LS7A_FANS 3

static struct device *pwm_hwmon_dev;
static int count = 255;
static unsigned long speed_lo = 0, speed_hi = 0;
enum fan_control_mode ls7a_fan_mode[MAX_LS7A_FANS];

#define pwm_read(addr)	readl(LS7A_PWM_REG_BASE + (addr))

#define pwm_write(val, addr)				\
	do {						\
		writel(val, LS7A_PWM_REG_BASE + (addr));\
		readl(LS7A_PWM_REG_BASE + (addr));	\
	} while (0)

static ssize_t get_hwmon_name(struct device *dev,
			struct device_attribute *attr, char *buf);
static SENSOR_DEVICE_ATTR(name, S_IRUGO, get_hwmon_name, NULL, 0);

static struct attribute *pwm_hwmon_attributes[] =
{
	&sensor_dev_attr_name.dev_attr.attr,
	NULL
};

/* Hwmon device attribute group */
static struct attribute_group pwm_hwmon_attribute_group =
{
	.attrs = pwm_hwmon_attributes,
};

/* Hwmon device get name */
static ssize_t get_hwmon_name(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ls7a-fan\n");
}

static ssize_t get_fan_level(struct device *dev,
			struct device_attribute *attr, char *buf);
static ssize_t set_fan_level(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count);
static ssize_t get_fan_mode(struct device *dev,
			struct device_attribute *attr, char *buf);
static ssize_t set_fan_mode(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count);

static SENSOR_DEVICE_ATTR(pwm1, S_IWUSR | S_IRUGO,
				get_fan_level, set_fan_level, 1);
static SENSOR_DEVICE_ATTR(pwm1_enable, S_IWUSR | S_IRUGO,
				get_fan_mode, set_fan_mode, 1);

static SENSOR_DEVICE_ATTR(pwm2, S_IWUSR | S_IRUGO,
				get_fan_level, set_fan_level, 2);
static SENSOR_DEVICE_ATTR(pwm2_enable, S_IWUSR | S_IRUGO,
				get_fan_mode, set_fan_mode, 2);

static SENSOR_DEVICE_ATTR(pwm3, S_IWUSR | S_IRUGO,
				get_fan_level, set_fan_level, 3);
static SENSOR_DEVICE_ATTR(pwm3_enable, S_IWUSR | S_IRUGO,
				get_fan_mode, set_fan_mode, 3);

static SENSOR_DEVICE_ATTR(pwm4, S_IWUSR | S_IRUGO,
				get_fan_level, set_fan_level, 4);
static SENSOR_DEVICE_ATTR(pwm4_enable, S_IWUSR | S_IRUGO,
				get_fan_mode, set_fan_mode, 4);

static const struct attribute *hwmon_fan[4][3] = {
	{
		&sensor_dev_attr_pwm1.dev_attr.attr,
		&sensor_dev_attr_pwm1_enable.dev_attr.attr,
		NULL
	},
	{
		&sensor_dev_attr_pwm2.dev_attr.attr,
		&sensor_dev_attr_pwm2_enable.dev_attr.attr,
		NULL
	},
	{
		&sensor_dev_attr_pwm3.dev_attr.attr,
		&sensor_dev_attr_pwm3_enable.dev_attr.attr,
		NULL
	},
	{
		&sensor_dev_attr_pwm4.dev_attr.attr,
		&sensor_dev_attr_pwm4_enable.dev_attr.attr,
		NULL
	}
};

static u32 pwm_get_fan_level(int id)
{
	if (id == 0) {
		speed_lo = pwm_read(LS7A_PWM0_LOW);
		speed_hi = pwm_read(LS7A_PWM0_FULL);
	}
	if (id == 1) {
		speed_lo = pwm_read(LS7A_PWM1_LOW);
		speed_hi = pwm_read(LS7A_PWM1_FULL);
	}
	if (id == 2) {
		speed_lo = pwm_read(LS7A_PWM2_LOW);
		speed_hi = pwm_read(LS7A_PWM2_FULL);
	}
	if (id == 3) {
		speed_lo = pwm_read(LS7A_PWM3_LOW);
		speed_hi = pwm_read(LS7A_PWM3_FULL);
	}
	if(speed_lo)
		return count * (speed_hi - speed_lo) / speed_hi + 1;
	else
		return count * (speed_hi - speed_lo) / speed_hi;
}

static ssize_t get_fan_level(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int id = (to_sensor_dev_attr(attr))->index - 1;
	u32 val = pwm_get_fan_level(id);

	return sprintf(buf, "%d\n", val);
}

static ssize_t set_fan_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 new_speed;
	int id = (to_sensor_dev_attr(attr))->index - 1;

	if(!ls7a_fan_mode[id])
		return count;

	new_speed = clamp_val(simple_strtoul(buf, NULL, 10), 0, 255);

	if(new_speed == 255)
		speed_lo = 0;
	else
		speed_lo = speed_hi - (new_speed * speed_hi) / 255;

	if (id == 0)
		pwm_write(speed_lo, LS7A_PWM0_LOW);
	if (id == 1)
		pwm_write(speed_lo, LS7A_PWM1_LOW);
	if (id == 2)
		pwm_write(speed_lo, LS7A_PWM2_LOW);
	if (id == 3)
		pwm_write(speed_lo, LS7A_PWM3_LOW);

	return count;
}

static ssize_t get_fan_mode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int id = (to_sensor_dev_attr(attr))->index - 1;
	return sprintf(buf, "%d\n", ls7a_fan_mode[id]);
}

static void ls7a_set_fan_level(u8 level, int id)
{
	if (id == 0){
		pwm_write(level, LS7A_PWM0_FULL);
		pwm_write(0, LS7A_PWM0_LOW);
	}
	if (id == 1){
		pwm_write(level, LS7A_PWM1_FULL);
		pwm_write(0, LS7A_PWM1_LOW);
	}
	if (id == 2){
		pwm_write(level, LS7A_PWM2_FULL);
		pwm_write(0, LS7A_PWM2_LOW);
	}
	if (id == 3){
		pwm_write(level, LS7A_PWM3_FULL);
		pwm_write(0, LS7A_PWM3_LOW);
	}

}

static ssize_t set_fan_mode(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int id = (to_sensor_dev_attr(attr))->index - 1;
	u8 new_mode;

	new_mode = clamp_val(simple_strtoul(buf, NULL, 10),
				FAN_FULL_MODE, FAN_MANUAL_MODE);

	if(new_mode > FAN_MANUAL_MODE)
		return count;

	switch (new_mode) {
	case FAN_FULL_MODE:
		ls7a_set_fan_level(MAX_FAN_LEVEL, id);
		break;
	case FAN_MANUAL_MODE:
		ls7a_fan_mode[id] = FAN_MANUAL_MODE;
		break;
	default:
		break;
	}

	ls7a_fan_mode[id] = new_mode;

	return count;
}

static void pwm_fan1_init(void)
{
	int ret;
	volatile unsigned long ls7a_pwm0_ctl;

	ls7a_pwm0_ctl = pwm_read(LS7A_PWM0_CTRL);
	ls7a_pwm0_ctl &= ~1;
	ls7a_pwm0_ctl |= 1;
	pwm_write(ls7a_pwm0_ctl,LS7A_PWM0_CTRL);

	ret = sysfs_create_files(&pwm_hwmon_dev->kobj, hwmon_fan[0]);
	if (ret)
		printk(KERN_ERR "fail to create sysfs files\n");
}

static void pwm_fan2_init(void)
{
	int ret;
	volatile unsigned long ls7a_pwm1_ctl;

	ls7a_pwm1_ctl = pwm_read(LS7A_PWM1_CTRL);
	ls7a_pwm1_ctl &= ~1;
	ls7a_pwm1_ctl |= 1;
	pwm_write(ls7a_pwm1_ctl,LS7A_PWM1_CTRL);

	ret = sysfs_create_files(&pwm_hwmon_dev->kobj, hwmon_fan[1]);
	if (ret)
		printk(KERN_ERR "fail to create sysfs files\n");
}

static void pwm_fan3_init(void)
{
	int ret;
	volatile unsigned long ls7a_pwm2_ctl;

	ls7a_pwm2_ctl = pwm_read(LS7A_PWM2_CTRL);
	ls7a_pwm2_ctl &= ~1;
	ls7a_pwm2_ctl |= 1;
	pwm_write(ls7a_pwm2_ctl,LS7A_PWM2_CTRL);

	ret = sysfs_create_files(&pwm_hwmon_dev->kobj, hwmon_fan[2]);
	if (ret)
		printk(KERN_ERR "fail to create sysfs files\n");
}

static void pwm_fan4_init(void)
{
	int ret;
	volatile unsigned long ls7a_pwm3_ctl;

	ls7a_pwm3_ctl = pwm_read(LS7A_PWM3_CTRL);
	ls7a_pwm3_ctl &= ~1;
	ls7a_pwm3_ctl |= 1;
	pwm_write(ls7a_pwm3_ctl,LS7A_PWM3_CTRL);

	ret = sysfs_create_files(&pwm_hwmon_dev->kobj, hwmon_fan[3]);
	if (ret)
		printk(KERN_ERR "fail to create sysfs files\n");
}

static int pwm_fan_probe(struct platform_device *dev)
{
	int id = dev->id - 1;

	if (id == 0)
		pwm_fan1_init();
	if (id == 1)
		pwm_fan2_init();
	if (id == 2)
		pwm_fan3_init();
	if (id == 3)
		pwm_fan4_init();

	return 0;
}


static struct platform_driver pwm_fan_driver = {
	.probe	= pwm_fan_probe,
	.driver = {
		   .name	= "ls7a-fan",
		   .owner	= THIS_MODULE,
	},
};


static int pwm_init(void)
{
	int ret;

	pwm_hwmon_dev = hwmon_device_register(NULL);
	if (IS_ERR(pwm_hwmon_dev)) {
		ret = -ENOMEM;
		printk(KERN_ERR "hwmon_device_register fail!\n");
		goto fail_hwmon_device_register;
	}

	ret = sysfs_create_group(&pwm_hwmon_dev->kobj,
				&pwm_hwmon_attribute_group);
	if (ret) {
		printk(KERN_ERR "fail to create loongson hwmon!\n");
		goto fail_sysfs_create_group_hwmon;
	}

	ret = platform_driver_register(&pwm_fan_driver);
	if (ret) {
		printk(KERN_ERR "fail to register fan driver!\n");
		goto fail_register_fan;
	}

	return 0;

fail_register_fan:
	sysfs_remove_group(&pwm_hwmon_dev->kobj,
				&pwm_hwmon_attribute_group);

fail_sysfs_create_group_hwmon:
	hwmon_device_unregister(pwm_hwmon_dev);

fail_hwmon_device_register:
	return ret;
}

static void pwm_exit(void)
{
	platform_driver_unregister(&pwm_fan_driver);
	sysfs_remove_group(&pwm_hwmon_dev->kobj,
				&pwm_hwmon_attribute_group);
	hwmon_device_unregister(pwm_hwmon_dev);
}

module_init(pwm_init);
module_exit(pwm_exit);

MODULE_AUTHOR("Sun Ce <sunc@lemote.com>");
MODULE_AUTHOR("Huacai Chen <chenhc@lemote.com>");
MODULE_DESCRIPTION("LS7A fan control driver");
MODULE_LICENSE("GPL");
