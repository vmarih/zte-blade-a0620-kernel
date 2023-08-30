/*
 *
 * FocalTech fts TouchScreen driver.
 * 
 * Copyright (c) 2010-2015, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR
 *    1.0		       2014-09			mshl
 *
 */

 /*******************************************************************************
*
* File Name: focaltech.c
*
* Author: mshl
*
* Created: 2014-09
*
* Modify by mshl on 2015-10-26
*
* Abstract:
*
* Reference:
*
*******************************************************************************/
/*******************************************************************************
* Included header files
*******************************************************************************/
//user defined include header files
#include "focaltech_core.h"

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
/* Early-suspend level */
#define FTS_SUSPEND_LEVEL 1
#endif


/*******************************************************************************
* Private constant and macro definitions using #define
*******************************************************************************/
#define FTS_META_REGS		3
#define FTS_ONE_TCH_LEN		6
#define FTS_TCH_LEN(x)		(FTS_META_REGS + FTS_ONE_TCH_LEN * x)

#define FTS_PRESS		0x7F
#define FTS_MAX_ID		0x0F
#define FTS_TOUCH_X_H_POS	3
#define FTS_TOUCH_X_L_POS	4
#define FTS_TOUCH_Y_H_POS	5
#define FTS_TOUCH_Y_L_POS	6
#define FTS_TOUCH_PRE_POS	7
#define FTS_TOUCH_AREA_POS	8
#define FTS_TOUCH_POINT_NUM		2
#define FTS_TOUCH_EVENT_POS	3
#define FTS_TOUCH_ID_POS		5

#define FTS_TOUCH_DOWN		0
#define FTS_TOUCH_UP		1
#define FTS_TOUCH_CONTACT	2

#define POINT_READ_BUF	(3 + FTS_ONE_TCH_LEN * FTS_MAX_POINTS)

/*register address*/
#define FTS_REG_DEV_MODE		0x00
#define FTS_DEV_MODE_REG_CAL	0x02

#define FTS_REG_PMODE		0xA5

#define FTS_REG_POINT_RATE	0x88
#define FTS_REG_THGROUP		0x80

/* power register bits*/
#define FTS_PMODE_ACTIVE		0x00
#define FTS_PMODE_MONITOR	0x01
#define FTS_PMODE_STANDBY	0x02
#define FTS_PMODE_HIBERNATE	0x03

#define FTS_STATUS_NUM_TP_MASK	0x0F

#define FTS_VTG_MIN_UV		2600000
#define FTS_VTG_MAX_UV		3300000
#define FTS_I2C_VTG_MIN_UV		1800000
#define FTS_I2C_VTG_MAX_UV		1800000

#define FTS_COORDS_ARR_SIZE	4
#define MAX_BUTTONS		4

#define FTS_8BIT_SHIFT		8
#define FTS_4BIT_SHIFT		4

/* psensor register address*/
#define FTS_REG_PSENSOR_ENABLE	0xB0
#define FTS_REG_PSENSOR_STATUS	0x01

/* psensor register bits*/
#define FTS_PSENSOR_ENABLE_MASK	0x01
#define FTS_PSENSOR_STATUS_NEAR	0xC0
#define FTS_PSENSOR_STATUS_FAR	0xE0
#define FTS_PSENSOR_FAR_TO_NEAR	0
#define FTS_PSENSOR_NEAR_TO_FAR	1
#define FTS_PSENSOR_ORIGINAL_STATE_FAR	1
#define FTS_PSENSOR_WAKEUP_TIMEOUT	500

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

//zz add by wangkuihua begin
#define FT5X0X_REG_FT5201ID 0xA8
#define FT5X0X_REG_FIRMID 0xA6
static struct i2c_client *update_client;

static struct proc_dir_entry *_gProcTouchscreenInfo = NULL;
static const struct file_operations info_proc_file_ops = {
    .read = FtsTouchscreenInfoRead,
    //.write = FtsTouchscreenInfoWrite,
};
static struct kobject *focaltech_tp_wake_gesture_kobj;

//zz end

/*******************************************************************************
* Private enumerations, structures and unions using typedef
*******************************************************************************/

/*******************************************************************************
* Static variables
*******************************************************************************/


/*******************************************************************************
* Global variable or extern global variabls/functions
*******************************************************************************/
struct i2c_client *fts_i2c_client;
struct fts_ts_data *fts_wq_data;
struct input_dev *fts_input_dev;

