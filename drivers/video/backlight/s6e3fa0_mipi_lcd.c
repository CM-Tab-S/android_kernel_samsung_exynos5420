/* linux/drivers/video/backlight/s6e3fa0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
<<<<<<< HEAD
=======
#include <linux/notifier.h>
#include <linux/fb.h>
>>>>>>> b682b99... importet sammy NJ2

#include <video/mipi_display.h>
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <plat/gpio-cfg.h>
#include <asm/system_info.h>

#include "s6e3fa0_param.h"

#include "dynamic_aid_s6e3fa0.h"
#include "dynamic_aid_s6e3fa0_old.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		162

#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)
#define LEVEL_IS_PSRE(level)		(level >= 6)

#define MAX_GAMMA			350
#define DEFAULT_GAMMA_LEVEL		GAMMA_143CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			33	/* MTP */
#define LDI_HBM_LEN			22	/* HBM V255 */
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			17
#define LDI_FPS_REG			0xD7
#define LDI_FPS_LEN			27
#define LDI_VDDM_REG			0xD7

#define LDI_COORDINATE_REG		0xA1
#define LDI_COORDINATE_LEN		4

#define LDI_TSET_REG			0xB8
#define LDI_TSET_LEN			8	/* REG address + 7 para */

#define LDI_DATE_REG			0xC8
#define LDI_DATE_LEN			42

#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

extern void print_reg_pm_disp1(void);

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			current_psre;
	unsigned int			current_tset;
<<<<<<< HEAD
 	unsigned int			ldi_enable;
=======
	unsigned int			ldi_enable;
>>>>>>> b682b99... importet sammy NJ2
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

<<<<<<< HEAD
=======
	struct notifier_block	fb_notif;
	unsigned int			fb_unblank;

>>>>>>> b682b99... importet sammy NJ2
	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table[ELVSS_TABLE_NUM];
	unsigned char			elvss_hbm[2];
	struct dynamic_aid_param_t daid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;

	unsigned char			**tset_table;
	int				temperature;

	unsigned int			coordinate[2];
	unsigned int			partial_range[2];
	unsigned char			ldi_fps;
	unsigned char			ldi_vddm;

	struct mipi_dsim_device		*dsim;
};

static const unsigned int candela_table[GAMMA_MAX] = {
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 19, 20, 21, 22, 24, 25, 27,
	29, 30, 32, 34, 37, 39, 41, 44, 47, 50,
	53, 56, 60, 64, 68, 72, 77, 82, 87, 93,
	98, 105, 111, 119, 126, 134, 143, 152, 162, 172,
	183, 195, 207, 220, 234, 249, 265, 282, 300, 316,
	333, MAX_GAMMA-1, 500,
};

static struct lcd_info *g_lcd;

void (*panel_touchkey_on)(void);
void (*panel_touchkey_off)(void);

#ifdef CONFIG_FB_I80IF
static int s6e3fa0_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret, len_wr = 0;
	int retry;
	u8 cmd;
	u8 seq_wr[30];

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	len_wr = (len - len_wr > 30) ? 30 : len;

	if (len_wr > 2)
		cmd = MIPI_DSI_DCS_LONG_WRITE;
	else if (len_wr == 2)
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	else if (len_wr == 1)
		cmd = MIPI_DSI_DCS_SHORT_WRITE;
	else {
		ret = -EINVAL;
		goto write_err;
	}

	retry = 2;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n",
				__func__);
		goto write_err;
	}
	ret = s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq, len_wr);
	if (ret != len_wr) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

	if (len != len_wr) {
		/* write GPARA */
		seq_wr[0] = 0xB0;
		seq_wr[1] = 29;
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq_wr, 2);
		/* write 2nd data */
		len_wr = len - len_wr + 1;
		seq_wr[0] = seq[0];
		memcpy(&seq_wr[1], &seq[30], len_wr);
		s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq_wr, len_wr);
		ret = len;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

#else
static int s6e3fa0_write(struct lcd_info *lcd, const u8 *seq, u32 len)
{
	int ret;
	int retry;
	u8 cmd;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);

	if (len > 2)
		cmd = MIPI_DSI_DCS_LONG_WRITE;
	else if (len == 2)
		cmd = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_DCS_SHORT_WRITE;
	else {
		ret = -EINVAL;
		goto write_err;
	}

	retry = 5;
write_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		print_reg_pm_disp1();
		goto write_err;
	}
	ret = s5p_mipi_dsi_wr_data(lcd->dsim, cmd, seq, len);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_write failed retry ..\n");
		retry--;
		goto write_data;
	}

write_err:
	mutex_unlock(&lcd->lock);
	return ret;
}
#endif

static int s6e3fa0_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd;
	int retry;

	if (!lcd->connected)
		return -EINVAL;

	mutex_lock(&lcd->lock);
	if (len > 2)
		cmd = MIPI_DSI_DCS_READ;
	else if (len == 2)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
	else if (len == 1)
		cmd = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
	else {
		ret = -EINVAL;
		goto read_err;
	}
	retry = 5;
read_data:
	if (!retry) {
		dev_err(&lcd->ld->dev, "%s failed: exceed retry count\n", __func__);
		goto read_err;
	}
	ret = s5p_mipi_dsi_rd_data(lcd->dsim, cmd, addr, len, buf, 1);
	if (ret != len) {
		dev_dbg(&lcd->ld->dev, "mipi_read failed retry ..\n");
		retry--;
		goto read_data;
	}
read_err:
	mutex_unlock(&lcd->lock);
	return ret;
}

static void s6e3fa0_read_coordinate(struct lcd_info *lcd)
{
	int ret;
	unsigned char buf[LDI_COORDINATE_LEN] = {0,};

	ret = s6e3fa0_read(lcd, LDI_COORDINATE_REG, buf, LDI_COORDINATE_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[0] << 8 | buf[1];	/* X */
	lcd->coordinate[1] = buf[2] << 8 | buf[3];	/* Y */
}

static void s6e3fa0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN);
	if (ret < 1) {
		lcd->connected = 0;
		dev_info(&lcd->ld->dev, "panel is not connected well\n");
	}
}

