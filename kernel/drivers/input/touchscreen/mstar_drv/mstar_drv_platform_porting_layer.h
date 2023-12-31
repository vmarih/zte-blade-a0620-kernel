////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006-2014 MStar Semiconductor, Inc.
// All rights reserved.
//
// Unless otherwise stipulated in writing, any and all information contained
// herein regardless in any format shall remain the sole proprietary of
// MStar Semiconductor Inc. and be kept in strict confidence
// (??MStar Confidential Information??) by the recipient.
// Any unauthorized act including without limitation unauthorized disclosure,
// copying, use, reproduction, sale, distribution, modification, disassembling,
// reverse engineering and compiling of the contents of MStar Confidential
// Information is unlawful and strictly prohibited. MStar hereby reserves the
// rights to any and all damages, losses, costs and expenses resulting therefrom.
//
////////////////////////////////////////////////////////////////////////////////

/**
 *
 * @file    mstar_drv_platform_porting_layer.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__
#define __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE                                                             */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)

#include <mach/board.h>
#include <mach/gpio.h>
//#include <soc/sprd/board.h>
//#include <soc/sprd/gpio.h>
//#include <soc/sprd/i2c-sprd.h>

#include <linux/of_gpio.h>

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <mach/regulator.h>
//#include <soc/sprd/regulator.h>
#include <linux/regulator/consumer.h>
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
//#include <linux/input/vir_ps.h> 
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
#include <linux/pinctrl/consumer.h>
#endif //CONFIG_ENABLE_TOUCH_PIN_CONTROL

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)

#include <linux/of_gpio.h>

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

#ifdef CONFIG_ENABLE_NOTIFIER_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif //CONFIG_ENABLE_NOTIFIER_FB

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
//#include <linux/input/vir_ps.h> 
#include <linux/sensors.h>
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
#include <linux/pinctrl/consumer.h>
#endif //CONFIG_ENABLE_TOUCH_PIN_CONTROL

#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)

#include <linux/sched.h>
#include <linux/kthread.h>
//#include <linux/rtpm_prio.h>
#include <linux/wait.h>
#include <linux/time.h>

#include <linux/namei.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
//#include <linux/sensors_io.h>
#include <linux/hwmsen_helper.h>
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>

#ifdef TIMER_DEBUG
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#endif //TIMER_DEBUG

#ifdef CONFIG_MTK_SENSOR_HUB_SUPPORT
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>
#endif //CONFIG_MTK_SENSOR_HUB_SUPPORT

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
#include <linux/regulator/consumer.h>
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON

#ifdef CONFIG_MTK_BOOT
#include "mt_boot_common.h"
#endif //CONFIG_MTK_BOOT

#else
#include <mach/mt_pm_ldo.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_boot.h>
#include <mach/mt_gpio.h>

#include <cust_eint.h>
#include <pmic_drv.h>

#include "cust_gpio_usage.h"
#endif //CONFIG_PLATFORM_USE_ANDROID_SDK_6_UPWARD

#define CONFIG_MT_RT_MONITOR
#ifdef CONFIG_MT_RT_MONITOR
#define MT_ALLOW_RT_PRIO_BIT 0x10000000
#else
#define MT_ALLOW_RT_PRIO_BIT 0x0
#endif //CONFIG_MT_RT_MONITOR
#define REG_RT_PRIO(x) ((x) | MT_ALLOW_RT_PRIO_BIT)

#define RTPM_PRIO_TPD  REG_RT_PRIO(4)

#include "tpd.h"

#endif

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION                                         */
/*--------------------------------------------------------------------------*/

/*
 * Note.
 * Please change the below GPIO pin setting to follow the platform that you are using(EX. MediaTek, Spreadtrum, Qualcomm).
 */


#ifndef CONFIG_ENABLE_TOUCH_PIN_CONTROL
// TODO : Please FAE colleague to confirm with customer device driver engineer about the value of RST and INT GPIO setting
#define MS_TS_MSG_IC_GPIO_RST   0
#define MS_TS_MSG_IC_GPIO_INT   1
#endif //CONFIG_ENABLE_TOUCH_PIN_CONTROL

#ifdef CONFIG_ENABLE_TOUCH_PIN_CONTROL
#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"
#endif //CONFIG_ENABLE_TOUCH_PIN_CONTROL

#ifdef CONFIG_TP_HAVE_KEY
#define TOUCH_KEY_MENU (139) //229
#define TOUCH_KEY_HOME (172) //102
#define TOUCH_KEY_BACK (158)
#define TOUCH_KEY_SEARCH (217)

#define MAX_KEY_NUM (4)
#endif //CONFIG_TP_HAVE_KEY



/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION                                              */
/*--------------------------------------------------------------------------*/

extern void DrvPlatformLyrDisableFingerTouchReport(void);
extern void DrvPlatformLyrEnableFingerTouchReport(void);
extern void DrvPlatformLyrFingerTouchPressed(s32 nX, s32 nY, s32 nPressure, s32 nId);
extern void DrvPlatformLyrFingerTouchReleased(s32 nX, s32 nY, s32 nId);
extern void DrvPlatformLyrVariableInitialize(void);
extern s32 DrvPlatformLyrInputDeviceInitialize(struct i2c_client *pClient);
extern void DrvPlatformLyrSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate);
extern void DrvPlatformLyrTouchDevicePowerOff(void);
extern void DrvPlatformLyrTouchDevicePowerOn(void);
#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
extern void DrvPlatformLyrTouchDeviceRegulatorPowerOn(bool nFlag);
#endif //CONFIG_ENABLE_REGULATOR_POWER_ON
extern void DrvPlatformLyrTouchDeviceRegisterEarlySuspend(void);
extern s32 DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler(void);
extern s32 DrvPlatformLyrTouchDeviceRemove(struct i2c_client *pClient);
extern s32 DrvPlatformLyrTouchDeviceRequestGPIO(struct i2c_client *pClient);        
extern void DrvPlatformLyrTouchDeviceResetHw(void);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern int DrvPlatformLyrGetTpPsData(void);
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)
extern void DrvPlatformLyrTpPsEnable(int nEnable);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
extern int DrvPlatformLyrTpPsEnable(struct sensors_classdev* pProximityCdev, unsigned int nEnable);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
extern int DrvPlatformLyrTpPsOperate(void* pSelf, u32 nCommand, void* pBuffIn, int nSizeIn, void* pBuffOut, int nSizeOut, int* pActualOut);
#endif
#endif //CONFIG_ENABLE_PROXIMITY_DETECTION

#ifdef CONFIG_ENABLE_ESD_PROTECTION
void DrvPlatformLyrEsdCheck(struct work_struct *pWork);
#endif //CONFIG_ENABLE_ESD_PROTECTION
        
#endif  /* __MSTAR_DRV_PLATFORM_PORTING_LAYER_H__ */
