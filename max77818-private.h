// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __LINUX_MAX77818_PRIVATE_
#define __LINUX_MAX77818_PRIVATE_

/*Bitmask */
#define BITS(_s, _e) GENMASK(_e, _s)
/*  Create shift from bitmask */
#define FFS(_mask) (ffs(_mask) - 1)

#define PMIC_I2C_ADDRESS            0x66        /* MAX77818 top I2C address  */
#define CHARGER_I2C_ADDRESS         0x69        /* MAX77818 Fuel Gauge block I2C address */
#define FUELGAUGE_I2C_ADDRESS       0x36        /* MAX77818 Charger I2C address */

#define MAX77818_ID                 0x23        /* MAX77818 identification value */

/* Clogic, safeout LDO and interrupt level I2C registers */

#define REG_PMICID                  0x20        /* PMIC identity register */
#define BIT_PMICID_ID               BITS(0,7)   /* MAX77818 identitiy bits [0:7] */

#define REG_PMICREV                 0x21        /* PMIC Version/Revision register */
#define BIT_REV                     BITS(0,2)   /* PMIC revision bits [0:2] */
#define BIT_VERSION                 BITS(3,7)   /* PMIC version bits [3:7] */

#define REG_INTSRC                  0x22        /* Interrupt source detection register */
#define BIT_CHGR_INT                BIT(0)      /* Charger interrupt pending detection bit */
#define BIT_FG_INT                  BIT(1)      /* Fuel gauge interrupt pending detection bit */
#define BIT_SYS_INT                 BIT(2)      /* System interrupt pending detection bit */

#define REG_INTSRCMASK              0x23        /* Interrupt source mask register */
#define BIT_CHGR_INT_MASK           BIT(0)      /* Charger interrupt mask configuration bit */
#define BIT_FG_INT_MASK             BIT(1)      /* Fuel gauge block interrupt mask configuration bit */
#define BIT_SYS_INT_MASK            BIT(2)      /* System interrupt mask configuration bit */

#define REG_SYSINTSRC               0x24        /* System interrupt source detection register */
#define BIT_SYSUVLO_INT             BIT(0)      /* System undervoltage lockout interrupt detection bit */
#define BIT_SYSOVLO_INT             BIT(1)      /* System overvoltage lockout interrupt detection bit */
#define BIT_TSHDN_INT               BIT(2)      /* Termal shutdown threshold interrupt detection bit */
#define BIT_TM_INT                  BIT(7)      /* Test mode interrupt detection bit */

#define REG_SYSINTMASK              0x26        /* System interrupt source mask register */
#define BIT_SYSUVLO_INT_MASK        BIT(0)      /* System undervoltage lockout interrupt mask configuration bit */
#define BIT_SYSOVLO_INT_MASK        BIT(1)      /* System overvoltage lockout interrupt mask configuration bit */
#define BIT_TSHDN_INT_MASK          BIT(2)      /* Thermal shutdown threshold interrupt mask configuration bit */
#define BIT_TM_INT_MASK             BIT(7)      /* Test mode interrupt mask configuration bit */

#define REG_SAFEOUTCTRL             0xC6        /* Safeout LDO linear regulator control register */
#define BIT_SAFEOUT1                BITS(0,1)   /* Safeout LDO1 output voltage configuration bits [0:1] */
#define BIT_SAFEOUT2                BITS(2,3)   /* Safeout LDO2 output voltage configuration bits [2:3] */
#define BIT_ACTDISSAFEO1            BIT(4)      /* Safeout LDO1 active discharge configuration bit */
#define BIT_ACTDISSAFEO2            BIT(5)      /* Safeout LDO2 active discharge configuration bit */
#define BIT_ENSAFEOUT1              BIT(6)      /* Safeout LDO1 enable configuration bit */
#define BIT_ENSAFEOUT2              BIT(7)      /* Safeout LDO2 enable configuration bit */

/* Charger registers */

#define REG_CHG_INT                 0xB0        /* Charger status interrupt detection register  */
#define BIT_INT_BYP_I               BIT(0)      /* Bypass node interrupt change detection bit */
#define BIT_INT_BATP_I              BIT(2)      /* Battery presence interrupt change detection bit */
#define BIT_INT_BAT_I               BIT(3)      /* Battery interrupt detection bit */
#define BIT_INT_CHG_I               BIT(4)      /* Charger interrupt detection bit */
#define BIT_INT_WCIN_I              BIT(5)      /* WCIN interrupt detection bit */
#define BIT_INT_CHGIN_I             BIT(6)      /* CHGIN interrupt detection bit */
#define BIT_INT_AICL_I              BIT(7)      /* AICL interrupt detection bit */

