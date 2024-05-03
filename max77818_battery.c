// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#include <linux/power/max77818_battery.h>
#include <linux/mfd/max77818-private.h>
#include <linux/mfd/max77818.h>

BLOCKING_NOTIFIER_HEAD(mode_notifier_list);

int register_mode_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&mode_notifier_list,n);
}
EXPORT_SYMBOL_GPL(register_mode_notifier);

int unregister_mode_notifier(struct notifier_block *n)
{
	return blocking_notifier_chain_unregister(&mode_notifier_list,n);
}
EXPORT_SYMBOL_GPL(unregister_mode_notifier);


static struct timer_list shutdown_timer;

static void shutdown_timer_callback(struct timer_list * t)
{
	orderly_poweroff(true);
}

static void temperature_sync_work_handler(struct work_struct *work)
{
	struct delayed_work *d_work = container_of(work, struct delayed_work, work);
	struct max77818_fg_dev *fg = container_of(d_work, struct max77818_fg_dev, d_work);

	if (fg->temp_status == MAX77818_TEMP_NORMAL) {
		blocking_notifier_call_chain(&mode_notifier_list,5,NULL);
	} else {
		blocking_notifier_call_chain(&mode_notifier_list,4,NULL);
	}
}

static int max77818_fg_write_custom_reg(struct max77818_fg_dev *fg,
					unsigned int reg, unsigned int val)
{
	int ret_val;
	ret_val = regmap_write(fg->regmap, reg, val);
	if (ret_val)
		dev_err(fg->dev, "Fail to write reg 0x%04x", reg);

	return ret_val;
}

static int max77818_fg_read_custom_reg(struct max77818_fg_dev *fg,
				       unsigned int reg, unsigned int *val)
{
	int ret_val;
	ret_val = regmap_read(fg->regmap, reg, val);
	if (ret_val)
		dev_err(fg->dev, "Fail to read reg 0x%04x", reg);

	return ret_val;
}

static int max77818_fg_write_verify_custom_reg(struct max77818_fg_dev *fg,
					       unsigned int reg, unsigned int val)
{
	int ret_val;
	unsigned int data = 0;

	ret_val = regmap_write(fg->regmap, reg, val);
	if (ret_val) {
		dev_err(fg->dev, "Fail to write reg: 0x%04x", reg);
		return ret_val;
	}
	ret_val = regmap_read(fg->regmap, reg, &data);
	if (ret_val) {
		dev_err(fg->dev, "Fail to read reg: 0x%04x", reg);
		return ret_val;
	}
	if(data != val) {
		dev_err(fg->dev, "Fail to verify reg: 0x%04x", reg);
	}
	return ret_val;
}

static int max77818_fg_write_model(struct max77818_fg_dev *fg)
{
	struct max77818_fg_platform_data *pdata = fg->pdata;
	int i, data;
	int ret_val;

	/* Unlock model */
	ret_val = max77818_fg_write_custom_reg(fg, REG_MLOCKReg1,
					       MAX77818_MODEL_UNLOCK1);
	if (ret_val) {
		return ret_val;
	}
	ret_val = max77818_fg_write_custom_reg(fg, REG_MLOCKReg2,
					       MAX77818_MODEL_UNLOCK2);
	if (ret_val) {
		return ret_val;
	}

	/* Write battery model */
	for (i = 0; i < MAX77818_OCV_LENGTH; i++) {
		ret_val = max77818_fg_write_custom_reg(fg, REG_OCV + i,
						       pdata->battery_ocv_model[i]);
		if (ret_val) {
			dev_err(fg->dev, "OCV table write failed\n");
			return ret_val;
		}
	}

	/* Verify battery model */
	for (i = 0; i < MAX77818_OCV_LENGTH; i++) {
		ret_val = max77818_fg_read_custom_reg(fg, REG_OCV + i, &data);
		if (ret_val < 0 || (data != pdata->battery_ocv_model[i])) {
			dev_err(fg->dev,  "OCV table verify failed at 0x%02x:\n",
				REG_OCV + i);
			return ret_val;
		}
	}

	/* Lock model */
	ret_val = max77818_fg_write_custom_reg(fg, REG_MLOCKReg1,
					       MAX77818_MODEL_LOCK);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_MLOCKReg2,
					       MAX77818_MODEL_LOCK);
	if (ret_val) {
		return ret_val;
	}

	/* Verify that model is locked */
	for (i = 0; i < MAX77818_OCV_LENGTH; i++) {
		ret_val = max77818_fg_read_custom_reg(fg, REG_OCV + i, &data);
		if (ret_val || (data != 0x0000)) {
			dev_err(fg->dev, "OCV table model lock failed\n");
			return ret_val;
		}
	}

	return 0;
}


static int max77818_fg_load_model(struct max77818_fg_dev *fg)
{
	int val, timeout=6500;
	int ret_val;

	ret_val = regmap_write_bits(fg->regmap, REG_Config2, BIT_LdMdl, 1<<FFS(BIT_LdMdl));
	if (ret_val) {
		return ret_val;
	}

	do {
		mdelay(1);
		ret_val = max77818_fg_read_custom_reg(fg, REG_Config2, &val);
		if (ret_val < 0) {
			return ret_val;
		}
		timeout = timeout - 10;
	} while( val & (1<<FFS(BIT_LdMdl) != 0) && (timeout != 0));

	if (val != 0 && timeout == 0) {
		return -EIO;
	}

	return 0;
}

