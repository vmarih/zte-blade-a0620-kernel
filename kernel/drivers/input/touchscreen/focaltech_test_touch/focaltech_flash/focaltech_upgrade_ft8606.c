/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2016, Focaltech Ltd. All rights reserved.
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
* File Name: focaltech_upgrade_ft8606.c
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

#if (FTS_CHIP_TYPE == _FT8606)
#include "../focaltech_flash.h"
#include "focaltech_upgrade_common.h"

/*****************************************************************************
* Static variables
*****************************************************************************/
#define APP_FILE_MAX_SIZE           (64 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define APP_FILE_VER_MAPPING        (0x10A)
#define APP_FILE_VENDORID_MAPPING   (0x108)
#define APP_FILE_CHIPID_MAPPING     (0x11E)
#define CONFIG_START_ADDR           (0x0780)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)
#define AL2_FCS_COEF    ((1 << 7) + (1 << 6) + (1 << 5))

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
static int fts_ctpm_get_app_i_file_ver(void);
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name);
static int fts_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client);
static int fts_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name);

struct fts_upgrade_fun fts_updatefun = {
	.get_app_bin_file_ver = fts_ctpm_get_app_bin_file_ver,
	.get_app_i_file_ver = fts_ctpm_get_app_i_file_ver,
	.upgrade_with_app_i_file = fts_ctpm_fw_upgrade_with_app_i_file,
	.upgrade_with_app_bin_file = fts_ctpm_fw_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_i_file = NULL,
	.upgrade_with_lcd_cfg_bin_file = NULL,
};

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/************************************************************************
* Name: fts_ctpm_get_app_bin_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ctpm_get_app_bin_file_ver(char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int fwsize = fts_GetFirmwareSize(firmware_name);
	int fw_ver = 0;

	FTS_FUNC_ENTER();

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		return -EIO;
	}

	pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		kfree(pbt_buf);
		return -EIO;
	}

	fw_ver = pbt_buf[APP_FILE_VER_MAPPING];

	kfree(pbt_buf);
	FTS_FUNC_EXIT();

	return fw_ver;
}

/************************************************************************
* Name: fts_ctpm_get_app_i_file_ver
* Brief:  get .i file version
* Input: no
* Output: no
* Return: fw version
***********************************************************************/
static int fts_ctpm_get_app_i_file_ver(void)
{
	int fwsize = fts_getsize(FW_SIZE);

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fwsize);
		return -EIO;
	}

	return CTPM_FW[APP_FILE_VER_MAPPING];
}

/************************************************************************
* Name: fts_ctpm_get_vendor_id_flash
* Brief:
* Input:
* Output:
* Return:
***********************************************************************/
static int fts_ctpm_get_vendor_id_flash(struct i2c_client *client)
{
#if FTS_GET_VENDOR_ID
	int i_ret = 0;
	u8 vendorid[4] = { 0 };
	u8 auc_i2c_write_buf[10];
	u8 i = 0;

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		auc_i2c_write_buf[0] = 0x03;
		auc_i2c_write_buf[1] = 0x00;

		auc_i2c_write_buf[2] = (u8) ((CONFIG_VENDOR_ID_ADDR - 1) >> 8);
		auc_i2c_write_buf[3] = (u8) (CONFIG_VENDOR_ID_ADDR - 1);
		i_ret = fts_i2c_read(client, auc_i2c_write_buf, 4, vendorid, 2);
		if (i_ret < 0) {
			FTS_DEBUG("[UPGRADE]: read flash : i_ret = %d!!", i_ret);
			continue;
		}
		else if ((vendorid[1] == FTS_VENDOR_1_ID) || (vendorid[1] == FTS_VENDOR_2_ID))
			break;
	}

	FTS_DEBUG("[UPGRADE]: vendor id from flash rom: 0x%x!!", vendorid[1]);
	if (i >= FTS_UPGRADE_LOOP) {
		FTS_ERROR("[UPGRADE]: read vendor id from flash more than 30 times!!");
		return -EIO;
	}

	return 0;
#else
	return 0;
#endif
}

