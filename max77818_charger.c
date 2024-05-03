// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>

#include <linux/mfd/max77818-private.h>
#include <linux/mfd/max77818.h>
#include <linux/power/max77818_charger.h>

static const char *max77818_charger_model = "max77818-chg";
static const char *max77818_charger_manufacturer = "maxim";

static enum power_supply_property max77818_chg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
};

static struct max77818_chg_irqs irqs[] = {
	{.name = MAX77818_CHG_BYP_INT,  .virq = 0},
	{.name = MAX77818_CHG_BATP_INT, .virq = 0},
	{.name = MAX77818_CHG_BAT_INT,  .virq = 0},
	{.name = MAX77818_CHG_CHG_INT,  .virq = 0},
	{.name = MAX77818_CHG_WCIN_INT, .virq = 0},
	{.name = MAX77818_CHG_AICL_INT, .virq = 0},
};



static int max77818_chg_get_charge_type(struct max77818_chg_dev *chg,  int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_DETAILS_01, &data);

	if (ret_val < 0)
		return ret_val;

	switch ((data & BIT_CHG_DTLS) >> FFS(BIT_CHG_DTLS)) {
	case MAX77818_CHARGING_TOP_OFF:
		*val = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	case MAX77818_CHARGING_FAST_CONST_CURRENT:
	case MAX77818_CHARGING_FAST_CONST_VOLTAGE:
		*val = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case MAX77818_CHARGING_PREQUALIFICATION:
	case MAX77818_CHARGING_DETBAT_SUSPEND:
	case MAX77818_CHARGING_TIMER_EXPIRED:
	case MAX77818_CHARGING_WATCHDOG_EXPIRED:
	case MAX77818_CHARGING_OVER_TEMP:
	case MAX77818_CHARGING_OFF:
		*val = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case MAX77818_CHARGING_DONE:
		*val = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		*val = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	return 0;
}

static int max77818_chg_get_charge_status(struct max77818_chg_dev *chg, int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_DETAILS_01, &data);

	if (ret_val < 0)
		return ret_val;

	switch ((data & BIT_CHG_DTLS) >> FFS(BIT_CHG_DTLS)) {
	case MAX77818_CHARGING_TOP_OFF:
		*val = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case MAX77818_CHARGING_FAST_CONST_CURRENT:
	case MAX77818_CHARGING_FAST_CONST_VOLTAGE:
		*val = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case MAX77818_CHARGING_PREQUALIFICATION:
	case MAX77818_CHARGING_DETBAT_SUSPEND:
	case MAX77818_CHARGING_TIMER_EXPIRED:
	case MAX77818_CHARGING_WATCHDOG_EXPIRED:
	case MAX77818_CHARGING_OVER_TEMP:
	case MAX77818_CHARGING_OFF:
		*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case MAX77818_CHARGING_DONE:
		*val = POWER_SUPPLY_STATUS_FULL;
		break;
	default:
		*val = POWER_SUPPLY_SCOPE_UNKNOWN;
	}

	return 0;
}

static int max77818_chg_get_battery_health(struct max77818_chg_dev *chg, int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_DETAILS_01, &data);

	if (ret_val < 0)
		return ret_val;

	if ((data & BIT_TREG) >> FFS(BIT_TREG)) {
		*val = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else {
		switch ((data & BIT_BAT_DTLS) >> FFS(BIT_BAT_DTLS)) {

		case MAX77818_BATTERY_PREQUALIFICATION:
			*val = POWER_SUPPLY_HEALTH_DEAD;
			break;
		case MAX77818_BATTERY_GOOD:
		case MAX77818_BATTERY_LOWVOLTAGE:
			*val = POWER_SUPPLY_HEALTH_GOOD;
			break;
		case MAX77818_BATTERY_TIMER_EXPIRED:
			*val = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
			break;
		case MAX77818_BATTERY_OVERVOLTAGE:
			*val = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			break;
		case MAX77818_BATTERY_OVERCURRENT:
			*val = POWER_SUPPLY_HEALTH_OVERCURRENT;
			break;
		case MAX77818_BATTERY_NOBAT:
		default:
			*val = POWER_SUPPLY_HEALTH_UNKNOWN;
			break;
		}
	}
	return 0;
}

