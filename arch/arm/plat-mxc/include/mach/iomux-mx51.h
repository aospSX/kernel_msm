/*
 * Copyright (C) 2009-2010 Amit Kucheria <amit.kucheria@canonical.com>
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __MACH_IOMUX_MX51_H__
#define __MACH_IOMUX_MX51_H__

#include <mach/iomux-v3.h>

/*
 * various IOMUX alternate output functions (1-7)
 */
typedef enum iomux_config {
	IOMUX_CONFIG_ALT0,
	IOMUX_CONFIG_ALT1,
	IOMUX_CONFIG_ALT2,
	IOMUX_CONFIG_ALT3,
	IOMUX_CONFIG_ALT4,
	IOMUX_CONFIG_ALT5,
	IOMUX_CONFIG_ALT6,
	IOMUX_CONFIG_ALT7,
	IOMUX_CONFIG_GPIO,      /* added to help user use GPIO mode */
	IOMUX_CONFIG_SION = 0x1 << 4,   /* LOOPBACK:MUX SION bit */
} iomux_pin_cfg_t;

/* Pad control groupings */
#define MX51_UART1_PAD_CTRL	(PAD_CTL_HYS | PAD_CTL_PKE | PAD_CTL_PUE | \
				PAD_CTL_DSE_HIGH)
#define MX51_UART2_PAD_CTRL	(PAD_CTL_PKE | PAD_CTL_PUE | PAD_CTL_DSE_HIGH | \
				PAD_CTL_SRE_FAST)
#define MX51_UART3_PAD_CTRL	(PAD_CTL_PKE | PAD_CTL_DSE_HIGH | \
				PAD_CTL_SRE_FAST)
#define MX51_I2C_PAD_CTRL	(PAD_CTL_SRE_FAST | PAD_CTL_ODE | \
				PAD_CTL_DSE_HIGH | PAD_CTL_PUS_100K_UP | PAD_CTL_HYS)
#define MX51_USBH1_PAD_CTRL	(PAD_CTL_SRE_FAST | PAD_CTL_DSE_HIGH | \
				PAD_CTL_PUS_100K_UP | PAD_CTL_PUE | \
				PAD_CTL_PKE | PAD_CTL_HYS)
#define MX51_GPIO_PAD_CTRL		(PAD_CTL_DSE_HIGH | PAD_CTL_PKE | \
				PAD_CTL_SRE_FAST)

/*
 * The naming convention for the pad modes is MX51_PAD_<padname>__<padmode>
 * If <padname> or <padmode> refers to a GPIO, it is named GPIO_<unit>_<num>
 * See also iomux-v3.h
 */

/*							  PAD    MUX   ALT INPSE PATH PADCTRL */
#define MX51_PAD_EIM_DA0__EIM_DA0		IOMUX_PAD(0x7a8, 0x01c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA1__EIM_DA1		IOMUX_PAD(0x7a8, 0x020, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA2__EIM_DA2		IOMUX_PAD(0x7a8, 0x024, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA3__EIM_DA3		IOMUX_PAD(0x7a8, 0x028, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA4__EIM_DA4		IOMUX_PAD(0x7ac, 0x02c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA5__EIM_DA5		IOMUX_PAD(0x7ac, 0x030, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA6__EIM_DA6		IOMUX_PAD(0x7ac, 0x034, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA7__EIM_DA7		IOMUX_PAD(0x7ac, 0x038, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA8__EIM_DA8		IOMUX_PAD(0x7b0, 0x03c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA9__EIM_DA9		IOMUX_PAD(0x7b0, 0x040, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA10__EIM_DA10		IOMUX_PAD(0x7b0, 0x044, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA11__EIM_DA11		IOMUX_PAD(0x7b0, 0x048, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA12__EIM_DA12		IOMUX_PAD(0x7bc, 0x04c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA13__EIM_DA13		IOMUX_PAD(0x7bc, 0x050, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA14__EIM_DA14		IOMUX_PAD(0x7bc, 0x054, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DA15__EIM_DA15		IOMUX_PAD(0x7bc, 0x058, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D16__GPIO_2_0              IOMUX_PAD(0x3f0, 0x05c, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D16__I2C1_SDA		IOMUX_PAD(0x3f0, 0x05c, (4 | IOMUX_CONFIG_SION), \
							0x09b4, 0, MX51_I2C_PAD_CTRL)
