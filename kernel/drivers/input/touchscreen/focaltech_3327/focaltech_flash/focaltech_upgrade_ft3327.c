/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
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
 */

/*****************************************************************************
*
* File Name: focaltech_upgrade_ft5x46.c
*
* Author:    fupeipei
*
* Created:    2016-08-15
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "../focaltech_core.h"

#if (IC_SERIALS == 0x02)
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
* Static variables
*****************************************************************************/
#define APP_FILE_MAX_SIZE           (60 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define CONFIG_START_ADDR           (0xD780)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)
/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
static int fts_ft5x46_get_i_file(struct i2c_client *client, int fw_valid);
static int fts_ft5x46_get_app_i_file_ver(void);
static int fts_ft5x46_get_app_bin_file_ver(char *firmware_name);
static int fts_ft5x46_upgrade_with_app_i_file(struct i2c_client *client);
static int fts_ft5x46_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name);
#ifdef CONFIG_CREATE_TPD_SYS_INTERFACE
static bool fts_check_app_bin_valid(u8 * pbt_buf, u32 dw_lenth);
//static int fts_ctpm_fw_upgrade_use_buf(struct i2c_client *client, u8 * pbt_buf, u32 dw_lenth);
#define GET_VENDORID_FROM_BOOTLOADER
#ifdef GET_VENDORID_FROM_BOOTLOADER
int tpd_get_chipvendor_from_bootloader(struct i2c_client *client, int *vendor_id, int *chip_id)
{
	int i_ret = -1;
	u8 reg_val[4] = { 0 };
	u8 vendorid[4] = { 0 };
	int i = 0;
	u8 auc_i2c_write_buf[10];

	*vendor_id = 0;
	*chip_id = 0;

	fts_ctpm_i2c_hid2std(client);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*write 0xaa to register FTS_RST_CMD_REG1 */
		fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
		msleep(10);

		/*write 0x55 to register FTS_RST_CMD_REG1 */
		fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_55);
		msleep(200);

		/*Enter upgrade mode */
		fts_ctpm_i2c_hid2std(client);

		msleep(10);
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
		i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
		if (i_ret < 0) {
			FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
			continue;
		}

		/*check run in bootloader or not */
		msleep(1);
		auc_i2c_write_buf[0] = FTS_READ_ID_REG;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);

		if (reg_val[0] == chip_types.bootloader_idh && reg_val[1] == chip_types.bootloader_idl) {
			FTS_INFO("[UPGRADE]: read bootload id ok!! ID1 = 0x%x,ID2 = 0x%x!!", reg_val[0], reg_val[1]);
			break;
		}
		else {
			FTS_ERROR("[UPGRADE]: read bootload id fail!! ID1 = 0x%x,ID2 = 0x%x!!", reg_val[0], reg_val[1]);
			continue;
		}
	}

	if (i >= FTS_UPGRADE_LOOP) {
		i_ret =  -EIO;
		goto out;
	}

	/*read vendor id from flash, if vendor id error, can not upgrade */
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		/*read project code */
		auc_i2c_write_buf[0] = 0x03;
		auc_i2c_write_buf[1] = 0x00;

		auc_i2c_write_buf[2] = (u8) ((CONFIG_VENDOR_ID_ADDR - 1) >> 8);
		auc_i2c_write_buf[3] = (u8) (CONFIG_VENDOR_ID_ADDR - 1);
		i_ret = fts_i2c_read(client, auc_i2c_write_buf, 4, vendorid, 2);
		if (i_ret < 0) {
			FTS_INFO("[UPGRADE]: read flash : i_ret = %d!!", i_ret);
			continue;
		} else {
			break;
		}
	}

	FTS_INFO("[UPGRADE]: vendor id from flash rom: 0x%x!!", vendorid[1]);
	if (i >= FTS_UPGRADE_LOOP) {
		FTS_ERROR("[UPGRADE]: read vendor id from flash more than 30 times!!");
		i_ret =  -EIO;
		goto out;
	}
	*vendor_id = vendorid[1];
	
	i_ret = 0;