#define REG_CHG_INT_MASK            0xB1        /* Charger interrupt mask register */
#define BIT_MASK_BYP_I              BIT(0)      /* Bypass node interrupt mask configuration bit */
#define BIT_MASK_BATP_I             BIT(2)      /* Battery presence interrupt mask configuration bit */
#define BIT_MASK_BAT_I              BIT(3)      /* Battery interrupt mask configuration bit */
#define BIT_MASK_CHG_I              BIT(4)      /* Charger interrupt mask configuration bit */
#define BIT_MASK_WCIN_I             BIT(5)      /* WCIN interrupt mask configuration bit */
#define BIT_MASK_CHGIN_I            BIT(6)      /* CHGIN interrupt mask configuration bit */
#define BIT_MASK_AICL_I             BIT(7)      /* AICL interrupt mask configuration bit */

#define REG_CHG_INT_OK              0xB2        /* Charger status interrupt status register */
#define BIT_OK_BYP_I                BIT(0)      /* Bypass node status indicator bit */
#define BIT_OK_BATP_I               BIT(2)      /* Battery presence indicator bit */
#define BIT_OK_BAT_I                BIT(3)      /* Battery status indicator bit */
#define BIT_OK_CHG_I                BIT(4)      /* Charger status indicator bit */
#define BIT_OK_WCIN_I               BIT(5)      /* Low voltage input (WCIN) status indicator bit */
#define BIT_OK_CHGIN_I              BIT(6)      /* High voltage input (CHGIN) status indicator bit */
#define BIT_OK_AICL_I               BIT(7)      /* AICL status indicator bit */

#define REG_CHG_DETAILS_00          0xB3        /* Charger details 00 register */
#define BIT_BATP_DTLS               BIT(0)      /* Battery presence status details bit */
#define BIT_WCIN_DTLS               BITS(3,4)   /* Low voltage input (WCIN) status details bits [3:4] */
#define BIT_CHGIN_DTLS              BITS(5,6)   /* High voltage input (CHGIN) status details bits [5:6] */

#define REG_CHG_DETAILS_01          0xB4        /* Charger details 01 register */
#define BIT_CHG_DTLS                BITS(0,3)   /* Charger status details bits [0:3] */
#define BIT_BAT_DTLS                BITS(4,6)   /* Battery status details bits [4:6] */
#define BIT_TREG                    BIT(7)      /* Temperature regulation status details bit */

#define REG_CHG_DETAILS_02          0xB5        /* Charger details 02 register */
#define BIT_BYP_DTLS                BITS(0,3)   /* Bypass node status details bits [0:3] */

#define REG_CHG_CNFG_00             0xB7        /* Charger configuration 00 register */
#define BIT_MODE                    BITS(0,3)   /* Smart power selector configuration bits [0:3] */
#define BIT_WDTEN                   BIT(4)      /* Whatchdog timer enable configuration bit */
#define BIT_SPREAD                  BIT(5)      /* Spread spectrum feature enable configuration bit */
#define BIT_DISBS                   BIT(6)      /* MBATT to SYS FET control enable configuration bit */
#define BIT_OTG_CTRL                BIT(7)      /* OTG FET control enable configuration bit */

#define REG_CHG_CNFG_01             0xB8        /* Charger configuration 01 register */
#define BIT_FCHGTIME                BITS(0,2)   /* Fast-charge timer duration time configuration bits [0:2] */
#define BIT_FSW                     BIT(3)      /* Switching frequency option configuration bit */
#define BIT_CHG_RSTRT               BITS(4,5)   /* Charger restart threshold configuration bits [4:5] */
#define BIT_LSEL                    BIT(6)      /* Inductor selection configuration bit */
#define BIT_PQUEN                   BIT(7)      /* Low battery prequalification mode enable configuration bit */

#define REG_CHG_CNFG_02             0xB9        /* Charger configuration register 02 */
#define BIT_CHG_CC                  BITS(0,5)   /* Fast-charge current selection configuration bits [0:5] */
#define BIT_OTG_ILIM                BITS(6,7)   /* CHGIN output current limit in OTG mode configuration bits [6:7] */

