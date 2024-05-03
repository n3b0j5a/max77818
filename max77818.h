// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef  __LINUX_MAX77818_
#define  __LINUX_MAX77818_

#define GPIO_UNUSED -1

struct max77818_dev {
	struct device *dev;

	struct gpio_desc *gpio;

	int irq;

	struct regmap_irq_chip_data *irq_chip_sys;
	struct regmap_irq_chip_data *irq_chip_chg;
	struct regmap_irq_chip_data *irq_chip_src;

	struct i2c_client *i2c_sys;
	struct i2c_client *i2c_chg;
	struct i2c_client *i2c_fg;

	struct regmap *regmap_sys;
	struct regmap *regmap_chg;
	struct regmap *regmap_fg;

	int battery_enable_gpio;
	int self_test_gpio;
};

enum max77818_irq {

	MAX77818_SRC_IRQ_FG = 0,

	MAX77818_SYS_IRQ_UVLO = 0,
	MAX77818_SYS_IRQ_OVLO,
	MAX77818_SYS_IRQ_TSHDN,
	MAX77818_SYS_IRQ_TM,

	MAX77818_CHG_IRQ_BYP_I = 0,
	MAX77818_CHG_IRQ_BATP_I,
	MAX77818_CHG_IRQ_BAT_I,
	MAX77818_CHG_IRQ_CHG_I,
	MAX77818_CHG_IRQ_WCIN_I,
	MAX77818_CHG_IRQ_CHGIN_I,
	MAX77818_CHG_IRQ_AICL_I,
};

int register_mode_notifier(struct notifier_block *n);
int unregister_mode_notifier(struct notifier_block *n);

#endif