static int s6e3fa0_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e3fa0_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN);

	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	for (i = 0; i < LDI_MTP_LEN ; i++)
		smtd_dbg("%02dth mtp value is %02x\n", i+1, (int)buf[i]);

	return ret;
}

static int s6e3fa0_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN);

	return ret;
}

static int s6e3fa0_read_date(struct lcd_info *lcd, u8 *buf)
{
	int ret;

	ret = s6e3fa0_read(lcd, LDI_DATE_REG, buf, LDI_DATE_LEN);

	return ret;
}

static int s6e3fa0_read_hbm(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	unsigned char SEQ_HBM_POSITION[] = {0xB0, 33};

	SEQ_HBM_POSITION[1] = 33;
	ret = s6e3fa0_write(lcd, SEQ_HBM_POSITION, ARRAY_SIZE(SEQ_HBM_POSITION));
	ret += s6e3fa0_read(lcd, LDI_MTP_REG, &buf[0], 6);
	SEQ_HBM_POSITION[1] = 72;
	ret = s6e3fa0_write(lcd, SEQ_HBM_POSITION, ARRAY_SIZE(SEQ_HBM_POSITION));
	ret += s6e3fa0_read(lcd, LDI_MTP_REG, &buf[6], 15);
	SEQ_HBM_POSITION[1] = 39;
	ret = s6e3fa0_write(lcd, SEQ_HBM_POSITION, ARRAY_SIZE(SEQ_HBM_POSITION));
	ret += s6e3fa0_read(lcd, LDI_MTP_REG, &buf[21], 1);

	return ret;
}

static int s6e3fa0_read_fps(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	unsigned char SEQ_FPS_POSITION[] = {0xB0, 0x1A};

	ret = s6e3fa0_write(lcd, SEQ_FPS_POSITION, ARRAY_SIZE(SEQ_FPS_POSITION));
	ret += s6e3fa0_read(lcd, LDI_FPS_REG, buf, 1);

	return ret;
}

static int s6e3fa0_read_vddm(struct lcd_info *lcd, u8 *buf)
{
	int ret;
	unsigned char SEQ_VDDM_POSITION[] = {0xB0, 0x16};

	ret = s6e3fa0_write(lcd, SEQ_VDDM_POSITION, ARRAY_SIZE(SEQ_VDDM_POSITION));
	ret += s6e3fa0_read(lcd, LDI_VDDM_REG, buf, 1);

	return ret;
}

static int s6e3fa0_write_vddm(struct lcd_info *lcd, u8 value)
{
	unsigned char vddm;
	unsigned char SEQ_VDDM_POSITION[] = {0xB0, 0x16};
	unsigned char SEQ_LDI_VDDM[] = {0xD7, 0x00};

	if(value > 127) {
		dev_info(&lcd->ld->dev, "%s:Invalid input_vddm : %d\n", __func__, value);
		return -EINVAL;
	}

	vddm = ldi_vddm_lut[value][1];

	if (!vddm)
		return -EINVAL;

	SEQ_LDI_VDDM[1] = vddm;

	s6e3fa0_write(lcd, SEQ_VDDM_POSITION, ARRAY_SIZE(SEQ_VDDM_POSITION));
	s6e3fa0_write(lcd, SEQ_LDI_VDDM, ARRAY_SIZE(SEQ_LDI_VDDM));

	dev_info(&lcd->ld->dev, "%s:vddm %d, vddm lut %d\n", __func__, value, vddm);
	return vddm;
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0 ... 5:
		backlightlevel = GAMMA_5CD;
		break;
	case 6:
		backlightlevel = GAMMA_6CD;
		break;
	case 7:
		backlightlevel = GAMMA_7CD;
		break;
	case 8:
		backlightlevel = GAMMA_8CD;
		break;
	case 9:
		backlightlevel = GAMMA_9CD;
		break;
	case 10:
		backlightlevel = GAMMA_10CD;
		break;
	case 11:
		backlightlevel = GAMMA_11CD;
		break;
	case 12:
		backlightlevel = GAMMA_12CD;
		break;
	case 13:
		backlightlevel = GAMMA_13CD;
		break;
	case 14:
		backlightlevel = GAMMA_14CD;
		break;
	case 15:
		backlightlevel = GAMMA_15CD;
		break;
	case 16:
		backlightlevel = GAMMA_16CD;
		break;
	case 17 ... 18:
		backlightlevel = GAMMA_17CD;
		break;
	case 19:
		backlightlevel = GAMMA_19CD;
		break;
	case 20:
		backlightlevel = GAMMA_20CD;
		break;
	case 21:
		backlightlevel = GAMMA_21CD;
		break;
	case 22 ... 23:
		backlightlevel = GAMMA_22CD;
		break;
	case 24:
		backlightlevel = GAMMA_24CD;
		break;
	case 25 ... 26:
		backlightlevel = GAMMA_25CD;
		break;
	case 27 ... 28:
		backlightlevel = GAMMA_27CD;
		break;
	case 29:
		backlightlevel = GAMMA_29CD;
		break;
	case 30 ... 31:
		backlightlevel = GAMMA_30CD;
		break;
	case 32 ... 33:
		backlightlevel = GAMMA_32CD;
		break;
	case 34 ... 36:
		backlightlevel = GAMMA_34CD;
		break;
	case 37 ... 38:
		backlightlevel = GAMMA_37CD;
		break;
	case 39 ... 40:
		backlightlevel = GAMMA_39CD;
		break;
	case 41 ... 43:
		backlightlevel = GAMMA_41CD;
		break;
	case 44 ... 46:
		backlightlevel = GAMMA_44CD;
		break;
	case 47 ... 49:
		backlightlevel = GAMMA_47CD;
		break;
	case 50 ... 52:
		backlightlevel = GAMMA_50CD;
		break;
	case 53 ... 55:
		backlightlevel = GAMMA_53CD;
		break;
	case 56 ... 59:
		backlightlevel = GAMMA_56CD;
		break;
	case 60 ... 63:
		backlightlevel = GAMMA_60CD;
		break;
	case 64 ... 67:
		backlightlevel = GAMMA_64CD;
		break;
	case 68 ... 71:
		backlightlevel = GAMMA_68CD;
		break;
	case 72 ... 76:
		backlightlevel = GAMMA_72CD;
		break;
	case 77 ... 81:
		backlightlevel = GAMMA_77CD;
		break;
	case 82 ... 86:
		backlightlevel = GAMMA_82CD;
		break;
	case 87 ... 92:
		backlightlevel = GAMMA_87CD;
		break;
	case 93 ... 97:
		backlightlevel = GAMMA_93CD;
		break;
	case 98 ... 104:
		backlightlevel = GAMMA_98CD;
		break;
	case 105 ... 110:
		backlightlevel = GAMMA_105CD;
		break;
	case 111 ... 118:
		backlightlevel = GAMMA_111CD;
		break;
	case 119 ... 125:
		backlightlevel = GAMMA_119CD;
		break;
	case 126 ... 133:
		backlightlevel = GAMMA_126CD;
		break;
	case 134 ... 142:
		backlightlevel = GAMMA_134CD;
		break;
	case 143 ... 149:
		backlightlevel = GAMMA_143CD;
		break;
	case 150 ... 161:
		backlightlevel = GAMMA_152CD;
		break;
	case 162 ... 171:
		backlightlevel = GAMMA_162CD;
		break;
	case 172 ... 181:
		backlightlevel = GAMMA_172CD;
		break;
	case 182 ... 193:
		backlightlevel = GAMMA_183CD;
		break;
	case 194 ... 205:
		backlightlevel = GAMMA_195CD;
		break;
	case 206 ... 218:
		backlightlevel = GAMMA_207CD;
		break;
	case 219 ... 229:
		backlightlevel = GAMMA_220CD;
		break;
	case 230 ... 237:
		backlightlevel = GAMMA_234CD;
		break;
	case 238 ... 241:
		backlightlevel = GAMMA_249CD;
		break;
	case 242 ... 244:
		backlightlevel = GAMMA_265CD;
		break;
	case 245 ... 247:
		backlightlevel = GAMMA_282CD;
		break;
	case 248 ... 249:
		backlightlevel = GAMMA_300CD;
		break;
	case 250 ... 251:
		backlightlevel = GAMMA_316CD;
		break;
	case 252 ... 253:
		backlightlevel = GAMMA_333CD;
		break;
	case 254 ... 255:
		backlightlevel = GAMMA_350CD;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}

	return backlightlevel;
}