static int max77818_fg_restore_learned_params(struct max77818_fg_dev *fg)
{
	int ret_val;
	int dqacc;

	ret_val = max77818_fg_write_custom_reg(fg, REG_RComp0,
					       fg->learned->rcomp0);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_TempCo,
					       fg->learned->temp_co);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_FullCapRep,
						      fg->learned->full_cap_rep);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_Cycles,
						      fg->learned->cycles);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable00,
						      fg->learned->qresidual00);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable10,
						      fg->learned->qresidual10);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable20,
						      fg->learned->qresidual20);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable30,
						      fg->learned->qresidual30);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_CV_MixCap,
						      fg->learned->cv_mixcap);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_CV_HalfTime,
						      fg->learned->cv_halftime);
	if(ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_dPacc, 0x3200);
	if(ret_val)
		return ret_val;

	dqacc = fg->learned->full_cap_nom / 4;
	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_dQacc, dqacc);
	if(ret_val)
		return ret_val;


	ret_val = max77818_fg_load_model(fg);
	if(ret_val)
		return ret_val;

	return 0;
}

static enum power_supply_property max77818_fg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_AVG,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_MAX,
	POWER_SUPPLY_PROP_TEMP_MIN,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int max77818_fg_property_is_writable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_CHARGE_NOW:
	case POWER_SUPPLY_PROP_CHARGE_AVG:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TEMP_MAX:
	case POWER_SUPPLY_PROP_TEMP_MIN:
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
	case POWER_SUPPLY_PROP_MODEL_NAME:
	case POWER_SUPPLY_PROP_MANUFACTURER:
		return 0;
	default:
		return -EINVAL;
	}
}

static int max77818_fg_get_capacity_level(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_RepSOC, &data);
	if (ret_val < 0)
		return ret_val;

	data = data >> 8;

	if (data > MAX77818_BATTERY_FULL)
		*val = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (data > MAX77818_BATTERY_HIGH)
		*val = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (data > MAX77818_BATTERY_NORMAL)
		*val = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (data > MAX77818_BATTERY_LOW)
		*val = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (data >= MAX77818_BATTERY_CRITICAL)
		*val = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		*val = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;

	return 0;
}

static int max77818_fg_get_capacity(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_RepSOC, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data >> 8;

	return 0;
}

static int max77818_fg_get_voltage_now(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_Vcell, &data);
	if (ret_val < 0)
		return ret_val;

	*val = (data * 625) / 8;

	return 0;
}

static int max77818_fg_get_voltage_avg(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_AvgVCell, &data);
	if (ret_val < 0)
		return ret_val;

	*val = (data * 625) / 8;

	return 0;
}

static int max77818_fg_get_voltage_ocv(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_VFOCV, &data);
	if (ret_val < 0)
		return ret_val;

	*val = (data * 625) / 8;

	return 0;
}

static int max77818_fg_get_voltage_max(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_MaxMinVolt, &data);
	if (ret_val < 0)
		return ret_val;

	*val = ((data & 0xFF00) >> 8) * 20000;

	return 0;
}

static int max77818_fg_get_voltage_min(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_MaxMinVolt, &data);
	if (ret_val < 0)
		return ret_val;

	*val = (data & 0x00FF) * 20000;

	return 0;
}

static int max77818_fg_get_current_now(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_Current, &data);
	if (ret_val < 0)
		return ret_val;
	if (data & 0x8000) {
		data = ((~data & 0x7FFF) + 1);
		*val = ((data * 15625) / 100);
	}

	*val = (data * 15625) / 100;

	return 0;
}

static int max77818_fg_get_current_avg(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_AvgCurrent, &data);
	if (ret_val < 0)
		return ret_val;

	if (data & 0x8000) {
		data = ((~data & 0x7FFF) + 1);
		*val = ((data * 156250) / 1000);
	}

	*val = (data * 156250) / 1000;

	return 0;
}

static int max77818_fg_get_design_charge(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_DesignCap, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data * 500;

	return 0;
}

static int max77818_fg_get_full_charge(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_FullCap, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data * 500;

	return 0;
}

static int max77818_fg_get_avg_charge(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_AvCap, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data * 500;

	return 0;
}

static int max77818_fg_get_charge(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_RepCap, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data * 500;

	return 0;
}

static int max77818_fg_get_cycle_count(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val =  max77818_fg_read_custom_reg(fg, REG_Cycles, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data;

	return ret_val;
}

static int max77818_fg_get_temp(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_Temp, &data);
	if (ret_val < 0)
		return ret_val;

	if (data & 0x8000) {
		data = (~data & 0x7FFF) + 1;
		*val = (-1)*((data * 10) / 256);
	} else {
		*val = (data * 10) / 256;
	}

	return 0;
}