out:
	fts_reset_proc(200);
	return i_ret;
}
#endif
#if 0
int tpd_fts_ctpm_firmware_upgrade(struct i2c_client *client, unsigned char * pbt_buf, unsigned int fwsize, unsigned int force_upg)
{
	int i_ret = 0;
	bool ecc_ok = false;
	
	/*check the firmware invalid or not */
	ecc_ok = fts_check_app_bin_valid(pbt_buf, fwsize);
	if (ecc_ok) {
		FTS_INFO("[UPGRADE] app.bin ecc ok");
		i_ret = fts_ctpm_fw_upgrade_use_buf(client, pbt_buf, fwsize);
		if (i_ret != 0) {
			FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
		} else {
			FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
		}
	} else {
		FTS_ERROR("[UPGRADE] app.bin ecc failed");
	}
	return i_ret;
}
#endif
int tpd_fts_ctpm_get_version_etc(struct i2c_client *i2c_client, int * chipid_in_chip, int *fwver_in_chip, int *module_id_in_chip)
{
	unsigned char uc_reg_value = 0;
#ifdef GET_VENDORID_FROM_BOOTLOADER
	int vendor_id = 0, chip_part_id = 0;
#endif
	int i = 0;

	while (i < 2) {
		i++;
		fts_i2c_read_reg(i2c_client, FTS_REG_CHIP_ID, &uc_reg_value);
		*chipid_in_chip = uc_reg_value;
		fts_i2c_read_reg(i2c_client, FTS_REG_FW_VER, &uc_reg_value);
		*fwver_in_chip = uc_reg_value;
		fts_i2c_read_reg(i2c_client, FTS_REG_VENDOR_ID, &uc_reg_value);
		*module_id_in_chip = uc_reg_value;
		
		if(0x00 == *module_id_in_chip || 0xff == *module_id_in_chip ||0xef == *module_id_in_chip || FTS_REG_VENDOR_ID == *module_id_in_chip) {
			printk("tpd vendor id read abnormal, reset and read again. \n");
#ifdef GET_VENDORID_FROM_BOOTLOADER
			tpd_get_chipvendor_from_bootloader(i2c_client, &vendor_id, &chip_part_id);
			*module_id_in_chip = vendor_id;
			*fwver_in_chip = 0x00;
			*chipid_in_chip = chip_part_id;
			break;
#else
			fts_reset_proc(200);
#endif
		} else {
			break;
		}
	}

#ifndef GET_VENDORID_FROM_BOOTLOADER
	if(0x00 == *module_id_in_chip || 0xff == *module_id_in_chip ||0xef == *module_id_in_chip ||  FTS_REG_VENDOR_ID == *module_id_in_chip) {
		printk("%s read firmware version is abnormal.", __func__);
		if(NULL != strstr(tpd_fw_cdev.lcm_info, "HeLiTai")){
			printk("tpd oncell structure, lcm dijing, tp Holitech.\n");
			*module_id_in_chip = 0x82;
		} else if(NULL != strstr(tpd_fw_cdev.lcm_info, "ChuangWei")){
			printk("tpd oncell structure, lcm chuangwei, tp chuangwei.\n");
			*module_id_in_chip = 0x15;
		}
	}
#endif

	return 0;
}
#endif