/************************************************************************
* Name: fts_ctpm_write_pram
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ctpm_write_pram(struct i2c_client *client, u8 * pbt_buf, u32 dw_lenth)
{
	int i_ret;
	bool inrom = false;

	FTS_FUNC_ENTER();

	/*check the length of the pramboot */
	if (dw_lenth > APP_FILE_MAX_SIZE || dw_lenth < APP_FILE_MIN_SIZE) {
		FTS_ERROR("[UPGRADE] pramboot length(%d) fail", dw_lenth);
		return -EIO;
	}

	/*send comond to FW, reset and start write pramboot */
	i_ret = fts_ctpm_start_fw_upgrade(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: send upgrade cmd to FW error!!");
		return i_ret;
	}

	/*check run in rom or not! if run in rom, will write pramboot */
	inrom = fts_ctpm_check_run_state(client, FTS_RUN_IN_ROM);
	if (!inrom) {
		FTS_ERROR("[UPGRADE]: not run in rom, write pramboot fail!!");
		return -EIO;
	}

	/*write pramboot to pram */
	i_ret = fts_ctpm_write_pramboot_for_idc(client, dw_lenth, aucFW_PRAM_BOOT);
	if (i_ret < 0) {
		return i_ret;
		FTS_ERROR("[UPGRADE]: write pramboot fail!!");
	}

	/*read out checksum */
	i_ret = fts_ctpm_pramboot_ecc(client);
	if (i_ret < 0) {
		return i_ret;
		FTS_ERROR("[UPGRADE]: write pramboot ecc error!!");
	}

	/*start pram */
	fts_ctpm_start_pramboot(client);

	FTS_FUNC_EXIT();

	return 0;
}

/************************************************************************
* Name: fts_ctpm_write_app
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ctpm_write_app(struct i2c_client *client, u8 * pbt_buf, u32 dw_lenth)
{
	u32 temp;
	int i_ret;
	bool inpram = false;

	FTS_FUNC_ENTER();

	/*check run in pramboot or not! if not rum in pramboot, can not upgrade */
	inpram = fts_ctpm_check_run_state(client, FTS_RUN_IN_PRAM);
	if (!inpram) {
		FTS_ERROR("[UPGRADE]: not run in pram, upgrade fail!!");
		return -EIO;
	}

	/*upgrade init */
	i_ret = fts_ctpm_upgrade_idc_init(client, dw_lenth);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: upgrade init error, upgrade fail!!");
		return i_ret;
	}

	/*read vendor id from flash, if vendor id error, can not upgrade */
	i_ret = fts_ctpm_get_vendor_id_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: read vendor id in flash fail!!");
		return i_ret;
	}

	/*erase the app erea in flash */
	i_ret = fts_ctpm_erase_flash(client);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: erase flash error!!");
		return i_ret;
	}

	/*start to write app */
	i_ret = fts_ctpm_write_app_for_idc(client, dw_lenth, pbt_buf);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: write app error!!");
		return i_ret;
	}

	/*read check sum */
	temp = 0x1000;
	i_ret = fts_ctpm_upgrade_ecc(client, temp, dw_lenth);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE]: ecc error!!");
		return i_ret;
	}

	/*upgrade success, reset the FW */
	fts_ctpm_rom_or_pram_reset(client);

	FTS_FUNC_EXIT();

	return 0;
}

/*****************************************************************************
*   Name: ecc_calc
*  Brief:
*  Input:
* Output:
* Return:
*****************************************************************************/
static u8 ecc_calc(u8 * pbt_buf, u16 start, u16 length)
{
	u8 cFcs = 0;
	u16 i, j;

	for (i = 0; i < length; i++) {
		cFcs ^= pbt_buf[start++];
		for (j = 0; j < 8; j++) {
			if (cFcs & 1) {
				cFcs = (u8) ((cFcs >> 1) ^ AL2_FCS_COEF);
			}
			else {
				cFcs >>= 1;
			}
		}
	}
	return cFcs;
}

/*****************************************************************************
* Name: fts_8606_check_app_bin_valid
* Brief:
* Input:
* Output:
* Return:
*****************************************************************************/
static bool fts_8606_check_app_bin_valid(u8 * pbt_buf)
{
	u8 ecc1;
	u8 ecc2;
	u8 ecc3;
	u8 ecc4;
	u16 len1;
	u16 len2;
	u8 cal_ecc1;
	u8 cal_ecc2;
	u16 usAddrInfo;

	/* 1. First Byte */
	if (pbt_buf[0] != 0x02) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- the first byte(%x) error", pbt_buf[0]);
		return false;
	}

	usAddrInfo = 0x100;

	/* 2.len */
	len1 = pbt_buf[usAddrInfo++] << 8;
	len1 += pbt_buf[usAddrInfo++];

	len2 = pbt_buf[usAddrInfo++] << 8;
	len2 += pbt_buf[usAddrInfo++];

	if ((len1 + len2) != 0xFFFF) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- LENGTH(%04x) XOR error", len1);
		return false;
	}

	/* 3.ecc */
	ecc1 = pbt_buf[usAddrInfo++];
	ecc2 = pbt_buf[usAddrInfo++];
	ecc3 = pbt_buf[usAddrInfo++];
	ecc4 = pbt_buf[usAddrInfo++];

	if (((ecc1 + ecc2) != 0xFF) || ((ecc3 + ecc4) != 0xFF)) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC(%x %x) XOR error", ecc1, ecc2);
		return false;
	}

	cal_ecc1 = ecc_calc(pbt_buf, 0x0, 0x100);
	cal_ecc2 = ecc_calc(pbt_buf, 0x100 + 0x20, len1 - (0x100 + 0x20));
	if ((ecc1 != cal_ecc1) || (ecc3 != cal_ecc2)) {
		FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC calc error");
		return false;
	}
	return true;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