static int s6e3fa0_gamma_ctl(struct lcd_info *lcd)
{
	s6e3fa0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int s6e3fa0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][3] !=  lcd->aor[lcd->current_bl][3])
		goto aid_update;
	else if (lcd->aor[lcd->bl][4] !=  lcd->aor[lcd->current_bl][4])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e3fa0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}

static int s6e3fa0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level = 0;

	if (LEVEL_IS_PSRE(lcd->auto_brightness))
		level = ACL_STATUS_25P_RE_LOW;
	else
		level = ACL_STATUS_25P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e3fa0_write(lcd, ACL_CUTOFF_TABLE[level], ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e3fa0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0, elvss;
	u32 candela = candela_table[lcd->bl];
	u8 update_hbm = 0;
	unsigned char SEQ_ELVSS_HBM[] = {0xB6, 0x00};

	switch (candela) {
	case 0 ... 105:
		elvss_level = ELVSS_STATUS_105;
		break;
	case 106 ... 111:
		elvss_level = ELVSS_STATUS_111;
		break;
	case 112 ... 119:
		elvss_level = ELVSS_STATUS_119;
		break;
	case 120 ... 126:
		elvss_level = ELVSS_STATUS_126;
		break;
	case 127 ... 134:
		elvss_level = ELVSS_STATUS_134;
		break;
	case 135 ... 143:
		elvss_level = ELVSS_STATUS_143;
		break;
	case 144 ... 152:
		elvss_level = ELVSS_STATUS_152;
		break;
	case 153 ... 162:
		elvss_level = ELVSS_STATUS_162;
		break;
	case 163 ... 172:
		elvss_level = ELVSS_STATUS_172;
		break;
	case 173 ... 183:
		elvss_level = ELVSS_STATUS_183;
		break;
	case 184 ... 195:
		elvss_level = ELVSS_STATUS_195;
		break;
	case 196 ... 207:
		elvss_level = ELVSS_STATUS_207;
		break;
	case 208 ... 220:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 221 ... 234:
		elvss_level = ELVSS_STATUS_234;
		break;
	case 235 ... 249:
		elvss_level = ELVSS_STATUS_249;
		break;
	case 250 ... 265:
		elvss_level = ELVSS_STATUS_265;
		break;
	case 266 ... 282:
		elvss_level = ELVSS_STATUS_282;
		break;
	case 283 ... 299:
		elvss_level = ELVSS_STATUS_300;
		break;
	case 400:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_300;
		break;
	}

	if (lcd->current_elvss != elvss_level) {
		if (elvss_level == ELVSS_STATUS_HBM || lcd->current_elvss == ELVSS_STATUS_HBM)
			update_hbm = 1;
	}

	elvss = lcd->elvss_table[lcd->acl_enable][elvss_level][2];

	if (force || lcd->elvss_table[lcd->acl_enable][lcd->current_elvss][2] != elvss) {
		if (lcd->temperature == 1) /* TSET_25_DEGREES */
			lcd->elvss_table[lcd->acl_enable][elvss_level][1] = 0x88;
		else
			lcd->elvss_table[lcd->acl_enable][elvss_level][1] = 0x8C;

		ret = s6e3fa0_write(lcd, lcd->elvss_table[lcd->acl_enable][elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = elvss_level;

		dev_dbg(&lcd->ld->dev, "elvss: %d, %d, %x\n", lcd->acl_enable, lcd->current_elvss,
			lcd->elvss_table[lcd->acl_enable][lcd->current_elvss][2]);
	}

	if (force || update_hbm) {
		if (elvss_level == ELVSS_STATUS_HBM)
			SEQ_ELVSS_HBM[1] = lcd->elvss_hbm[1];
		else
			SEQ_ELVSS_HBM[1] = lcd->elvss_hbm[0];

		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_HBM, ARRAY_SIZE(SEQ_GLOBAL_PARAM_HBM));
		s6e3fa0_write(lcd, SEQ_ELVSS_HBM, ARRAY_SIZE(SEQ_ELVSS_HBM));
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static int s6e3fa0_set_tset(struct lcd_info *lcd, u8 force)
{
	int ret = 0, tset_level = 0;

	switch (lcd->temperature) {
	case 1:
		tset_level = TSET_25_DEGREES;
		break;
	case 0:
	case -19:
		tset_level = TSET_MINUS_0_DEGREES;
		break;
	case -20:
		tset_level = TSET_MINUS_20_DEGREES;
		break;
	}

	if (force || lcd->current_tset != tset_level) {
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_TSET, ARRAY_SIZE(SEQ_GLOBAL_PARAM_TSET));
		ret = s6e3fa0_write(lcd, lcd->tset_table[tset_level], 2);
		lcd->current_tset = tset_level;
		dev_info(&lcd->ld->dev, "tset: %d\n", lcd->current_tset);
	}

	if (!ret) {
		ret = -EPERM;
		goto err;
	}

err:
	return ret;
}

void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	if (lcd->id[2] <= 0x23) {
		lcd->daid.br_base = brightness_base_table_I;
		lcd->daid.offset_gra = offset_gradation_I;
		lcd->daid.offset_color = offset_color_I;
	}
	else {
		lcd->daid.br_base = brightness_base_table;
		lcd->daid.offset_gra = offset_gradation;
		lcd->daid.offset_color = offset_color;
	}
}

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;

	int *mtp;

	mtp = lcd->daid.mtp;

	for (c = 0, j = 0; c < CI_MAX ; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_MAX - 2; i >= 0; i--) {
		for (c=0; c<CI_MAX ; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n",j, mtp_data[j]);

	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c=0; c<CI_MAX ; c++, j++)
			smtd_dbg("mtp[%d] = %d\n",j, mtp[j]);
}

static int init_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma;

	/* allocate memory for local gamma table */
	gamma = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma);

	/* relocate gamma order */
	for (i = 0; i < GAMMA_MAX - 1; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX ; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma[i][v * CI_MAX];
			for (c = 0; c < CI_MAX ; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma[i]);
	kfree(gamma);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma[i-1]);
		i--;
	}
	kfree(gamma);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_info *lcd)
{
	int i, j;

	if (lcd->id[2] <= 0x21) {
		paor_cmd = aor_cmd_G;
		pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL_G;
	}
	else if (lcd->id[2] == 0x22) {
		paor_cmd = aor_cmd_H;
		pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL_H;
	}
	else if (lcd->id[2] == 0x23) {
		paor_cmd = aor_cmd_I;
		pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL_I;
	}
	else {
		paor_cmd = aor_cmd;
		pSEQ_AOR_CONTROL = SEQ_AOR_CONTROL;
	}

	for (i = 0; i < GAMMA_MAX; i++)
		memcpy(lcd->aor[i], pSEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));

	for (i = 0; i < GAMMA_MAX -1; i++) {
		lcd->aor[i][3] = paor_cmd[i][1];
		lcd->aor[i][4] = paor_cmd[i][2];
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, j, k, ret;

	if (lcd->id[2] <= 0x23)
		pELVSS_TABLE = ELVSS_TABLE_I;
	else
		pELVSS_TABLE = ELVSS_TABLE;

	for (k = 0; k < ELVSS_TABLE_NUM; k++) {
		lcd->elvss_table[k] = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

		if (!lcd->elvss_table[k]) {
			pr_err("failed to allocate elvss table\n");
			ret = -ENOMEM;
			goto err_alloc_elvss_table;
		}
	}

	for (k = 0; k < ELVSS_TABLE_NUM; k++) {
		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			lcd->elvss_table[k][i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
			if (!lcd->elvss_table[k][i]) {
				pr_err("failed to allocate elvss\n");
				ret = -ENOMEM;
				goto err_alloc_elvss;
			}

			lcd->elvss_table[k][i][0] = 0xB6;
			lcd->elvss_table[k][i][1] = 0x88;
			lcd->elvss_table[k][i][2] = 0x0A;  /* elvss_data[1]; */
			lcd->elvss_table[k][i][2] += pELVSS_TABLE[i][k];
		}

		for (i = 0; i < ELVSS_STATUS_MAX; i++) {
			for (j = 0; j < ELVSS_PARAM_SIZE; j++)
				smtd_dbg("0x%02x, ", lcd->elvss_table[k][i][j]);
			smtd_dbg("\n");
		}
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with k */
	while (k >= 0) {
		while (i > 0)
			kfree(lcd->elvss_table[k][--i]);

		i = ELVSS_STATUS_MAX;
		k--;
	}
	k = ELVSS_TABLE_NUM;
err_alloc_elvss_table:
	while (k > 0)
		kfree(lcd->elvss_table[--k]);

	return ret;
}

static int init_tset_table(struct lcd_info *lcd)
{
	int i, j, ret;

	lcd->tset_table = kzalloc(TSET_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->tset_table) {
		pr_err("failed to allocate tset table\n");
		ret = -ENOMEM;
		goto err_alloc_tset_table;
	}

	for (i = 0; i < TSET_STATUS_MAX; i++) {
		lcd->tset_table[i] = kzalloc(LDI_TSET_LEN * sizeof(u8), GFP_KERNEL);
		if (!lcd->tset_table[i]) {
			pr_err("failed to allocate tset\n");
			ret = -ENOMEM;
			goto err_alloc_tset;
		}

		lcd->tset_table[i][0] = SEQ_TSET[0];
		lcd->tset_table[i][1] = TSET_TABLE[i];
	}

	for (i = 0; i < TSET_STATUS_MAX; i++) {
		for (j = 0; j < 2; j++)
			smtd_dbg("0x%02x, ", lcd->tset_table[i][j]);
		smtd_dbg("\n");
	}

	return 0;

err_alloc_tset:
	while (i > 0)
		kfree(lcd->tset_table[--i]);

err_alloc_tset_table:
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd,
	const u8 *mtp_data, const u8 *hbm_data, const u8 *elvss_data)
{
	int i;

	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		lcd->gamma_table[GAMMA_HBM][i] = lcd->gamma_table[GAMMA_300CD][i];

	/* C8 34~39, 73~87 -> CA 1~21 */
	for (i = 0; i < LDI_HBM_LEN; i++)
		lcd->gamma_table[GAMMA_HBM][i + 1] = hbm_data[i];

	lcd->elvss_table[0][ELVSS_STATUS_HBM][2] = 0x0C;
	lcd->elvss_table[1][ELVSS_STATUS_HBM][2] = 0x0C;

	lcd->elvss_hbm[1] = hbm_data[LDI_HBM_LEN - 1];
	lcd->elvss_hbm[0] = elvss_data[LDI_ELVSS_LEN - 1];

	return 0;

}

static int update_brightness(struct lcd_info *lcd, u8 force, int use_ielcd)
{
	u32 brightness;

	mutex_lock(&lcd->bl_lock);

	brightness = lcd->bd->props.brightness;

	lcd->bl = get_backlight_level_from_brightness(brightness);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == lcd->bd->props.max_brightness))
		lcd->bl = GAMMA_HBM;

	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {

#ifdef CONFIG_FB_I80IF
		lcd->dsim->use_ielcd_command = use_ielcd;
#endif
		s6e3fa0_gamma_ctl(lcd);

		s6e3fa0_aid_parameter_ctl(lcd, force);

		s6e3fa0_set_acl(lcd, force);

		s6e3fa0_set_elvss(lcd, force);

		s6e3fa0_set_tset(lcd, force);

#ifdef CONFIG_FB_I80IF
		if (lcd->dsim->use_ielcd_command == true) {
			s5p_mipi_dsi_command_run(lcd->dsim);
			lcd->dsim->use_ielcd_command = false;
		}
#endif
		lcd->current_bl = lcd->bl;

		dev_info(&lcd->ld->dev, "brightness=%d, bl=%d, candela=%d\n", \
			brightness, lcd->bl, candela_table[lcd->bl]);
	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}