static int max77818_chg_get_online(struct max77818_chg_dev *chg, int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_INT_OK, &data);
	if (ret_val < 0)
		return ret_val;

	*val = (data & BIT_OK_CHGIN_I) ? 1 : 0;

	return 0;
}

static int max77818_chg_get_present(struct max77818_chg_dev *chg, int *val)
{
	unsigned int data;
	int ret;

	ret = regmap_read(chg->regmap, REG_CHG_INT_OK, &data);
	if (ret < 0)
		return ret;

	*val = (data & BIT_OK_BATP_I) ? 1 : 0;

	return 0;
}

static int max77818_chg_get_mode(struct max77818_chg_dev *chg, int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_CNFG_00, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data & BIT_MODE >> FFS(BIT_MODE);

	return 0;
}

static int max77818_chg_get_byp_dtls(struct max77818_chg_dev *chg, int *val)
{
	int ret_val;
	unsigned int data;

	ret_val = regmap_read(chg->regmap, REG_CHG_DETAILS_02, &data);
	if (ret_val < 0)
		return ret_val;

	*val = data & BIT_BYP_DTLS >> FFS(BIT_BYP_DTLS);

	return 0;
}

static int max77818_chg_set_fast_charge_timer_timeout(struct max77818_chg_dev *chg, int val)
{
	int ret_val;
	unsigned int data;

	switch (val) {
	case 4 ... 16:
		data = (val - 4) / 2 + 1;
		break;
	case 0:
		data = 0;
		break;
	default:
		return -EINVAL;
	}

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_01, BIT_FCHGTIME,
					(data << FFS(BIT_FCHGTIME)));

	return ret_val;
}

static int max77818_chg_set_charge_current_limit(struct max77818_chg_dev *chg,
						 int val)
{
	int ret_val;
	unsigned int data;

	if (val < 100000 || val > 3000000)
		return -EINVAL;

	data = val / 50000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_02, BIT_CHG_CC,
					(data << FFS(BIT_CHG_CC)));

	return ret_val;
}

static int max77818_chg_set_otg_output_current_limit(struct max77818_chg_dev *chg,
						     int val)
{
	int ret_val;
	unsigned int data;

	switch (val) {
	case 500000:
		data = 0x00;
		break;
	case 900000:
		data = 0x01;
		break;
	case 1200000:
		data = 0x02;
		break;
	case 1500000:
		data = 0x03;
		break;
	default:
		return -EINVAL;
	}

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_02, BIT_OTG_ILIM,
					(data << FFS(BIT_OTG_ILIM)));

	return ret_val;
}

static int max77818_chg_set_topoff_current_threshold(struct max77818_chg_dev *chg,
						     int val)
{
	int ret_val;
	unsigned int data;

	if (val < 100000 || val > 350000)
		return -EINVAL;

	if (val <= 200000)
		data = (val - 100000) / 25000;
	else
		data = val / 50000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_03, BIT_TO_ITH,
					(data << FFS(BIT_TO_ITH)));

	return ret_val;
}

static int max77818_chg_set_topoff_timer_timeout(struct max77818_chg_dev *chg,
						 int val)
{
	int ret_val;
	unsigned int data;

	if (val > 70)
		return -EINVAL;

	data = val / 10;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_03, BIT_TO_TIME,
					(data << FFS(BIT_TO_TIME)));

	return ret_val;
}

static int max77818_chg_set_prim_charge_term_voltage(struct max77818_chg_dev *chg,
						     int val)
{
	int ret_val;
	unsigned int data = 0;

	if (val < 3650000 || val >4700000 )
		return -EINVAL;

	if (val >=3650000 && val < 4340000)
		data = (val - 3650000) / 25000;
	else if (val == 4340000)
		data = 0x1C;
	else if (val > 4340000 && val <= 4700000)
		data = 0x1D + (val - 4350000) / 25000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_01, BIT_CHG_RSTRT,
					(data << FFS(BIT_CHG_RSTRT)));

	return ret_val;
}

static int max77818_chg_set_min_system_reg_voltage(struct max77818_chg_dev *chg,
						   int val)
{
	int ret_val;
	unsigned int data;

	if (val < 3000000 || val > 3700000)
		return -EINVAL;

	data = (val - 3400000) / 100000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_04, BIT_MINVSYS,
					(data << FFS(BIT_MINVSYS)));

	return ret_val;
}