static int max77818_fg_get_max_temp(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_MaxMinTemp, &data);
	if (ret_val < 0)
		return ret_val;

	data = data >> 8;
	if (data & 0x80) {
		*val = (-10)*(~data & 0x7F) + 10;
	} else {
		*val = (data * 10);
	}

	return 0;
}

static int max77818_fg_get_min_temp(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_MaxMinTemp, &data);
	if (ret_val < 0)
		return ret_val;

	data = data & 0xFF;

	if (data & 0x80) {
		*val = (-10)*(~data & 0x7F) + 10;
	} else {
		*val = (data * 10);
	}

	return 0;
}

static int max77818_fg_get_time_to_full(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_TTF, &data);
	if (ret_val < 0)
		return ret_val;

	*val  = ((data & 0xFC00) >> 10) *  5760;
	*val += ((data & 0x03F0) >> 4) * 90;
	*val += ((data & 0x000F) * 5625) / 1000;

	return 0;
}

static int max77818_fg_get_time_to_empty(struct max77818_fg_dev *fg, int *val)
{
	unsigned int data;
	unsigned int ret_val;

	ret_val = max77818_fg_read_custom_reg(fg, REG_TTE, &data);
	if (ret_val < 0)
		return ret_val;

	*val  = ((data & 0xFC00) >> 10) *  5760;
	*val += ((data & 0x03F0) >> 4) * 90;
	*val += ((data & 0x000F) * 5625) / 1000;

	return 0;
}


static int max77818_fg_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	struct max77818_fg_dev *fg = power_supply_get_drvdata(psy);
	int ret_val = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret_val = max77818_fg_get_capacity_level(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret_val = max77818_fg_get_capacity(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret_val = max77818_fg_get_voltage_now(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret_val = max77818_fg_get_voltage_avg(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret_val = max77818_fg_get_voltage_ocv(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		ret_val = max77818_fg_get_voltage_max(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN:
		ret_val = max77818_fg_get_voltage_min(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret_val = max77818_fg_get_current_now(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret_val = max77818_fg_get_current_avg(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret_val = max77818_fg_get_design_charge(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret_val = max77818_fg_get_full_charge(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_AVG:
		ret_val = max77818_fg_get_avg_charge(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret_val = max77818_fg_get_charge(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret_val = max77818_fg_get_cycle_count(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret_val = max77818_fg_get_temp(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_MAX:
		ret_val = max77818_fg_get_max_temp(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TEMP_MIN:
		ret_val = max77818_fg_get_min_temp(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret_val = max77818_fg_get_time_to_empty(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret_val = max77818_fg_get_time_to_full(fg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		ret_val = 0;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "maxim";
		ret_val = 0;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = "max77818-fuelgauge";
		ret_val = 0;
		break;
	default:
		ret_val = -EINVAL;
	}

	if (ret_val)
		dev_err(fg->dev, "%s: get property %d failed with: %d\n",
			__func__, psp, ret_val);

	return ret_val;
}

static int max77818_fg_set_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    const union power_supply_propval *val)
{
	int ret_val;
	struct max77818_fg_dev *fg = power_supply_get_drvdata(psy);

	switch (psp) {
	default:
		ret_val = -EINVAL;
	}
	if (ret_val)
		dev_err(fg->dev, "%s: set property %d failed with: %d\n",
			__func__, psp, ret_val);

	return ret_val;
}


static ssize_t learned_rcomp0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_RComp0,
					      &fg->learned->rcomp0);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->rcomp0);
}

static ssize_t learned_rcomp0_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->rcomp0);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_temp_co_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_TempCo,
					      &fg->learned->temp_co);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->temp_co);
}

static ssize_t learned_temp_co_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->temp_co);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_full_cap_rep_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_FullCapRep,
					      &fg->learned->full_cap_rep);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->full_cap_rep);
}

static ssize_t learned_full_cap_rep_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->full_cap_rep);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_cycles_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_Cycles,
					      &fg->learned->cycles);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->cycles);
}

static ssize_t learned_cycles_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->cycles);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_full_cap_nom_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_FullCapNom,
					      &fg->learned->full_cap_nom);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->full_cap_nom);
}

static ssize_t learned_full_cap_nom_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->full_cap_nom);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_qresidual00_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_QRTable00,
					      &fg->learned->qresidual00);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->qresidual00);
}

static ssize_t learned_qresidual00_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->qresidual00);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_qresidual10_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_QRTable10,
					      &fg->learned->qresidual10);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->qresidual10);
}

static ssize_t learned_qresidual10_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->qresidual10);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_qresidual20_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_QRTable20,
					      &fg->learned->qresidual20);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->qresidual20);
}

static ssize_t learned_qresidual20_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->qresidual20);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_qresidual30_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_QRTable30,
					      &fg->learned->qresidual30);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->qresidual30);
}

static ssize_t learned_qresidual30_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->qresidual30);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_cv_mixcap_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_MixCap,
					      &fg->learned->cv_mixcap);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->cv_mixcap);
}

static ssize_t learned_cv_mixcap_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->cv_mixcap);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t learned_cv_halftime_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_CV_HalfTime,
					      &fg->learned->cv_halftime);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", fg->learned->cv_halftime);
}

