// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __LINUX_MAX77818_FG_
#define __LINUX_MAX77818_FG_

#define MAX77818_OCV_LENGTH        48

#define MAX77818_BATTERY_FULL      95
#define MAX77818_BATTERY_HIGH      80
#define MAX77818_BATTERY_NORMAL    20
#define MAX77818_BATTERY_LOW       5
#define MAX77818_BATTERY_CRITICAL  1

#define MAX77818_MODEL_UNLOCK1     0x0059
#define MAX77818_MODEL_UNLOCK2     0x00C4
#define MAX77818_MODEL_LOCK        0x0000

enum max77818_temp_status {
	MAX77818_TEMP_LOW,
	MAX77818_TEMP_NORMAL,
	MAX77818_TEMP_HIGH,
};


struct max77818_fg_platform_data {

	/* Pointer to battery model array */
	unsigned int *battery_ocv_model;

	/* Misc configuration registers */
	unsigned int vfsoc0;
	unsigned int design_cap;
	unsigned int config;
	unsigned int config2;
	unsigned int dqacc;
	unsigned int dpacc;
	unsigned int filter_cfg;
	unsigned int full_cap_nom;
	unsigned int full_cap_rep;
	unsigned int full_soc_thr;
	unsigned int iavg_empty;
	unsigned int i_chg_term;
	unsigned int learn_cfg;
	unsigned int qresidual00;
	unsigned int qresidual10;
	unsigned int qresidual20;
	unsigned int qresidual30;
	unsigned int rcomp0;
	unsigned int relax_cfg;
	unsigned int temp_co;
	unsigned int v_empty;
	unsigned int tgain;
	unsigned int toff;
	unsigned int curve;

	/* Extra configuration registers */
	unsigned int at_rate;
	unsigned int cv_mixcap;
	unsigned int cv_halftime;
	unsigned int smartchgcfg;
	unsigned int convg_cfg;

	/* Temperature alert thresholds */
	unsigned int talrt_low;
	unsigned int talrt_norm;
	unsigned int talrt_high;
};

struct max77818_fg_learned_params {

	/* Learned params */
	unsigned int rcomp0;
	unsigned int temp_co;
	unsigned int full_cap_rep;
	unsigned int cycles;
	unsigned int full_cap_nom;
	unsigned int qresidual00;
	unsigned int qresidual10;
	unsigned int qresidual20;
	unsigned int qresidual30;
	unsigned int cv_mixcap;
	unsigned int cv_halftime;
};

struct max77818_fg_dev {

	struct device *dev;
	struct power_supply *fuelgauge;

	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_chip;
	struct max77818_dev *max77818;
	struct max77818_fg_platform_data *pdata;
	struct max77818_fg_learned_params *learned;

	struct delayed_work d_work;

	enum max77818_temp_status temp_status;

	int virq;
};

#endif