static int max77818_chg_set_thermal_reg_temperature(struct max77818_chg_dev *chg,
						    int val)
{
	int ret_val;
	unsigned int data;

	switch (val) {
	case 85:
	case 100:
	case 115:
	case 130:
		data = (val - 85) / 15;
		break;
	default:
		return -EINVAL;
	}

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_07, BIT_REGTEMP,
					(data << FFS(BIT_REGTEMP)));

	return ret_val;
}

static int max77818_chg_set_chgin_input_current_limit(struct max77818_chg_dev *chg,
						      int val)
{
	int ret_val;
	unsigned int data;

	if (val < 100000 || val > 4000000)
		return -EINVAL;

	data = (val - 1000) / 33000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_09, BIT_CHGIN_ILIM,
					(data << FFS(BIT_CHGIN_ILIM)));

	return ret_val;
}

static int max77818_chg_set_wchgin_input_current_limit(struct max77818_chg_dev *chg,
						       int val)
{
	int ret_val;
	unsigned int data;

	if (val < 60000 || val > 1260000)
		return -EINVAL;

	data = val / 20000;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_10, BIT_WCIN_ILIM,
					(data << FFS(BIT_WCIN_ILIM)));

	return ret_val;
}


static int max77818_chg_set_battery_overcurrent_threshold(struct max77818_chg_dev *chg,
							  int val)
{
	int ret_val;
	unsigned int data;

	if (val && (val < 3000000 || val > 4500000))
		return -EINVAL;

	if (val)
		data = ((val - 3000000) / 250000) + 1;
	else
		data = 0;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_12, BIT_B2SOVRC,
					(data << FFS(BIT_B2SOVRC)));

	return ret_val;
}

static int max77818_chg_set_chgin_input_voltage_threshold(struct max77818_chg_dev *chg,
							  int val)
{
	int ret_val;
	unsigned int data;

	switch (val) {
	case 4300000:
		data = 0x0;
		break;
	case 4700000:
	case 4800000:
	case 4900000:
		data = (val - 4700000) / 100000;
		break;
	default:
		return -EINVAL;
	}

	ret_val =  regmap_update_bits(chg->regmap, REG_CHG_CNFG_12, BIT_VCHGIN_REG,
					(data << FFS(BIT_VCHGIN_REG)));

	return ret_val;
}

static int max77818_chg_set_mode(struct max77818_chg_dev *chg, int val)
{
	int ret_val;
	unsigned int data;

	data = val << FFS(BIT_MODE);

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_00, BIT_MODE, data);

	return ret_val;
}

static int max77818_chg_property_is_writable(struct power_supply *psy,
						enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_MANUFACTURER:
	case POWER_SUPPLY_PROP_MODEL_NAME:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_ONLINE:
		return 0;
	default:
		return -EINVAL;
	}
}

static int max77818_chg_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct max77818_chg_dev *chg = power_supply_get_drvdata(psy);
	int ret_val;

	switch (psp) {
	default:
		ret_val = -EINVAL;
	}

	if (ret_val < 0)
		dev_err(chg->dev, "set property %d failed: %d\n", psp, ret_val);

	return ret_val;
}