struct fts_upgrade_fun fts_updatefun =
{
    .get_i_file = fts_ft5x46_get_i_file,
    .get_app_bin_file_ver = fts_ft5x46_get_app_bin_file_ver,
    .get_app_i_file_ver = fts_ft5x46_get_app_i_file_ver,
    .upgrade_with_app_i_file = fts_ft5x46_upgrade_with_app_i_file,
    .upgrade_with_app_bin_file = fts_ft5x46_upgrade_with_app_bin_file,
    .upgrade_with_lcd_cfg_i_file = NULL,
    .upgrade_with_lcd_cfg_bin_file = NULL,
#ifdef CONFIG_CREATE_TPD_SYS_INTERFACE
			.tpd_get_version_etc = tpd_fts_ctpm_get_version_etc,
			//.tpd_firmware_upgrade = tpd_fts_ctpm_firmware_upgrade,
#endif

};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
#if (FTS_GET_VENDOR_ID_NUM != 0)
/************************************************************************
* Name: fts_ft5x46_get_vendor_id_flash
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int fts_ft5x46_get_vendor_id_flash(struct i2c_client *client, u8 *vendor_id)
{
    u8 reg_val[2] = {0};
    u32 i = 0;
    u8 rw_buf[10];
    int i_ret;

    fts_ctpm_i2c_hid2std(client);

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        rw_buf[0] = FTS_UPGRADE_55;
        rw_buf[1] = FTS_UPGRADE_AA;
        i_ret = fts_i2c_write(client, rw_buf, 2);
        if (i_ret < 0)
        {
            FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
            continue;
        }

        /*check run in bootloader or not*/
        msleep(1);
        rw_buf[0] = FTS_READ_ID_REG;
        rw_buf[1] = rw_buf[2] = rw_buf[3] = 0x00;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, rw_buf, 4, reg_val, 2);

        if ((reg_val[0] == chip_types.bootloader_idh)
            && (reg_val[1] == chip_types.bootloader_idl))
        {
            FTS_DEBUG("[UPGRADE]: read bootloader id ok!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            break;
        }
        else
        {
            FTS_ERROR("[UPGRADE]: read bootloader id fail!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP )
        return -EIO;

    /*read vendor id*/
    rw_buf[0] = 0x03;
    rw_buf[1] = 0x00;
    rw_buf[2] = (u8)(CONFIG_VENDOR_ID_ADDR >> 8);
    rw_buf[3] = (u8)(CONFIG_VENDOR_ID_ADDR);
    i_ret = fts_i2c_write(client, rw_buf, 4);
    msleep(10); /* must wait, otherwise read vendor id wrong */
    i_ret = fts_i2c_read(client, NULL, 0, vendor_id, 1);
    if (i_ret < 0)
    {
        return -EIO;
    }
    FTS_DEBUG("Vendor ID from Flash:%x", *vendor_id);
    return 0;
}
#endif

/************************************************************************
* Name: fts_ft5x46_get_i_file
* Brief: get .i file
* Input:
* Output:
* Return: 0   - ok
*         <0 - fail
***********************************************************************/
static int fts_ft5x46_get_i_file(struct i2c_client *client, int fw_valid)
{
    int ret = 0;

#if (FTS_GET_VENDOR_ID_NUM != 0)
    u8 vendor_id = 0;
    if (fw_valid)
        ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &vendor_id);
    else
        ret = fts_ft5x46_get_vendor_id_flash(client, &vendor_id);

    FTS_DEBUG("[UPGRADE] tp_vendor_id=%x", vendor_id);
    if (ret < 0)
    {
        FTS_ERROR("Get upgrade file fail because of Vendor ID wrong");
        return ret;
    }

    FTS_INFO("[UPGRADE]tp vendor id:%x, FTS_VENDOR_ID:%02x %02x %02x",
             vendor_id, FTS_VENDOR_1_ID, FTS_VENDOR_2_ID, FTS_VENDOR_3_ID);
    ret = 0;
    switch (vendor_id)
    {
#if (FTS_GET_VENDOR_ID_NUM >= 1)        
    case FTS_VENDOR_1_ID:
        g_fw_file = CTPM_FW;
        g_fw_len = fts_getsize(FW_SIZE);
        FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW, SIZE:%x", g_fw_len);
        break;
#endif 
#if (FTS_GET_VENDOR_ID_NUM >= 2)  
    case FTS_VENDOR_2_ID:
        g_fw_file = CTPM_FW2;
        g_fw_len = fts_getsize(FW2_SIZE);
        FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW2, SIZE:%x", g_fw_len);
        break;   
#endif
#if (FTS_GET_VENDOR_ID_NUM >= 3)  
    case FTS_VENDOR_3_ID:
        g_fw_file = CTPM_FW3;
        g_fw_len = fts_getsize(FW3_SIZE);
        FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW3, SIZE:%x", g_fw_len);
        break;
#endif        
    default:
        FTS_ERROR("[UPGRADE]Vendor ID check fail, get fw file fail");
        ret = -EIO;
        break;
    }
#else
    /* (FTS_GET_VENDOR_ID_NUM == 0) */
    g_fw_file = CTPM_FW;
    g_fw_len = fts_getsize(FW_SIZE);
    FTS_DEBUG("[UPGRADE]FW FILE:CTPM_FW, SIZE:%x", g_fw_len);
#endif

    return ret;
}

/************************************************************************
* Name: fts_ft5x46_get_app_bin_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ft5x46_get_app_bin_file_ver(char *firmware_name)
{
    u8 *pbt_buf = NULL;
    int fwsize = 0;
    int fw_ver = 0;

    FTS_FUNC_ENTER();

    fwsize = fts_GetFirmwareSize(firmware_name);
    if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE)
    {
        FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
        return -EIO;
    }

    pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
    if (fts_ReadFirmware(firmware_name, pbt_buf))
    {
        FTS_ERROR("[UPGRADE]: request_firmware failed!!");
        kfree(pbt_buf);
        return -EIO;
    }

    if (fwsize > 2)
    {
        fw_ver = pbt_buf[fwsize-2];
    }

    kfree(pbt_buf);
    FTS_FUNC_EXIT();

    return fw_ver;
}

/************************************************************************
* Name: fts_ft5x46_get_app_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ft5x46_get_app_i_file_ver(void)
{
    int fwsize = g_fw_len;

    if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE)
    {
        FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
        return 0;
    }

    return g_fw_file[fwsize-2];
}

#define AL2_FCS_COEF    ((1 << 7) + (1 << 6) + (1 << 5))
/*****************************************************************************
*   Name: ecc_calc
*  Brief:
*  Input:
* Output:
* Return:
*****************************************************************************/
static u8 ecc_calc(u8 *pbt_buf, u16 start, u16 length)
{
    u8 cFcs = 0;
    u16 i, j;

    for ( i = 0; i < length; i++ )
    {
        cFcs ^= pbt_buf[start++];
        for (j = 0; j < 8; j ++)
        {
            if (cFcs & 1)
            {
                cFcs = (u8)((cFcs >> 1) ^ AL2_FCS_COEF);
            }
            else
            {
                cFcs >>= 1;
            }
        }
    }
    return cFcs;
}

/*****************************************************************************
* Name: fts_check_app_bin_valid
* Brief:
* Input:
* Output:
* Return:
*****************************************************************************/
static bool fts_check_app_bin_valid(u8 *pbt_buf, u32 dw_lenth)
{
    u8 ecc1;
    u8 ecc2;
    u16 len1;
    u16 len2;
    u8 cal_ecc;
    u16 usAddrInfo;

    /* 1. First Byte */
    if (pbt_buf[0] != 0x02)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- the first byte(%x) error", pbt_buf[0]);
        return false;
    }

    usAddrInfo = dw_lenth - 8;

    /* 2.len */
    len1  = pbt_buf[usAddrInfo++] << 8;
    len1 += pbt_buf[usAddrInfo++];

    len2  = pbt_buf[usAddrInfo++] << 8;
    len2 += pbt_buf[usAddrInfo++];

    if ((len1 + len2) != 0xFFFF)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- LENGTH(%04x) XOR error", len1);
        return false;
    }

    /* 3.ecc */
    ecc1 = pbt_buf[usAddrInfo++];
    ecc2 = pbt_buf[usAddrInfo++];

    if ((ecc1 + ecc2) != 0xFF)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC(%x) XOR error", ecc1);
        return false;
    }

    cal_ecc = ecc_calc(pbt_buf, 0x0, len1);

    if (ecc1 != cal_ecc)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC calc error");
        return false;
    }
    return true;
}


