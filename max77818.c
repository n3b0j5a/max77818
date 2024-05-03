// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>

#include <linux/mfd/max77818-private.h>
#include <linux/mfd/max77818.h>

struct mfd_cell max77818_devices[] = {
	{ .name = "max77818-reg", .of_compatible="maxim,max77818-reg" },
	{ .name = "max77818-fg",  .of_compatible="maxim,max77818-fg"},
	{ .name = "max77818-chg", .of_compatible="maxim,max77818-chg" },
};

static const struct regmap_config max77818_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config max77818_fg_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
	.cache_type = REGCACHE_NONE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static const struct regmap_irq max77818_src_irqs[] = {
	{ .reg_offset = 0, .mask = BIT_FG_INT },
};

static const struct regmap_irq_chip max77818_src_irq_chip = {
	.name = "max77818 src int",
	.status_base = REG_INTSRC,
	.mask_base = REG_INTSRCMASK,
	.num_regs = 1,
	.irqs = max77818_src_irqs,
	.num_irqs = ARRAY_SIZE(max77818_src_irqs),
};

static const struct regmap_irq max77818_sys_irqs[] = {
	{ .reg_offset = 0, .mask = BIT_SYSUVLO_INT },
	{ .reg_offset = 0, .mask = BIT_SYSOVLO_INT },
	{ .reg_offset = 0, .mask = BIT_TSHDN_INT },
	{ .reg_offset = 0, .mask = BIT_TM_INT },
};

static const struct regmap_irq_chip max77818_sys_irq_chip = {
	.name = "max77818 sys int",
	.status_base = REG_SYSINTSRC,
	.mask_base = REG_SYSINTMASK,
	.num_regs = 1,
	.irqs = max77818_sys_irqs,
	.num_irqs = ARRAY_SIZE(max77818_sys_irqs),
};

static const struct regmap_irq max77818_chg_irqs[] = {
	{ .reg_offset = 0, .mask = BIT_INT_BYP_I },
	{ .reg_offset = 0, .mask = BIT_INT_BATP_I },
	{ .reg_offset = 0, .mask = BIT_INT_BAT_I },
	{ .reg_offset = 0, .mask = BIT_INT_CHG_I },
	{ .reg_offset = 0, .mask = BIT_INT_WCIN_I },
	{ .reg_offset = 0, .mask = BIT_INT_CHGIN_I },
	{ .reg_offset = 0, .mask = BIT_INT_AICL_I },
};

static const struct regmap_irq_chip max77818_chg_irq_chip = {
	.name = "max77818 chg int",
	.status_base = REG_CHG_INT,
	.mask_base = REG_CHG_INT_MASK,
	.num_regs = 1,
	.irqs = max77818_chg_irqs,
	.num_irqs = ARRAY_SIZE(max77818_chg_irqs),
};