static int s6e3fa0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	lcd->connected = 1;

	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e3fa0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	msleep(20);

	s6e3fa0_read_id(lcd, lcd->id);
	s6e3fa0_read_fps(lcd, &lcd->ldi_fps);

	/* 1. Common Setting */
	if (lcd->id[2] < 0x22) {
		s6e3fa0_write(lcd, SEQ_ACL_CONDITION, ARRAY_SIZE(SEQ_ACL_CONDITION));
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_TEMP_OFFSET, ARRAY_SIZE(SEQ_GLOBAL_PARAM_TEMP_OFFSET));
		s6e3fa0_write(lcd, SEQ_TEMP_OFFSET_CONDITION, ARRAY_SIZE(SEQ_TEMP_OFFSET_CONDITION));
		s6e3fa0_write(lcd, SEQ_PENTILE_CONDITION, ARRAY_SIZE(SEQ_PENTILE_CONDITION));
	}
	s6e3fa0_write(lcd, SEQ_RE_SETTING_EVT1_1, ARRAY_SIZE(SEQ_RE_SETTING_EVT1_1));
	s6e3fa0_write(lcd, SEQ_RE_SETTING_EVT1_2, ARRAY_SIZE(SEQ_RE_SETTING_EVT1_2));
	s6e3fa0_write(lcd, SEQ_RE_SETTING_EVT1_3, ARRAY_SIZE(SEQ_RE_SETTING_EVT1_3));
	s6e3fa0_write(lcd, SEQ_RE_SETTING_EVT1_4, ARRAY_SIZE(SEQ_RE_SETTING_EVT1_4));
	s6e3fa0_write(lcd, SEQ_RE_SETTING_EVT1_5, ARRAY_SIZE(SEQ_RE_SETTING_EVT1_5));