#define REG_CHG_CNFG_03             0xBA        /* Charger configuration register 03 */
#define BIT_TO_ITH                  BITS(0,2)   /* Top-off current threshold configuration bits [0:2] */
#define BIT_TO_TIME                 BITS(3,5)   /* Top-off timer setting configuration bits [3:5] */
#define BIT_ILIM                    BITS(6,7)   /* Program buck peak current limit configuration bits [6:7] */

#define REG_CHG_CNFG_04             0xBB        /* Charger configuration register 04 */
#define BIT_CHG_CV_PRM              BITS(0,5)   /* Primary charge termination voltage setting configuration bits [0:5] */
#define BIT_MINVSYS                 BITS(6,7)   /* Minimum system regulation voltage configuration bits [6:7] */

#define REG_CHG_CNFG_06             0xBD        /* Charger configuration register 06 */
#define BIT_WTDCLR                  BITS(0,1)   /* Watchdog timer clear bits [0:1] */
#define BIT_CHGPROT                 BITS(2,3)   /* Charger settings protection configuration bits [2:3] */

#define REG_CHG_CNFG_07             0xBE        /* Charger configuration register 07 */
#define BIT_REGTEMP                 BITS(5,6)   /* Junction temperature thermal regulation loop set point configuration bits [5:6] */

#define REG_CHG_CNFG_09             0xC0        /* Charger configuration register 09 */
#define BIT_CHGIN_ILIM              BITS(0,6)   /* Maximum input current limit selection configuration bits [0:6] */

#define REG_CHG_CNFG_10             0xC1        /* Charger configuration register 10 */
#define BIT_WCIN_ILIM               BITS(0,5)   /* Maximum current limit selection configuration bits [0:5] */

#define REG_CHG_CNFG_11             0xC2        /* Charger configuration register 11 */
#define BIT_VBYPSET                 BITS(0,6)   /* Bypass target output voltage in boost mode configuration bits [0:6] */

#define REG_CHG_CNFG_12             0xC3        /* Charger configuration register 12 */
#define BIT_B2SOVRC                 BITS(0,2)   /* BAT to SYS overcurrent threshold  configuration bits [0:2] */
#define BIT_VCHGIN_REG              BITS(3,4)   /* CHGIN voltage regulation threshold adjustment configuration bits [3:4] */
#define BIT_CHGINSEL                BIT(5)      /* CHGIN/USB input channel selection enable configuration bit */
#define BIT_WCINSEL                 BIT(6)      /* WCIN input channel selection enable configuration bit */

/*Model Gauge m5 register map */

#define REG_TAlrtTh2                0xB2        /* Temperature threshold control register */
#define BIT_TempCool                BIT(0,7)    /* Temperature threshold used for smart charging as T1 configuration bits [0:7] */
#define BIT_TempWarm                BIT(8,15)   /* Temperature threshold used for smart charging as T4 configuration bits [8:15] */

#define REG_SmartChgCfg             0xDB        /* Smart charge configuration register */
#define BIT_EnSF                    BIT(0)      /* SmartFull enable configuration bit */
#define BIT_EnsC                    BIT(1)      /* SmartCarging enable configuration bit */
#define BIT_UseVF                   BIT(4)      /* Input SoC for smart charging selection configuration bit */
#define BIT_DisJEITA                BIT(5)      /* JEITA battery temperature monitor adjusts disable configuration bit */

/* Status and configuration registers */

#define REG_Status                  0x00        /* Interrupt status register for the FG block */
#define BIT_Imn                     BIT(0)      /* Minimum Isys threshold exceeded indication bit */
#define BIT_POR                     BIT(1)      /* Power-on reset indication bit */
#define BIT_Bst                     BIT(3)      /* Battery status indication bit */
#define BIT_Isysmx                  BIT(4)      /* Maximum Isys threshold exceeded bit */
#define BIT_ThmHot                  BIT(6)      /* Fuel gauge control charger input current limit status bit indication */
#define BIT_dSOCi                   BIT(7)      /* 1% state of charge alert status bit */
#define BIT_Vmn                     BIT(8)      /* Minimum voltage threshold exceeded indication bit */
#define BIT_Tmn                     BIT(9)      /* Minimum temperature alert threshold exceeded indication bit */
#define BIT_Smn                     BIT(10)     /* Minimum state of charge alert (SOC) threshold exceeded indication bit */
#define BIT_Bi                      BIT(11)     /* Battery insertion indication bit */
#define BIT_Vmx                     BIT(12)     /* Maximum voltage alert threshold exceeded indication bit */
#define BIT_Tmx                     BIT(13)     /* Maximum temperature alert threshold exceeded indication bit */
#define BIT_Smx                     BIT(14)     /* Maximum state of charge (SOC) alert threshold exceeded indication bit*/
#define BIT_Br                      BIT(15)     /* Battery removal indication bit*/