static int max77818_i2c_probe (struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct max77818_dev *max77818;
	struct device_node *np;
	int ret_val;
	unsigned int chip_id = 0, chip_rev = 0;

	max77818 = devm_kzalloc(&client->dev, sizeof(*max77818), GFP_KERNEL);
	if(!max77818){
		dev_err(max77818->dev, "%s: memory allocation failed\n", __func__);
		return -ENOMEM;
	}

	i2c_set_clientdata(client, max77818);

	max77818->dev = &client->dev;
	max77818->irq = client->irq;
	np = max77818->dev->of_node;
	if(np == NULL) {
		return -ENODEV;
	}

	dev_info(max77818->dev, "%s: allocated interrupt: %d\n", __func__, max77818->irq);

	max77818->i2c_sys = client;

	max77818->i2c_chg = i2c_new_dummy(client->adapter, CHARGER_I2C_ADDRESS);
	if(IS_ERR(max77818->i2c_chg)) {
		dev_err(max77818->dev, "%s: failed to allocate i2c device for charger!",__func__);
		return -ENODEV;
	}
	i2c_set_clientdata(max77818->i2c_chg, max77818);

	max77818->i2c_fg = i2c_new_dummy(client->adapter, FUELGAUGE_I2C_ADDRESS);
	if(IS_ERR(max77818->i2c_fg)) {
		dev_err(max77818->dev, "%s: failed to allocate i2c device for fuelgauge!",__func__);
		goto err_i2c_fg;
	}
	i2c_set_clientdata(max77818->i2c_fg, max77818);

	max77818->regmap_sys = devm_regmap_init_i2c(client, &max77818_regmap_config);
	if (IS_ERR(max77818->regmap_sys)) {
		dev_err(max77818->dev, "%s: failed to initialize pmic regmap!",__func__);
		goto err_regmap;
	}

	max77818->regmap_fg = devm_regmap_init_i2c(max77818->i2c_fg,
					&max77818_fg_regmap_config);
	if (IS_ERR(max77818->regmap_fg)) {
		dev_err(max77818->dev, "%s: failed to initialize fuelgauge regmap!",__func__);
		goto err_regmap;
	}

	max77818->regmap_chg = devm_regmap_init_i2c(max77818->i2c_chg,
							&max77818_regmap_config);
	if (IS_ERR(max77818->regmap_chg)) {
		dev_err(max77818->dev, "%s: failed to initialize charger regmap!",__func__);
		goto err_regmap;
	}

	regmap_read(max77818->regmap_sys, REG_PMICID, &chip_id);
	regmap_read(max77818->regmap_sys, REG_PMICREV, &chip_rev);
	if( chip_id != MAX77818_ID ) {
		dev_err(max77818->dev, "%s: max77818 ID mismatch! got: [%Xh]",__func__, chip_id);
		goto err_regmap;
	}

	ret_val = of_get_named_gpio(np, "battery-enable-gpios", 0);
	if(ret_val < 0) {
		max77818->battery_enable_gpio = GPIO_UNUSED;
		dev_warn(max77818->dev, "Could not get battery enable gpio from OF node (Optional)");
	} else {
		max77818->battery_enable_gpio = ret_val;
		dev_info(max77818->dev, "Got battery enable gpio: %d", ret_val);
	}
	ret_val = gpio_request(max77818->battery_enable_gpio, "battery_enable_gpio");
	if (ret_val < 0) {
		dev_err(max77818->dev, "Request gpio %d failed", max77818->battery_enable_gpio);
		return ret_val;
	} else {
		gpio_direction_output(max77818->battery_enable_gpio, 1);
	}

	ret_val = of_get_named_gpio(np, "self-test-gpios", 0);
	if(ret_val < 0) {
		max77818->self_test_gpio = GPIO_UNUSED;
		dev_warn(max77818->dev, "Could not get self test gpio from OF node (Optional)");
	} else {
		max77818->self_test_gpio = ret_val;
		dev_info(max77818->dev, "Got self test enable gpio: %d", ret_val);
	}
	ret_val = gpio_request(max77818->self_test_gpio, "self_test_gpio");
	if (ret_val < 0) {
		dev_err(max77818->dev, "Request gpio %d failed", max77818->self_test_gpio);
		return ret_val;
	} else {
		gpio_direction_output(max77818->self_test_gpio, 0);
	}

	ret_val = regmap_add_irq_chip(max77818->regmap_sys, max77818->irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, 0,
				&max77818_src_irq_chip,
				&max77818->irq_chip_src);
	if (ret_val != 0) {
		dev_err(max77818->dev, "%s: sys irq chip init failed: %d", __func__, ret_val);
		goto err_regmap;
	}

	ret_val = regmap_add_irq_chip(max77818->regmap_sys, max77818->irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, 0,
				&max77818_sys_irq_chip,
				&max77818->irq_chip_sys);
	if (ret_val != 0) {
		dev_err(max77818->dev, "%s: fg irq chip init failed: %d", __func__, ret_val);
		goto err_irq_src;
	}

	ret_val = regmap_add_irq_chip(max77818->regmap_chg, max77818->irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, 0,
				&max77818_chg_irq_chip,
				&max77818->irq_chip_chg);
	if (ret_val != 0) {
		dev_err(max77818->dev, "%s: chg irq chip init failed: %d", __func__, ret_val);
		goto err_irq_sys;
	}

	ret_val = mfd_add_devices(max77818->dev, -1, max77818_devices,
				ARRAY_SIZE(max77818_devices), NULL, 0, NULL);
	if (ret_val)
		goto err_irq_chg;

	dev_info(max77818->dev, "%s: max77818 init success. id: %Xh, rev: %X\n",__func__, chip_id, chip_rev);

	return 0;

err_irq_chg:
	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_chg);
err_irq_sys:
	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_sys);
err_irq_src:
	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_src);
err_regmap:
	i2c_unregister_device(max77818->i2c_fg);
err_i2c_fg:
	i2c_unregister_device(max77818->i2c_chg);

	return -1;
}

static int max77818_i2c_remove(struct i2c_client *i2c){

	struct max77818_dev *max77818 = i2c_get_clientdata(i2c);

	mfd_remove_devices(max77818->dev);

	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_src);
	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_sys);
	regmap_del_irq_chip(max77818->irq, max77818->irq_chip_chg);

	i2c_unregister_device(max77818->i2c_fg);
	i2c_unregister_device(max77818->i2c_chg);

	return 0;
}

static struct of_device_id max77818_of_id[] = {
	{ .compatible = "maxim,max77818" },
	{  },
};
MODULE_DEVICE_TABLE(of, max77818_of_id);

static const struct i2c_device_id max77818_i2c_id[] = {
	{ "max77818", 0 },
	{  },
};
MODULE_DEVICE_TABLE(i2c, max77818_i2c_id);

static struct i2c_driver max77818_i2c_driver = {
	.driver = {
		.name = "max77818",
		.owner = THIS_MODULE,
		.of_match_table = max77818_of_id,
	},
	.probe = max77818_i2c_probe,
	.remove = max77818_i2c_remove,
	.id_table = max77818_i2c_id,
};

module_i2c_driver(max77818_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nebojsa Stojiljkovic <nebojsa@keemail.me>");
MODULE_DESCRIPTION("MAX77818 MFD Core Driver");
MODULE_ALIAS("mfd:max77818-core");