static unsigned int buf_count_add=0;
static unsigned int buf_count_neg=0;
extern u8 fw_vendor_id;
u8 buf_touch_data[30*POINT_READ_BUF] = { 0 };
bool enable_wakeup_gesture = 0;
#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
static struct sensors_classdev __maybe_unused sensors_proximity_cdev = {
	.name = "fts-proximity",
	.vendor = "FocalTech",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5.0",
	.resolution = "5.0",
	.sensor_power = "0.1",
	.min_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 200,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#endif

/*******************************************************************************
* Static function prototypes
*******************************************************************************/
static int fts_ts_start(struct device *dev);
static int fts_ts_stop(struct device *dev);
//add by wkh begin for gesture
static ssize_t focaltech_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t focaltech_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
//end

#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
/*******************************************************************************
*  Name: fts_psensor_support_enabled
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static inline bool fts_psensor_support_enabled(void)
{
	return config_enabled(CONFIG_TOUCHSCREEN_FTS_PSENSOR);
}
#endif

/*******************************************************************************
*  Name: fts_i2c_read
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret = 0;

	mutex_lock(&i2c_rw_access);

	if(readlen > 0)
	{
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = 0,
					 .len = writelen,
					 .buf = writebuf,
				 },
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
				dev_err(&client->dev, "%s: i2c read error.\n", __func__);
		} else {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				dev_err(&client->dev, "%s:i2c read error.\n", __func__);
		}
	}

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}

/*******************************************************************************
*  Name: fts_i2c_write
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret = 0;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	mutex_lock(&i2c_rw_access);

	if(writelen > 0)
	{
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c write error.\n", __func__);
	}

	mutex_unlock(&i2c_rw_access);
	
	return ret;
}

/*******************************************************************************
*  Name: fts_write_reg
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return fts_i2c_write(client, buf, sizeof(buf));
}

/*******************************************************************************
*  Name: fts_read_reg
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return fts_i2c_read(client, &addr, 1, val, 1);
}

#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
/*******************************************************************************
*  Name: fts_psensor_enable
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static void fts_psensor_enable(struct fts_ts_data *data, int enable)
{
	u8 state;
	int ret = -1;

	if (data->client == NULL)
		return;

	fts_read_reg(data->client, FTS_REG_PSENSOR_ENABLE, &state);
	if (enable)
		state |= FTS_PSENSOR_ENABLE_MASK;
	else
		state &= ~FTS_PSENSOR_ENABLE_MASK;

	ret = fts_write_reg(data->client, FTS_REG_PSENSOR_ENABLE, state);
	if (ret < 0)
		dev_err(&data->client->dev,
			"write psensor switch command failed\n");
	return;
}

/*******************************************************************************
*  Name: fts_psensor_enable_set
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_psensor_enable_set(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct fts_psensor_platform_data *psensor_pdata =
		container_of(sensors_cdev,
			struct fts_psensor_platform_data, ps_cdev);
	struct fts_ts_data *data = psensor_pdata->data;
	struct input_dev *input_dev = data->psensor_pdata->input_psensor_dev;

	mutex_lock(&input_dev->mutex);
	fts_psensor_enable(data, enable);
	psensor_pdata->tp_psensor_data = FTS_PSENSOR_ORIGINAL_STATE_FAR;
	if (enable)
		psensor_pdata->tp_psensor_opened = 1;
	else
		psensor_pdata->tp_psensor_opened = 0;
	mutex_unlock(&input_dev->mutex);
	return enable;
}

/*******************************************************************************
*  Name: fts_read_tp_psensor_data
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_read_tp_psensor_data(struct fts_ts_data *data)
{
	u8 psensor_status;
	char tmp;
	int ret = 1;

	fts_read_reg(data->client,
			FTS_REG_PSENSOR_STATUS, &psensor_status);

	tmp = data->psensor_pdata->tp_psensor_data;
	if (psensor_status == FTS_PSENSOR_STATUS_NEAR)
		data->psensor_pdata->tp_psensor_data =
						FTS_PSENSOR_FAR_TO_NEAR;
	else if (psensor_status == FTS_PSENSOR_STATUS_FAR)
		data->psensor_pdata->tp_psensor_data =
						FTS_PSENSOR_NEAR_TO_FAR;

	if (tmp != data->psensor_pdata->tp_psensor_data) {
		dev_dbg(&data->client->dev,
				"%s sensor data changed\n", __func__);
		ret = 0;
	}
	return ret;
}
#else
/*******************************************************************************
*  Name: fts_psensor_enable_set
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
/*
static int fts_psensor_enable_set(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	return enable;
}
*/

/*******************************************************************************
*  Name: fts_read_tp_psensor_data
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
/*
static int fts_read_tp_psensor_data(struct fts_ts_data *data)
{
	return 0;
}
*/
#endif

/*******************************************************************************
*  Name: fts_ts_interrupt
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static irqreturn_t fts_ts_interrupt(int irq, void *dev_id)
{
	struct fts_ts_data *fts_ts = dev_id;

	//disable_irq_nosync(fts_ts->client->irq);//wkh delete

	if (!fts_ts) {
		pr_err("%s: Invalid fts_ts\n", __func__);
		return IRQ_HANDLED;
	}

	queue_work(fts_ts->ts_workqueue, &fts_ts->touch_event_work);

	return IRQ_HANDLED;
}

/*******************************************************************************
*  Name: fts_read_Touchdata
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_read_Touchdata(struct fts_ts_data *data)
{
	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
		int rc = 0;
	#endif

	#if FTS_GESTRUE_EN
		u8 state;
	#endif

	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;


	#if FTS_GESTRUE_EN
	if(data->suspended)
	{
		fts_read_reg(data->client, 0xd0, &state);
	       if(state ==1)
	       {
	          fts_read_Gestruedata();
		   return 1;
	      }
	}
      #endif

	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	if (fts_psensor_support_enabled() && data->pdata->psensor_support &&
		data->psensor_pdata->tp_psensor_opened) {
		rc = fts_read_tp_psensor_data(data);
		if (!rc) {
			if (data->suspended)
				pm_wakeup_event(&data->client->dev,
					FTS_PSENSOR_WAKEUP_TIMEOUT);
			input_report_abs(data->psensor_pdata->input_psensor_dev,
					ABS_DISTANCE,
					data->psensor_pdata->tp_psensor_data);
			input_sync(data->psensor_pdata->input_psensor_dev);
			if (data->suspended)
				return 1;
		}
		if (data->suspended)
			return 1;
	}
	#endif
	
	ret = fts_i2c_read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		dev_err(&data->client->dev, "%s read touchdata failed.\n", __func__);
		return ret;
	}

	buf_count_add++;
	memcpy( buf_touch_data+(((buf_count_add-1)%30)*POINT_READ_BUF), buf, sizeof(u8)*POINT_READ_BUF );

	return 0;
}

/*******************************************************************************
*  Name: fts_report_value
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static void fts_report_value(struct fts_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i;
	int uppoint = 0;
	int touchs = 0;
	u8 pointid = FTS_MAX_ID;
	
	u8 buf[POINT_READ_BUF] = { 0 };

	buf_count_neg++;
	
	memcpy( buf,buf_touch_data+(((buf_count_neg-1)%30)*POINT_READ_BUF), sizeof(u8)*POINT_READ_BUF );


	memset(event, 0, sizeof(struct ts_event));

	event->point_num=buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	event->touch_point = 0;
	for (i = 0; i < FTS_MAX_POINTS; i++) {
		pointid = (buf[FTS_TOUCH_ID_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else
			event->touch_point++;
		
		event->au16_x[i] =
		    (s16) (buf[FTS_TOUCH_X_H_POS + FTS_ONE_TCH_LEN * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_X_L_POS + FTS_ONE_TCH_LEN * i];
		event->au16_y[i] =
		    (s16) (buf[FTS_TOUCH_Y_H_POS + FTS_ONE_TCH_LEN * i] & 0x0F) <<
		    8 | (s16) buf[FTS_TOUCH_Y_L_POS + FTS_ONE_TCH_LEN * i];
		event->au8_touch_event[i] =
		    buf[FTS_TOUCH_EVENT_POS + FTS_ONE_TCH_LEN * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FTS_TOUCH_ID_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		event->area[i] =
			(buf[FTS_TOUCH_AREA_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		event->pressure[i] =
			(s16) buf[FTS_TOUCH_PRE_POS + FTS_ONE_TCH_LEN * i];

		if(0 == event->area[i])
			event->area[i] = 0x09;

		if(0 == event->pressure[i])
			event->pressure[i] = 0x3f;

		if((event->au8_touch_event[i]==0 || event->au8_touch_event[i]==2)&&(event->point_num==0))
			return;
	}
	
	for (i = 0; i < event->touch_point; i++)
	{
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);
		
		
		if (event->au8_touch_event[i] == FTS_TOUCH_DOWN || event->au8_touch_event[i] == FTS_TOUCH_CONTACT)
		{
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			//dev_err(&data->client->dev, "%s FTS_TOUCH_DOWN || FTS_TOUCH_CONTACT .\n", __func__);
			//input_report_key(data->input_dev, BTN_TOUCH, 1);
			//input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->area[i]);
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, event->pressure[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
			touchs |= BIT(event->au8_finger_id[i]);
			data->touchs |= BIT(event->au8_finger_id[i]);
		}
		else
		{
		    //dev_err(&data->client->dev, "%s else.\n", __func__);
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			//input_report_key(data->input_dev, BTN_TOUCH, 0);
			data->touchs &= ~BIT(event->au8_finger_id[i]);
                        #if ZTE_FASTMMI_PV
                        input_sync(data->input_dev); // zz
                        #endif
		}
        //input_mt_sync(data->input_dev);
	}

	if(unlikely(data->touchs ^ touchs))
	{
		for(i = 0; i < FTS_MAX_POINTS; i++)
		{
			if(BIT(i) & (data->touchs ^ touchs))
			{
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;
	
	if(event->touch_point == uppoint)
	{
		input_report_key(data->input_dev, BTN_TOUCH, 0);
		//dev_err(&data->client->dev, "%s BTN_TOUCH 0 .\n", __func__);
	}
	else
	{
		input_report_key(data->input_dev, BTN_TOUCH, event->touch_point > 0);
		//dev_err(&data->client->dev, "%s BTN_TOUCH %d.\n", __func__,event->touch_point > 0);
	}
        //input_mt_sync(data->input_dev);//add
	input_sync(data->input_dev);
}

/*******************************************************************************
*  Name: fts_touch_irq_work
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
#if 0 //zhangjian modify
static void fts_touch_irq_work(struct work_struct *work)
{
	int ret = -1;
	
	ret = fts_read_Touchdata(fts_wq_data);
	if (ret == 0)
		fts_report_value(fts_wq_data);
	
	enable_irq(fts_wq_data->client->irq);
}
#else
static void fts_touch_irq_work(struct work_struct *work)
{
    int ret = -1;
    static bool irq_enter = false;
    if (!irq_enter)
    {
        irq_enter = true;
        ret = fts_read_Touchdata(fts_wq_data);
        if (ret == 0)
        {
            fts_report_value(fts_wq_data);
        }
        irq_enter = false;
    }

}
#endif
//end modify 
/*******************************************************************************
*  Name: fts_gpio_configure
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_gpio_configure(struct fts_ts_data *data, bool on)
{
	int err = 0;

	if (on) {
		if (gpio_is_valid(data->pdata->irq_gpio)) {
			err = gpio_request(data->pdata->irq_gpio,
						"fts_irq_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"irq gpio request failed");
				goto err_irq_gpio_req;
			}

			err = gpio_direction_input(data->pdata->irq_gpio);
			if (err) {
				dev_err(&data->client->dev,
					"set_direction for irq gpio failed\n");
				goto err_irq_gpio_dir;
			}
		}

		if (gpio_is_valid(data->pdata->reset_gpio)) {
			err = gpio_request(data->pdata->reset_gpio,
						"fts_reset_gpio");
			if (err) {
				dev_err(&data->client->dev,
					"reset gpio request failed");
				goto err_irq_gpio_dir;
			}

			err = gpio_direction_output(data->pdata->reset_gpio, 0);
			if (err) {
				dev_err(&data->client->dev,
				"set_direction for reset gpio failed\n");
				goto err_reset_gpio_dir;
			}
			msleep(data->pdata->hard_rst_dly);
			gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
		}

		return 0;
	} else {
		if (gpio_is_valid(data->pdata->irq_gpio))
			gpio_free(data->pdata->irq_gpio);
		if (gpio_is_valid(data->pdata->reset_gpio)) {
			/*
			 * This is intended to save leakage current
			 * only. Even if the call(gpio_direction_input)
			 * fails, only leakage current will be more but
			 * functionality will not be affected.
			 */
			//err = gpio_direction_input(data->pdata->reset_gpio);
			err = gpio_direction_output(data->pdata->reset_gpio,1);//modify by wkh
			if (err) {
				dev_err(&data->client->dev,
					"unable to set direction for gpio "
					"[%d]\n", data->pdata->irq_gpio);
			}
			gpio_free(data->pdata->reset_gpio);
		}

		return 0;
	}

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	return err;
}

