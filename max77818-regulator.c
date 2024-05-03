// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>

#include <linux/mfd/max77818-private.h>
#include <linux/mfd/max77818.h>

enum max77818_regulator_type {
	MAX77818_SAFEOUT1 = 0,
	MAX77818_SAFEOUT2,
	MAX77818_NUM
};

static const unsigned int max77818_safeout_table[] = {
	4850000,
	4900000,
	4950000,
	3300000,
};

static const struct regulator_ops max77818_safeout_ops = {
	.list_voltage    = regulator_list_voltage_table,
	.is_enabled      = regulator_is_enabled_regmap,
	.enable          = regulator_enable_regmap,
	.disable         = regulator_disable_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

#define max77818_regulator_desc_safeout(_num) {      \
	.name            = "SAFEOUT"#_num,i              \
	.id              = MAX77818_SAFEOUT##_num,       \
	.of_match        = of_match_ptr("SAFEOUT"#_num), \
	.regulators_node = of_match_ptr("regulators"),   \
	.n_voltages      = 4,                            \
	.ops             = &max77818_safeout_ops,        \
	.type            = REGULATOR_VOLTAGE,            \
	.owner           = THIS_MODULE,                  \
	.volt_table      = max77818_safeout_table,       \
	.vsel_reg        = REG_SAFEOUTCTRL,              \
	.vsel_mask       = BIT_SAFEOUT##_num,            \
	.enable_reg      = REG_SAFEOUTCTRL,              \
	.enable_mask     = BIT_ENSAFEOUT##_num,          \
}

static const struct regulator_desc max77818_supported_regulators[] = {
	max77818_regulator_desc_safeout(1),
	max77818_regulator_desc_safeout(2),
};

static int max77818_reg_probe(struct platform_device *pdev)
{

	struct max77818_dev *max77818 = dev_get_drvdata(pdev->dev.parent);
	const struct regulator_desc *regulators;
	unsigned int regulators_size;
	struct regulator_config config = { };
	int i;

	config.dev = max77818->dev;

	regulators = max77818_supported_regulators;
	regulators_size = ARRAY_SIZE(max77818_supported_regulators);

	for (i = 0; i < regulators_size; i++) {
		struct regulator_dev *rdev;

		config.regmap = max77818->regmap_sys;

		rdev = devm_regulator_register(&pdev->dev,
						&regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"Failed to initialize regulator-%d\n", i);
			return PTR_ERR(rdev);
		}
	}

	return 0;
}

static const struct of_device_id max77818_reg_of_ids[] = {

	{ .compatible = "maxim,max77818-reg" },
	{  },
};
MODULE_DEVICE_TABLE(of, max77818_reg_of_ids);

static const struct platform_device_id max77818_reg_id[] = {

	{ "max77818-reg", 0, },
	{  },
};
MODULE_DEVICE_TABLE(platform, max77818_reg_id);

static struct platform_driver max77818_reg_driver = {

	.driver = {
		.name = "max77818-reg",
		.owner = THIS_MODULE,
		.of_match_table = max77818_reg_of_ids,
	},
	.probe = max77818_reg_probe,
	.id_table = max77818_reg_id,
};
module_platform_driver(max77818_reg_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nebojsa Stojiljkovic <nebojsa@keemail.me");
MODULE_DESCRIPTION("MAX77818 Regulator Driver");
MODULE_ALIAS("mfd:max77818-rgl");