static ssize_t learned_cv_halftime_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	int ret_val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &fg->learned->cv_halftime);
	if (ret_val)
		return ret_val;

	return count;
}

static ssize_t load_params_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret_val;
	int val;

	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = kstrtoint(buf, 10, &val);
	if (ret_val)
		return ret_val;
	if (val == 1) {
		ret_val = max77818_fg_restore_learned_params(fg);
		if (ret_val)
			return ret_val;
	}
	return count;
}

static ssize_t ain0_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int ret_val;
	unsigned int val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);

	ret_val = max77818_fg_read_custom_reg(fg, REG_AIN0,
					      &val);
	if (ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t self_test_show(struct device *dev, 
					struct device_attribute *attr, char *buf)
{
	int val;
	struct max77818_fg_dev *fg = dev_get_drvdata(dev);
	blocking_notifier_call_chain(&mode_notifier_list,12,NULL);
	gpio_set_value(fg->max77818->self_test_gpio, 1);
	mdelay(5000);
	max77818_fg_get_voltage_now(fg, &val);
	gpio_set_value(fg->max77818->self_test_gpio, 0);
	if (fg->temp_status == MAX77818_TEMP_NORMAL) {
		blocking_notifier_call_chain(&mode_notifier_list,5,NULL);
	} else {
		blocking_notifier_call_chain(&mode_notifier_list,4,NULL);
	}

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static DEVICE_ATTR_RW(learned_rcomp0);
static DEVICE_ATTR_RW(learned_temp_co);
static DEVICE_ATTR_RW(learned_full_cap_rep);
static DEVICE_ATTR_RW(learned_cycles);
static DEVICE_ATTR_RW(learned_full_cap_nom);
static DEVICE_ATTR_RW(learned_qresidual00);
static DEVICE_ATTR_RW(learned_qresidual10);
static DEVICE_ATTR_RW(learned_qresidual20);
static DEVICE_ATTR_RW(learned_qresidual30);
static DEVICE_ATTR_RW(learned_cv_mixcap);
static DEVICE_ATTR_RW(learned_cv_halftime);
static DEVICE_ATTR_WO(load_params);
static DEVICE_ATTR_RO(ain0);
static DEVICE_ATTR_RO(self_test);

static struct power_supply_config max77818_fg_config = {

};

static const struct power_supply_desc max77818_fg_desc = {
	.name = "max77818-fg",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = max77818_fg_props,
	.num_properties = ARRAY_SIZE(max77818_fg_props),
	.get_property = max77818_fg_get_property,
	.set_property = max77818_fg_set_property,
	.property_is_writeable = max77818_fg_property_is_writable,
};

static int max77818_fg_reg_init(struct max77818_fg_dev *fg)
{
	struct max77818_fg_platform_data *pdata = fg->pdata;
	unsigned int data;
	int ret_val;

	/* Check for Status POR bit */
	ret_val = max77818_fg_read_custom_reg(fg, REG_Status,
					      &data);
	if (ret_val < 0) {
		return ret_val;
	}
	if ((data & BIT_POR) == 0) {
		dev_info(fg->dev, "Fuelgauge already set up\n");
		return 0;
	}

	/* Load battery model */
	ret_val = max77818_fg_write_model(fg);
	if (ret_val) {
		return ret_val;
	}

	/* Write custom register values */
	ret_val=max77818_fg_write_custom_reg(fg, REG_RepCap, 0x0000);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_read_custom_reg(fg, REG_VFSOC,
					      &pdata->vfsoc0);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_VFSOC0,
						      pdata->vfsoc0);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_DesignCap,
					       pdata->design_cap);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_Config,
					       pdata->config);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_Config2,
					       pdata->config2);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_dQacc,
						      pdata->dqacc);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_dPacc,
						      pdata->dpacc);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_FilterCfg,
					       pdata->filter_cfg);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_FullCapNom,
						      pdata->full_cap_nom);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_FullCapRep,
						      pdata->full_cap_rep);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_FullSocThr,
					       pdata->full_soc_thr);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_IavgEmpty,
					       pdata->iavg_empty);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_IchgTerm,
					       pdata->i_chg_term);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_LearnCfg,
					       pdata->learn_cfg);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable00,
						      pdata->qresidual00);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable10,
						      pdata->qresidual10);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable20,
						      pdata->qresidual20);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_QRTable30,
						      pdata->qresidual30);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_RComp0,
						      pdata->rcomp0);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_RelaxCfg,
					       pdata->relax_cfg);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_verify_custom_reg(fg, REG_TempCo,
						      pdata->temp_co);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_V_empty,
					       pdata->v_empty);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_TGain,
					       pdata->tgain);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_TOff,
					       pdata->toff);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_Curve,
					       pdata->curve);
	if (ret_val)
		return ret_val;

	//Restart max and min temperature counters
	ret_val = max77818_fg_write_custom_reg(fg, REG_MaxMinTemp,
				       0x007F);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_VFSOC0Enable,
					       0x0080);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_AtRate,
					       pdata->at_rate);
	if (ret_val) {
		return ret_val;
	}

	if (pdata->cv_mixcap) {
		ret_val = max77818_fg_write_verify_custom_reg(fg,
							      REG_CV_MixCap,
							      pdata->cv_mixcap);
		if (ret_val) {
			return ret_val;
		}
		ret_val = max77818_fg_write_verify_custom_reg(fg,
							      REG_CV_HalfTime,
							      pdata->cv_halftime);
		if (ret_val) {
			return ret_val;
		}
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_SmartChgCfg,
					       pdata->smartchgcfg);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_ConvgCfg,
					       pdata->convg_cfg);
	if (ret_val) {
		return ret_val;
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_VFSOC0Enable,
					       0x0000);
	if (ret_val) {
		return ret_val;
	}

	/* Load model */
	ret_val = max77818_fg_load_model(fg);
	if (ret_val) {
		return ret_val;
	}

	return 0;
}