/*******************************************************************************
*  Name: fts_power_on
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_power_on(struct fts_ts_data *data, bool on)
{
	int rc = 0;

	if (!on)
	{
		goto power_off;
	}

	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev, "Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev, "Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}

	return rc;


power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev, "Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev, "Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
		}
	}

	return rc;
}

/*******************************************************************************
*  Name: fts_power_init
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_power_init(struct fts_ts_data *data, bool on)
{
	int rc;

	if (!on)
	{
		dev_err(&data->client->dev, "fts_power_init false \n");
	}

	data->vdd = regulator_get(&data->client->dev, "avdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev, "Regulator get failed vdd rc=%d\n", rc);
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FTS_VTG_MIN_UV, FTS_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev, "Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(&data->client->dev, "Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev, "Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}

	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

/*******************************************************************************
*  Name: fts_ts_pinctrl_init
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
#ifdef MSM_NEW_VER
static int fts_ts_pinctrl_init(struct fts_ts_data *fts_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	fts_data->ts_pinctrl = devm_pinctrl_get(&(fts_data->client->dev));
	if (IS_ERR_OR_NULL(fts_data->ts_pinctrl)) {
		retval = PTR_ERR(fts_data->ts_pinctrl);
		dev_dbg(&fts_data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	fts_data->pinctrl_state_active
		= pinctrl_lookup_state(fts_data->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(fts_data->pinctrl_state_active)) {
		retval = PTR_ERR(fts_data->pinctrl_state_active);
		dev_err(&fts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	fts_data->pinctrl_state_suspend
		= pinctrl_lookup_state(fts_data->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(fts_data->pinctrl_state_suspend)) {
		retval = PTR_ERR(fts_data->pinctrl_state_suspend);
		dev_err(&fts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	fts_data->pinctrl_state_release
		= pinctrl_lookup_state(fts_data->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(fts_data->pinctrl_state_release)) {
		retval = PTR_ERR(fts_data->pinctrl_state_release);
		dev_dbg(&fts_data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(fts_data->ts_pinctrl);
err_pinctrl_get:
	fts_data->ts_pinctrl = NULL;
	return retval;
}
#endif

#ifdef CONFIG_PM
/*******************************************************************************
*  Name: fts_ts_start
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_start(struct device *dev)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);
	int err;

	if (data->pdata->power_on) {
		err = data->pdata->power_on(true);
		if (err) {
			dev_err(dev, "power on failed");
			return err;
		}
	} else {
		err = fts_power_on(data, true);
		if (err) {
			dev_err(dev, "power on failed");
			return err;
		}
	}

	#ifdef MSM_NEW_VER
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
				data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}
	#endif

	err = fts_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in resue state\n");
		goto err_gpio_configuration;
	}

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}

	msleep(data->pdata->soft_rst_dly);

	enable_irq(data->client->irq);
	data->suspended = false;

	return 0;

err_gpio_configuration:
	#ifdef MSM_NEW_VER
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}
	#endif
	
	if (data->pdata->power_on) {
		err = data->pdata->power_on(false);
		if (err)
			dev_err(dev, "power off failed");
	} else {
		err = fts_power_on(data, false);
		if (err)
			dev_err(dev, "power off failed");
	}
	return err;
}

/*******************************************************************************
*  Name: fts_ts_stop
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_stop(struct device *dev)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);
	char txbuf[2];
	int i, err;

	disable_irq(data->client->irq);

	/* release all touches */
	for (i = 0; i < data->pdata->num_max_touches; i++) {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
	}
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		txbuf[0] = FTS_REG_PMODE;
		txbuf[1] = FTS_PMODE_HIBERNATE;
		fts_i2c_write(data->client, txbuf, sizeof(txbuf));
	}

	if (data->pdata->power_on) {
		err = data->pdata->power_on(false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	} else {
		err = fts_power_on(data, false);
		if (err) {
			dev_err(dev, "power off failed");
			goto pwr_off_fail;
		}
	}

	#ifdef MSM_NEW_VER
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_suspend);
		if (err < 0)
			dev_err(dev, "Cannot get suspend pinctrl state\n");
	}
	#endif
	
	err = fts_gpio_configure(data, false);
	if (err < 0) {
		dev_err(&data->client->dev,
			"failed to put gpios in suspend state\n");
		goto gpio_configure_fail;
	}

	data->suspended = true;

	return 0;

