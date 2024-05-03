// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __LINUX_MAX77818_CHG_
#define __LINUX_MAX77818_CHG_

#define MAX77818_CHG_MAX_IRQS (7)

#define MAX77818_CHG_BYP_INT   "BYP interrupt"
#define MAX77818_CHG_BATP_INT  "BATP interrupt"
#define MAX77818_CHG_BAT_INT   "BAT interrupt"
#define MAX77818_CHG_CHG_INT   "CHG interrupt"
#define MAX77818_CHG_WCIN_INT  "WCIN interrupt"
#define MAX77818_CHG_CHGIN_INT "CHGIN interrupt"
#define MAX77818_CHG_AICL_INT  "AICL interrupt"

struct max77818_chg_platform_data {
	int fast_charge_timer_timeout;          /* Fast charge timer duration [hrs]*/
	int charge_current_limit;               /* Maximum allowed fast charge current limit selection [mA]*/
	int otg_output_current_limit;           /* Charger output current limit in OTG mode [mA]*/
	int topoff_current_threshold;           /* Top-Off current threshold [mA]*/
	int topoff_timer_timeout;               /* Top-Off timer setting [min]*/
	int prim_charge_term_voltage;           /* Primary charge termination voltage [mV]*/
	int min_system_reg_voltage;             /* Minimum system regulation voltage */
	int thermal_reg_temperature;            /* Thermal regulation temperature */
	int chgin_input_current_limit;          /* Maximum  CHGIN input current limit selection [mA]*/
	int wchgin_input_current_limit;         /* Maximum WCHGIN input current limit selection [mA]*/
	int battery_overcurrent_threshold;      /* BAT to VSYS protection threshold */
	int chgin_input_voltage_threshold;      /* CHGIN input voltage threshold */
};

struct max77818_chg_irqs {
	const char *name;
	int virq;
};

struct max77818_chg_dev {
	struct device *dev;
	struct power_supply *supply;

	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_chip;
	struct max77818_dev *max77818;
	struct max77818_chg_platform_data *pdata;

	struct max77818_chg_irqs *irqs;

	struct notifier_block mode_notifier;
};

enum max77818_charger_details {
	MAX77818_CHARGING_PREQUALIFICATION   = 0x00,
	MAX77818_CHARGING_FAST_CONST_CURRENT = 0x01,
	MAX77818_CHARGING_FAST_CONST_VOLTAGE = 0x02,
	MAX77818_CHARGING_TOP_OFF            = 0x03,
	MAX77818_CHARGING_DONE               = 0x04,
	MAX77818_CHARGING_WATCHDOG_EXPIRED   = 0x05,
	MAX77818_CHARGING_TIMER_EXPIRED      = 0x06,
	MAX77818_CHARGING_DETBAT_SUSPEND     = 0x07,
	MAX77818_CHARGING_OFF                = 0x08,
	MAX77818_CHARGING_RESERVED           = 0x09,
	MAX77818_CHARGING_OVER_TEMP          = 0x0A,

};

enum max77818_chg_battery_details {
	MAX77818_BATTERY_NOBAT               = 0x00,
	MAX77818_BATTERY_PREQUALIFICATION    = 0x01,
	MAX77818_BATTERY_TIMER_EXPIRED       = 0x02,
	MAX77818_BATTERY_GOOD                = 0x03,
	MAX77818_BATTERY_LOWVOLTAGE          = 0x04,
	MAX77818_BATTERY_OVERVOLTAGE         = 0x05,
	MAX77818_BATTERY_OVERCURRENT         = 0x06,
	MAX77818_BATTERY_RESERVED            = 0x07,
};

#endif