#if 1
	update_brightness(lcd, 1, false);
#else
	/* 2. Brightness Control : 300nit, White Balance 7500K/10MPCD*/
	s6e3fa0_write(lcd, SEQ_GAMMA_CONTROL_SET_300CD, ARRAY_SIZE(SEQ_GAMMA_CONTROL_SET_300CD));
	s6e3fa0_write(lcd, SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
	s6e3fa0_write(lcd, SEQ_ELVSS_CONDITION_SET, ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET));
	s6e3fa0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	s6e3fa0_write(lcd, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));

	/* 3. ELVSS Control */
	s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_TSET, ARRAY_SIZE(SEQ_GLOBAL_PARAM_TSET));
	s6e3fa0_write(lcd, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	s6e3fa0_write(lcd, SEQ_ELVSS_CONDITION_SET, ARRAY_SIZE(SEQ_ELVSS_CONDITION_SET));
#endif

	/* 4. Etc Condition */
	s6e3fa0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (lcd->id[2] < 0x22) {
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_SOURCE_AMP, ARRAY_SIZE(SEQ_GLOBAL_PARAM_SOURCE_AMP));
		s6e3fa0_write(lcd, SEQ_SOURCE_AMP_A, ARRAY_SIZE(SEQ_SOURCE_AMP_A));
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_BIAS_CURRENT, ARRAY_SIZE(SEQ_GLOBAL_PARAM_BIAS_CURRENT));
		s6e3fa0_write(lcd, SEQ_BIAS_CURRENT, ARRAY_SIZE(SEQ_BIAS_CURRENT));
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_ILVL, ARRAY_SIZE(SEQ_GLOBAL_PARAM_ILVL));
		s6e3fa0_write(lcd, SEQ_ILVL, ARRAY_SIZE(SEQ_ILVL));
	}
	if (lcd->id[2] < 0x23) {
		s6e3fa0_write(lcd, SEQ_GLOBAL_PARAM_VLIN1, ARRAY_SIZE(SEQ_GLOBAL_PARAM_VLIN1));
		s6e3fa0_write(lcd, SEQ_VLIN1, ARRAY_SIZE(SEQ_VLIN1));
	}
	s6e3fa0_write(lcd, SEQ_ERR_FG, ARRAY_SIZE(SEQ_ERR_FG));
	s6e3fa0_write(lcd, SEQ_TOUCHKEY_ON, ARRAY_SIZE(SEQ_TOUCHKEY_ON));
	s6e3fa0_write_vddm(lcd, lcd->ldi_vddm);
	s6e3fa0_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