gpio_configure_fail:
	#ifdef MSM_NEW_VER
	if (data->ts_pinctrl) {
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Cannot get active pinctrl state\n");
	}
	#endif
	
	if (data->pdata->power_on) {
		err = data->pdata->power_on(true);
		if (err)
			dev_err(dev, "power on failed");
	} else {
		err = fts_power_on(data, true);
		if (err)
			dev_err(dev, "power on failed");
	}
pwr_off_fail:
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(data->pdata->hard_rst_dly);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}
	enable_irq(data->client->irq);
	return err;
}

/*******************************************************************************
*  Name: fts_ts_suspend
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_ts_suspend(struct device *dev)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);

	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	int err = 0;
	#endif

	#if FTS_GESTRUE_EN
	if(enable_wakeup_gesture)
    {
	    printk("fts,enter gesture1\n");
	         fts_write_reg(fts_i2c_client, 0xd0, 0x01);
		  if (fts_updateinfo_curr.CHIP_ID==0x54 || fts_updateinfo_curr.CHIP_ID==0x58 || fts_updateinfo_curr.CHIP_ID==0x86 || fts_updateinfo_curr.CHIP_ID==0x87)
		  {
		  	fts_write_reg(fts_i2c_client, 0xd1, 0xff);
			fts_write_reg(fts_i2c_client, 0xd2, 0xff);
			fts_write_reg(fts_i2c_client, 0xd5, 0xff);
			fts_write_reg(fts_i2c_client, 0xd6, 0xff);
			fts_write_reg(fts_i2c_client, 0xd7, 0xff);
			fts_write_reg(fts_i2c_client, 0xd8, 0xff);
		  }

		data->suspended = true;
	enable_irq_wake(fts_i2c_client->irq);
	       return 0;
	}
	#endif
	if (data->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (data->suspended) {
		dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR	
	if (fts_psensor_support_enabled() && data->pdata->psensor_support &&
		device_may_wakeup(dev) &&
		data->psensor_pdata->tp_psensor_opened) {

		err = enable_irq_wake(data->client->irq);
		if (err)
			dev_err(&data->client->dev,
				"%s: set_irq_wake failed\n", __func__);
		data->suspended = true;
		return err;
	}
	#endif

	return fts_ts_stop(dev);
}

/*******************************************************************************
*  Name: fts_ts_resume
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_ts_resume(struct device *dev)
{
	int err;
	struct fts_ts_data *data = dev_get_drvdata(dev);

	if (!data->suspended) {
		dev_dbg(dev, "Already in awake state\n");
		return 0;
	}

#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	if (fts_psensor_support_enabled() && data->pdata->psensor_support &&
		device_may_wakeup(dev) &&
		data->psensor_pdata->tp_psensor_opened) {
		err = disable_irq_wake(data->client->irq);
		if (err)
			dev_err(&data->client->dev,
				"%s: disable_irq_wake failed\n",
				__func__);
		data->suspended = false;
		return err;
	}
#endif
    #ifdef FTS_GESTRUE_EN
	if(enable_wakeup_gesture)
	{
    		fts_write_reg(data->client,0xD0,0x00);
			disable_irq_wake(data->client->irq);
	}
	#endif
	err = fts_ts_start(dev);
	if (err < 0)
		return err;

	return 0;
}

static const struct dev_pm_ops fts_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = fts_ts_suspend,
	.resume = fts_ts_resume,
#endif
};
#else
/*******************************************************************************
*  Name: fts_ts_suspend
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_suspend(struct device *dev)
{
	return 0;
}
/*******************************************************************************
*  Name: fts_ts_resume
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_resume(struct device *dev)
{
	return 0;
}
#endif

#if defined(CONFIG_FB)
//add by zsw wkh for resume faster
static void fb_notify_resume_work(struct work_struct *work)
{
    struct fts_ts_data *fts_data =
		container_of(work, struct fts_ts_data, fb_notify_work);
	fts_ts_resume(&fts_data->client->dev);
}

/*******************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct fts_ts_data *fts_data =
		container_of(self, struct fts_ts_data, fb_notif);

	//if (evdata && evdata->data && event == FB_EVENT_BLANK &&
	//		fts_data && fts_data->client) {
	if(evdata && evdata->data && fts_data && fts_data->client){
		blank = evdata->data;
		//if (*blank == FB_BLANK_UNBLANK)
		//	fts_ts_resume(&fts_data->client->dev);
		//else if (*blank == FB_BLANK_POWERDOWN)
		//	fts_ts_suspend(&fts_data->client->dev);
		if (fts_data->pdata->resume_in_workqueue) {
			if (event == FB_EARLY_EVENT_BLANK &&
						 *blank == FB_BLANK_UNBLANK)
				schedule_work(&fts_data->fb_notify_work);
			else if (event == FB_EVENT_BLANK &&
						 *blank == FB_BLANK_POWERDOWN) {
				flush_work(&fts_data->fb_notify_work);
				fts_ts_suspend(&fts_data->client->dev);
			}
		} else {
			if (event == FB_EVENT_BLANK) {
		if (*blank == FB_BLANK_UNBLANK)
					fts_ts_resume(
						&fts_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
					fts_ts_suspend(
						&fts_data->client->dev);
			}
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*******************************************************************************
*  Name: fts_ts_early_suspend
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *data = container_of(handler,
						   struct fts_ts_data,
						   early_suspend);

	fts_ts_suspend(&data->client->dev);
}

/*******************************************************************************
*  Name: fts_ts_late_resume
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *data = container_of(handler,
						   struct fts_ts_data,
						   early_suspend);

	fts_ts_resume(&data->client->dev);
}
#endif


#ifdef CONFIG_OF
/*******************************************************************************
*  Name: fts_get_dt_coords
*  Brief:
*  Input:
*  Output:
*  Return: 
*******************************************************************************/
static int fts_get_dt_coords(struct device *dev, char *name,
				struct fts_ts_platform_data *pdata)
{
	u32 coords[FTS_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);	
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;
	

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}