#define REG_VAlrtTh                 0x01        /* Voltage alert threshold configuration register */
#define BIT_MinVoltageAlrt          BITS(0,7)   /* Minimum voltage alert threshold configuration bits [0:7]  */
#define BIT_MaxVoltageAlrt          BITS(8,15)  /* Maximum voltage alert threshold configuration bits [8:15] */

#define REG_TAlrtTh                 0x02        /* Temperature alert threshold configuration register */
#define BIT_MinTempAlrt             BITS(0,7)   /* Minimum temperature alert threshold configuration bits [0:7]*/
#define BIT_MaxTempAlrt             BITS(8,15)  /* Maximum temperature alert threshold configuration bits [8:15] */

#define REG_SAlrtTh                 0x03        /* State of charge alert configuration register */
#define BIT_MinSocAlrt              BITS(0,7)   /* Minimum state of charge alert configuration bits [0:7] */
#define BIT_MaxSocAlrt              BITS(8,15)  /* Maximum state of charge alert configuration bits [8:15] */

#define REG_AtRate                  0x04        /* At-Rate register */
#define BIT_AtRate                  BITS(0,15)  /* Negative complement two value of a theoretical load current prior to reading any of at-rate output registers */

#define REG_QRTable00               0x12        /* QRTable 00 register */
#define BIT_QRTable00               BITS(0,15)  /* QRTable00 value bits [0:15] */

#define REG_FullSocThr              0x13        /* Full state of charge threshold register  */
#define BIT_FullSOCthr              BITS(0,15)  /* Full state of charge threshold value bits [0:15] */

#define REG_Config                  0x1D        /* Fuel gauge configuration register */
#define BIT_Ber                     BIT(0)      /* Fuel gauge charger fail enable configuration bit */
#define BIT_Bei                     BIT(1)      /* State of charge alert sticky bit */
#define BIT_Aen                     BIT(2)      /* Temperature alert sticky bit */
#define BIT_FTHRM                   BIT(3)      /* Voltage alert sticky bit */
#define BIT_ETHRM                   BIT(4)      /* Fuel gauge charger control bit */
#define BIT_I2CSH                   BIT(6)      /* I2C shutdown configuration bit */
#define BIT_SHDN                    BIT(7)      /* Shutdown configuration bit */
#define BIT_Tex                     BIT(8)      /* Temperature external configuration bit */
#define BIT_Ten                     BIT(9)      /* Enable temperature channel */
#define BIT_AINSH                   BIT(10)     /* AIN pin shutdown configuration bit */
#define BIT_SPR_11                  BIT(11)     /* Fuel gauge charger control */
#define BIT_Vs                      BIT(12)     /* Voltage alert sticky bit */
#define BIT_Ts                      BIT(13)     /* Temperature alert sticky bit */
#define BIT_Ss                      BIT(14)     /* State of charge alert sticky bit */
#define BIT_SPR_15                  BIT(15)     /* Fuel gauge charger fail enable */

#define REG_DesignCap               0x18        /* Designed capacity register */
#define BIT_DesignCap               BITS(0,15)  /* Designed capacity used to measure the age of the battery */

#define REG_IchgTerm                0x1E        /*  */
#define BIT_IchgTerm                BIT(0)

#define REG_DevName                 0x21        /* Firmware version information register */
#define BIT_DevName                 BITS(0,15)  /* Firware version value [0:15] */

#define REG_QRTable10               0x22        /* QRTable 10 register  */
#define BIT_QRTable10               BITS(0,15)  /* QRTable10 value bits [0:15]  */

#define REG_FullCapNom              0x23        /* Nominal full capacity register  */
#define BIT_FullCapNom              BITS(0,15)  /* Nominal full capacity for room temperature value [0:15] */