#define MX51_PAD_EIM_D17__GPIO_2_1              IOMUX_PAD(0x3f4, 0x060, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D18__GPIO_2_2              IOMUX_PAD(0x3f8, 0x064, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D19__GPIO_2_3              IOMUX_PAD(0x3fc, 0x068, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D19__I2C1_SCL		IOMUX_PAD(0x3fc, 0x068, (4 | IOMUX_CONFIG_SION), \
							0x09b0, 0, MX51_I2C_PAD_CTRL)
#define MX51_PAD_EIM_D20__GPIO_2_4              IOMUX_PAD(0x400, 0x06c, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D21__GPIO_2_5		IOMUX_PAD(0x404, 0x070, 1, 0x0,   0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_EIM_D22__GPIO_2_6              IOMUX_PAD(0x408, 0x074, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D23__GPIO_2_7              IOMUX_PAD(0x40c, 0x078, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D24__UART3_CTS             IOMUX_PAD(0x410, 0x07c, 3, 0x0,   0, MX51_UART3_PAD_CTRL)
#define MX51_PAD_EIM_D25__UART3_RXD             IOMUX_PAD(0x414, 0x080, 3, 0x9f4, 0, MX51_UART3_PAD_CTRL)
#define MX51_PAD_EIM_D26__UART3_TXD             IOMUX_PAD(0x418, 0x084, 3, 0x0,   0, MX51_UART3_PAD_CTRL)
#define MX51_PAD_EIM_D27__UART3_RTS             IOMUX_PAD(0x41c, 0x088, 3, 0x9f0, 0, MX51_UART3_PAD_CTRL)
#define MX51_PAD_EIM_D28__EIM_D28               IOMUX_PAD(0x420, 0x08c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D29__EIM_D29               IOMUX_PAD(0x424, 0x090, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D30__EIM_D30               IOMUX_PAD(0x428, 0x094, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_D31__EIM_D31               IOMUX_PAD(0x42c, 0x09c, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A16__GPIO_2_10             IOMUX_PAD(0x430, 0x09c, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A17__GPIO_2_11             IOMUX_PAD(0x434, 0x0a0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A18__GPIO_2_12             IOMUX_PAD(0x438, 0x0a4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A19__GPIO_2_13             IOMUX_PAD(0x43c, 0x0a8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A20__GPIO_2_14             IOMUX_PAD(0x440, 0x0ac, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A21__GPIO_2_15             IOMUX_PAD(0x444, 0x0b0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A22__GPIO_2_16             IOMUX_PAD(0x448, 0x0b4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A23__GPIO_2_17             IOMUX_PAD(0x44c, 0x0b8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A24__GPIO_2_18             IOMUX_PAD(0x450, 0x0bc, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A25__GPIO_2_19             IOMUX_PAD(0x454, 0x0c0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A26__GPIO_2_20             IOMUX_PAD(0x458, 0x0c4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_A27__GPIO_2_21             IOMUX_PAD(0x45c, 0x0c8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_EB0__EIM_EB0               IOMUX_PAD(0x460, 0x0cc, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_EB1__EIM_EB1               IOMUX_PAD(0x464, 0x0d0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_EB2__GPIO_2_22             IOMUX_PAD(0x468, 0x0d4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_EB3__GPIO_2_23             IOMUX_PAD(0x46c, 0x0d8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_OE__GPIO_2_24              IOMUX_PAD(0x470, 0x0dc, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS0__GPIO_2_25             IOMUX_PAD(0x474, 0x0e0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS1__GPIO_2_26             IOMUX_PAD(0x478, 0x0e4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS2__GPIO_2_27             IOMUX_PAD(0x47c, 0x0e8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS3__GPIO_2_28             IOMUX_PAD(0x480, 0x0ec, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS4__GPIO_2_29             IOMUX_PAD(0x484, 0x0f0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CS5__GPIO_2_30             IOMUX_PAD(0x488, 0x0f4, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_DTACK__GPIO_2_31           IOMUX_PAD(0x48c, 0x0f8, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_LBA__GPIO_3_1              IOMUX_PAD(0x494, 0x0FC, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_EIM_CRE__GPIO_3_2              IOMUX_PAD(0x4A0, 0x100, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DRAM_CS1__DRAM_CS1             IOMUX_PAD(0x4D0, 0x104, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_WE_B__GPIO_3_3           IOMUX_PAD(0x4E4, 0x108, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RE_B__GPIO_3_4           IOMUX_PAD(0x4E8, 0x10C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_ALE__GPIO_3_5            IOMUX_PAD(0x4EC, 0x110, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CLE__GPIO_3_6            IOMUX_PAD(0x4F0, 0x114, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_WP_B__GPIO_3_7           IOMUX_PAD(0x4F4, 0x118, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RB0__GPIO_3_8            IOMUX_PAD(0x4F8, 0x11C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RB1__GPIO_3_9            IOMUX_PAD(0x4FC, 0x120, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RB2__GPIO_3_10           IOMUX_PAD(0x500, 0x124, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RB3__GPIO_3_11           IOMUX_PAD(0x504, 0x128, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_NAND__GPIO_3_12           IOMUX_PAD(0x514, 0x12C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS0__GPIO_3_16           IOMUX_PAD(0x518, 0x130, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS1__GPIO_3_17           IOMUX_PAD(0x51C, 0x134, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS2__GPIO_3_18           IOMUX_PAD(0x520, 0x138, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS3__GPIO_3_19           IOMUX_PAD(0x524, 0x13C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS4__GPIO_3_20           IOMUX_PAD(0x528, 0x140, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS5__GPIO_3_21           IOMUX_PAD(0x52C, 0x144, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS6__GPIO_3_22           IOMUX_PAD(0x530, 0x148, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_CS7__GPIO_3_23           IOMUX_PAD(0x534, 0x14C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_RDY_INT__GPIO_3_24       IOMUX_PAD(0x538, 0x150, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D15__GPIO_3_25           IOMUX_PAD(0x53C, 0x154, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D14__GPIO_3_26           IOMUX_PAD(0x540, 0x158, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D13__GPIO_3_27           IOMUX_PAD(0x544, 0x15C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D12__GPIO_3_28           IOMUX_PAD(0x548, 0x160, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D11__GPIO_3_29           IOMUX_PAD(0x54C, 0x164, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D10__GPIO_3_30           IOMUX_PAD(0x550, 0x168, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D9__GPIO_3_31            IOMUX_PAD(0x554, 0x16C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D8__GPIO_4_0             IOMUX_PAD(0x558, 0x170, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D7__GPIO_4_1             IOMUX_PAD(0x55C, 0x174, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D6__GPIO_4_2             IOMUX_PAD(0x560, 0x178, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D5__GPIO_4_3             IOMUX_PAD(0x564, 0x17C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D4__GPIO_4_4             IOMUX_PAD(0x568, 0x180, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D3__GPIO_4_5             IOMUX_PAD(0x56C, 0x184, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D2__GPIO_4_6             IOMUX_PAD(0x570, 0x188, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D1__GPIO_4_7             IOMUX_PAD(0x574, 0x18C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_NANDF_D0__GPIO_4_8             IOMUX_PAD(0x578, 0x190, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D8__GPIO_3_12             IOMUX_PAD(0x57C, 0x194, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D9__GPIO_3_13             IOMUX_PAD(0x580, 0x198, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D10__CSI1_D10             IOMUX_PAD(0x584, 0x19C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D11__CSI1_D11             IOMUX_PAD(0x588, 0x1A0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D12__CSI1_D12             IOMUX_PAD(0x58C, 0x1A4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D13__CSI1_D13             IOMUX_PAD(0x590, 0x1A8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D14__CSI1_D14             IOMUX_PAD(0x594, 0x1AC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D15__CSI1_D15             IOMUX_PAD(0x598, 0x1B0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D16__CSI1_D16             IOMUX_PAD(0x59C, 0x1B4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D17__CSI1_D17             IOMUX_PAD(0x5A0, 0x1B8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D18__CSI1_D18             IOMUX_PAD(0x5A4, 0x1BC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_D19__CSI1_D19             IOMUX_PAD(0x5A8, 0x1C0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_VSYNC__CSI1_VSYNC         IOMUX_PAD(0x5AC, 0x1C4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_HSYNC__CSI1_HSYNC         IOMUX_PAD(0x5B0, 0x1C8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_PIXCLK__CSI1_PIXCLK       IOMUX_PAD(0x5B4, 0x000, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_MCLK__CSI1_MCLK           IOMUX_PAD(0x5B8, 0x000, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI1_PKE0__CSI1_PKE0           IOMUX_PAD(0x860, 0x000, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D12__GPIO_4_9             IOMUX_PAD(0x5BC, 0x1CC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D13__GPIO_4_10            IOMUX_PAD(0x5C0, 0x1D0, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D14__GPIO_4_11            IOMUX_PAD(0x5C4, 0x1D4, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D15__GPIO_4_12            IOMUX_PAD(0x5C8, 0x1D8, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D16__GPIO_4_11            IOMUX_PAD(0x5CC, 0x1DC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D17__GPIO_4_12            IOMUX_PAD(0x5D0, 0x1E0, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D18__GPIO_4_11            IOMUX_PAD(0x5D4, 0x1E4, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_D19__GPIO_4_12            IOMUX_PAD(0x5D8, 0x1E8, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_VSYNC__GPIO_4_13          IOMUX_PAD(0x5DC, 0x1EC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_HSYNC__GPIO_4_14          IOMUX_PAD(0x5E0, 0x1F0, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSI2_PIXCLK__GPIO_4_15         IOMUX_PAD(0x5E4, 0x1F4, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_CLK__GPIO_4_16            IOMUX_PAD(0x5E8, 0x1F8, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_CLK__HSI2C_CLK		IOMUX_PAD(0x5E8, 0x1F8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_DAT__GPIO_4_17            IOMUX_PAD(0x5EC, 0x1FC, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_I2C1_DAT__HSI2C_DAT		IOMUX_PAD(0x5EC, 0x1FC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_AUD3_BB_TXD__GPIO_4_18         IOMUX_PAD(0x5F0, 0x200, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_AUD3_BB_RXD__GPIO_4_19         IOMUX_PAD(0x5F4, 0x204, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_AUD3_BB_CK__GPIO_4_20          IOMUX_PAD(0x5F8, 0x208, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_AUD3_BB_FS__GPIO_4_21          IOMUX_PAD(0x5FC, 0x20C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_MOSI__GPIO_4_22          IOMUX_PAD(0x600, 0x210, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_MISO__GPIO_4_23          IOMUX_PAD(0x604, 0x214, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_SS0__GPIO_4_24           IOMUX_PAD(0x608, 0x218, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_SS1__GPIO_4_25           IOMUX_PAD(0x60C, 0x21C, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_RDY__GPIO_4_26           IOMUX_PAD(0x610, 0x220, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_CSPI1_SCLK__GPIO_4_27          IOMUX_PAD(0x614, 0x224, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_UART1_RXD__UART1_RXD           IOMUX_PAD(0x618, 0x228, 0, 0x9e4, 0, MX51_UART1_PAD_CTRL | PAD_CTL_SRE_FAST)
#define MX51_PAD_UART1_TXD__UART1_TXD           IOMUX_PAD(0x61C, 0x22C, 0, 0x0,   0, MX51_UART1_PAD_CTRL | PAD_CTL_SRE_FAST)
#define MX51_PAD_UART1_RTS__UART1_RTS           IOMUX_PAD(0x620, 0x230, 0, 0x9e0, 0, MX51_UART1_PAD_CTRL)
#define MX51_PAD_UART1_CTS__UART1_CTS           IOMUX_PAD(0x624, 0x234, 0, 0x0,   0, MX51_UART1_PAD_CTRL)
#define MX51_PAD_UART2_RXD__UART2_RXD           IOMUX_PAD(0x628, 0x238, 0, 0x9ec, 2, MX51_UART2_PAD_CTRL)
#define MX51_PAD_UART2_TXD__UART2_TXD           IOMUX_PAD(0x62C, 0x23C, 0, 0x0,   0, MX51_UART2_PAD_CTRL)
#define MX51_PAD_UART3_RXD__GPIO_1_22           IOMUX_PAD(0x630, 0x240, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_UART3_TXD__GPIO_1_23           IOMUX_PAD(0x634, 0x244, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_OWIRE_LINE__GPIO_1_24          IOMUX_PAD(0x638, 0x248, 3, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_ROW0__KEY_ROW0             IOMUX_PAD(0x63C, 0x24C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_ROW1__KEY_ROW1             IOMUX_PAD(0x640, 0x250, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_ROW2__KEY_ROW2             IOMUX_PAD(0x644, 0x254, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_ROW3__KEY_ROW3             IOMUX_PAD(0x648, 0x258, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL0__KEY_COL0             IOMUX_PAD(0x64C, 0x25C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL1__KEY_COL1             IOMUX_PAD(0x650, 0x260, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL2__KEY_COL2             IOMUX_PAD(0x654, 0x264, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL3__KEY_COL3             IOMUX_PAD(0x658, 0x268, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL4__KEY_COL4             IOMUX_PAD(0x65C, 0x26C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL4__I2C2_SCL		IOMUX_PAD(0x65C, 0x26C, (3 | IOMUX_CONFIG_SION), \
							0x09b8, 1, MX51_I2C_PAD_CTRL)
