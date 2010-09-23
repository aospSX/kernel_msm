/*
 * Copyright (C) 2009 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 *
 * Modified from mach-omap2/board-ldp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <plat/common.h>
#include <plat/board.h>

#include <mach/board-zoom.h>

#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"

static void __init omap_zoom2_init_irq(void)
{
	omap2_init_common_hw(mt46h32m32lf6_sdrc_params,
				 mt46h32m32lf6_sdrc_params);
	omap_init_irq();
	omap_gpio_init();
}

/* EXTMUTE callback function */
void zoom2_set_hs_extmute(int mute)
{
	gpio_set_value(ZOOM2_HEADSET_EXTMUTE_GPIO, mute);
}

static struct twl4030_codec_audio_data zoom2_audio_data = {
	.audio_mclk = 26000000,
	.ramp_delay_value = 3,	/* 161 ms */
	.hs_extmute = 1,
	.set_hs_extmute = zoom2_set_hs_extmute,
};

static struct twl4030_codec_data zoom2_codec_data = {
	.audio_mclk = 26000000,
	.audio = &zoom2_audio_data,
};

static struct twl4030_platform_data zoom2_twldata = {
	.irq_base	= TWL4030_IRQ_BASE,
	.irq_end	= TWL4030_IRQ_END,

	/* platform_data for children goes here */
	.codec		= &zoom2_codec_data,
};

static struct i2c_board_info __initdata zoom2_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("twl4030", 0x48),
		.flags = I2C_CLIENT_WAKE,
		.irq = INT_34XX_SYS_NIRQ,
		.platform_data = &zoom2_twldata,
	},
};

static int __init omap3_zoom2_i2c_init(void)
{
	omap_register_i2c_bus(1, 2600, zoom2_i2c_boardinfo,
			ARRAY_SIZE(zoom2_i2c_boardinfo));
	return 0;
}


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/* WLAN IRQ - GPIO 162 */
	OMAP3_MUX(MCBSP1_CLKX, OMAP_MUX_MODE4 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN POWER ENABLE - GPIO 101 */
	OMAP3_MUX(CAM_D2, OMAP_MUX_MODE4 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC3 CMD */
	OMAP3_MUX(MCSPI1_CS1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 CLK */
	OMAP3_MUX(ETK_CLK, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 DAT[0-3] */
	OMAP3_MUX(ETK_D3, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D4, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D5, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D6, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

static struct mtd_partition zoom_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader-NAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),	/* 512KB, 0x80000 */
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x80000 */
		.size		= 10 * (64 * 2048),	/* 1.25MB, 0x140000 */
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "Boot Env-NAND",
		.offset		= MTDPART_OFS_APPEND,   /* Offset = 0x1c0000 */
		.size		= 2 * (64 * 2048),	/* 256KB, 0x40000 */
	},
	{
		.name		= "Kernel-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x0200000*/
		.size		= 240 * (64 * 2048),	/* 30M, 0x1E00000 */
	},
	{
		.name		= "system",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x2000000 */
		.size		= 3328 * (64 * 2048),	/* 416M, 0x1A000000 */
	},
	{
		.name		= "userdata",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x1C000000*/
		.size		= 256 * (64 * 2048),	/* 32M, 0x2000000 */
	},
	{
		.name		= "cache",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x1E000000*/
		.size		= 256 * (64 * 2048),	/* 32M, 0x2000000 */
	},
};

static void __init omap_zoom2_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	zoom_peripherals_init();
	omap3_zoom2_i2c_init();
	board_nand_init(zoom_nand_partitions,
			ARRAY_SIZE(zoom_nand_partitions), ZOOM_NAND_CS);
	zoom_debugboard_init();
}

MACHINE_START(OMAP_ZOOM2, "OMAP Zoom2 board")
	.phys_io	= ZOOM_UART_BASE,
	.io_pg_offst	= (ZOOM_UART_VIRT >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap3_map_io,
	.reserve	= omap_reserve,
	.init_irq	= omap_zoom2_init_irq,
	.init_machine	= omap_zoom2_init,
	.timer		= &omap_timer,
MACHINE_END