#define REG_TempNom                 0x24        /* Nominal temperature register */
#define BIT_TempNom                 BITS(6,15)  /* Nominal temperature value bits [6:15] */

#define REG_TempLim                 0x25        /* Temperature limit register */
#define BIT_TempHot                 BITS(0,7)   /* Cold temperature value bits [0:7] */
#define BIT_TempCold                BITS(8,15)  /* Ht temperature value bits [8:15] */

#define REG_LearnCfg                0x28        /* Learn configuration register */
#define BIT_MixEn                   BIT(1)      /* Mixing enable configuration bit */
#define BIT_FillEmpty               BIT(2)      /* Filtered or unfiltered voltage empty configuration bit */
#define BIT_FCLmStage               BITS(4,6)   /* Full capacity learning stage configuration bits [4:6] */
#define BIT_FCx                     BIT(7)      /* Full charge source value configuration bit */
#define BIT_FCLm                    BITS(8,9)   /* Full capacity learning method configuration bits [8:9] */
#define BIT_LearnTCO                BITS(10,12) /* Temperature compensation learning rate configuration bit [10:12] */
#define BIT_LearnRCOMP              BITS(13,15) /* RCOMP0 learning rate configuration bit [13:15] */

#define REG_FilterCfg               0x29        /* Filter configuration register */
#define BIT_NCURR                   BITS(0,3)   /* Average current time constant configuration bits [0:3] */
#define BIT_NAVGCELL                BITS(4,6)   /* Average VCELL time constant configuration bits [4:6] */
#define BIT_NMIX                    BITS(7,10)  /* Mixing algorithm time constant configuration bits [7:10] */
#define BIT_NTEMP                   BITS(11,13) /* Average temperature time constant configuration bits [11:13] */
#define BIT_NEMPTY                  BITS(14,15) /* Set filtering for empty learning for I_Avgempty and QRTable registers configuration bits [14:15] */

#define REG_RelaxCfg                0x2A        /* Relaxed state configuration bits */
#define BIT_DTThr                   BITS(0,3)   /* Relaxation timer configuration bits [0:3] */
#define BIT_dVThr                   BITS(4,8)   /* Relaxation criteria between VCELL and OCV configuration bits [4:8] */
#define BIT_LoadThr                 BITS(9,15)  /* Load threshold configuration bits [9:15] */

#define REG_MiscCfg                 0x2B        /* Miscellanies configuration register */
#define BIT_SACFG                   BITS(0,1)   /* State of charge alert configuration bits [0:1] */
#define BIT_Vex                     BIT(2)      /* Disable voltage measurements configuration bit */
#define BIT_vttl                    BIT(3)      /* Lower voltage thermistor pullup configuration bit */
#define BIT_RdFCLrn                 BIT(4)      /* Automaticaly clear full charge learning bits configuration bit */
#define BIT_MixRate                 BITS(5,9)   /* Strength of servo mixing rate after the final mixing stage has been reached configuration bits [5:9] */
#define BIT_InitVFG                 BIT(10)     /* Reinitialize fuel gauge configuration bit */
#define BIT_EnBi1                   BIT(11)     /* Enable reset on battery insert detection */
#define BIT_OopsFilter              BITS(12,15) /* Oops filter configuration bits [12:15] */

#define REG_TGain                   0x2C        /* Temperature measurement gain on AIN pin register */
#define BIT_TGAIN                   BITS(0,15)  /* Temperature gain value configuration bits [0:15] */

#define REG_TOff                    0x2D        /* Temperature measurement offset on AIN pin register */
#define BIT_TOFF                    BITS(0,15)  /* Temperature offset value configuration bits [0:15] */

#define REG_Cgain                   0x2E        /* Current measurement gain register */
#define BIT_CGAIN                   BITS(0,15)  /* Current gain value configuration bits [0:15] */

#define REG_COff                    0x2F        /* Current measurement offset register */
#define BIT_COFF                    BITS(0,15)  /* Current measurement offset value bits [0:15] */

#define REG_QRTable20               0x32        /* QRTable 20 register */
#define BIT_QRTable20               BITS(0,15)  /* QRTable 20 value bits [0:15] */

#define REG_IavgEmpty               0x36        /* Average current sampled at last several empty events */
#define BIT_Iavg_empty              BITS(0,15)  /* Average current value bits [0:15] */