*         success =0
***********************************************************************/
static int fts_ctpm_fw_upgrade_use_buf(struct i2c_client *client, u8 * pbt_buf, u32 fwsize)
{
	int i_ret = 0;
	int fw_len;

	FTS_FUNC_ENTER();

	/*write pramboot */
	fw_len = fts_getsize(PRAMBOOT_SIZE);
	FTS_DEBUG("[UPGRADE]: pramboot size : %d!!", fw_len);
	i_ret = fts_ctpm_write_pram(client, aucFW_PRAM_BOOT, fw_len);
	if (i_ret != 0) {
		FTS_ERROR("[UPGRADE]: write pram failed!!");
		return -EIO;
	}

	/*write app */
	i_ret = fts_ctpm_write_app(client, pbt_buf, fwsize);

	FTS_FUNC_EXIT();

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output:
* Return: fail < 0
***********************************************************************/
static int fts_ctpm_fw_upgrade_with_app_i_file(struct i2c_client *client)
{
	int i_ret = 0;
	u32 fw_len;

	FTS_INFO("[UPGRADE]**********start upgrade with app.i**********");

	fw_len = fts_getsize(FW_SIZE);
	if (fw_len < APP_FILE_MIN_SIZE || fw_len > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: FW length(%x) error", fw_len);
		return -EIO;
	}

	i_ret = fts_ctpm_fw_upgrade_use_buf(client, CTPM_FW, fw_len);
	if (i_ret != 0) {
		FTS_ERROR("[UPGRADE] upgrade app.i failed");
	}
	else {
		FTS_INFO("[UPGRADE]: upgrade app.i succeed");
	}

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_app_bin_file
* Brief: upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
static int fts_ctpm_fw_upgrade_with_app_bin_file(struct i2c_client *client, char *firmware_name)
{
	u8 *pbt_buf = NULL;
	int i_ret = 0;
	bool ecc_ok = false;
	int fwsize = fts_GetFirmwareSize(firmware_name);

	FTS_INFO("[UPGRADE]**********start upgrade with app.bin**********");

	if (fwsize < APP_FILE_MIN_SIZE || fwsize > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE]: app.bin length(%x) error, upgrade fail", fwsize);
		return -EIO;
	}

	pbt_buf = (unsigned char *)kmalloc(fwsize + 1, GFP_KERNEL);
	if (NULL == pbt_buf) {
		FTS_ERROR(" malloc pbt_buf failed ");
		goto ERROR_BIN;
	}

	if (fts_ReadFirmware(firmware_name, pbt_buf)) {
		FTS_ERROR("[UPGRADE]: request_firmware failed!!");
		goto ERROR_BIN;
	}
#if FTS_GET_VENDOR_ID
	if ((pbt_buf[APP_FILE_VENDORID_MAPPING] != FTS_VENDOR_1_ID) && (pbt_buf[APP_FILE_VENDORID_MAPPING] != FTS_VENDOR_2_ID)) {
		FTS_ERROR("[UPGRADE]: vendor id is error, app.bin upgrade failed!!");
		goto ERROR_BIN;
	}
#endif
	if ((pbt_buf[APP_FILE_CHIPID_MAPPING] != chip_types.pramboot_idh)
		|| (pbt_buf[APP_FILE_CHIPID_MAPPING + 1] != chip_types.pramboot_idl)) {
		FTS_ERROR("[UPGRADE]: chip id is error, app.bin upgrade failed!!");
		goto ERROR_BIN;
	}

	/*check the app.bin invalid or not */
	ecc_ok = fts_8606_check_app_bin_valid(pbt_buf);

	if (ecc_ok) {
		FTS_INFO("[UPGRADE] app.bin ecc ok");
		i_ret = fts_ctpm_fw_upgrade_use_buf(client, pbt_buf, fwsize);
		if (i_ret != 0) {
			FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
			goto ERROR_BIN;
		}
		else {
			FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
		}
	}
	else {
		FTS_ERROR("[UPGRADE] app.bin ecc failed");
		goto ERROR_BIN;
	}

	kfree(pbt_buf);
	return i_ret;
ERROR_BIN:
	kfree(pbt_buf);
	return -EIO;
}
#endif /* #if (FTS_CHIP_TYPE == _FT8606) */