/*******************************************************************************
*  Name: fts_parse_dt
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val, num_buttons;
	u32 button_map[MAX_BUTTONS];

	pdata->name = "focaltech";
	rc = of_property_read_string(np, "focaltech,name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
	}

	rc = fts_get_dt_coords(dev, "focaltech,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to get panel-coords \n");

	rc = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		dev_err(dev, "Unable to get display-coords \n");

	pdata->i2c_pull_up = of_property_read_bool(np, "focaltech,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np, "focaltech,no-force-update");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
	{
		dev_err(dev, "Unable to get reset_gpio \n");
	}

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
	{
		dev_err(dev, "Unable to get irq_gpio \n");
	}

	pdata->fw_name = "ft_fw.bin";
	rc = of_property_read_string(np, "focaltech,fw-name", &pdata->fw_name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw name\n");
	}

	rc = of_property_read_u32(np, "focaltech,group-id", &temp_val);
	if (!rc)
		pdata->group_id = temp_val;
	else
		dev_err(dev, "Unable to get group-id \n");

	rc = of_property_read_u32(np, "focaltech,hard-reset-delay-ms", &temp_val);
	if (!rc)
		pdata->hard_rst_dly = temp_val;
	else
		dev_err(dev, "Unable to get hard-reset-delay-ms \n");

	rc = of_property_read_u32(np, "focaltech,soft-reset-delay-ms", &temp_val);
	if (!rc)
		pdata->soft_rst_dly = temp_val;
	else
		dev_err(dev, "Unable to get soft-reset-delay-ms \n");

	rc = of_property_read_u32(np, "focaltech,num-max-touches", &temp_val);
	if (!rc)
		pdata->num_max_touches = temp_val;
	else
		dev_err(dev, "Unable to num-max-touches \n");

	rc = of_property_read_u32(np, "focaltech,fw-delay-aa-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay aa\n");
	} else if (rc != -EINVAL)
		pdata->info.delay_aa =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-55-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay 55\n");
	} else if (rc != -EINVAL)
		pdata->info.delay_55 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id1", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id1\n");
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_1 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-upgrade-id2", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw upgrade id2\n");
	} else if (rc != -EINVAL)
		pdata->info.upgrade_id_2 =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-readid-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay read id-ms \n");
	} else if (rc != -EINVAL)
		pdata->info.delay_readid =  temp_val;

	rc = of_property_read_u32(np, "focaltech,fw-delay-era-flsh-ms", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay erase flash\n");
	} else if (rc != -EINVAL)
		pdata->info.delay_erase_flash =  temp_val;

	pdata->info.AUTO_CLB = of_property_read_bool(np, "focaltech,fw-auto-cal");

	pdata->fw_vkey_support = of_property_read_bool(np, "focaltech,fw-vkey-support");

	pdata->ignore_id_check = of_property_read_bool(np, "focaltech,ignore-id-check");

	pdata->psensor_support = of_property_read_bool(np,
						"focaltech,psensor-support");
	
	rc = of_property_read_u32(np, "focaltech,family-id", &temp_val);
	if (!rc)
		pdata->family_id = temp_val;
	else
		dev_err(dev, "Unable to read family-id \n");

	prop = of_find_property(np, "focaltech,button-map", NULL);
	if (prop) {
		num_buttons = prop->length / sizeof(temp_val);
		if (num_buttons > MAX_BUTTONS)
			dev_err(dev, "Num_buttons is more than MAX_BUTTONS\n");
		rc = of_property_read_u32_array(np, "focaltech,button-map", button_map, num_buttons);
		if (rc) {
			dev_err(dev, "Unable to read key codes\n");
		}
	}
	//printk("fts,get resume_in_workqueue\n");
    pdata->resume_in_workqueue = of_property_read_bool(np,
					"focaltech,resume-in-workqueue");
	//printk("fts,get resume_in_workqueue= %d\n",pdata->resume_in_workqueue);
	if(pdata->resume_in_workqueue)
	    printk("gtp: it's true\n");

	return 0;
}
#else
/*******************************************************************************
*  Name: fts_parse_dt
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

/*******************************************************************************
*  Name: fts_debug_addr_is_valid
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static bool fts_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0xFF) {
		pr_err("FT reg address is invalid: 0x%x\n", addr);
		return false;
	}

	return true;
}

/*******************************************************************************
*  Name: fts_debug_data_set
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_data_set(void *_data, u64 val)
{
	struct fts_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (fts_debug_addr_is_valid(data->addr))
		dev_info(&data->client->dev,
			"Writing into FT registers not supported\n");

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

/*******************************************************************************
*  Name: fts_debug_data_get
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_data_get(void *_data, u64 *val)
{
	struct fts_ts_data *data = _data;
	int rc;
	u8 reg;

	mutex_lock(&data->input_dev->mutex);

	if (fts_debug_addr_is_valid(data->addr)) {
		rc = fts_read_reg(data->client, data->addr, &reg);
		if (rc < 0)
			dev_err(&data->client->dev,
				"FT read register 0x%x failed (%d)\n",
				data->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, fts_debug_data_get, fts_debug_data_set, "0x%02llX\n");

/*******************************************************************************
*  Name: fts_debug_addr_set
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_addr_set(void *_data, u64 val)
{
	struct fts_ts_data *data = _data;

	if (fts_debug_addr_is_valid(val)) {
		mutex_lock(&data->input_dev->mutex);
		data->addr = val;
		mutex_unlock(&data->input_dev->mutex);
	}

	return 0;
}

/*******************************************************************************
*  Name: fts_debug_addr_get
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_addr_get(void *_data, u64 *val)
{
	struct fts_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (fts_debug_addr_is_valid(data->addr))
		*val = data->addr;

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, fts_debug_addr_get, fts_debug_addr_set, "0x%02llX\n");

/*******************************************************************************
*  Name: fts_debug_suspend_set
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_suspend_set(void *_data, u64 val)
{
	struct fts_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		fts_ts_suspend(&data->client->dev);
	else
		fts_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

/*******************************************************************************
*  Name: fts_debug_suspend_get
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_debug_suspend_get(void *_data, u64 *val)
{
	struct fts_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, fts_debug_suspend_get, fts_debug_suspend_set, "%lld\n");

/*******************************************************************************
*  Name: fts_debug_dump_info
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_debug_dump_info(struct seq_file *m, void *v)
{
	struct fts_ts_data *data = m->private;

	seq_printf(m, "%s\n", data->ts_info);

	return 0;
}

/*******************************************************************************
*  Name: debugfs_dump_info_open
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, fts_debug_dump_info, inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};
//add by wkh begin
static ssize_t focaltech_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	//struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	//struct fts_ts_data *drvdata= dev_get_drvdata(dev);
    
	dev_info(&update_client->dev, "fts,enter show\n");
	return snprintf(buf, sizeof(enable_wakeup_gesture) + 2, "%u\n",enable_wakeup_gesture);
}


static ssize_t focaltech_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	//struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
    //struct fts_ts_data *drvdata= dev_get_drvdata(dev);
    //printk("fts,enter store\n");
    dev_info(&update_client->dev, "fts,enter store\n");
	if (sscanf(buf, "%u", &input) != 1)
	{
		//printk("fts,can not read input\n");
		dev_info(&update_client->dev, "fts,can not read input\n");
		return -EINVAL;
	}

	input = input > 0 ? 1 : 0;

	if (enable_wakeup_gesture != input)
	{
		enable_wakeup_gesture = input;
		
		if(enable_wakeup_gesture == 1)
	    {
		    fts_Gesture_init(fts_input_dev);
	    }
	}
	
	return count;
}

static DEVICE_ATTR(wake_gesture, (S_IRUGO | S_IWUSR | S_IWGRP),  focaltech_rmi4_wake_gesture_show,  focaltech_rmi4_wake_gesture_store); 

static struct attribute *focaltech_wake_gesture_attributes[] = {	
	&dev_attr_wake_gesture.attr,		
	NULL
};
static struct attribute_group focaltech_wake_gesture_attribute_group = {	
	.attrs = focaltech_wake_gesture_attributes
};

//end

//zz
ssize_t FtsTouchscreenInfoRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
    char szOut[100] = {0}, szValue[10] = {0};
	//char buf = 0;
	int err = 0;
	u8 reg_value;
	u8 reg_addr;
	u32 nLength = 0;
	
	if (*pPos != 0)
    {
        return 0;
    }
	
	strcat(szOut, "Focaltech FT3327\n");
	reg_addr = FT5X0X_REG_FT5201ID;
	err = fts_i2c_read(update_client, &reg_addr, 1, &reg_value, 1);
	//dev_info(&update_client->dev, "show Firmware version = 0x%x\n", reg_value);
	switch (reg_value){
	case 0x15://
	    strcat(szOut,"module : chuangwei");
		sprintf(szValue, "(0x%x)",reg_value);
		break;
        case 0x82://
	    strcat(szOut,"module : helitai");
		sprintf(szValue, "(0x%x)",reg_value);
		break;
	default:
	    strcat(szOut,"unknown vendor");
		sprintf(szValue, "(0x%x)",reg_value);
	}
	strcat(szOut, szValue);
    strcat(szOut, "\n");
	
	/* read firmware version */
	reg_addr = FT5X0X_REG_FIRMID;	
	err = fts_i2c_read(update_client, &reg_addr, 1, &reg_value, 1);
	sprintf(szValue, "(0x%x)",reg_value);
	strcat(szOut,"firmware version");
	strcat(szOut, szValue);
	strcat(szOut, "\n");
	
	nLength = sprintf(pBuffer, "%s\n", szOut);

    *pPos += nLength;
    
    return nLength;	
		
}
//zz