#define REG_RComp0                  0x38        /* RCOMP value register */
#define BIT_RCOMP0                  BITS(0,7)   /* RCOMP value bits [0:7] */

#define REG_TempCo                  0x39        /* Temperature Co register */
#define BIT_TempCoHot               BITS(0,7)   /* Cold temperature co value bits [0:7] */
#define BIT_TempCoCold              BITS(7,15)  /* Hot temperature co value bits [8:15] */

#define REG_V_empty                 0x3A        /* Empty voltage configuration register */
#define BIT_V_Empty                 BITS(0,6)   /* Recovery voltage configuration bits [0:6] */
#define BIT_V_Recover               BITS(7,15)  /* Empty voltage configuration bits [7:15] */

#define REG_QRTable30               0x42        /* QRTable 30 register */
#define BIT_QRTable30               BITS(0,15)  /* QRTable 30 value bits */

#define REG_FCTC                    0x37        /* Temperature correction factor register */
#define BIT_FCTC                    BITS(0,15)  /* Temperature correction value bits [0:15] */

#define REG_ConvgCfg                0x49        /* Coverage to empty control register */
#define BIT_RepL_per_stage          BITS(0,2)   /* RepLow threshold configuration bits [0:2] */
#define BIT_MinSlopeX               BITS(3,6)   /* Slope-shallowing configuration bits [3:6] */
#define BIT_VoltLowOff              BITS(7,11)  /* Low voltage off configuration register [7:11] */
#define BIT_RepLow                  BITS(12,15) /* RepCap low threshold configuration bits [12:15] */

#define REG_Status2                 0xB0        /* Status 2 register */
#define BIT_Hib                     BIT(1)      /* Fuel gauge hibernation mode status bit */
#define BIT_FullDet                 BIT(5)      /* Fully charged configuration bit */

#define REG_TTF_CFG                 0xB5        /* TTF calculation configuration register */
#define BIT_TTF_CFG                 BITS(0,2)   /* Filtering rate for learning CV halftime [0:2] */

#define REG_CV_MixCap               0xB6        /* Mix capacity configuration register  */
#define BIT_CV_MixCap               BITS(0,15)  /* Mix capacity when CV mode has been observed configuration bits [0:15] */

#define REG_CV_HalfTime             0xB7        /* Half time configuration register */
#define BIT_CV_HalfTime             BITS(0,15)  /* Half time configuration bits [0:15] */

#define REG_CGTempCo                0xB8        /* CG temperature configuration register */
#define BIT_CGTempCo                BITS(0,15)  /* CG temperature configuration bits [0:15] */

#define REG_Curve                   0xB9        /* Thermistor curvature adjustment register */
#define BIT_TCURVE                  BITS(0,7)   /* Thermistor calculation curve compensation configuration bits [0:7]  */
#define BIT_ECURVE                  BITS(8,15)  /* Ground resistance thermistor compensation configuration bits [8:15] */

#define REG_Config2                 0xBB        /* Fuel gauge configuration register */
#define BIT_IsysNCurr               BITS(0,3)   /* Time constant for AvgIsys register configuration bits [0:3] */
#define BIT_OCVQen                  BIT(4)      /* Enable automatic compensation based on VFCONF information configuration register */
#define BIT_LdMdl                   BIT(5)      /* Initiate firmware to finish processing a newly loaded model bit */
#define BIT_TAlrtEn                 BIT(6)      /* Enable temperature alert bit */
#define BIT_dSOCen                  BIT(7)      /* Enable state of charge 1% change alert bit */
#define BIT_ThmHotAlrtEn            BIT(8)      /* Enable thermistor hot alert bit */
#define BIT_ThmHotEn                BIT(9)      /* Enable thermistor hot function configuration bit */
#define BIT_FCThmHot                BIT(10)     /* Enable thermistor hot forcedly configuration bit */

#define REG_RippleCfg               0xBD        /* Ripple configuration register */
#define BIT_NR                      BITS(0,2)   /* Filter magnitude for ripple observation configuration bits [0:2] */
#define BIT_kDV                     BITS(3,15)  /* Corresponding amount of capacity to compensate proportional to ripple [3:15] */

/* Measurement registers */

