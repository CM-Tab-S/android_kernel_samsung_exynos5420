/*
 * linux/drivers/media/video/exynos/mfc/s5p_mfc_dec.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_DEC_H_
#define __S5P_MFC_DEC_H_ __FILE__

#define MAX_FRAME_SIZE		(2*1024*1024)
#define DEFAULT_TAG		(0xE05)
#define DEC_MAX_FPS		(60000)

const struct v4l2_ioctl_ops *get_dec_v4l2_ioctl_ops(void);
int s5p_mfc_init_dec_ctx(struct s5p_mfc_ctx *ctx);
int s5p_mfc_dec_ctx_ready(struct s5p_mfc_ctx *ctx);
void s5p_mfc_dec_store_crop_info(struct s5p_mfc_ctx *ctx);
int dec_cleanup_user_shared_handle(struct s5p_mfc_ctx *ctx);

#endif /* __S5P_MFC_DEC_H_ */