/*******************************************************************************
*  Name: fts_ts_probe
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fts_ts_platform_data *pdata;
	
	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	struct fts_psensor_platform_data *psensor_pdata;
	struct input_dev *psensor_input_dev;
	#endif
	
	struct fts_ts_data *data;
	struct input_dev *input_dev;
	
	struct dentry *temp;
	u8 reg_value;
	u8 reg_addr;
	int err, len;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct fts_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = fts_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "DT parsing failed\n");
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Not enough memory\n");
		kfree(pdata);
		return -ENOMEM;
	}

	fts_wq_data = data;

	if (pdata->fw_name) {
		len = strlen(pdata->fw_name);
		if (len > FTS_FW_NAME_MAX_LEN - 1) {
			dev_err(&client->dev, "Invalid firmware name\n");
			return -EINVAL;
		}

		strlcpy(data->fw_name, pdata->fw_name, len + 1);
	}

	data->tch_data_len = FTS_TCH_LEN(pdata->num_max_touches);
	data->tch_data = devm_kzalloc(&client->dev, data->tch_data_len, GFP_KERNEL);
	
	if (!data->tch_data) {
		dev_err(&client->dev, "Not enough memory\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		goto free_input_dev;
		//return -ENOMEM;
	}

	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;

	input_dev->name = "fts_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, pdata->num_max_touches,INPUT_MT_DIRECT);
//	input_mt_init_slots(input_dev, pdata->num_max_touches);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0x0f, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xff, 0, 0);

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev, "Input device registration failed\n");
		goto free_inputdev;
	}

	if (pdata->power_init) {
		err = pdata->power_init(true);
		if (err) {
			dev_err(&client->dev, "pdata->power_init power init failed");
			goto unreg_inputdev;
		}
	} else {
		err = fts_power_init(data, true);
		if (err) {
			dev_err(&client->dev, "fts_power_init power init failed");
			goto unreg_inputdev;
		}
	}

	if (pdata->power_on) {
		err = pdata->power_on(true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	} else {
		err = fts_power_on(data, true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	}

	#ifdef MSM_NEW_VER
	err = fts_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0) {
			dev_err(&client->dev,
				"failed to select pin to active state");
		}
	}
	#endif

	err = fts_gpio_configure(data, true);
	if (err < 0) {
		dev_err(&client->dev,
			"Failed to configure the gpios\n");
		goto err_gpio_req;
	}

	/* make sure CTP already finish startup process */
	msleep(data->pdata->soft_rst_dly);

	INIT_WORK(&data->touch_event_work, fts_touch_irq_work);
	data->ts_workqueue = create_workqueue(FTS_WORKQUEUE_NAME);
	if (!data->ts_workqueue)
	{
		err = -ESRCH;
		goto exit_create_singlethread;
	}


	/* check the controller id */
	reg_addr = FTS_REG_ID;
	err = fts_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0) {
		dev_err(&client->dev, "version read failed");
	}

	dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);

	if ((pdata->family_id != reg_value) && (!pdata->ignore_id_check)) {
		dev_err(&client->dev, "%s:Unsupported controller\n", __func__);
	}

	data->family_id = pdata->family_id;

	fts_i2c_client = client;
	fts_input_dev = input_dev;

	fts_get_upgrade_array();

	err = request_threaded_irq(client->irq, NULL, fts_ts_interrupt,
				pdata->irqflags | IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
				client->dev.driver->name, data);
	if (err) {
		dev_err(&client->dev, "request irq failed\n");
		goto free_gpio;
	}

	disable_irq(client->irq);

	#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	if (fts_psensor_support_enabled() && data->pdata->psensor_support) {
		device_init_wakeup(&client->dev, 1);
		psensor_pdata = devm_kzalloc(&client->dev,
				sizeof(struct fts_psensor_platform_data),
				GFP_KERNEL);
		if (!psensor_pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			goto irq_free;
		}
		data->psensor_pdata = psensor_pdata;

		psensor_input_dev = input_allocate_device();
		if (!psensor_input_dev) {
			dev_err(&data->client->dev,
				"Failed to allocate device\n");
			goto free_psensor_pdata;
		}

		__set_bit(EV_ABS, psensor_input_dev->evbit);
		input_set_abs_params(psensor_input_dev,
					ABS_DISTANCE, 0, 1, 0, 0);
		psensor_input_dev->name = "proximity";
		psensor_input_dev->id.bustype = BUS_I2C;
		psensor_input_dev->dev.parent = &data->client->dev;
		data->psensor_pdata->input_psensor_dev = psensor_input_dev;

		err = input_register_device(psensor_input_dev);
		if (err) {
			dev_err(&data->client->dev,
				"Unable to register device, err=%d\n", err);
			goto free_psensor_input_dev;
		}

		psensor_pdata->ps_cdev = sensors_proximity_cdev;
		psensor_pdata->ps_cdev.sensors_enable =
					fts_psensor_enable_set;
		psensor_pdata->data = data;

		err = sensors_classdev_register(&client->dev,
						&psensor_pdata->ps_cdev);
		if (err)
			goto unregister_psensor_input_device;
	}
	#endif
	
	data->dir = debugfs_create_dir(FTS_DEBUG_DIR_NAME, NULL);
	if (data->dir == NULL || IS_ERR(data->dir)) {
		pr_err("debugfs_create_dir failed(%ld)\n", PTR_ERR(data->dir));
		err = PTR_ERR(data->dir);
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_dump_info_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}
	
	data->ts_info = devm_kzalloc(&client->dev, FTS_INFO_MAX_LEN, GFP_KERNEL);
	if (!data->ts_info) {
		dev_err(&client->dev, "Not enough memory\n");
		goto free_debug_dir;
	}

	/*get some register information */
	reg_addr = FTS_REG_POINT_RATE;
	fts_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "report rate read failed");

	dev_info(&client->dev, "report rate = %dHz\n", reg_value * 10);

	reg_addr = FTS_REG_THGROUP;
	err = fts_i2c_read(client, &reg_addr, 1, &reg_value, 1);
	if (err < 0)
		dev_err(&client->dev, "threshold read failed");

	dev_dbg(&client->dev, "touch threshold = %d\n", reg_value * 4);

	fts_update_fw_ver(data);
	fts_update_fw_vendor_id(client);

	FTS_STORE_TS_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);

	#ifdef FTS_APK_DEBUG
		fts_create_apk_debug_channel(client);
	#endif

	#ifdef FTS_SYSFS_DEBUG
		fts_create_sysfs(client);
	#endif