static int max77818_chg_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct max77818_chg_dev *chg = power_supply_get_drvdata(psy);
	int ret_val = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret_val = max77818_chg_get_charge_status(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret_val = max77818_chg_get_charge_type(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret_val = max77818_chg_get_battery_health(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret_val = max77818_chg_get_online(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		ret_val = max77818_chg_get_present(chg, &val->intval);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = max77818_charger_model;
		ret_val = 0;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = max77818_charger_manufacturer;
		ret_val = 0;
		break;
	default:
		ret_val = -EINVAL;
	}

	if (ret_val < 0)
		dev_err(chg->dev, "get property %d failed: %d\n", psp, ret_val);

	return ret_val;
}

static struct power_supply_config max77818_chg_config = {

};

static const struct power_supply_desc max77818_chg_desc = {

	.name = "max77818-chg",
	.type = POWER_SUPPLY_TYPE_MAINS,
	.properties = max77818_chg_props,
	.num_properties = ARRAY_SIZE(max77818_chg_props),
	.get_property = max77818_chg_get_property,
	.set_property = max77818_chg_set_property,
	.property_is_writeable = max77818_chg_property_is_writable,
};

static int max77818_chg_reg_init(struct max77818_chg_dev *chg)
{
	int ret_val;
	struct max77818_chg_platform_data *pdata = chg->pdata;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_06,
					BIT_CHGPROT, 0x03 << FFS(BIT_CHGPROT));
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_fast_charge_timer_timeout(chg,
						pdata->fast_charge_timer_timeout);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_charge_current_limit(chg,
						pdata->charge_current_limit);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_otg_output_current_limit(chg,
						pdata->otg_output_current_limit);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_topoff_current_threshold(chg,
						pdata->topoff_current_threshold);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_topoff_timer_timeout(chg,
						pdata->topoff_timer_timeout);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_prim_charge_term_voltage(chg,
						pdata->prim_charge_term_voltage);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_min_system_reg_voltage(chg,
						pdata->min_system_reg_voltage);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_thermal_reg_temperature(chg,
						pdata->thermal_reg_temperature);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_chgin_input_current_limit(chg,
						pdata->chgin_input_current_limit);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_wchgin_input_current_limit(chg,
						pdata->wchgin_input_current_limit);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_battery_overcurrent_threshold(chg,
						pdata->battery_overcurrent_threshold);
	if(ret_val)
		return ret_val;

	ret_val = max77818_chg_set_chgin_input_voltage_threshold(chg,
						pdata->chgin_input_voltage_threshold);
	if(ret_val < 0)
		return ret_val;

	ret_val = regmap_update_bits(chg->regmap, REG_CHG_CNFG_06,
					BIT_CHGPROT, 0x00 << FFS(BIT_CHGPROT));
	if(ret_val)
		return ret_val;

	return 0;
}

static int max77818_chg_parse_dt(struct max77818_chg_dev *chg)
{
	struct max77818_chg_platform_data *pdata = chg->pdata;

	struct device_node *np = of_find_node_by_name(chg->dev->parent->of_node,
						      "charger");

	if (of_property_read_u32(np, "fast_charge_timer_timeout",
				&pdata->fast_charge_timer_timeout))
		pdata->fast_charge_timer_timeout = 0;

	if (of_property_read_u32(np, "charge_current_limit",
				&pdata->charge_current_limit))
		pdata->charge_current_limit = 1600000;

	if (of_property_read_u32(np, "otg_output_current_limit",
				&pdata->otg_output_current_limit))
		pdata->otg_output_current_limit = 1500000;

	if (of_property_read_u32(np, "topoff_current_threshold",
				&pdata->topoff_current_threshold))
		pdata->topoff_current_threshold = 125000;

	if (of_property_read_u32(np, "topoff_timer_timeout",
				&pdata->topoff_timer_timeout))
		pdata->topoff_timer_timeout = 0;

	if (of_property_read_u32(np, "prim_charge_term_voltage",
				&pdata->prim_charge_term_voltage))
		pdata->prim_charge_term_voltage = 4200000;

	if (of_property_read_u32(np, "min_system_reg_voltage",
				&pdata->min_system_reg_voltage))
		pdata->min_system_reg_voltage = 3600000;

	if (of_property_read_u32(np, "thermal_reg_temperature",
				&pdata->thermal_reg_temperature))
		pdata->thermal_reg_temperature = 115;

	if (of_property_read_u32(np, "chgin_input_current_limit",
				&pdata->chgin_input_current_limit))
		pdata->chgin_input_current_limit = 1700000;

	if (of_property_read_u32(np, "wchgin_input_current_limit",
				&pdata->wchgin_input_current_limit))
		pdata->wchgin_input_current_limit = 500000;

	if (of_property_read_u32(np, "battery_overcurrent_threshold",
				&pdata->battery_overcurrent_threshold))
		pdata->battery_overcurrent_threshold = 4500000;

	if (of_property_read_u32(np, "chgin_input_voltage_threshold",
				&pdata->chgin_input_voltage_threshold))
		pdata->chgin_input_voltage_threshold = 4300000;

	dev_dbg(chg->dev, "fast_charge_timer_timeout: %d sec\n",
		pdata->fast_charge_timer_timeout);

	dev_dbg(chg->dev, "charge_current_limit: %d uA\n",
		pdata->charge_current_limit);

	dev_dbg(chg->dev, "otg_output_current_limit: %d uA\n",
		pdata->otg_output_current_limit);

	dev_dbg(chg->dev, "topoff_current_threshold: %d uA\n",
		pdata->topoff_current_threshold);

	dev_dbg(chg->dev, "topoff_timer_timeout: %d s\n",
		pdata->topoff_timer_timeout);

	dev_dbg(chg->dev, "prim_charge_term_voltage: %d uV\n",
		pdata->prim_charge_term_voltage);

	dev_dbg(chg->dev, "min_system_reg_voltage: %d uV\n",
		pdata->min_system_reg_voltage);

	dev_dbg(chg->dev, "thermal_reg_temperature: %d C\n",
		pdata->thermal_reg_temperature);

	dev_dbg(chg->dev, "chgin_input_current_limit: %d uA \n",
		pdata->chgin_input_current_limit);

	dev_dbg(chg->dev, "wchgin_input_current_limit: %d uA \n",
		pdata->wchgin_input_current_limit);

	dev_dbg(chg->dev, "battery_overcurrent_threshold: %d uA\n",
		pdata->battery_overcurrent_threshold);

	dev_dbg(chg->dev, "chgin_input_voltage_threshold: %d uV\n",
		pdata->chgin_input_voltage_threshold);

	return 0;
}

static irqreturn_t max77818_chg_isr(int irq, void *data)
{
	struct max77818_chg_dev *chg = data;

	irq = irq - chg->irqs->virq;

	switch (irq) {
	case MAX77818_CHG_IRQ_BATP_I:
		dev_dbg(chg->dev, "Battery present status updated\n");
		break;
	case MAX77818_CHG_IRQ_CHGIN_I:
		dev_dbg(chg->dev, "CHGIN input status changed\n");
		break;
	case MAX77818_CHG_IRQ_WCIN_I:
		dev_dbg(chg->dev, "WCIN input status changed\n");
		break;
	case MAX77818_CHG_IRQ_CHG_I:
		dev_dbg(chg->dev, "Charger status changed\n");
		break;
	case MAX77818_CHG_IRQ_BAT_I:
		dev_dbg(chg->dev, "Battery status changed\n");
		break;
	default:
		break;
	}

	power_supply_changed(chg->supply);

	return IRQ_HANDLED;
}


static int max77818_chg_init_irqs(struct max77818_chg_dev *chg)
{
	int ret_val;
	int i, val;

	chg->irqs = irqs;

	for (i = 0; i < MAX77818_CHG_MAX_IRQS - 1; i++) {

		irqs[i].virq = regmap_irq_get_virq(chg->irq_chip, i);
		if (!irqs[i].virq) {
			dev_warn(chg->dev, "get virq for %s failed\n",
				 irqs[i].name);
		} else {
			ret_val = request_threaded_irq(irqs[i].virq,
					       NULL, max77818_chg_isr,
					       IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					       irqs[i].name, chg);
			if (ret_val < 0)
			dev_warn(chg->dev, "thread irq for %s failed\n",
				 irqs[i].name);
		}
	}

	regmap_read(chg->regmap, REG_CHG_INT, &val);

	return 0;
}

static int mode_event_notify(struct notifier_block *this, unsigned long mode,
		void *unused)
{
	struct max77818_chg_dev * chg;

	chg = container_of(this, struct max77818_chg_dev, mode_notifier);

	dev_info(chg->dev, "mode requested from fg: %lu\n", mode);
	max77818_chg_set_mode(chg, mode);

	power_supply_changed(chg->supply);
	return NOTIFY_DONE;
}

static ssize_t device_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf,
		int (*fn)(struct max77818_chg_dev *, int *))
{
	struct max77818_chg_dev *chg = dev_get_drvdata(dev);
	int val;
	int ret_val;

	ret_val = fn(chg, &val);
	if(ret_val)
		return ret_val;

	return scnprintf(buf, PAGE_SIZE, "%u\n", val);
}