#if defined(GPIO_PCD_INT)
	s6e3fa0_write(lcd, SEQ_PCD_SET_DET_LOW, ARRAY_SIZE(SEQ_PCD_SET_DET_LOW));
#endif

#if defined(CONFIG_FB_I80IF)
	s6e3fa0_write(lcd, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
#else
	s6e3fa0_write(lcd, SEQ_DISPCTL, ARRAY_SIZE(SEQ_DISPCTL));
#endif

	msleep(120);

	return ret;
}

static int s6e3fa0_ldi_enable(struct lcd_info *lcd)
{
	int ret = 0;

	s6e3fa0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}

static int s6e3fa0_ldi_disable(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

#ifdef CONFIG_FB_I80IF
	mutex_lock(&lcd->bl_lock);
	lcd->dsim->use_ielcd_command = true;
	s6e3fa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	lcd->dsim->use_ielcd_command = false;
	s5p_mipi_dsi_command_run(lcd->dsim);
	mutex_unlock(&lcd->bl_lock);

<<<<<<< HEAD
	msleep(35);

	/* after display off there is okay to send the commands via MIPI DSI Command
	because we don't need to worry about screen blinking. */
	mutex_lock(&lcd->bl_lock);
	lcd->dsim->use_ielcd_command = true;
	s6e3fa0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	lcd->dsim->use_ielcd_command = false;
	s5p_mipi_dsi_command_run(lcd->dsim);
	mutex_unlock(&lcd->bl_lock);
#else
	s6e3fa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));

=======
#else
	s6e3fa0_write(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
#endif
>>>>>>> b682b99... importet sammy NJ2
	msleep(35);

	/* after display off there is okay to send the commands via MIPI DSI Command
	because we don't need to worry about screen blinking. */
	s6e3fa0_write(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
<<<<<<< HEAD
#endif

=======

	
>>>>>>> b682b99... importet sammy NJ2
	msleep(125);
	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3fa0_power_on(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	ret = s6e3fa0_ldi_init(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to initialize ldi.\n");
		goto err;
	}

	ret = s6e3fa0_ldi_enable(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "failed to enable ldi.\n");
		goto err;
	}

	lcd->ldi_enable = 1;

	/* update_brightness(lcd, 1); */

	dev_info(&lcd->ld->dev, "- %s\n", __func__);
err:
	return ret;
}

static int s6e3fa0_power_off(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->ldi_enable = 0;

	ret = s6e3fa0_ldi_disable(lcd);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return ret;
}

static int s6e3fa0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = s6e3fa0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = s6e3fa0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int s6e3fa0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return s6e3fa0_power(lcd, power);
}

static int s6e3fa0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

<<<<<<< HEAD

=======
>>>>>>> b682b99... importet sammy NJ2
static int s6e3fa0_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, brightness); */

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(&bd->dev, "lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd->bd->props.max_brightness, brightness);
		return -EINVAL;
	}

<<<<<<< HEAD
	if (lcd->ldi_enable) {
=======
	if (lcd->ldi_enable && lcd->fb_unblank) {
>>>>>>> b682b99... importet sammy NJ2
		ret = update_brightness(lcd, 0, true);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int s6e3fa0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return candela_table[lcd->bl];
}

static int s6e3fa0_check_fb(struct lcd_device *ld, struct fb_info *fb)
{
	return 0;
}

static struct lcd_ops s6e3fa0_lcd_ops = {
	.set_power = s6e3fa0_set_power,
	.get_power = s6e3fa0_get_power,
	.check_fb  = s6e3fa0_check_fb,
};

static const struct backlight_ops s6e3fa0_backlight_ops  = {
	.get_brightness = s6e3fa0_get_brightness,
	.update_status = s6e3fa0_set_brightness,
};

<<<<<<< HEAD
=======
static int s6e3fa0_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct lcd_info *lcd;
	struct fb_event *blank = (struct fb_event*) data;
	unsigned int *value = (unsigned int*)blank->data;

	lcd = container_of(self, struct lcd_info, fb_notif);

	if (event == FB_EVENT_BLANK) {
		switch (*value) {
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_NORMAL:
			lcd->fb_unblank = 0;
			break;
		case FB_BLANK_UNBLANK:
			update_brightness(lcd, 0, true);
			lcd->fb_unblank = 1;
			break;
		default:
			break;
		}
	}

	return 0;
}

static int s6e3fa0_register_fb(struct lcd_info *lcd)
{
	memset(&lcd->fb_notif, 0, sizeof(lcd->fb_notif));
	lcd->fb_notif.notifier_call = s6e3fa0_fb_notifier_callback;

	return fb_register_client(&lcd->fb_notif);
}

>>>>>>> b682b99... importet sammy NJ2
static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1, true);
		}
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "SMD_AMS568AT01-0\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];