#define MX51_PAD_KEY_COL5__KEY_COL5             IOMUX_PAD(0x660, 0x270, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_KEY_COL5__I2C2_SDA		IOMUX_PAD(0x660, 0x270, (3 | IOMUX_CONFIG_SION), \
							0x09bc, 1, MX51_I2C_PAD_CTRL)
#define MX51_PAD_USBH1_CLK__USBH1_CLK           IOMUX_PAD(0x678, 0x278, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DIR__USBH1_DIR           IOMUX_PAD(0x67C, 0x27C, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_STP__USBH1_STP           IOMUX_PAD(0x680, 0x280, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_STP__GPIO_1_27           IOMUX_PAD(0x680, 0x280, IOMUX_CONFIG_GPIO, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_NXT__USBH1_NXT           IOMUX_PAD(0x684, 0x284, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA0__USBH1_DATA0       IOMUX_PAD(0x688, 0x288, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA1__USBH1_DATA1       IOMUX_PAD(0x68C, 0x28C, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA2__USBH1_DATA2       IOMUX_PAD(0x690, 0x290, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA3__USBH1_DATA3       IOMUX_PAD(0x694, 0x294, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA4__USBH1_DATA4       IOMUX_PAD(0x698, 0x298, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA5__USBH1_DATA5       IOMUX_PAD(0x69C, 0x29C, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA6__USBH1_DATA6       IOMUX_PAD(0x6A0, 0x2A0, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_USBH1_DATA7__USBH1_DATA7       IOMUX_PAD(0x6A4, 0x2A4, 0, 0x0,   0, MX51_USBH1_PAD_CTRL)
#define MX51_PAD_DI1_PIN11__GPIO_3_0            IOMUX_PAD(0x6A8, 0x2A8, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_PIN12__GPIO_3_1            IOMUX_PAD(0x6AC, 0x2AC, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_PIN13__GPIO_3_2            IOMUX_PAD(0x6B0, 0x2B0, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_D0_CS__GPIO_3_3            IOMUX_PAD(0x6B4, 0x2B4, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_D1_CS__GPIO_3_4            IOMUX_PAD(0x6B8, 0x2B8, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_DIN__GPIO_3_5       IOMUX_PAD(0x6BC, 0x2BC, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_DIO__GPIO_3_6       IOMUX_PAD(0x6C0, 0x2C0, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_CLK__GPIO_3_7       IOMUX_PAD(0x6C4, 0x2C4, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISPB2_SER_RS__GPIO_3_8        IOMUX_PAD(0x6C8, 0x2C8, 4, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT0__DISP1_DAT0         IOMUX_PAD(0x6CC, 0x2CC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT1__DISP1_DAT1         IOMUX_PAD(0x6D0, 0x2D0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT2__DISP1_DAT2         IOMUX_PAD(0x6D4, 0x2D4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT3__DISP1_DAT3         IOMUX_PAD(0x6D8, 0x2D8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT4__DISP1_DAT4         IOMUX_PAD(0x6DC, 0x2DC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT5__DISP1_DAT5         IOMUX_PAD(0x6E0, 0x2E0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT6__DISP1_DAT6         IOMUX_PAD(0x6E4, 0x2E4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT7__DISP1_DAT7         IOMUX_PAD(0x6E8, 0x2E8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT8__DISP1_DAT8         IOMUX_PAD(0x6EC, 0x2EC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT9__DISP1_DAT9         IOMUX_PAD(0x6F0, 0x2F0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT10__DISP1_DAT10       IOMUX_PAD(0x6F4, 0x2F4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT11__DISP1_DAT11       IOMUX_PAD(0x6F8, 0x2F8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT12__DISP1_DAT12       IOMUX_PAD(0x6FC, 0x2FC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT13__DISP1_DAT13       IOMUX_PAD(0x700, 0x300, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT14__DISP1_DAT14       IOMUX_PAD(0x704, 0x304, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT15__DISP1_DAT15       IOMUX_PAD(0x708, 0x308, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT16__DISP1_DAT16       IOMUX_PAD(0x70C, 0x30C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT17__DISP1_DAT17       IOMUX_PAD(0x710, 0x310, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT18__DISP1_DAT18       IOMUX_PAD(0x714, 0x314, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT19__DISP1_DAT19       IOMUX_PAD(0x718, 0x318, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT20__DISP1_DAT20       IOMUX_PAD(0x71C, 0x31C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT21__DISP1_DAT21       IOMUX_PAD(0x720, 0x320, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT22__DISP1_DAT22       IOMUX_PAD(0x724, 0x324, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP1_DAT23__DISP1_DAT23       IOMUX_PAD(0x728, 0x328, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_PIN3__DI1_PIN3             IOMUX_PAD(0x72C, 0x32C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI1_PIN2__DI1_PIN2             IOMUX_PAD(0x734, 0x330, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI_GP1__DI_GP1                 IOMUX_PAD(0x73C, 0x334, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI_GP2__DI_GP2                 IOMUX_PAD(0x740, 0x338, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI_GP3__DI_GP3                 IOMUX_PAD(0x744, 0x33C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI2_PIN4__DI2_PIN4             IOMUX_PAD(0x748, 0x340, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI2_PIN2__DI2_PIN2             IOMUX_PAD(0x74C, 0x344, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI2_PIN3__DI2_PIN3             IOMUX_PAD(0x750, 0x348, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI2_DISP_CLK__DI2_DISP_CLK     IOMUX_PAD(0x754, 0x34C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DI_GP4__DI_GP4                 IOMUX_PAD(0x758, 0x350, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT0__DISP2_DAT0         IOMUX_PAD(0x75C, 0x354, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT1__DISP2_DAT1         IOMUX_PAD(0x760, 0x358, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT2__DISP2_DAT2         IOMUX_PAD(0x764, 0x35C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT3__DISP2_DAT3         IOMUX_PAD(0x768, 0x360, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT4__DISP2_DAT4         IOMUX_PAD(0x76C, 0x364, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT5__DISP2_DAT5         IOMUX_PAD(0x770, 0x368, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT6__GPIO_1_19          IOMUX_PAD(0x774, 0x36C, 5, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT7__GPIO_1_29          IOMUX_PAD(0x778, 0x370, 5, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT8__GPIO_1_30          IOMUX_PAD(0x77C, 0x374, 5, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT9__GPIO_1_31          IOMUX_PAD(0x780, 0x378, 5, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT10__DISP2_DAT10       IOMUX_PAD(0x784, 0x37C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT11__DISP2_DAT11       IOMUX_PAD(0x788, 0x380, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT12__DISP2_DAT12       IOMUX_PAD(0x78C, 0x384, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT13__DISP2_DAT13       IOMUX_PAD(0x790, 0x388, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT14__DISP2_DAT14       IOMUX_PAD(0x794, 0x38C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_DISP2_DAT15__DISP2_DAT15       IOMUX_PAD(0x798, 0x390, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_CMD__SD1_CMD               IOMUX_PAD(0x79C, 0x394, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_CLK__SD1_CLK               IOMUX_PAD(0x7A0, 0x398, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_DATA0__SD1_DATA0           IOMUX_PAD(0x7A4, 0x39C, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_DATA1__SD1_DATA1           IOMUX_PAD(0x7A8, 0x3A0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_DATA2__SD1_DATA2           IOMUX_PAD(0x7AC, 0x3A4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD1_DATA3__SD1_DATA3           IOMUX_PAD(0x7B0, 0x3A8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_0__GPIO_1_0		IOMUX_PAD(0x7B4, 0x3AC, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_1__GPIO_1_1		IOMUX_PAD(0x7B8, 0x3B0, 1, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_CMD__SD2_CMD               IOMUX_PAD(0x7BC, 0x3B4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_CLK__SD2_CLK               IOMUX_PAD(0x7C0, 0x3B8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_DATA0__SD2_DATA0           IOMUX_PAD(0x7C4, 0x3BC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_DATA1__SD2_DATA1           IOMUX_PAD(0x7C8, 0x3C0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_DATA2__SD2_DATA2           IOMUX_PAD(0x7CC, 0x3C4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_SD2_DATA3__SD2_DATA3           IOMUX_PAD(0x7D0, 0x3C8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_2__GPIO_1_2		IOMUX_PAD(0x7D4, 0x3CC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_3__GPIO_1_3		IOMUX_PAD(0x7D8, 0x3D0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_PMIC_INT_REQ__PMIC_INT_REQ	IOMUX_PAD(0x7FC, 0x3D4, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_4__GPIO_1_4		IOMUX_PAD(0x804, 0x3D8, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_5__GPIO_1_5		IOMUX_PAD(0x808, 0x3DC, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_6__GPIO_1_6		IOMUX_PAD(0x80C, 0x3E0, 0, 0x0,   0, NO_PAD_CTRL)
#define MX51_PAD_GPIO_1_7__GPIO_1_7		IOMUX_PAD(0x810, 0x3E4, 0, 0x0,   0, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_GPIO_1_8__GPIO_1_8		IOMUX_PAD(0x814, 0x3E8, 0, 0x0,   1, MX51_GPIO_PAD_CTRL)
#define MX51_PAD_GPIO_1_9__GPIO_1_9		IOMUX_PAD(0x818, 0x3EC, 0, 0x0,   0, NO_PAD_CTRL)

#endif /* __MACH_IOMUX_MX51_H__ */
