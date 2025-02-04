/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is vender functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_VENDOR_CONFIG_H
#define FIMC_IS_VENDOR_CONFIG_H

#if defined(CONFIG_CAMERA_A7Y18)
#include "fimc-is-vendor-config_a7y18.h"
#elif defined(CONFIG_CAMERA_WISDOM)
#include "fimc-is-vendor-config_wisdom.h"
#elif defined(CONFIG_CAMERA_GTA3XL)
#include "fimc-is-vendor-config_gta3xl.h"
#elif defined(CONFIG_CAMERA_A30)
#include "fimc-is-vendor-config_a30.h"
#elif defined(CONFIG_CAMERA_A40)
#include "fimc-is-vendor-config_a40.h"
#elif defined(CONFIG_CAMERA_A20)
#include "fimc-is-vendor-config_a20.h"
#else
#include "fimc-is-vendor-config_common.h"
#endif

#endif