//zz add by wkh begin for ic info
    update_client = client;
	_gProcTouchscreenInfo = proc_mkdir("touchscreen",NULL);
    if (NULL == proc_create("touchscreen_info", 0644, NULL, &info_proc_file_ops))
    {
        printk("Failed to create procfs file node!\n");
    }   
    else 
    {
        printk("Create procfs file node OK!\n");
    }
	focaltech_tp_wake_gesture_kobj = kobject_create_and_add("tp_wake_gesture", NULL);	 
	if (!focaltech_tp_wake_gesture_kobj)	
	{		 
		err = -EINVAL;		 
		//printk("syna--ERROR Unable to create shtsc_wake_gesture_kobj\n");
		dev_err(&client->dev, "fts--ERROR Unable to create shtsc_wake_gesture_kobj\n");
	}	 
	err = sysfs_create_group(focaltech_tp_wake_gesture_kobj, &focaltech_wake_gesture_attribute_group);	 
	if (0 != err)	 
	{		 
		//printk("syna--ERROR: sysfs_create_group() failed.\n");
		dev_err(&client->dev, "fts--ERROR: sysfs_create_group() failed.\n");
		//sysfs_remove_group(&client->dev.kobj, &gt9xx_wake_gesture_attribute_group);		 
		kobject_put(focaltech_tp_wake_gesture_kobj);		
		//return -EIO;	
	}	  
	else	 
	{		 
		//printk("GTP:sysfs_create_group() succeeded.\n");
		dev_err(&client->dev, "fts--sysfs_create_group() succeeded.\n");
	}
	//end
    //add for gesture begin

	//end