/************************************************************************
* Name: fts_ft5x46_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ft5x46_upgrade_use_buf(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
    u8 reg_val[4] = {0};
    u32 i = 0;
    u32 packet_number;
    u32 j = 0;
    u32 temp;
    u32 lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 upgrade_ecc;
    int i_ret;

    fts_ctpm_i2c_hid2std(client);

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        /*********Step 1:Reset  CTPM *****/
        fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
        msleep(10);
        fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_55);
        msleep(200);

        /*********Step 2:Enter upgrade mode *****/
        fts_ctpm_i2c_hid2std(client);
        msleep(5);

        auc_i2c_write_buf[0] = FTS_UPGRADE_55;
        auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
        i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
        if (i_ret < 0)
        {
            FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
            continue;
        }

        /*********Step 3:Check bootloader ID *****/
        msleep(1);
        auc_i2c_write_buf[0] = FTS_READ_ID_REG;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
        if ((reg_val[0] == chip_types.bootloader_idh)
            && (reg_val[1] == chip_types.bootloader_idl))
        {
            FTS_DEBUG("[UPGRADE]: read bootload id ok!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            break;
        }
        else
        {
            FTS_ERROR("[UPGRADE]: read bootload id fail!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP)
    {
        FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa : i = %d!!", i);
        return -EIO;
    }

    /*Step 4:erase app and panel paramenter area*/
    FTS_DEBUG("[UPGRADE]: erase app and panel paramenter area!!");
    auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(1350);
    for (i = 0; i < 15; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
        if ((0xF0 == reg_val[0]) && (0xAA == reg_val[1]))
        {
            break;
        }
        msleep(50);
    }
    FTS_DEBUG("[UPGRADE]: erase app area reg_val[0] = %x reg_val[1] = %x!!", reg_val[0], reg_val[1]);

    auc_i2c_write_buf[0] = 0xB0;
    auc_i2c_write_buf[1] = (u8) ((dw_lenth >> 16) & 0xFF);
    auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
    auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);
    fts_i2c_write(client, auc_i2c_write_buf, 4);

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    upgrade_ecc = 0;
    FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = FTS_FW_WRITE_CMD;
    packet_buf[1] = 0x00;

    for (j = 0; j < packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (lenght >> 8);
        packet_buf[5] = (u8) lenght;
        for (i = 0; i < FTS_PACKET_LENGTH; i++)
        {
            packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
            upgrade_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);
        msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            FTS_DEBUG("[UPGRADE]: reg_val[0] = %x reg_val[1] = %x!!", reg_val[0], reg_val[1]);
            //msleep(1);
            fts_ctpm_upgrade_delay(1000);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (temp >> 8);
        packet_buf[5] = (u8) temp;
        for (i = 0; i < temp; i++)
        {
            packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
            upgrade_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_write(client, packet_buf, temp + 6);
        msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((0x1000 + ((packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            FTS_DEBUG("[UPGRADE]: reg_val[0] = %x reg_val[1] = %x  reg_val[2] = 0x%x!!", reg_val[0], reg_val[1], (((packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))+0x1000));
            //msleep(1);
            fts_ctpm_upgrade_delay(1000);
        }
    }

    msleep(50);

    /*********Step 6: read out checksum***********************/
    /*send the opration head */
    FTS_DEBUG("[UPGRADE]: read out checksum!!");
    auc_i2c_write_buf[0] = 0x64;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(300);

    temp = 0;
    auc_i2c_write_buf[0] = 0x65;
    auc_i2c_write_buf[1] = (u8)(temp >> 16);
    auc_i2c_write_buf[2] = (u8)(temp >> 8);
    auc_i2c_write_buf[3] = (u8)(temp);
    temp = dw_lenth;
    auc_i2c_write_buf[4] = (u8)(temp >> 8);
    auc_i2c_write_buf[5] = (u8)(temp);
    i_ret = fts_i2c_write(client, auc_i2c_write_buf, 6);
    msleep(dw_lenth/256);

    for (i = 0; i < 100; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
        FTS_DEBUG("[UPGRADE]: reg_val[0]=%02x reg_val[0]=%02x!!", reg_val[0], reg_val[1]);
        if ((0xF0 == reg_val[0]) && (0x55 == reg_val[1]))
        {
            break;
        }
        msleep(1);
    }
    auc_i2c_write_buf[0] = 0x66;
    fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
    if (reg_val[0] != upgrade_ecc)
    {
        FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!", reg_val[0], upgrade_ecc);
        return -EIO;
    }
    FTS_DEBUG("[UPGRADE]: checksum %x %x!!",reg_val[0],upgrade_ecc);

    FTS_DEBUG("[UPGRADE]: reset the new FW!!");
    auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(200);

    fts_ctpm_i2c_hid2std(client);

    return 0;
}

/************************************************************************
* Name: fts_ft5x46_upgrade_with_app_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output:
* Return: fail < 0
***********************************************************************/
static int fts_ft5x46_upgrade_with_app_i_file(struct i2c_client *client)
{
    int i_ret = 0;
    u32 fw_len;
    u8 *fw_buf;

    FTS_INFO("[UPGRADE]**********start upgrade with app.i**********");

    fw_len = g_fw_len;
    fw_buf = g_fw_file;
    if (fw_len < APP_FILE_MIN_SIZE || fw_len > APP_FILE_MAX_SIZE)
    {
        FTS_ERROR("[UPGRADE]: FW length(%x) error", fw_len);
        return -EIO;
    }

    i_ret = fts_ft5x46_upgrade_use_buf(client, fw_buf, fw_len);
    if (i_ret != 0)
    {
        FTS_ERROR("[UPGRADE] upgrade app.i failed");
    }
    else
    {
        FTS_INFO("[UPGRADE]: upgrade app.i succeed");
    }

    return i_ret;
}

/************************************************************************
* Name: fts_ft5x46_upgrade_with_app_bin_file
* Brief: upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
static int fts_ft5x46_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name)
{
    u8 *pbt_buf = NULL;
    int i_ret=0;
    bool ecc_ok = false;
    int fwsize = 0;

    FTS_INFO("[UPGRADE]**********start upgrade with app.bin**********");

    fwsize = fts_GetFirmwareSize(firmware_name);
    if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE)
    {
        FTS_ERROR("[UPGRADE]: app.bin length(%x) error, upgrade fail", fwsize);
        return -EIO;
    }

    pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
    if (NULL == pbt_buf)
    {
        FTS_ERROR(" malloc pbt_buf failed ");
        goto ERROR_BIN;
    }

    if (fts_ReadFirmware(firmware_name, pbt_buf))
    {
        FTS_ERROR("[UPGRADE]: request_firmware failed!!");
        goto ERROR_BIN;
    }

    /*check the app.bin invalid or not*/
    ecc_ok = fts_check_app_bin_valid(pbt_buf, fwsize);

    if (ecc_ok)
    {
        FTS_INFO("[UPGRADE] app.bin ecc ok");
        i_ret = fts_ft5x46_upgrade_use_buf(client, pbt_buf, fwsize);
        if (i_ret != 0)
        {
            FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
            goto ERROR_BIN;
        }
        else
        {
            FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
        }
    }
    else
    {
        FTS_ERROR("[UPGRADE] app.bin ecc failed");
        goto ERROR_BIN;
    }

    kfree(pbt_buf);
    return i_ret;
ERROR_BIN:
    kfree(pbt_buf);
    return -EIO;
}
#endif /* FT5X46 */