#if 0
	if (lcd->ldi_enable)
		s6e3fa0_read_id(lcd, lcd->id);
#endif

	sprintf(temp, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t gamma_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int i, j;

	for (i = 0; i < GAMMA_MAX; i++) {
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->gamma_table[i][j]);
		printk("\n");
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			printk("0x%02x, ", lcd->elvss_table[0][i][j]);
		printk("\n");
	}

	return strlen(buf);
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 0, true);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable)
				update_brightness(lcd, 1, true);
		}
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, rc, temperature = 0 ;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		switch (value) {
		case 1:
		case 0:
		case -19:
			temperature = value;
 			break;
		case -20:
			temperature = value;
 			break;
		}

		mutex_lock(&lcd->bl_lock);
		lcd->temperature = temperature;
		mutex_unlock(&lcd->bl_lock);

		if (lcd->ldi_enable)
			update_brightness(lcd, 0, true);

		dev_info(dev, "%s: %d, %d\n", __func__, value, lcd->temperature );
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d, %d\n", lcd->coordinate[0], lcd->coordinate[1]);

	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, manufacture_data[LDI_DATE_LEN] = {0,};

#if 0
	if (lcd->ldi_enable)
		s6e3fa0_read_date(lcd, manufacture_data);
#endif

	year = ((manufacture_data[40] & 0xF0) >> 4) + 2011;
	month = manufacture_data[40] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, manufacture_data[41]);

	return strlen(buf);
}

static ssize_t parameter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	unsigned char temp[50] = {0,};
	int i;

	if (!lcd->ldi_enable)
		return -EINVAL;

#if 0
	/* ID */
	s6e3fa0_read(lcd, LDI_ID_REG, temp, LDI_ID_LEN);
	pos += sprintf(pos, "ID    [04]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);

#if 0
	/* PSRE */
	s6e3fa0_read(lcd, 0xB5, temp, 3);
	pos += sprintf(pos, "PSRE  [B5]: %02x, %02x, %02x\n", temp[0], temp[1], temp[2]);
#endif

	/* ACL */
	s6e3fa0_read(lcd, 0x56, temp, 1);
	pos += sprintf(pos, "ACL   [56]: %02x\n", temp[0]);

	/* ELVSS */
	s6e3fa0_read(lcd, LDI_ELVSS_REG, temp, ELVSS_PARAM_SIZE - 1);
	pos += sprintf(pos, "ELVSS [B6]: %02x, %02x\n", temp[0], temp[1]);

	/* TSET */
	s6e3fa0_read(lcd, LDI_TSET_REG, temp, LDI_TSET_LEN - 1);
	pos += sprintf(pos, "TSET  [B8]: ");
	for (i = 0; i < LDI_TSET_LEN - 1; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "/ %d, %d\n", lcd->temperature, lcd->current_tset);

	/* GAMMA */
	s6e3fa0_read(lcd, 0xCA, temp, GAMMA_PARAM_SIZE - 1);
	pos += sprintf(pos, "GAMMA [CA]: ");
	for (i = 0; i < GAMMA_PARAM_SIZE - 1; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");
#endif

	return pos - buf;
}

static ssize_t partial_disp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf((char *)buf, "%d, %d\n", lcd->partial_range[0], lcd->partial_range[1]);

	dev_info(dev, "%s: %d, %d\n", __func__, lcd->partial_range[0], lcd->partial_range[1]);

	return strlen(buf);
}

static ssize_t partial_disp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned char SEQ_PARTIAL_DISP[] = {0x30, 0x00, 0x00, 0x00, 0x00};

	sscanf(buf, "%9d %9d" , &lcd->partial_range[0], &lcd->partial_range[1]);

	dev_info(dev, "%s: %d, %d\n", __func__, lcd->partial_range[0], lcd->partial_range[1]);

	if((lcd->partial_range[0] > 1919 || lcd->partial_range[1] > 1919)
		||(lcd->partial_range[0] > lcd->partial_range[1]))
	{
		pr_err("%s:Invalid Input\n",__func__);
		return size;
	}

	SEQ_PARTIAL_DISP[1] = (lcd->partial_range[0] >> 8) & 0xFF;/*select msb 1byte*/
	SEQ_PARTIAL_DISP[2] = lcd->partial_range[0] & 0xFF;
	SEQ_PARTIAL_DISP[3] = (lcd->partial_range[1] >> 8) & 0xFF;/*select msb 1byte*/
	SEQ_PARTIAL_DISP[4] = lcd->partial_range[1] & 0xFF;

	if (lcd->ldi_enable) {
#ifdef CONFIG_FB_I80IF
		mutex_lock(&lcd->bl_lock);
		lcd->dsim->use_ielcd_command = true;

		if (lcd->partial_range[0] || lcd->partial_range[1]) {
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP, ARRAY_SIZE(SEQ_PARTIAL_DISP));
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP_ON, ARRAY_SIZE(SEQ_PARTIAL_DISP_ON));
		}
		else
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP_OFF, ARRAY_SIZE(SEQ_PARTIAL_DISP_OFF));

		lcd->dsim->use_ielcd_command = false;
		s5p_mipi_dsi_command_run(lcd->dsim);
		mutex_unlock(&lcd->bl_lock);
#else
		if (lcd->partial_range[0] || lcd->partial_range[1]) {
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP, ARRAY_SIZE(SEQ_PARTIAL_DISP));
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP_ON, ARRAY_SIZE(SEQ_PARTIAL_DISP_ON));
		}
		else
			s6e3fa0_write(lcd, SEQ_PARTIAL_DISP_OFF, ARRAY_SIZE(SEQ_PARTIAL_DISP_OFF));
#endif
	}

	return size;
}

static ssize_t ldi_fps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf((char *)buf, "%d\n", lcd->ldi_fps);

	dev_info(dev, "%s: %d\n", __func__, lcd->ldi_fps);

	return strlen(buf);
}