static int max77818_fg_parse_dt(struct max77818_fg_dev *fg)
{
	struct max77818_fg_platform_data *pdata = fg->pdata;
	struct device_node *np = of_find_node_by_name(fg->dev->parent->of_node, "fuelgauge");
	unsigned int *ocv_model;

	ocv_model = kzalloc(sizeof(ocv_model)*MAX77818_OCV_LENGTH, GFP_KERNEL);
	if (!ocv_model) {
		dev_err(fg->dev,  "%s: memory allocation failed\n",  __func__);
		return -ENOMEM;
	}
	if (of_property_read_u32_array(np, "battery_ocv_model", ocv_model, MAX77818_OCV_LENGTH)) {
		dev_warn(fg->dev, "OCV table not found\n");
		kfree(ocv_model);
		return -EINVAL;
	}
	pdata->battery_ocv_model = ocv_model;

	if (of_property_read_u32(np, "design_cap", &pdata->design_cap)) {
		dev_err(fg-> dev, "Property design_cap not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "config", &pdata->config)) {
		dev_err(fg-> dev, "Property config not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "config2", &pdata->config2)) {
		dev_err(fg-> dev, "Property config2 not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "dpacc", &pdata->dpacc)) {
		dev_err(fg-> dev, "Property dpacc not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "dqacc", &pdata->dqacc)) {
		dev_err(fg-> dev, "Property dqacc not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "filter_cfg", &pdata->filter_cfg)) {
		dev_err(fg-> dev, "Property filter_cfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "full_cap_nom", &pdata->full_cap_nom)) {
		dev_err(fg-> dev, "Property full_cap_nom not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "full_cap_rep", &pdata->full_cap_rep)) {
		dev_err(fg-> dev, "Property full_cap_rep not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "full_soc_thr", &pdata->full_soc_thr)) {
		dev_err(fg-> dev, "Property full_soc_thr not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "iavg_empty", &pdata->iavg_empty)) {
		dev_err(fg-> dev, "Property iavg_empty not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "i_chg_term", &pdata->i_chg_term)) {
		dev_err(fg-> dev, "Property i_charge_term not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "learn_cfg", &pdata->learn_cfg)) {
		dev_err(fg-> dev, "Property learn_cfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "qresidual00", &pdata->qresidual00)) {
		dev_err(fg-> dev, "Property qresidual00  not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "qresidual10", &pdata->qresidual10)) {
		dev_err(fg-> dev, "Property qresidual10 not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "qresidual20", &pdata->qresidual20)) {
		dev_err(fg-> dev, "Property qresidual20 not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "qresidual30", &pdata->qresidual30)) {
		dev_err(fg-> dev, "Property qresidual30 not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "rcomp0", &pdata->rcomp0)) {
		dev_err(fg-> dev, "Property rcomp0 not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "relax_cfg", &pdata->relax_cfg)) {
		dev_err(fg-> dev, "Property relax_cfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "temp_co", &pdata->temp_co)) {
		dev_err(fg-> dev, "Property relax_cfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "v_empty", &pdata->v_empty)) {
		dev_err(fg-> dev, "Property v_empty not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "tgain", &pdata->tgain)) {
		dev_err(fg-> dev, "Property tgain not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "toff", &pdata->toff)) {
		dev_err(fg-> dev, "Property toff not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "curve", &pdata->curve)) {
		dev_err(fg-> dev, "Property curve not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "at_rate", &pdata->at_rate)) {
		dev_err(fg-> dev, "Property at_rate not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "cv_mixcap", &pdata->cv_mixcap)) {
		dev_warn(fg-> dev, "Property cv_mixcap not found.\n");
	}

	if (of_property_read_u32(np, "cv_halftime", &pdata->cv_halftime)) {
		dev_warn(fg-> dev, "Property cv_halftime not found.\n");
	}

	if (of_property_read_u32(np, "smartchgcfg", &pdata->smartchgcfg)) {
		dev_err(fg-> dev, "Property smartchgcfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "convg_cfg", &pdata->convg_cfg)) {
		dev_err(fg-> dev, "Property convg_cfg not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "talrt_low", &pdata->talrt_low)) {
		dev_err(fg-> dev, "Property talrt_low not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "talrt_norm", &pdata->talrt_norm)) {
		dev_err(fg-> dev, "Property talrt_norm not found.\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "talrt_high", &pdata->talrt_high)) {
		dev_err(fg-> dev, "Property talrt_high not found.\n");
		return -EINVAL;
	}

	dev_dbg(fg->dev, "design_cap: 0x%04x\n", pdata->design_cap);
	dev_dbg(fg->dev, "config: 0x%04x\n", pdata->config);
	dev_dbg(fg->dev, "config2: 0x%04x\n", pdata->config2);
	dev_dbg(fg->dev, "dqacc: 0x%04x\n", pdata->dqacc);
	dev_dbg(fg->dev, "dpacc: 0x%04x\n", pdata->dpacc);
	dev_dbg(fg->dev, "filter_cfg: 0x%04x\n", pdata->filter_cfg);
	dev_dbg(fg->dev, "full_cap_nom: 0x%04x\n", pdata->full_cap_nom);
	dev_dbg(fg->dev, "full_cap_rep: 0x%04x\n", pdata->full_cap_rep);
	dev_dbg(fg->dev, "full_soc_thr: 0x%04x\n", pdata->full_soc_thr);
	dev_dbg(fg->dev, "iavg_empty: 0x%04x\n", pdata->iavg_empty);
	dev_dbg(fg->dev, "i_charge_term: 0x%04x\n", pdata->i_chg_term);
	dev_dbg(fg->dev, "learn_cfg: 0x%04x\n", pdata->learn_cfg);
	dev_dbg(fg->dev, "qresidual00: 0x%04x\n", pdata->qresidual00);
	dev_dbg(fg->dev, "qresidual10: 0x%04x\n", pdata->qresidual10);
	dev_dbg(fg->dev, "qresidual20: 0x%04x\n", pdata->qresidual20);
	dev_dbg(fg->dev, "qresidual30: 0x%04x\n", pdata->qresidual30);
	dev_dbg(fg->dev, "rcomp0: 0x%04x\n", pdata->rcomp0);
	dev_dbg(fg->dev, "relax_cfg: 0x%04x\n", pdata->relax_cfg);
	dev_dbg(fg->dev, "temp_co: 0x%04x\n", pdata->temp_co);
	dev_dbg(fg->dev, "v_empty: 0x%04x\n", pdata->v_empty);
	dev_dbg(fg->dev, "tgain: 0x%04x\n", pdata->tgain);
	dev_dbg(fg->dev, "toff: 0x%04x\n", pdata->toff);
	dev_dbg(fg->dev, "toff: 0x%04x\n", pdata->toff);
	dev_dbg(fg->dev, "at_rate: 0x%04x\n", pdata->at_rate);
	dev_dbg(fg->dev, "cv_mixcap: 0x%04x\n", pdata->cv_mixcap);
	dev_dbg(fg->dev, "cv_halftime: 0x%04x\n", pdata->cv_halftime);
	dev_dbg(fg->dev, "smartchgcfg: 0x%04x\n", pdata->smartchgcfg);
	dev_dbg(fg->dev, "convg_cfg: 0x%04x\n", pdata->convg_cfg);
	dev_dbg(fg->dev, "talrt_low: 0x%04x\n", pdata->talrt_low);
	dev_dbg(fg->dev, "talrt_norm: 0x%04x\n", pdata->talrt_norm);
	dev_dbg(fg->dev, "talrt_high: 0x%04x\n", pdata->talrt_high);

	return 0;
}

static int max77818_fg_alert_init(struct max77818_fg_dev *fg)
{
	int ret_val;

	fg->temp_status = MAX77818_TEMP_NORMAL;

	ret_val = max77818_fg_write_custom_reg(fg, REG_TAlrtTh, fg->pdata->talrt_norm);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_VAlrtTh, 0xFF00);
	if (ret_val)
		return ret_val;

	ret_val = max77818_fg_write_custom_reg(fg, REG_SAlrtTh, 0xFF00);
	if (ret_val)
		return ret_val;

	ret_val = regmap_update_bits(fg->regmap, REG_Config, BIT_Aen, 1<<FFS(BIT_Aen));
	if (ret_val)
		return ret_val;

	ret_val = regmap_update_bits(fg->regmap, REG_Config2, BIT_dSOCen, 1<<FFS(BIT_dSOCen));
	if (ret_val)
		return ret_val;

	ret_val = regmap_update_bits(fg->regmap, REG_Config2, BIT_TAlrtEn, 1<<FFS(BIT_TAlrtEn));
	if (ret_val)
		return ret_val;

	return 0;
}

static irqreturn_t max77818_fg_isr(int irq, void *dev) {


	struct max77818_fg_dev *fg = dev;
	int ret_val;
	int vcell = 0, soc = 0, data = 0, temp = 0;


	ret_val = max77818_fg_read_custom_reg(fg, REG_Status, &data);
	if (ret_val)
		return IRQ_NONE;

	if (data & BIT_dSOCi) {
		max77818_fg_get_voltage_now(fg, &vcell);
		max77818_fg_get_capacity(fg, &soc);
		dev_info(fg->dev, "max77818 fuelgauge status changed: SOC=%d, VCELL=%d\n", soc, vcell);
		power_supply_changed(fg->fuelgauge);
	}

	if (data & BIT_Tmx || data & BIT_Tmn) {
		dev_dbg(fg->dev, "Temperature alert activated: %d\n", temp);
		max77818_fg_get_temp(fg, &temp);
		switch (fg->temp_status) {
		case MAX77818_TEMP_LOW:
			if(data & BIT_Tmn) {
				//emergency shutdown after timeout
				dev_err(fg->dev, "Temperature level critical low: %d. Shutting down...", temp);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, 0x7f80);
				mod_timer(&shutdown_timer, jiffies + msecs_to_jiffies(30000));
			} else if (data & BIT_Tmx) {
				dev_info(fg->dev, "Temperature level back to normal: %d", temp);
				fg->temp_status = MAX77818_TEMP_NORMAL;
				blocking_notifier_call_chain(&mode_notifier_list,5,NULL);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, fg->pdata->talrt_norm);
			}
			break;
		case MAX77818_TEMP_NORMAL:
			if(data & BIT_Tmn) {
				dev_warn(fg->dev, "Temperature level low: %d", temp);
				fg->temp_status = MAX77818_TEMP_LOW;
				blocking_notifier_call_chain(&mode_notifier_list,4,NULL);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, fg->pdata->talrt_low);
			} else if (data & BIT_Tmx) {
				dev_warn(fg->dev, "Temperature level high: %d", temp);
				fg->temp_status = MAX77818_TEMP_HIGH;
				blocking_notifier_call_chain(&mode_notifier_list,4,NULL);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, fg->pdata->talrt_high);
			}
			break;
		case MAX77818_TEMP_HIGH:
			if(data & BIT_Tmn) {
				dev_info(fg->dev, "Temperature level back to normal: %d", temp);
				fg->temp_status = MAX77818_TEMP_NORMAL;
				blocking_notifier_call_chain(&mode_notifier_list,5,NULL);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, fg->pdata->talrt_norm);
			} else if (data & BIT_Tmx) {
				//emergency shutdown after timeout
				dev_err(fg->dev, "Temperature level critical high: %d. Shutting down", temp);
				max77818_fg_write_custom_reg(fg, REG_TAlrtTh, 0x7f80);
				mod_timer(&shutdown_timer, jiffies + msecs_to_jiffies(30000));
			}
			break;
		}
	}

	ret_val = max77818_fg_write_custom_reg(fg, REG_Status, 0x0000);
	if (ret_val)
		return IRQ_NONE;

	return IRQ_HANDLED;
}

static int max77818_fg_probe(struct platform_device *pdev)
{
	struct max77818_dev *max77818 = dev_get_drvdata(pdev->dev.parent);
	struct max77818_fg_platform_data *pdata;
	struct max77818_fg_learned_params *learned;
	struct max77818_fg_dev *fg;
	struct power_supply *fuelgauge;

	int ret_val = 0;

	fg = kzalloc(sizeof(*fg), GFP_KERNEL);
	if (!fg) {
		dev_err(fg->dev,  "%s: memory allocation failed\n",  __func__);
		return -ENOMEM;
	}
	pdata = devm_kzalloc(&pdev->dev,  sizeof(*pdata),  GFP_KERNEL);
	if (!pdata) {
		dev_err(fg->dev,  "%s: memory allocation failed\n",  __func__);
		return -ENOMEM;
	}
	learned = devm_kzalloc(&pdev->dev,  sizeof(*learned),  GFP_KERNEL);
	if (!learned) {
		dev_err(fg->dev,  "%s: memory allocation failed\n",  __func__);
		return -ENOMEM;
	}

	timer_setup(&shutdown_timer, shutdown_timer_callback, 0);

	fg->pdata = pdata;
	fg->learned = learned;
	fg->dev = &pdev->dev;
	fg->max77818 = max77818;
	fg->regmap = max77818->regmap_fg;
	fg->irq_chip = max77818->irq_chip_src;

	platform_set_drvdata(pdev, fg);

	ret_val = max77818_fg_parse_dt(fg);
	if (ret_val) {
		dev_err(fg->dev, "%s: parse device tree failed: %d\n",
			__func__, ret_val);
		return ret_val;
	}

	max77818_fg_config.drv_data = fg;

	fuelgauge = power_supply_register(fg->dev,  &max77818_fg_desc,
					  &max77818_fg_config);
	if (IS_ERR(fuelgauge)) {
		return PTR_ERR(fuelgauge);
	}
	fg->fuelgauge = fuelgauge;

	fg->virq = regmap_irq_get_virq(fg->irq_chip, MAX77818_SRC_IRQ_FG);
	if (!fg->virq) {
		dev_warn(fg->dev, "get virq for fg failed\n");
	}

	ret_val = request_threaded_irq(fg->virq, NULL,  max77818_fg_isr,
				       IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				       "max77818 fg int", fg);
	if (ret_val) {
		dev_err(fg->dev, "%s: virq request failed: %d\n",
			__func__, ret_val);
		goto err_virq;
	}

	ret_val = max77818_fg_reg_init(fg);
	if (ret_val) {
		dev_err(fg->dev, "%s: reg init failed: %d\n",
			__func__, ret_val);
		goto err_reg_init;
	}

	ret_val = max77818_fg_alert_init(fg);
	if (ret_val) {
		dev_err(fg->dev, "%s: alert init failed: %d\n",
			__func__, ret_val);
		goto err_alert_init;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_rcomp0);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_rcomp0 file\n");
		goto err_rcomp0;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_temp_co);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_temp_co file\n");
		goto err_temp_co;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_full_cap_rep);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_full_cap_rep file\n");
		goto err_full_cap_rep;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_cycles);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_cycles file\n");
		goto err_cycles;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_full_cap_nom);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_full_cap_nom file\n");
		goto err_full_cap_nom;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_qresidual00);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_qresidual00 file\n");
		goto err_qresidual00;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_qresidual10);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_qresidual10 file\n");
		goto err_qresidual10;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_qresidual20);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_qresidual20 file\n");
		goto err_qresidual20;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_qresidual30);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_qresidual30 file\n");
		goto err_qresidual30;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_cv_mixcap);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_cv_mixcap file\n");
		goto err_cv_mixcap;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_learned_cv_halftime);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create learned_cv_halftime file\n");
		goto err_cv_halftime;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_load_params);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create load_params file\n");
		goto err_load_params;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_self_test);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create ain0 file\n");
		goto err_self_test;
	}

	ret_val = device_create_file(fg->dev, &dev_attr_ain0);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create ain0 file\n");
		goto err;
	}

	//Sync temperature and charger mode after chgarger driver is loaded
	INIT_DELAYED_WORK(&fg->d_work, temperature_sync_work_handler);
	schedule_delayed_work(&fg->d_work, msecs_to_jiffies(1000));

	return 0;

err:
	device_remove_file(fg->dev, &dev_attr_ain0);
err_self_test:
	device_remove_file(fg->dev, &dev_attr_self_test);
err_load_params:
	device_remove_file(fg->dev, &dev_attr_load_params);
err_cv_halftime:
	device_remove_file(fg->dev, &dev_attr_learned_cv_mixcap);
err_cv_mixcap:
	device_remove_file(fg->dev, &dev_attr_learned_qresidual30);
err_qresidual30:
	device_remove_file(fg->dev, &dev_attr_learned_qresidual20);
err_qresidual20:
	device_remove_file(fg->dev, &dev_attr_learned_qresidual10);
err_qresidual10:
	device_remove_file(fg->dev, &dev_attr_learned_qresidual00);
err_qresidual00:
	device_remove_file(fg->dev, &dev_attr_learned_full_cap_nom);
err_full_cap_nom:
	device_remove_file(fg->dev, &dev_attr_learned_cycles);
err_cycles:
	device_remove_file(fg->dev, &dev_attr_learned_full_cap_rep);
err_full_cap_rep:
	device_remove_file(fg->dev, &dev_attr_learned_temp_co);
err_temp_co:
	device_remove_file(fg->dev, &dev_attr_learned_rcomp0);
err_rcomp0:
err_alert_init:
err_reg_init:
err_virq:
	power_supply_unregister(fg->fuelgauge);
	return ret_val;
}

int max77818_fg_remove(struct platform_device *pdev)
{
	struct max77818_fg_dev *fg;
	fg = platform_get_drvdata(pdev);
	device_remove_file(fg->dev, &dev_attr_ain0);
	device_remove_file(fg->dev, &dev_attr_self_test);
	device_remove_file(fg->dev, &dev_attr_load_params);
	device_remove_file(fg->dev, &dev_attr_learned_cv_mixcap);
	device_remove_file(fg->dev, &dev_attr_learned_qresidual30);
	device_remove_file(fg->dev, &dev_attr_learned_qresidual20);
	device_remove_file(fg->dev, &dev_attr_learned_qresidual10);
	device_remove_file(fg->dev, &dev_attr_learned_qresidual00);
	device_remove_file(fg->dev, &dev_attr_learned_full_cap_nom);
	device_remove_file(fg->dev, &dev_attr_learned_cycles);
	device_remove_file(fg->dev, &dev_attr_learned_full_cap_rep);
	device_remove_file(fg->dev, &dev_attr_learned_temp_co);
	device_remove_file(fg->dev, &dev_attr_learned_rcomp0);
	power_supply_unregister(fg->fuelgauge);
	return 0;
}

static const struct of_device_id max77818_fg_of_ids[] = {
	{ .compatible = "maxim,max77818-fg" },
	{   },
};
MODULE_DEVICE_TABLE(of, max77818_fg_of_ids);

static const struct platform_device_id max77818_fg_id[] = {
	{ "max77818-fuelgauge", 0, },
	{  },
};
MODULE_DEVICE_TABLE(platform, max77818_fg_id);

static struct platform_driver max77818_fg_driver = {
	.driver = {
		.name = "max77818-fg",
		.owner = THIS_MODULE,
		.of_match_table = max77818_fg_of_ids,
	},
	.probe = max77818_fg_probe,
	.remove = max77818_fg_remove,
	.id_table = max77818_fg_id,
};

module_platform_driver(max77818_fg_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nebojsa Stojiljkovic <nebojsa@keemail.me>");
MODULE_DESCRIPTION("MAX77818 Fuelgauge Driver");
MODULE_ALIAS("mfd:max77818-fg");