static ssize_t max77818_chg_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return device_attr_show(dev, attr, buf,
			max77818_chg_get_mode);
}

static ssize_t max77818_chg_byp_dtls_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return device_attr_show(dev, attr, buf,
			max77818_chg_get_byp_dtls);
}


static DEVICE_ATTR_RO(max77818_chg_mode);
static DEVICE_ATTR_RO(max77818_chg_byp_dtls);

static int max77818_chg_power_supply_init(struct max77818_chg_dev *chg)
{
	struct power_supply *supply;
	max77818_chg_config.drv_data = chg;

	supply = devm_kzalloc(chg->dev, sizeof(*supply), GFP_KERNEL);
	if (!supply)
		return -ENOMEM;

	supply = power_supply_register(chg->dev, &max77818_chg_desc,
					&max77818_chg_config);
	if (IS_ERR(supply))
		return PTR_ERR(supply);

	chg->supply =supply;

	return 0;
}

static int max77818_chg_probe(struct platform_device *pdev)
{

	struct max77818_dev *max77818 = dev_get_drvdata(pdev->dev.parent);
	struct max77818_chg_dev *chg;
	struct max77818_chg_platform_data *pdata;
	int ret_val;

	chg = kzalloc(sizeof(*chg), GFP_KERNEL);
	if(!chg) {
		dev_err(chg->dev, "memory allocation failed\n");
		return -ENOMEM;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if(!pdata) {
		kfree(chg);
		return -ENOMEM;
	}

	chg->pdata = pdata;
	chg->dev = &pdev->dev;
	chg->max77818 = max77818;
	chg->regmap = max77818->regmap_chg;
	chg->irq_chip = max77818->irq_chip_chg;
	chg->mode_notifier.notifier_call = mode_event_notify;

#if defined(CONFIG_OF)

	ret_val = max77818_chg_parse_dt(chg);
	if (ret_val) {
		dev_err(chg->dev, "parse dt failed: %d\n", ret_val);
		return ret_val;
	}
#else
	dev_get_platdata(chg->dev);
#endif

	platform_set_drvdata(pdev, chg);

	ret_val = max77818_chg_reg_init(chg);
	if (ret_val) {
		dev_err(chg->dev, "init chg regs failed: %d\n", ret_val);
		return ret_val;
	}

	ret_val = max77818_chg_init_irqs(chg);
	if (ret_val) {
		dev_err(chg->dev, "irqs request failed %d\n", ret_val);
		return ret_val;
	}

	ret_val = device_create_file(chg->dev, &dev_attr_max77818_chg_mode);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create charger mode sysfs entry\n");
		goto err;
	}

	ret_val = device_create_file(chg->dev, &dev_attr_max77818_chg_byp_dtls);
	if (ret_val) {
		dev_err(&pdev->dev, "fail to create charger byp_dtls sysfs entry\n");
		goto err;
	}

	ret_val = max77818_chg_power_supply_init(chg);
	if (ret_val) {
		dev_err(chg->dev, "power supply init failed %d\n", ret_val);
		return ret_val;
	}

	ret_val = register_mode_notifier(&chg->mode_notifier);
	if(ret_val) {
		dev_err(chg->dev, "mode notifier register fail %d\n", ret_val);
	}

	return 0;

err:
	device_remove_file(chg->dev, &dev_attr_max77818_chg_mode);
	device_remove_file(chg->dev, &dev_attr_max77818_chg_byp_dtls);

	return ret_val;
}