static ssize_t ldi_fps_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value, offset, rc, i;
	unsigned char SEQ_LDI_FPS[] = {0xD7, 0x00};

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;

	if (!lcd->ldi_enable)
		return -EINVAL;

	if(value < 57000 || value > 63000) {
		dev_info(dev, "%s:Invalid input_fps : %d\n", __func__, value);
		return -EINVAL;
	}

	/* find fps offset */
	for (i = 0; i < LDI_FPS_OFFSET_MAX - 1; i++)
		if (value < ldi_fps_offset[i][0])
			break;

	offset = ldi_fps_offset[i][1];

	if (!offset)
		return -EINVAL;

	lcd->ldi_fps += offset;
	SEQ_LDI_FPS[1] = lcd->ldi_fps;

#ifdef CONFIG_FB_I80IF
	mutex_lock(&lcd->bl_lock);
	lcd->dsim->use_ielcd_command = true;

	s6e3fa0_write(lcd, SEQ_LDI_FPS_POS, ARRAY_SIZE(SEQ_LDI_FPS_POS));
	s6e3fa0_write(lcd, SEQ_LDI_FPS, ARRAY_SIZE(SEQ_LDI_FPS));

	lcd->dsim->use_ielcd_command = false;
	s5p_mipi_dsi_command_run(lcd->dsim);
	mutex_unlock(&lcd->bl_lock);
#else
	s6e3fa0_write(lcd, SEQ_LDI_FPS_POS, ARRAY_SIZE(SEQ_LDI_FPS_POS));
	s6e3fa0_write(lcd, SEQ_LDI_FPS, ARRAY_SIZE(SEQ_LDI_FPS));
#endif

	dev_info(dev, "%s:value %d, offset %d, curr_reg 0x%x, dest_reg 0x%x\n",
		__func__, value, offset, lcd->ldi_fps - offset, lcd->ldi_fps);

	return size;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(gamma_table, 0444, gamma_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(parameter, 0444, parameter_show, NULL);
static DEVICE_ATTR(partial_disp, 0664, partial_disp_show, partial_disp_store);
static DEVICE_ATTR(ldi_fps, 0664, ldi_fps_show, ldi_fps_store);

static int s6e3fa0_probe(struct mipi_dsim_device *dsim)
{
	int ret;
	struct lcd_info *lcd;

	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};
 	u8 hbm_data[LDI_HBM_LEN] = {0,};

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	g_lcd = lcd;

	lcd->ld = lcd_device_register("panel", dsim->dev, lcd, &s6e3fa0_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		pr_err("failed to register lcd device\n");
		ret = PTR_ERR(lcd->ld);
		goto out_free_lcd;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &s6e3fa0_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd->bd);
		goto out_free_backlight;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;
	lcd->temperature = 1;
	lcd->current_tset = TSET_25_DEGREES;
<<<<<<< HEAD
=======
	lcd->fb_unblank = 1;
>>>>>>> b682b99... importet sammy NJ2

	ret = device_create_file(&lcd->ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_gamma_table);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_parameter);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_ldi_fps);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* dev_set_drvdata(dsim->dev, lcd); */

<<<<<<< HEAD
=======
	ret = s6e3fa0_register_fb(lcd);
	if (ret)
		dev_err(&lcd->ld->dev, "failed to register fb notifier chain\n");

>>>>>>> b682b99... importet sammy NJ2
	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e3fa0_read_id(lcd, lcd->id);
	s6e3fa0_read_fps(lcd, &lcd->ldi_fps);
	s6e3fa0_read_vddm(lcd, &lcd->ldi_vddm);
	s6e3fa0_write_vddm(lcd, lcd->ldi_vddm);
	s6e3fa0_read_coordinate(lcd);
	s6e3fa0_read_mtp(lcd, mtp_data);
	s6e3fa0_read_elvss(lcd, elvss_data);
	s6e3fa0_read_hbm(lcd, hbm_data);

	dev_info(&lcd->ld->dev, "ID: %x, %x, %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	init_dynamic_aid(lcd);

	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_tset_table(lcd);
	ret += init_hbm_parameter(lcd, mtp_data, hbm_data, elvss_data);

	if (ret)
		dev_info(&lcd->ld->dev, "gamma table generation is failed\n");

	if (lcd->power == FB_BLANK_POWERDOWN)
		s6e3fa0_power(lcd, FB_BLANK_UNBLANK);
	else
		update_brightness(lcd, 1, false);

	lcd->ldi_enable = 1;
	dev_info(&lcd->ld->dev, "%s lcd panel driver has been probed.\n", __FILE__);

	return 0;

out_free_backlight:
	lcd_device_unregister(lcd->ld);
	kfree(lcd);
	return ret;

out_free_lcd:
	kfree(lcd);
	return ret;

err_alloc:
	return ret;
}


static int s6e3fa0_displayon(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e3fa0_power(lcd, FB_BLANK_UNBLANK);

	return 0;
}

static int s6e3fa0_suspend(struct mipi_dsim_device *dsim)
{
	struct lcd_info *lcd = g_lcd;

	s6e3fa0_power(lcd, FB_BLANK_POWERDOWN);

	return 0;
}

static int s6e3fa0_resume(struct mipi_dsim_device *dsim)
{
	return 0;
}

struct mipi_dsim_lcd_driver s6e3fa0_mipi_lcd_driver = {
	.probe		= s6e3fa0_probe,
	.displayon	= s6e3fa0_displayon,
	.suspend	= s6e3fa0_suspend,
	.resume		= s6e3fa0_resume,
};

static int s6e3fa0_init(void)
{
	return 0 ;
#if 0
	s5p_mipi_dsi_register_lcd_driver(&s6e3fa0_mipi_lcd_driver);
	exynos_mipi_dsi_register_lcd_driver
#endif
}

static void s6e3fa0_exit(void)
{
	return;
}

module_init(s6e3fa0_init);
module_exit(s6e3fa0_exit);

MODULE_DESCRIPTION("MIPI-DSI S6E3FA0 (1080*1920) Panel Driver");
MODULE_LICENSE("GPL");