//zz add by wangkuihua end
	
	#ifdef FTS_CTL_IIC
		if (fts_rw_iic_drv_init(client) < 0)	
		{
			dev_err(&client->dev, "%s:[FTS] create fts control iic driver failed\n",	__func__);
		}
	#endif
	
#if 0
	#if FTS_GESTRUE_EN
		fts_Gesture_init(input_dev);
		 if (fts_updateinfo_curr.CHIP_ID != 0x54 &&  fts_updateinfo_curr.CHIP_ID != 0x58 &&  fts_updateinfo_curr.CHIP_ID != 0x86 &&  fts_updateinfo_curr.CHIP_ID != 0x87)
		 {
		  	init_para(720,1280,0,0,0);
		 }
	#endif
#endif
	
	#ifdef FTS_AUTO_UPGRADE
	printk("********************Enter CTP Auto Upgrade********************\n");
	//if(fw_vendor_id == FTS_VENDOR_ID_CHUANGWEI)
	fts_ctpm_auto_upgrade(client);
	#endif 
	

	
#if defined(CONFIG_FB)
    INIT_WORK(&data->fb_notify_work, fb_notify_resume_work);
	data->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&data->fb_notif);

	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n", err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	data->early_suspend.suspend = fts_ts_early_suspend;
	data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	enable_irq(client->irq);

	return 0;

free_debug_dir:
	debugfs_remove_recursive(data->dir);
	
#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
unregister_psensor_input_device:
	if (fts_psensor_support_enabled() && data->pdata->psensor_support)
		input_unregister_device(data->psensor_pdata->input_psensor_dev);
free_psensor_input_dev:
	if (fts_psensor_support_enabled() && data->pdata->psensor_support)
		input_free_device(data->psensor_pdata->input_psensor_dev);
free_psensor_pdata:
	if (fts_psensor_support_enabled() && data->pdata->psensor_support) {
		devm_kfree(&client->dev, psensor_pdata);
		data->psensor_pdata = NULL;
	}
irq_free:
	if ((fts_psensor_support_enabled() &&
		data->pdata->psensor_support))
		device_init_wakeup(&client->dev, 0);
	free_irq(client->irq, data);
#endif

free_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
exit_create_singlethread:
	printk("==singlethread error =\n");
	i2c_set_clientdata(client, NULL);
err_gpio_req:
	#ifdef MSM_NEW_VER
	if (data->ts_pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_release);
			if (err)
				pr_err("failed to select relase pinctrl state\n");
		}
	}
	#endif

	if (pdata->power_on)
		pdata->power_on(false);
	else
		fts_power_on(data, false);
pwr_deinit:
	if (pdata->power_init)
		pdata->power_init(false);
	else
		fts_power_init(data, false);
unreg_inputdev:
	input_unregister_device(input_dev);
	//input_dev = NULL;
free_inputdev:
	input_free_device(input_dev);
free_input_dev:
    devm_kfree(&client->dev,data->tch_data);
	devm_kfree(&client->dev,data);
	devm_kfree(&client->dev,pdata);
	return err;
}

/*******************************************************************************
*  Name: fts_ts_remove
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int fts_ts_remove(struct i2c_client *client)
{
	struct fts_ts_data *data = i2c_get_clientdata(client);

	cancel_work_sync(&data->touch_event_work);
	destroy_workqueue(data->ts_workqueue);

	debugfs_remove_recursive(data->dir);
	//add by wkh begin
	if(focaltech_tp_wake_gesture_kobj)
		{
			sysfs_remove_group(focaltech_tp_wake_gesture_kobj, &focaltech_wake_gesture_attribute_group);	
			kobject_put(focaltech_tp_wake_gesture_kobj);
		}
	
	//end

#ifdef CONFIG_TOUCHSCREEN_FTS_PSENSOR
	if (fts_psensor_support_enabled() && data->pdata->psensor_support) {

		device_init_wakeup(&client->dev, 0);
		sensors_classdev_unregister(&data->psensor_pdata->ps_cdev);
		input_unregister_device(data->psensor_pdata->input_psensor_dev);
		devm_kfree(&client->dev, data->psensor_pdata);
		data->psensor_pdata = NULL;
	}
#endif
	
#ifdef FTS_APK_DEBUG
		fts_release_apk_debug_channel();
#endif

#ifdef FTS_SYSFS_DEBUG
		fts_remove_sysfs(fts_i2c_client);
#endif


#ifdef FTS_CTL_IIC
		fts_rw_iic_drv_exit();
#endif


#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	if (data->pdata->power_on)
		data->pdata->power_on(false);
	else
		fts_power_on(data, false);

	if (data->pdata->power_init)
		data->pdata->power_init(false);
	else
		fts_power_init(data, false);

	input_unregister_device(data->input_dev);

	return 0;
}

static const struct i2c_device_id fts_ts_id[] = {
	{"fts_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, fts_ts_id);

#ifdef CONFIG_OF
static struct of_device_id fts_match_table[] = {
	{ .compatible = "focaltech,fts",},
	{ },
};
#else
#define fts_match_table NULL
#endif

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		   .name = "fts_ts",
		   .owner = THIS_MODULE,
		   .of_match_table = fts_match_table,
#ifdef CONFIG_PM
		   .pm = &fts_ts_pm_ops,
#endif
		   },
	.id_table = fts_ts_id,
};

/*******************************************************************************
*  Name: fts_ts_init
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static int __init fts_ts_init(void)
{
	return i2c_add_driver(&fts_ts_driver);
}

/*******************************************************************************
*  Name: fts_ts_exit
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_DESCRIPTION("FocalTech fts TouchScreen driver");
MODULE_LICENSE("GPL v2");