static int max77818_chg_remove(struct platform_device *pdev)
{
	struct max77818_chg_dev *chg;
	chg = platform_get_drvdata(pdev);

	device_remove_file(chg->dev, &dev_attr_max77818_chg_mode);
	device_remove_file(chg->dev, &dev_attr_max77818_chg_byp_dtls);
	power_supply_unregister(chg->supply);

	return 0;
}


static const struct of_device_id max77818_chg_of_ids[] = {

	{ .compatible = "maxim,max77818-chg" },
	{  },
};
MODULE_DEVICE_TABLE(of, max77818_chg_of_ids);

static const struct platform_device_id max77818_chg_id[] = {

	{ "max77818-chg", 0, },
	{  },
};
MODULE_DEVICE_TABLE(platform, max77818_chg_id);

static struct platform_driver max77818_chg_driver = {

	.driver = {
		.name = "max77818-chg",
		.owner = THIS_MODULE,
		.of_match_table = max77818_chg_of_ids,
	},
	.probe = max77818_chg_probe,
	.remove = max77818_chg_remove,
	.id_table = max77818_chg_id,
};
module_platform_driver(max77818_chg_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nebojsa Stojiljkovic <nebojsa@keemail.me");
MODULE_DESCRIPTION("MAX77818 Charger Driver");
MODULE_ALIAS("mfd:max77818-chg");
MODULE_SOFTDEP("pre: max77818-fg");