#define REG_Temp                    0x08        /* Trimmed temperature measurement register */
#define BIT_TEMP                    BITS(0,15)  /* Trimmed temperature measurement register value bits [0:15] */

#define REG_Vcell                   0x09        /* Trimmed cell voltage measurement register */
#define BIT_VCELL                   BITS(0,15)  /* Trimmed cell voltage measurement value bits [0:15] */

#define REG_Current                 0x0A        /* Current measurement register */
#define BIT_Current                 BITS(0,15)  /* Current measurement value bits [0:15] */

#define REG_AvgCurrent              0x0B        /* Average IIR current register */
#define BIT_AvgCurrent              BITS(0,15)  /* Average current value bits [0:15] */

#define REG_AvgTA                   0x16        /* Average IIR temperature register */
#define BIT_AvgTA                   BITS(0,15)  /* Average temperature value bits [0:15] */

#define REG_AvgVCell                0x19        /* Average IIR VCELL register */
#define BIT_AvgVCELL                BITS(0,15)  /* Average VCELL value bits [0:15] */

#define REG_MaxMinTemp              0x1A        /* Maximum and minimum temperature measurement register */
#define BIT_MinTemperature          BITS(0,7)   /* Minimum temperature value bits [0:7]  */
#define BIT_MaxTemperature          BITS(8,15)  /* Maximum temperature value bits [8:15] */

#define REG_MaxMinVolt              0x1B        /* Maximum and minimum VCELL voltage measurement register */
#define BIT_MinVoltage              BITS(0,7)   /* Minimum VCELL voltage value bits [0:7] */
#define BIT_MaxVoltage              BITS(8,15)  /* Maximum VCELL voltage value bits [8:15] */

#define REG_MaxMinCurr              0x1C        /* Maximum and minimum charge current measurement register */
#define BIT_MinCurrent              BITS(0,7)   /* Minimum charge current value bits [0:7] */
#define BIT_MaxCurrent             BITS(8,15)  /* Maximum charge current value bits [8:15] */

#define REG_AIN0                    0x27        /* Trimmed ratiometric AIN0 measurement register */
#define BIT_AIN0                    BITS(0,15)  /* AIN0 value bits [0:15] */

#define REG_AtTTF                   0x33        /*  */
#define BIT_AtTTF                   BITS(0,15)  /*  */

#define REG_Timer                  0x3E         /* Timmer register */
#define BIT_TIMER                  BITS(0,15)   /* Timmer value bits [0:15] */

#define REG_ShdnTimer               0x3F        /* Shutdown timer register */
#define BIT_SHDNCTR                 BITS(0,12)  /* Shutdown counter value bits [0:12] */
#define BIT_SHDN_THR                BITS(13,15) /* Shutdown timer period configuration bits [13:15] */

#define REG_QH0                     0x4C        /* QH measurement register */
#define BIT_QH0                     BITS(0,15)  /* Last sampled QH for dQ calculation */

#define REG_VRipple                 0xBC        /* Voltage ripple compensation on battery capacity report */
#define BIT_Vripple                 BITS(0,15)  /* Voltage ripple value bits [0:15] */

#define REG_TimerH                  0xBE        /* Timer H register */
#define BIT_TIMERH                  BITS(0,15)  /* Timmer value bits [0:15] */

/* ModelGauge m5 output registers */

#define REG_RepCap                  0x05        /* Reported capacity register */
#define BIT_RepCap                  BITS(0,15)  /* Reported capacity value bits [0:15] */

#define REG_RepSOC                  0x06        /* Reported state of charge register */
#define BIT_RepSOC                  BITS(0,15)  /* Reported state of charge value bits [0:15] */

#define REG_Age                     0x07        /* Percentage of full capacity relative to full capacaity register */
#define BIT_Age                     BITS(0,15)  /* Age value bits [0:15] */

#define REG_Qresidual               0x0C        /* Unavailable capacity due to battery impedance and load current */
#define BIT_Qresidual               BITS(0,15)  /* Qresidual value bits [0:15] */

#define REG_MixSOC                  0x0D        /* Mixed state of charge register */
#define BIT_MixSOC                  BITS(0,15)  /* Mixed state of charge value bits [0:15] */

#define REG_AvSOC                   0x0E        /* Average state of charge register */
#define BIT_AvSOC                   BITS(0,15)  /* Average state of charge value bits [0:15] */

