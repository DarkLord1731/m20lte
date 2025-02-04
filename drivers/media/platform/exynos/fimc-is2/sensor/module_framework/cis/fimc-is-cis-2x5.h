/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_2X5_H
#define FIMC_IS_CIS_2X5_H

#include "fimc-is-cis.h"

#define SUPPORT_2X5_SENSOR_VARIATION
/* #define SENSOR_2X5_CAL_UPLOAD */

#define SENSOR_2X5_CAL_XTALK_SIZE		(1040)
#define SENSOR_2X5_CAL_LSC_SIZE			(464)
#define SENSOR_2X5_XTALK_ADDR_PAGE		(0x2004)
#define SENSOR_2X5_XTALK_ADDR_OFFSET		(0x56E8)
#define SENSOR_2X5_LSC_ADDR_PAGE		(0x2004)
#define SENSOR_2X5_LSC_ADDR_OFFSET		(0x4A38)

#define EXT_CLK_Mhz (26)

#define SENSOR_2X5_MAX_WIDTH		(5760 + 0)
#define SENSOR_2X5_MAX_HEIGHT		(4312 + 0)

/* TODO: Check below values are valid */
#define SENSOR_2X5_FINE_INTEGRATION_TIME_MIN                0x0618
#define SENSOR_2X5_FINE_INTEGRATION_TIME_MAX                0x0618
#define SENSOR_2X5_COARSE_INTEGRATION_TIME_MIN              0x07
#define SENSOR_2X5_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x08

#define USE_GROUP_PARAM_HOLD	(0)

typedef enum
{
	SENSOR_2X5_MODE_2880_2156_30 = 0,	/* 0, 4:3 */
	SENSOR_2X5_MODE_2880_1620_30, 		/* 1 */
	SENSOR_2X5_MODE_2880_1332_30,		/* 2, 19.5 : 9 */
	SENSOR_2X5_MODE_2156_2156_30,		/* 3 */
	SENSOR_2X5_MODE_2880_2156_120,		/* 4 */
	SENSOR_2X5_MODE_REMOSAIC_START,		/* 5, Remosaic */
	SENSOR_2X5_MODE_5760_4312_30 = SENSOR_2X5_MODE_REMOSAIC_START,
	SENSOR_2X5_MODE_5760_3240_30,		/* 6 */
	SENSOR_2X5_MODE_5760_2664_30,		/* 7 */
	SENSOR_2X5_MODE_4312_4312_30,		/* 8 */
    SENSOR_2X5_MODE_END
} SENSOR_2X5_MODE_ENUM;

enum {
	TETRA_ISP_6MP = 0,
	TETRA_ISP_24MP,
};

enum {
	SENSOR_2X5_VER_SP03 = 0xA0, /* for test */
	SENSOR_2X5_VER_SP13 = 0xA1,
	SENSOR_2X5_VER_SP14 = 0xA1,
};

#endif