#define REG_MixCap                  0x0F        /* Remaining capacity with columb-counter and fuel gauge mixing with unavailable capacity register */
#define BIT_MixCapH                 BITS(0,15)  /* Remaining capacity value bits [0:15] */

#define REG_FullCap                 0x10        /* Temperature compensated full capacity register */
#define BIT_FullCAP                 BITS(0,15)  /* Temperature compensated full capacity value bits [0:15] */

#define REG_TTE                     0x11        /* Time to empty register */
#define BIT_sec                     BITS(0,3)   /* Remaining seconds value bits [0:3] */
#define BIT_mn                      BITS(4,9)   /* Remaining minutes value bits [4:9] */
#define BIT_hr                      BITS(10,15) /* Remaining hours value bits [10:15] */

#define REG_Rslow                   0x14        /* Battery slow internal resistance register */
#define BIT_RSLOW                   BITS(0,15)  /* Battery slow internal register value bits [0:15] */

#define REG_Cycles                  0x17        /* Odometer style accoumulation of battery cycles register */
#define BIT_Cycles                  BITS(0,15)  /* Battery cycles value bits [0:15] */

#define REG_AvCap                   0x1F        /* Remaining capacity with columb-counter and fuel gauge mixing excluding unavailable capacity register*/
#define BIT_AvCap                   BITS(0,15)  /* Remaining capacity value bits [0:15] */

#define REG_TTF                     0x20        /* Remaining time to full register */
#define BIT_Sec                     BITS(0,3)   /* Remaining seconds value [0:3] */
#define BIT_mn                      BITS(4,9)   /* Remaining minutes value [4:9] */
#define BIT_hr                      BITS(10,15) /* Remaining hours value [10:15] */

#define REG_FullCapRep              0x35        /* Full capacity using MAX17047 method register */
#define BIT_FullCapRep              BITS(0,15)  /* Full capacity value bits [0:15] */

#define REG_dQacc                   0x45        /*  */
#define BIT_dQacc                   BITS(0,15)  /*  */

#define REG_dPacc                   0x46        /*  */
#define BIT_dPacc                   BITS(0,15)  /*  */

#define REG_VFRemCap                0x4A        /* Remaining capacity according to voltage fuel gauge  */
#define BIT_VFRemCap                BITS(0,15)  /* Remaining capacity value bits [0:15]  */

#define REG_MaxError                0xBF        /* Filter for new error */
#define BIT_MaxError                BITS(0,15)  /* Filter for new error value bits [0:15]  */

#define REG_AtQresidual             0xDC        /* Unavailable capacity calculated using AtRate register */
#define BIT_AtQresidual             BITS(0,15)  /* Unavailable capacity value bits [0:15] */

#define REG_AtTTE                   0xDD        /* Calculated time-to empty register */
#define BIT_AtTTE                   BITS(0,15)  /* Calculated time to empty value bits [0:15] */

#define REG_AtAvSOC                 0xDE        /* Average state of charge calculated using AtRate register */
#define BIT_AtAvSOC                 BITS(0,15)  /* Average state of charge value bits [0:15] */

#define REG_AtAvCap                 0xDF        /* Remaining capacity calculated using AtQResidual */
#define BIT_AtAvCap                 BITS(0,15)  /* Remaining capacity value bits [0:15] */

/* Battery model registers */

#define REG_OCV                     0x80
#define BIT_OCV                     BITS(0,15)

#define REG_CAP                     0x90
#define BIT_CAP                     BITS(0,15)

#define REG_RCOMPseg                0xA0
#define BIT_RCOMPseg                BITS(0,15)

/* Undocumented registers */

#define REG_ChargeState0            0xD1
#define REG_ChargeState1            0xD2
#define REG_ChargeState2            0xD3
#define REG_ChargeState3            0xD4
#define REG_ChargeState4            0xD5
#define REG_ChargeState5            0xD6
#define REG_ChargeState6            0xD7
#define REG_ChargeState7            0xD8
#define REG_JEITA_Volt              0xD9
#define REG_JEITA_Curr              0xDA
#define REG_HibCFG                  0xBA
#define REG_VFOCV                   0xFB
#define REG_VFSOC0                  0x48
#define REG_VFSOC                   0xFF
#define REG_VFSOC0Enable            0x60
#define REG_MLOCKReg1               0x62
#define REG_MLOCKReg2               0x63

#endif
