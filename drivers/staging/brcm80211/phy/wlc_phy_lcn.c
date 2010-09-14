/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wlc_cfg.h>
#include <qmath.h>
#include <osl.h>
#include <siutils.h>
#include <bitfuncs.h>
#include <hndpmu.h>

#include <wlc_phy_radio.h>
#include <wlc_phy_int.h>
#include <wlc_phy_lcn.h>
#include <wlc_phytbl_lcn.h>

#define PLL_2064_NDIV		90
#define PLL_2064_LOW_END_VCO 	3000
#define PLL_2064_LOW_END_KVCO 	27
#define PLL_2064_HIGH_END_VCO	4200
#define PLL_2064_HIGH_END_KVCO	68
#define PLL_2064_LOOP_BW_DOUBLER	200
#define PLL_2064_D30_DOUBLER		10500
#define PLL_2064_LOOP_BW	260
#define PLL_2064_D30		8000
#define PLL_2064_CAL_REF_TO	8
#define PLL_2064_MHZ		1000000
#define PLL_2064_OPEN_LOOP_DELAY	5

#define TEMPSENSE 			1
#define VBATSENSE           2

#define NOISE_IF_UPD_CHK_INTERVAL	1
#define NOISE_IF_UPD_RST_INTERVAL	60
#define NOISE_IF_UPD_THRESHOLD_CNT	1
#define NOISE_IF_UPD_TRHRESHOLD	50
#define NOISE_IF_UPD_TIMEOUT		1000
#define NOISE_IF_OFF			0
#define NOISE_IF_CHK			1
#define NOISE_IF_ON			2

#define PAPD_BLANKING_PROFILE 		3
#define PAPD2LUT			0
#define PAPD_CORR_NORM 			0
#define PAPD_BLANKING_THRESHOLD 	0
#define PAPD_STOP_AFTER_LAST_UPDATE	0

#define LCN_TARGET_PWR  60

#define LCN_VBAT_OFFSET_433X 34649679
#define LCN_VBAT_SLOPE_433X  8258032

#define LCN_VBAT_SCALE_NOM  53
#define LCN_VBAT_SCALE_DEN  432

#define LCN_TEMPSENSE_OFFSET  80812
#define LCN_TEMPSENSE_DEN  2647

#define LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(0 + 8)
#define LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(0 + 8)
#define LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define wlc_lcnphy_enable_tx_gain_override(pi) \
	wlc_lcnphy_set_tx_gain_override(pi, TRUE)
#define wlc_lcnphy_disable_tx_gain_override(pi) \
	wlc_lcnphy_set_tx_gain_override(pi, FALSE)

#define wlc_lcnphy_iqcal_active(pi)	\
	(read_phy_reg((pi), 0x451) & \
	((0x1 << 15) | (0x1 << 14)))

#define txpwrctrl_off(pi) (0x7 != ((read_phy_reg(pi, 0x4a4) & 0xE000) >> 13))
#define wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) \
	(pi->temppwrctrl_capable)
#define wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) \
	(pi->hwpwrctrl_capable)

#define SWCTRL_BT_TX		0x18
#define SWCTRL_OVR_DISABLE	0x40

#define	AFE_CLK_INIT_MODE_TXRX2X	1
#define	AFE_CLK_INIT_MODE_PAPD		0

#define LCNPHY_TBL_ID_IQLOCAL			0x00

#define LCNPHY_TBL_ID_RFSEQ         0x08
#define LCNPHY_TBL_ID_GAIN_IDX		0x0d
#define LCNPHY_TBL_ID_SW_CTRL			0x0f
#define LCNPHY_TBL_ID_GAIN_TBL		0x12
#define LCNPHY_TBL_ID_SPUR			0x14
#define LCNPHY_TBL_ID_SAMPLEPLAY		0x15
#define LCNPHY_TBL_ID_SAMPLEPLAY1		0x16

#define LCNPHY_TX_PWR_CTRL_RATE_OFFSET 	832
#define LCNPHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define LCNPHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define LCNPHY_TX_PWR_CTRL_IQ_OFFSET		320
#define LCNPHY_TX_PWR_CTRL_LO_OFFSET		448
#define LCNPHY_TX_PWR_CTRL_PWR_OFFSET		576

#define LCNPHY_TX_PWR_CTRL_START_INDEX_2G_4313	140

#define LCNPHY_TX_PWR_CTRL_START_NPT		1
#define LCNPHY_TX_PWR_CTRL_MAX_NPT			7

#define LCNPHY_NOISE_SAMPLES_DEFAULT 5000

#define LCNPHY_ACI_DETECT_START      1
#define LCNPHY_ACI_DETECT_PROGRESS   2
#define LCNPHY_ACI_DETECT_STOP       3

#define LCNPHY_ACI_CRSHIFRMLO_TRSH 100
#define LCNPHY_ACI_GLITCH_TRSH 2000
#define	LCNPHY_ACI_TMOUT 250
#define LCNPHY_ACI_DETECT_TIMEOUT  2
#define LCNPHY_ACI_START_DELAY 0

#define wlc_lcnphy_tx_gain_override_enabled(pi) \
	(0 != (read_phy_reg((pi), 0x43b) & (0x1 << 6)))

#define wlc_lcnphy_total_tx_frames(pi) \
	wlapi_bmac_read_shm((pi)->sh->physhim, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txallfrm))

typedef struct {
	uint16 gm_gain;
	uint16 pga_gain;
	uint16 pad_gain;
	uint16 dac_gain;
} lcnphy_txgains_t;

typedef enum {
	LCNPHY_CAL_FULL,
	LCNPHY_CAL_RECAL,
	LCNPHY_CAL_CURRECAL,
	LCNPHY_CAL_DIGCAL,
	LCNPHY_CAL_GCTRL
} lcnphy_cal_mode_t;

typedef struct {
	lcnphy_txgains_t gains;
	bool useindex;
	uint8 index;
} lcnphy_txcalgains_t;

typedef struct {
	uint8 chan;
	int16 a;
	int16 b;
} lcnphy_rx_iqcomp_t;

typedef struct {
	int16 re;
	int16 im;
} lcnphy_spb_tone_t;

typedef struct {
	uint16 re;
	uint16 im;
} lcnphy_unsign16_struct;

typedef struct {
	uint32 iq_prod;
	uint32 i_pwr;
	uint32 q_pwr;
} lcnphy_iq_est_t;

typedef struct {
	uint16 ptcentreTs20;
	uint16 ptcentreFactor;
} lcnphy_sfo_cfg_t;

typedef enum {
	LCNPHY_PAPD_CAL_CW,
	LCNPHY_PAPD_CAL_OFDM
} lcnphy_papd_cal_type_t;

typedef uint16 iqcal_gain_params_lcnphy[9];

static const iqcal_gain_params_lcnphy tbl_iqcal_gainparams_lcnphy_2G[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const iqcal_gain_params_lcnphy *tbl_iqcal_gainparams_lcnphy[1] = {
	tbl_iqcal_gainparams_lcnphy_2G,
};

static const uint16 iqcal_gainparams_numgains_lcnphy[1] = {
	sizeof(tbl_iqcal_gainparams_lcnphy_2G) /
	    sizeof(*tbl_iqcal_gainparams_lcnphy_2G),
};

static const lcnphy_sfo_cfg_t lcnphy_sfo_cfg[] = {
	{965, 1087},
	{967, 1085},
	{969, 1082},
	{971, 1080},
	{973, 1078},
	{975, 1076},
	{977, 1073},
	{979, 1071},
	{981, 1069},
	{983, 1067},
	{985, 1065},
	{987, 1063},
	{989, 1060},
	{994, 1055}
};

static const
uint16 lcnphy_iqcal_loft_gainladder[] = {
	((2 << 8) | 0),
	((3 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((16 << 8) | 1),
	((16 << 8) | 2),
	((16 << 8) | 3),
	((16 << 8) | 4),
	((16 << 8) | 5),
	((16 << 8) | 6),
	((16 << 8) | 7),
	((23 << 8) | 7),
	((32 << 8) | 7),
	((45 << 8) | 7),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

static const
uint16 lcnphy_iqcal_ir_gainladder[] = {
	((1 << 8) | 0),
	((2 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((23 << 8) | 0),
	((32 << 8) | 0),
	((45 << 8) | 0),
	((64 << 8) | 0),
	((64 << 8) | 1),
	((64 << 8) | 2),
	((64 << 8) | 3),
	((64 << 8) | 4),
	((64 << 8) | 5),
	((64 << 8) | 6),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

static const
lcnphy_spb_tone_t lcnphy_spb_tone_3750[] = {
	{88, 0},
	{73, 49},
	{34, 81},
	{-17, 86},
	{-62, 62},
	{-86, 17},
	{-81, -34},
	{-49, -73},
	{0, -88},
	{49, -73},
	{81, -34},
	{86, 17},
	{62, 62},
	{17, 86},
	{-34, 81},
	{-73, 49},
	{-88, 0},
	{-73, -49},
	{-34, -81},
	{17, -86},
	{62, -62},
	{86, -17},
	{81, 34},
	{49, 73},
	{0, 88},
	{-49, 73},
	{-81, 34},
	{-86, -17},
	{-62, -62},
	{-17, -86},
	{34, -81},
	{73, -49},
};

static const
uint16 iqlo_loopback_rf_regs[20] = {
	RADIO_2064_REG036,
	RADIO_2064_REG11A,
	RADIO_2064_REG03A,
	RADIO_2064_REG025,
	RADIO_2064_REG028,
	RADIO_2064_REG005,
	RADIO_2064_REG112,
	RADIO_2064_REG0FF,
	RADIO_2064_REG11F,
	RADIO_2064_REG00B,
	RADIO_2064_REG113,
	RADIO_2064_REG007,
	RADIO_2064_REG0FC,
	RADIO_2064_REG0FD,
	RADIO_2064_REG012,
	RADIO_2064_REG057,
	RADIO_2064_REG059,
	RADIO_2064_REG05C,
	RADIO_2064_REG078,
	RADIO_2064_REG092,
};

static const
uint16 tempsense_phy_regs[14] = {
	0x503,
	0x4a4,
	0x4d0,
	0x4d9,
	0x4da,
	0x4a6,
	0x938,
	0x939,
	0x4d8,
	0x4d0,
	0x4d7,
	0x4a5,
	0x40d,
	0x4a2,
};

static const
uint16 rxiq_cal_rf_reg[11] = {
	RADIO_2064_REG098,
	RADIO_2064_REG116,
	RADIO_2064_REG12C,
	RADIO_2064_REG06A,
	RADIO_2064_REG00B,
	RADIO_2064_REG01B,
	RADIO_2064_REG113,
	RADIO_2064_REG01D,
	RADIO_2064_REG114,
	RADIO_2064_REG02E,
	RADIO_2064_REG12A,
};

static const
lcnphy_rx_iqcomp_t lcnphy_rx_iqcomp_table_rev0[] = {
	{1, 0, 0},
	{2, 0, 0},
	{3, 0, 0},
	{4, 0, 0},
	{5, 0, 0},
	{6, 0, 0},
	{7, 0, 0},
	{8, 0, 0},
	{9, 0, 0},
	{10, 0, 0},
	{11, 0, 0},
	{12, 0, 0},
	{13, 0, 0},
	{14, 0, 0},
	{34, 0, 0},
	{38, 0, 0},
	{42, 0, 0},
	{46, 0, 0},
	{36, 0, 0},
	{40, 0, 0},
	{44, 0, 0},
	{48, 0, 0},
	{52, 0, 0},
	{56, 0, 0},
	{60, 0, 0},
	{64, 0, 0},
	{100, 0, 0},
	{104, 0, 0},
	{108, 0, 0},
	{112, 0, 0},
	{116, 0, 0},
	{120, 0, 0},
	{124, 0, 0},
	{128, 0, 0},
	{132, 0, 0},
	{136, 0, 0},
	{140, 0, 0},
	{149, 0, 0},
	{153, 0, 0},
	{157, 0, 0},
	{161, 0, 0},
	{165, 0, 0},
	{184, 0, 0},
	{188, 0, 0},
	{192, 0, 0},
	{196, 0, 0},
	{200, 0, 0},
	{204, 0, 0},
	{208, 0, 0},
	{212, 0, 0},
	{216, 0, 0},
};

static const uint32 lcnphy_23bitgaincode_table[] = {
	0x200100,
	0x200200,
	0x200004,
	0x200014,
	0x200024,
	0x200034,
	0x200134,
	0x200234,
	0x200334,
	0x200434,
	0x200037,
	0x200137,
	0x200237,
	0x200337,
	0x200437,
	0x000035,
	0x000135,
	0x000235,
	0x000037,
	0x000137,
	0x000237,
	0x000337,
	0x00013f,
	0x00023f,
	0x00033f,
	0x00034f,
	0x00044f,
	0x00144f,
	0x00244f,
	0x00254f,
	0x00354f,
	0x00454f,
	0x00464f,
	0x01464f,
	0x02464f,
	0x03464f,
	0x04464f,
};

static const int8 lcnphy_gain_table[] = {
	-16,
	-13,
	10,
	7,
	4,
	0,
	3,
	6,
	9,
	12,
	15,
	18,
	21,
	24,
	27,
	30,
	33,
	36,
	39,
	42,
	45,
	48,
	50,
	53,
	56,
	59,
	62,
	65,
	68,
	71,
	74,
	77,
	80,
	83,
	86,
	89,
	92,
};

static const int8 lcnphy_gain_index_offset_for_rssi[] = {
	7,
	7,
	7,
	7,
	7,
	7,
	7,
	8,
	7,
	7,
	6,
	7,
	7,
	4,
	4,
	4,
	4,
	4,
	4,
	4,
	4,
	3,
	3,
	3,
	3,
	3,
	3,
	4,
	2,
	2,
	2,
	2,
	2,
	2,
	-1,
	-2,
	-2,
	-2
};

extern CONST uint8 spur_tbl_rev0[];
extern CONST uint32 dot11lcnphytbl_rx_gain_info_sz_rev1;
extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_rev1[];
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_bt_epa;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250;

typedef struct _chan_info_2064_lcnphy {
	uint chan;
	uint freq;
	uint8 logen_buftune;
	uint8 logen_rccr_tx;
	uint8 txrf_mix_tune_ctrl;
	uint8 pa_input_tune_g;
	uint8 logen_rccr_rx;
	uint8 pa_rxrf_lna1_freq_tune;
	uint8 pa_rxrf_lna2_freq_tune;
	uint8 rxrf_rxrf_spare1;
} chan_info_2064_lcnphy_t;

static chan_info_2064_lcnphy_t chan_info_2064_lcnphy[] = {
	{1, 2412, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{2, 2417, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{3, 2422, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{4, 2427, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{5, 2432, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{6, 2437, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{7, 2442, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{8, 2447, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{9, 2452, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{10, 2457, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{11, 2462, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{12, 2467, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{13, 2472, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
	{14, 2484, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80},
};

lcnphy_radio_regs_t lcnphy_radio_regs_2064[] = {
	{0x00, 0, 0, 0, 0},
	{0x01, 0x64, 0x64, 0, 0},
	{0x02, 0x20, 0x20, 0, 0},
	{0x03, 0x66, 0x66, 0, 0},
	{0x04, 0xf8, 0xf8, 0, 0},
	{0x05, 0, 0, 0, 0},
	{0x06, 0x10, 0x10, 0, 0},
	{0x07, 0, 0, 0, 0},
	{0x08, 0, 0, 0, 0},
	{0x09, 0, 0, 0, 0},
	{0x0A, 0x37, 0x37, 0, 0},
	{0x0B, 0x6, 0x6, 0, 0},
	{0x0C, 0x55, 0x55, 0, 0},
	{0x0D, 0x8b, 0x8b, 0, 0},
	{0x0E, 0, 0, 0, 0},
	{0x0F, 0x5, 0x5, 0, 0},
	{0x10, 0, 0, 0, 0},
	{0x11, 0xe, 0xe, 0, 0},
	{0x12, 0, 0, 0, 0},
	{0x13, 0xb, 0xb, 0, 0},
	{0x14, 0x2, 0x2, 0, 0},
	{0x15, 0x12, 0x12, 0, 0},
	{0x16, 0x12, 0x12, 0, 0},
	{0x17, 0xc, 0xc, 0, 0},
	{0x18, 0xc, 0xc, 0, 0},
	{0x19, 0xc, 0xc, 0, 0},
	{0x1A, 0x8, 0x8, 0, 0},
	{0x1B, 0x2, 0x2, 0, 0},
	{0x1C, 0, 0, 0, 0},
	{0x1D, 0x1, 0x1, 0, 0},
	{0x1E, 0x12, 0x12, 0, 0},
	{0x1F, 0x6e, 0x6e, 0, 0},
	{0x20, 0x2, 0x2, 0, 0},
	{0x21, 0x23, 0x23, 0, 0},
	{0x22, 0x8, 0x8, 0, 0},
	{0x23, 0, 0, 0, 0},
	{0x24, 0, 0, 0, 0},
	{0x25, 0xc, 0xc, 0, 0},
	{0x26, 0x33, 0x33, 0, 0},
	{0x27, 0x55, 0x55, 0, 0},
	{0x28, 0, 0, 0, 0},
	{0x29, 0x30, 0x30, 0, 0},
	{0x2A, 0xb, 0xb, 0, 0},
	{0x2B, 0x1b, 0x1b, 0, 0},
	{0x2C, 0x3, 0x3, 0, 0},
	{0x2D, 0x1b, 0x1b, 0, 0},
	{0x2E, 0, 0, 0, 0},
	{0x2F, 0x20, 0x20, 0, 0},
	{0x30, 0xa, 0xa, 0, 0},
	{0x31, 0, 0, 0, 0},
	{0x32, 0x62, 0x62, 0, 0},
	{0x33, 0x19, 0x19, 0, 0},
	{0x34, 0x33, 0x33, 0, 0},
	{0x35, 0x77, 0x77, 0, 0},
	{0x36, 0, 0, 0, 0},
	{0x37, 0x70, 0x70, 0, 0},
	{0x38, 0x3, 0x3, 0, 0},
	{0x39, 0xf, 0xf, 0, 0},
	{0x3A, 0x6, 0x6, 0, 0},
	{0x3B, 0xcf, 0xcf, 0, 0},
	{0x3C, 0x1a, 0x1a, 0, 0},
	{0x3D, 0x6, 0x6, 0, 0},
	{0x3E, 0x42, 0x42, 0, 0},
	{0x3F, 0, 0, 0, 0},
	{0x40, 0xfb, 0xfb, 0, 0},
	{0x41, 0x9a, 0x9a, 0, 0},
	{0x42, 0x7a, 0x7a, 0, 0},
	{0x43, 0x29, 0x29, 0, 0},
	{0x44, 0, 0, 0, 0},
	{0x45, 0x8, 0x8, 0, 0},
	{0x46, 0xce, 0xce, 0, 0},
	{0x47, 0x27, 0x27, 0, 0},
	{0x48, 0x62, 0x62, 0, 0},
	{0x49, 0x6, 0x6, 0, 0},
	{0x4A, 0x58, 0x58, 0, 0},
	{0x4B, 0xf7, 0xf7, 0, 0},
	{0x4C, 0, 0, 0, 0},
	{0x4D, 0xb3, 0xb3, 0, 0},
	{0x4E, 0, 0, 0, 0},
	{0x4F, 0x2, 0x2, 0, 0},
	{0x50, 0, 0, 0, 0},
	{0x51, 0x9, 0x9, 0, 0},
	{0x52, 0x5, 0x5, 0, 0},
	{0x53, 0x17, 0x17, 0, 0},
	{0x54, 0x38, 0x38, 0, 0},
	{0x55, 0, 0, 0, 0},
	{0x56, 0, 0, 0, 0},
	{0x57, 0xb, 0xb, 0, 0},
	{0x58, 0, 0, 0, 0},
	{0x59, 0, 0, 0, 0},
	{0x5A, 0, 0, 0, 0},
	{0x5B, 0, 0, 0, 0},
	{0x5C, 0, 0, 0, 0},
	{0x5D, 0, 0, 0, 0},
	{0x5E, 0x88, 0x88, 0, 0},
	{0x5F, 0xcc, 0xcc, 0, 0},
	{0x60, 0x74, 0x74, 0, 0},
	{0x61, 0x74, 0x74, 0, 0},
	{0x62, 0x74, 0x74, 0, 0},
	{0x63, 0x44, 0x44, 0, 0},
	{0x64, 0x77, 0x77, 0, 0},
	{0x65, 0x44, 0x44, 0, 0},
	{0x66, 0x77, 0x77, 0, 0},
	{0x67, 0x55, 0x55, 0, 0},
	{0x68, 0x77, 0x77, 0, 0},
	{0x69, 0x77, 0x77, 0, 0},
	{0x6A, 0, 0, 0, 0},
	{0x6B, 0x7f, 0x7f, 0, 0},
	{0x6C, 0x8, 0x8, 0, 0},
	{0x6D, 0, 0, 0, 0},
	{0x6E, 0x88, 0x88, 0, 0},
	{0x6F, 0x66, 0x66, 0, 0},
	{0x70, 0x66, 0x66, 0, 0},
	{0x71, 0x28, 0x28, 0, 0},
	{0x72, 0x55, 0x55, 0, 0},
	{0x73, 0x4, 0x4, 0, 0},
	{0x74, 0, 0, 0, 0},
	{0x75, 0, 0, 0, 0},
	{0x76, 0, 0, 0, 0},
	{0x77, 0x1, 0x1, 0, 0},
	{0x78, 0xd6, 0xd6, 0, 0},
	{0x79, 0, 0, 0, 0},
	{0x7A, 0, 0, 0, 0},
	{0x7B, 0, 0, 0, 0},
	{0x7C, 0, 0, 0, 0},
	{0x7D, 0, 0, 0, 0},
	{0x7E, 0, 0, 0, 0},
	{0x7F, 0, 0, 0, 0},
	{0x80, 0, 0, 0, 0},
	{0x81, 0, 0, 0, 0},
	{0x82, 0, 0, 0, 0},
	{0x83, 0xb4, 0xb4, 0, 0},
	{0x84, 0x1, 0x1, 0, 0},
	{0x85, 0x20, 0x20, 0, 0},
	{0x86, 0x5, 0x5, 0, 0},
	{0x87, 0xff, 0xff, 0, 0},
	{0x88, 0x7, 0x7, 0, 0},
	{0x89, 0x77, 0x77, 0, 0},
	{0x8A, 0x77, 0x77, 0, 0},
	{0x8B, 0x77, 0x77, 0, 0},
	{0x8C, 0x77, 0x77, 0, 0},
	{0x8D, 0x8, 0x8, 0, 0},
	{0x8E, 0xa, 0xa, 0, 0},
	{0x8F, 0x8, 0x8, 0, 0},
	{0x90, 0x18, 0x18, 0, 0},
	{0x91, 0x5, 0x5, 0, 0},
	{0x92, 0x1f, 0x1f, 0, 0},
	{0x93, 0x10, 0x10, 0, 0},
	{0x94, 0x3, 0x3, 0, 0},
	{0x95, 0, 0, 0, 0},
	{0x96, 0, 0, 0, 0},
	{0x97, 0xaa, 0xaa, 0, 0},
	{0x98, 0, 0, 0, 0},
	{0x99, 0x23, 0x23, 0, 0},
	{0x9A, 0x7, 0x7, 0, 0},
	{0x9B, 0xf, 0xf, 0, 0},
	{0x9C, 0x10, 0x10, 0, 0},
	{0x9D, 0x3, 0x3, 0, 0},
	{0x9E, 0x4, 0x4, 0, 0},
	{0x9F, 0x20, 0x20, 0, 0},
	{0xA0, 0, 0, 0, 0},
	{0xA1, 0, 0, 0, 0},
	{0xA2, 0, 0, 0, 0},
	{0xA3, 0, 0, 0, 0},
	{0xA4, 0x1, 0x1, 0, 0},
	{0xA5, 0x77, 0x77, 0, 0},
	{0xA6, 0x77, 0x77, 0, 0},
	{0xA7, 0x77, 0x77, 0, 0},
	{0xA8, 0x77, 0x77, 0, 0},
	{0xA9, 0x8c, 0x8c, 0, 0},
	{0xAA, 0x88, 0x88, 0, 0},
	{0xAB, 0x78, 0x78, 0, 0},
	{0xAC, 0x57, 0x57, 0, 0},
	{0xAD, 0x88, 0x88, 0, 0},
	{0xAE, 0, 0, 0, 0},
	{0xAF, 0x8, 0x8, 0, 0},
	{0xB0, 0x88, 0x88, 0, 0},
	{0xB1, 0, 0, 0, 0},
	{0xB2, 0x1b, 0x1b, 0, 0},
	{0xB3, 0x3, 0x3, 0, 0},
	{0xB4, 0x24, 0x24, 0, 0},
	{0xB5, 0x3, 0x3, 0, 0},
	{0xB6, 0x1b, 0x1b, 0, 0},
	{0xB7, 0x24, 0x24, 0, 0},
	{0xB8, 0x3, 0x3, 0, 0},
	{0xB9, 0, 0, 0, 0},
	{0xBA, 0xaa, 0xaa, 0, 0},
	{0xBB, 0, 0, 0, 0},
	{0xBC, 0x4, 0x4, 0, 0},
	{0xBD, 0, 0, 0, 0},
	{0xBE, 0x8, 0x8, 0, 0},
	{0xBF, 0x11, 0x11, 0, 0},
	{0xC0, 0, 0, 0, 0},
	{0xC1, 0, 0, 0, 0},
	{0xC2, 0x62, 0x62, 0, 0},
	{0xC3, 0x1e, 0x1e, 0, 0},
	{0xC4, 0x33, 0x33, 0, 0},
	{0xC5, 0x37, 0x37, 0, 0},
	{0xC6, 0, 0, 0, 0},
	{0xC7, 0x70, 0x70, 0, 0},
	{0xC8, 0x1e, 0x1e, 0, 0},
	{0xC9, 0x6, 0x6, 0, 0},
	{0xCA, 0x4, 0x4, 0, 0},
	{0xCB, 0x2f, 0x2f, 0, 0},
	{0xCC, 0xf, 0xf, 0, 0},
	{0xCD, 0, 0, 0, 0},
	{0xCE, 0xff, 0xff, 0, 0},
	{0xCF, 0x8, 0x8, 0, 0},
	{0xD0, 0x3f, 0x3f, 0, 0},
	{0xD1, 0x3f, 0x3f, 0, 0},
	{0xD2, 0x3f, 0x3f, 0, 0},
	{0xD3, 0, 0, 0, 0},
	{0xD4, 0, 0, 0, 0},
	{0xD5, 0, 0, 0, 0},
	{0xD6, 0xcc, 0xcc, 0, 0},
	{0xD7, 0, 0, 0, 0},
	{0xD8, 0x8, 0x8, 0, 0},
	{0xD9, 0x8, 0x8, 0, 0},
	{0xDA, 0x8, 0x8, 0, 0},
	{0xDB, 0x11, 0x11, 0, 0},
	{0xDC, 0, 0, 0, 0},
	{0xDD, 0x87, 0x87, 0, 0},
	{0xDE, 0x88, 0x88, 0, 0},
	{0xDF, 0x8, 0x8, 0, 0},
	{0xE0, 0x8, 0x8, 0, 0},
	{0xE1, 0x8, 0x8, 0, 0},
	{0xE2, 0, 0, 0, 0},
	{0xE3, 0, 0, 0, 0},
	{0xE4, 0, 0, 0, 0},
	{0xE5, 0xf5, 0xf5, 0, 0},
	{0xE6, 0x30, 0x30, 0, 0},
	{0xE7, 0x1, 0x1, 0, 0},
	{0xE8, 0, 0, 0, 0},
	{0xE9, 0xff, 0xff, 0, 0},
	{0xEA, 0, 0, 0, 0},
	{0xEB, 0, 0, 0, 0},
	{0xEC, 0x22, 0x22, 0, 0},
	{0xED, 0, 0, 0, 0},
	{0xEE, 0, 0, 0, 0},
	{0xEF, 0, 0, 0, 0},
	{0xF0, 0x3, 0x3, 0, 0},
	{0xF1, 0x1, 0x1, 0, 0},
	{0xF2, 0, 0, 0, 0},
	{0xF3, 0, 0, 0, 0},
	{0xF4, 0, 0, 0, 0},
	{0xF5, 0, 0, 0, 0},
	{0xF6, 0, 0, 0, 0},
	{0xF7, 0x6, 0x6, 0, 0},
	{0xF8, 0, 0, 0, 0},
	{0xF9, 0, 0, 0, 0},
	{0xFA, 0x40, 0x40, 0, 0},
	{0xFB, 0, 0, 0, 0},
	{0xFC, 0x1, 0x1, 0, 0},
	{0xFD, 0x80, 0x80, 0, 0},
	{0xFE, 0x2, 0x2, 0, 0},
	{0xFF, 0x10, 0x10, 0, 0},
	{0x100, 0x2, 0x2, 0, 0},
	{0x101, 0x1e, 0x1e, 0, 0},
	{0x102, 0x1e, 0x1e, 0, 0},
	{0x103, 0, 0, 0, 0},
	{0x104, 0x1f, 0x1f, 0, 0},
	{0x105, 0, 0x8, 0, 1},
	{0x106, 0x2a, 0x2a, 0, 0},
	{0x107, 0xf, 0xf, 0, 0},
	{0x108, 0, 0, 0, 0},
	{0x109, 0, 0, 0, 0},
	{0x10A, 0, 0, 0, 0},
	{0x10B, 0, 0, 0, 0},
	{0x10C, 0, 0, 0, 0},
	{0x10D, 0, 0, 0, 0},
	{0x10E, 0, 0, 0, 0},
	{0x10F, 0, 0, 0, 0},
	{0x110, 0, 0, 0, 0},
	{0x111, 0, 0, 0, 0},
	{0x112, 0, 0, 0, 0},
	{0x113, 0, 0, 0, 0},
	{0x114, 0, 0, 0, 0},
	{0x115, 0, 0, 0, 0},
	{0x116, 0, 0, 0, 0},
	{0x117, 0, 0, 0, 0},
	{0x118, 0, 0, 0, 0},
	{0x119, 0, 0, 0, 0},
	{0x11A, 0, 0, 0, 0},
	{0x11B, 0, 0, 0, 0},
	{0x11C, 0x1, 0x1, 0, 0},
	{0x11D, 0, 0, 0, 0},
	{0x11E, 0, 0, 0, 0},
	{0x11F, 0, 0, 0, 0},
	{0x120, 0, 0, 0, 0},
	{0x121, 0, 0, 0, 0},
	{0x122, 0x80, 0x80, 0, 0},
	{0x123, 0, 0, 0, 0},
	{0x124, 0xf8, 0xf8, 0, 0},
	{0x125, 0, 0, 0, 0},
	{0x126, 0, 0, 0, 0},
	{0x127, 0, 0, 0, 0},
	{0x128, 0, 0, 0, 0},
	{0x129, 0, 0, 0, 0},
	{0x12A, 0, 0, 0, 0},
	{0x12B, 0, 0, 0, 0},
	{0x12C, 0, 0, 0, 0},
	{0x12D, 0, 0, 0, 0},
	{0x12E, 0, 0, 0, 0},
	{0x12F, 0, 0, 0, 0},
	{0x130, 0, 0, 0, 0},
	{0xFFFF, 0, 0, 0, 0}
};

#define LCNPHY_NUM_DIG_FILT_COEFFS 16
#define LCNPHY_NUM_TX_DIG_FILTERS_CCK 13

uint16
    LCNPHY_txdigfiltcoeffs_cck[LCNPHY_NUM_TX_DIG_FILTERS_CCK]
    [LCNPHY_NUM_DIG_FILT_COEFFS + 1] = {
	{0, 1, 415, 1874, 64, 128, 64, 792, 1656, 64, 128, 64, 778, 1582, 64,
	 128, 64,},
	{1, 1, 402, 1847, 259, 59, 259, 671, 1794, 68, 54, 68, 608, 1863, 93,
	 167, 93,},
	{2, 1, 415, 1874, 64, 128, 64, 792, 1656, 192, 384, 192, 778, 1582, 64,
	 128, 64,},
	{3, 1, 302, 1841, 129, 258, 129, 658, 1720, 205, 410, 205, 754, 1760,
	 170, 340, 170,},
	{20, 1, 360, 1884, 242, 1734, 242, 752, 1720, 205, 1845, 205, 767, 1760,
	 256, 185, 256,},
	{21, 1, 360, 1884, 149, 1874, 149, 752, 1720, 205, 1883, 205, 767, 1760,
	 256, 273, 256,},
	{22, 1, 360, 1884, 98, 1948, 98, 752, 1720, 205, 1924, 205, 767, 1760,
	 256, 352, 256,},
	{23, 1, 350, 1884, 116, 1966, 116, 752, 1720, 205, 2008, 205, 767, 1760,
	 128, 233, 128,},
	{24, 1, 325, 1884, 32, 40, 32, 756, 1720, 256, 471, 256, 766, 1760, 256,
	 1881, 256,},
	{25, 1, 299, 1884, 51, 64, 51, 736, 1720, 256, 471, 256, 765, 1760, 256,
	 1881, 256,},
	{26, 1, 277, 1943, 39, 117, 88, 637, 1838, 64, 192, 144, 614, 1864, 128,
	 384, 288,},
	{27, 1, 245, 1943, 49, 147, 110, 626, 1838, 256, 768, 576, 613, 1864,
	 128, 384, 288,},
	{30, 1, 302, 1841, 61, 122, 61, 658, 1720, 205, 410, 205, 754, 1760,
	 170, 340, 170,},
};

#define LCNPHY_NUM_TX_DIG_FILTERS_OFDM 3
uint16
    LCNPHY_txdigfiltcoeffs_ofdm[LCNPHY_NUM_TX_DIG_FILTERS_OFDM]
    [LCNPHY_NUM_DIG_FILT_COEFFS + 1] = {
	{0, 0, 0xa2, 0x0, 0x100, 0x100, 0x0, 0x0, 0x0, 0x100, 0x0, 0x0,
	 0x278, 0xfea0, 0x80, 0x100, 0x80,},
	{1, 0, 374, 0xFF79, 16, 32, 16, 799, 0xFE74, 50, 32, 50,
	 750, 0xFE2B, 212, 0xFFCE, 212,},
	{2, 0, 375, 0xFF16, 37, 76, 37, 799, 0xFE74, 32, 20, 32, 748,
	 0xFEF2, 128, 0xFFE2, 128}
};

#define wlc_lcnphy_set_start_tx_pwr_idx(pi, idx) \
	mod_phy_reg(pi, 0x4a4, \
		(0x1ff << 0), \
		(uint16)(idx) << 0)

#define wlc_lcnphy_set_tx_pwr_npt(pi, npt) \
	mod_phy_reg(pi, 0x4a5, \
		(0x7 << 8), \
		(uint16)(npt) << 8)

#define wlc_lcnphy_get_tx_pwr_ctrl(pi) \
	(read_phy_reg((pi), 0x4a4) & \
			((0x1 << 15) | \
			(0x1 << 14) | \
			(0x1 << 13)))

#define wlc_lcnphy_get_tx_pwr_npt(pi) \
	((read_phy_reg(pi, 0x4a5) & \
		(0x7 << 8)) >> \
		8)

#define wlc_lcnphy_get_current_tx_pwr_idx_if_pwrctrl_on(pi) \
	(read_phy_reg(pi, 0x473) & 0x1ff)

#define wlc_lcnphy_get_target_tx_pwr(pi) \
	((read_phy_reg(pi, 0x4a7) & \
		(0xff << 0)) >> \
		0)

#define wlc_lcnphy_set_target_tx_pwr(pi, target) \
	mod_phy_reg(pi, 0x4a7, \
		(0xff << 0), \
		(uint16)(target) << 0)

#define wlc_radio_2064_rcal_done(pi) (0 != (read_radio_reg(pi, RADIO_2064_REG05C) & 0x20))
#define tempsense_done(pi) (0x8000 == (read_phy_reg(pi, 0x476) & 0x8000))

#define LCNPHY_IQLOCC_READ(val) ((uint8)(-(int8)(((val) & 0xf0) >> 4) + (int8)((val) & 0x0f)))
#define FIXED_TXPWR 78
#define LCNPHY_TEMPSENSE(val) ((int16)((val > 255) ? (val - 512) : val))

static uint32 wlc_lcnphy_qdiv_roundup(uint32 divident, uint32 divisor,
				      uint8 precision);
static void wlc_lcnphy_set_rx_gain_by_distribution(phy_info_t *pi,
						   uint16 ext_lna, uint16 trsw,
						   uint16 biq2, uint16 biq1,
						   uint16 tia, uint16 lna2,
						   uint16 lna1);
static void wlc_lcnphy_clear_tx_power_offsets(phy_info_t *pi);
static void wlc_lcnphy_set_pa_gain(phy_info_t *pi, uint16 gain);
static void wlc_lcnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx);
static void wlc_lcnphy_set_bbmult(phy_info_t *pi, uint8 m0);
static uint8 wlc_lcnphy_get_bbmult(phy_info_t *pi);
static void wlc_lcnphy_get_tx_gain(phy_info_t *pi, lcnphy_txgains_t *gains);
static void wlc_lcnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable);
static void wlc_lcnphy_toggle_afe_pwdn(phy_info_t *pi);
static void wlc_lcnphy_rx_gain_override_enable(phy_info_t *pi, bool enable);
static void wlc_lcnphy_set_tx_gain(phy_info_t *pi,
				   lcnphy_txgains_t *target_gains);
static bool wlc_lcnphy_rx_iq_est(phy_info_t *pi, uint16 num_samps,
				 uint8 wait_time, lcnphy_iq_est_t *iq_est);
static bool wlc_lcnphy_calc_rx_iq_comp(phy_info_t *pi, uint16 num_samps);
static uint16 wlc_lcnphy_get_pa_gain(phy_info_t *pi);
static void wlc_lcnphy_afe_clk_init(phy_info_t *pi, uint8 mode);
extern void wlc_lcnphy_tx_pwr_ctrl_init(wlc_phy_t *ppi);
extern void wlc_lcnphy_pktengtx(wlc_phy_t *ppi, wl_pkteng_t *pkteng,
				uint8 rate, struct ether_addr *sa,
				uint32 wait_delay);
static void wlc_lcnphy_radio_2064_channel_tune_4313(phy_info_t *pi,
						    uint8 channel);

static void wlc_lcnphy_load_tx_gain_table(phy_info_t *pi,
					  const lcnphy_tx_gain_tbl_entry *g);

static void wlc_lcnphy_samp_cap(phy_info_t *pi, int clip_detect_algo,
				uint16 thresh, int16 *ptr, int mode);
static int wlc_lcnphy_calc_floor(int16 coeff, int type);
static void wlc_lcnphy_tx_iqlo_loopback(phy_info_t *pi,
					uint16 *values_to_save);
static void wlc_lcnphy_tx_iqlo_loopback_cleanup(phy_info_t *pi,
						uint16 *values_to_save);
static void wlc_lcnphy_set_cc(phy_info_t *pi, int cal_type, int16 coeff_x,
			      int16 coeff_y);
static lcnphy_unsign16_struct wlc_lcnphy_get_cc(phy_info_t *pi, int cal_type);
static void wlc_lcnphy_a1(phy_info_t *pi, int cal_type,
			  int num_levels, int step_size_lg2);
static void wlc_lcnphy_tx_iqlo_soft_cal_full(phy_info_t *pi);

static void wlc_lcnphy_set_chanspec_tweaks(phy_info_t *pi,
					   chanspec_t chanspec);
static void wlc_lcnphy_agc_temp_init(phy_info_t *pi);
static void wlc_lcnphy_temp_adj(phy_info_t *pi);
static void wlc_lcnphy_clear_papd_comptable(phy_info_t *pi);
static void wlc_lcnphy_baseband_init(phy_info_t *pi);
static void wlc_lcnphy_radio_init(phy_info_t *pi);
static void wlc_lcnphy_rc_cal(phy_info_t *pi);
static void wlc_lcnphy_rcal(phy_info_t *pi);
static void wlc_lcnphy_txrx_spur_avoidance_mode(phy_info_t *pi, bool enable);
static int wlc_lcnphy_load_tx_iir_filter(phy_info_t *pi, bool is_ofdm,
					 int16 filt_type);
static void wlc_lcnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a, uint16 b);

void wlc_lcnphy_write_table(phy_info_t *pi, const phytbl_info_t *pti)
{
	wlc_phy_write_table(pi, pti, 0x455, 0x457, 0x456);
}

void wlc_lcnphy_read_table(phy_info_t *pi, phytbl_info_t *pti)
{
	wlc_phy_read_table(pi, pti, 0x455, 0x457, 0x456);
}

static void
wlc_lcnphy_common_read_table(phy_info_t *pi, uint32 tbl_id,
			     CONST void *tbl_ptr, uint32 tbl_len,
			     uint32 tbl_width, uint32 tbl_offset)
{
	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;
	tab.tbl_len = tbl_len;
	tab.tbl_width = tbl_width;
	tab.tbl_offset = tbl_offset;
	wlc_lcnphy_read_table(pi, &tab);
}

static void
wlc_lcnphy_common_write_table(phy_info_t *pi, uint32 tbl_id,
			      CONST void *tbl_ptr, uint32 tbl_len,
			      uint32 tbl_width, uint32 tbl_offset)
{

	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;
	tab.tbl_len = tbl_len;
	tab.tbl_width = tbl_width;
	tab.tbl_offset = tbl_offset;
	wlc_lcnphy_write_table(pi, &tab);
}

static uint32
wlc_lcnphy_qdiv_roundup(uint32 dividend, uint32 divisor, uint8 precision)
{
	uint32 quotient, remainder, roundup, rbit;

	ASSERT(divisor);

	quotient = dividend / divisor;
	remainder = dividend % divisor;
	rbit = divisor & 1;
	roundup = (divisor >> 1) + rbit;

	while (precision--) {
		quotient <<= 1;
		if (remainder >= roundup) {
			quotient++;
			remainder = ((remainder - roundup) << 1) + rbit;
		} else {
			remainder <<= 1;
		}
	}

	if (remainder >= roundup)
		quotient++;

	return quotient;
}

static int wlc_lcnphy_calc_floor(int16 coeff_x, int type)
{
	int k;
	k = 0;
	if (type == 0) {
		if (coeff_x < 0) {
			k = (coeff_x - 1) / 2;
		} else {
			k = coeff_x / 2;
		}
	}
	if (type == 1) {
		if ((coeff_x + 1) < 0)
			k = (coeff_x) / 2;
		else
			k = (coeff_x + 1) / 2;
	}
	return k;
}

int8 wlc_lcnphy_get_current_tx_pwr_idx(phy_info_t *pi)
{
	int8 index;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (txpwrctrl_off(pi))
		index = pi_lcn->lcnphy_current_index;
	else if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		index =
		    (int8) (wlc_lcnphy_get_current_tx_pwr_idx_if_pwrctrl_on(pi)
			    / 2);
	else
		index = pi_lcn->lcnphy_current_index;
	return index;
}

static uint32 wlc_lcnphy_measure_digital_power(phy_info_t *pi, uint16 nsamples)
{
	lcnphy_iq_est_t iq_est = { 0, 0, 0 };

	if (!wlc_lcnphy_rx_iq_est(pi, nsamples, 32, &iq_est))
		return 0;
	return (iq_est.i_pwr + iq_est.q_pwr) / nsamples;
}

void wlc_lcnphy_crsuprs(phy_info_t *pi, int channel)
{
	uint16 afectrlovr, afectrlovrval;
	afectrlovr = read_phy_reg(pi, 0x43b);
	afectrlovrval = read_phy_reg(pi, 0x43c);
	if (channel != 0) {
		mod_phy_reg(pi, 0x43b, (0x1 << 1), (1) << 1);

		mod_phy_reg(pi, 0x43c, (0x1 << 1), (0) << 1);

		mod_phy_reg(pi, 0x43b, (0x1 << 4), (1) << 4);

		mod_phy_reg(pi, 0x43c, (0x1 << 6), (0) << 6);

		write_phy_reg(pi, 0x44b, 0xffff);
		wlc_lcnphy_tx_pu(pi, 1);

		mod_phy_reg(pi, 0x634, (0xff << 8), (0) << 8);

		or_phy_reg(pi, 0x6da, 0x0080);

		or_phy_reg(pi, 0x00a, 0x228);
	} else {
		and_phy_reg(pi, 0x00a, ~(0x228));

		and_phy_reg(pi, 0x6da, 0xFF7F);
		write_phy_reg(pi, 0x43b, afectrlovr);
		write_phy_reg(pi, 0x43c, afectrlovrval);
	}
}

static void wlc_lcnphy_toggle_afe_pwdn(phy_info_t *pi)
{
	uint16 save_AfeCtrlOvrVal, save_AfeCtrlOvr;

	save_AfeCtrlOvrVal = read_phy_reg(pi, 0x43c);
	save_AfeCtrlOvr = read_phy_reg(pi, 0x43b);

	write_phy_reg(pi, 0x43c, save_AfeCtrlOvrVal | 0x1);
	write_phy_reg(pi, 0x43b, save_AfeCtrlOvr | 0x1);

	write_phy_reg(pi, 0x43c, save_AfeCtrlOvrVal & 0xfffe);
	write_phy_reg(pi, 0x43b, save_AfeCtrlOvr & 0xfffe);

	write_phy_reg(pi, 0x43c, save_AfeCtrlOvrVal);
	write_phy_reg(pi, 0x43b, save_AfeCtrlOvr);
}

static void wlc_lcnphy_txrx_spur_avoidance_mode(phy_info_t *pi, bool enable)
{
	if (enable) {
		write_phy_reg(pi, 0x942, 0x7);
		write_phy_reg(pi, 0x93b, ((1 << 13) + 23));
		write_phy_reg(pi, 0x93c, ((1 << 13) + 1989));

		write_phy_reg(pi, 0x44a, 0x084);
		write_phy_reg(pi, 0x44a, 0x080);
		write_phy_reg(pi, 0x6d3, 0x2222);
		write_phy_reg(pi, 0x6d3, 0x2220);
	} else {
		write_phy_reg(pi, 0x942, 0x0);
		write_phy_reg(pi, 0x93b, ((0 << 13) + 23));
		write_phy_reg(pi, 0x93c, ((0 << 13) + 1989));
	}
	wlapi_switch_macfreq(pi->sh->physhim, enable);
}

void wlc_phy_chanspec_set_lcnphy(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec);

	wlc_phy_chanspec_radio_set((wlc_phy_t *) pi, chanspec);

	wlc_lcnphy_set_chanspec_tweaks(pi, pi->radio_chanspec);

	or_phy_reg(pi, 0x44a, 0x44);
	write_phy_reg(pi, 0x44a, 0x80);

	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_lcnphy_radio_2064_channel_tune_4313(pi, channel);
		OSL_DELAY(1000);
	}

	wlc_lcnphy_toggle_afe_pwdn(pi);

	write_phy_reg(pi, 0x657, lcnphy_sfo_cfg[channel - 1].ptcentreTs20);
	write_phy_reg(pi, 0x658, lcnphy_sfo_cfg[channel - 1].ptcentreFactor);

	if (CHSPEC_CHANNEL(pi->radio_chanspec) == 14) {
		mod_phy_reg(pi, 0x448, (0x3 << 8), (2) << 8);

		wlc_lcnphy_load_tx_iir_filter(pi, FALSE, 3);
	} else {
		mod_phy_reg(pi, 0x448, (0x3 << 8), (1) << 8);

		wlc_lcnphy_load_tx_iir_filter(pi, FALSE, 2);
	}

	wlc_lcnphy_load_tx_iir_filter(pi, TRUE, 0);

	mod_phy_reg(pi, 0x4eb, (0x7 << 3), (1) << 3);

}

static void wlc_lcnphy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl = (read_phy_reg(pi, 0x439) >> 0);
	dac_ctrl = dac_ctrl & 0xc7f;
	dac_ctrl = dac_ctrl | (dac_gain << 7);
	mod_phy_reg(pi, 0x439, (0xfff << 0), (dac_ctrl) << 0);

}

static void wlc_lcnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	mod_phy_reg(pi, 0x4b0, (0x1 << 7), bit << 7);

	mod_phy_reg(pi, 0x4b0, (0x1 << 14), bit << 14);

	mod_phy_reg(pi, 0x43b, (0x1 << 6), bit << 6);
}

static uint16 wlc_lcnphy_get_pa_gain(phy_info_t *pi)
{
	uint16 pa_gain;

	pa_gain = (read_phy_reg(pi, 0x4fb) &
		   LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK) >>
	    LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT;

	return pa_gain;
}

static void
wlc_lcnphy_set_tx_gain(phy_info_t *pi, lcnphy_txgains_t *target_gains)
{
	uint16 pa_gain = wlc_lcnphy_get_pa_gain(pi);

	mod_phy_reg(pi, 0x4b5,
		    (0xffff << 0),
		    ((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		    0);
	mod_phy_reg(pi, 0x4fb,
		    (0x7fff << 0),
		    ((target_gains->pad_gain) | (pa_gain << 8)) << 0);

	mod_phy_reg(pi, 0x4fc,
		    (0xffff << 0),
		    ((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		    0);
	mod_phy_reg(pi, 0x4fd,
		    (0x7fff << 0),
		    ((target_gains->pad_gain) | (pa_gain << 8)) << 0);

	wlc_lcnphy_set_dac_gain(pi, target_gains->dac_gain);

	wlc_lcnphy_enable_tx_gain_override(pi);
}

static void wlc_lcnphy_set_bbmult(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16) m0 << 8;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1;
	tab.tbl_len = 1;
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;
	tab.tbl_offset = 87;
	tab.tbl_width = 16;
	wlc_lcnphy_write_table(pi, &tab);
}

static void wlc_lcnphy_clear_tx_power_offsets(phy_info_t *pi)
{
	uint32 data_buf[64];
	phytbl_info_t tab;

	bzero(data_buf, sizeof(data_buf));

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_ptr = data_buf;

	if (!wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {

		tab.tbl_len = 30;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
		wlc_lcnphy_write_table(pi, &tab);
	}

	tab.tbl_len = 64;
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_lcnphy_write_table(pi, &tab);
}

typedef enum {
	LCNPHY_TSSI_PRE_PA,
	LCNPHY_TSSI_POST_PA,
	LCNPHY_TSSI_EXT
} lcnphy_tssi_mode_t;

static void wlc_lcnphy_set_tssi_mux(phy_info_t *pi, lcnphy_tssi_mode_t pos)
{
	mod_phy_reg(pi, 0x4d7, (0x1 << 0), (0x1) << 0);

	mod_phy_reg(pi, 0x4d7, (0x1 << 6), (1) << 6);

	if (LCNPHY_TSSI_POST_PA == pos) {
		mod_phy_reg(pi, 0x4d9, (0x1 << 2), (0) << 2);

		mod_phy_reg(pi, 0x4d9, (0x1 << 3), (1) << 3);

		if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
			mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
		} else {
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0x1);
			mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 0x8);
		}
	} else {
		mod_phy_reg(pi, 0x4d9, (0x1 << 2), (0x1) << 2);

		mod_phy_reg(pi, 0x4d9, (0x1 << 3), (0) << 3);

		if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
			mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
		} else {
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0);
			mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 0x8);
		}
	}
	mod_phy_reg(pi, 0x637, (0x3 << 14), (0) << 14);

	if (LCNPHY_TSSI_EXT == pos) {
		write_radio_reg(pi, RADIO_2064_REG07F, 1);
		mod_radio_reg(pi, RADIO_2064_REG005, 0x7, 0x2);
		mod_radio_reg(pi, RADIO_2064_REG112, 0x80, 0x1 << 7);
		mod_radio_reg(pi, RADIO_2064_REG028, 0x1f, 0x3);
	}
}

static uint16 wlc_lcnphy_rfseq_tbl_adc_pwrup(phy_info_t *pi)
{
	uint16 N1, N2, N3, N4, N5, N6, N;
	N1 = ((read_phy_reg(pi, 0x4a5) & (0xff << 0))
	      >> 0);
	N2 = 1 << ((read_phy_reg(pi, 0x4a5) & (0x7 << 12))
		   >> 12);
	N3 = ((read_phy_reg(pi, 0x40d) & (0xff << 0))
	      >> 0);
	N4 = 1 << ((read_phy_reg(pi, 0x40d) & (0x7 << 8))
		   >> 8);
	N5 = ((read_phy_reg(pi, 0x4a2) & (0xff << 0))
	      >> 0);
	N6 = 1 << ((read_phy_reg(pi, 0x4a2) & (0x7 << 8))
		   >> 8);
	N = 2 * (N1 + N2 + N3 + N4 + 2 * (N5 + N6)) + 80;
	if (N < 1600)
		N = 1600;
	return N;
}

static void wlc_lcnphy_pwrctrl_rssiparams(phy_info_t *pi)
{
	uint16 auxpga_vmid, auxpga_vmid_temp, auxpga_gain_temp;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	auxpga_vmid =
	    (2 << 8) | (pi_lcn->lcnphy_rssi_vc << 4) | pi_lcn->lcnphy_rssi_vf;
	auxpga_vmid_temp = (2 << 8) | (8 << 4) | 4;
	auxpga_gain_temp = 2;

	mod_phy_reg(pi, 0x4d8, (0x1 << 0), (0) << 0);

	mod_phy_reg(pi, 0x4d8, (0x1 << 1), (0) << 1);

	mod_phy_reg(pi, 0x4d7, (0x1 << 3), (0) << 3);

	mod_phy_reg(pi, 0x4db,
		    (0x3ff << 0) |
		    (0x7 << 12),
		    (auxpga_vmid << 0) | (pi_lcn->lcnphy_rssi_gs << 12));

	mod_phy_reg(pi, 0x4dc,
		    (0x3ff << 0) |
		    (0x7 << 12),
		    (auxpga_vmid << 0) | (pi_lcn->lcnphy_rssi_gs << 12));

	mod_phy_reg(pi, 0x40a,
		    (0x3ff << 0) |
		    (0x7 << 12),
		    (auxpga_vmid << 0) | (pi_lcn->lcnphy_rssi_gs << 12));

	mod_phy_reg(pi, 0x40b,
		    (0x3ff << 0) |
		    (0x7 << 12),
		    (auxpga_vmid_temp << 0) | (auxpga_gain_temp << 12));

	mod_phy_reg(pi, 0x40c,
		    (0x3ff << 0) |
		    (0x7 << 12),
		    (auxpga_vmid_temp << 0) | (auxpga_gain_temp << 12));

	mod_radio_reg(pi, RADIO_2064_REG082, (1 << 5), (1 << 5));
}

static void wlc_lcnphy_tssi_setup(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 rfseq, ind;

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_ptr = &ind;
	tab.tbl_len = 1;
	tab.tbl_offset = 0;
	for (ind = 0; ind < 128; ind++) {
		wlc_lcnphy_write_table(pi, &tab);
		tab.tbl_offset++;
	}
	tab.tbl_offset = 704;
	for (ind = 0; ind < 128; ind++) {
		wlc_lcnphy_write_table(pi, &tab);
		tab.tbl_offset++;
	}
	mod_phy_reg(pi, 0x503, (0x1 << 0), (0) << 0);

	mod_phy_reg(pi, 0x503, (0x1 << 2), (0) << 2);

	mod_phy_reg(pi, 0x503, (0x1 << 4), (1) << 4);

	wlc_lcnphy_set_tssi_mux(pi, LCNPHY_TSSI_EXT);
	mod_phy_reg(pi, 0x4a4, (0x1 << 14), (0) << 14);

	mod_phy_reg(pi, 0x4a4, (0x1 << 15), (1) << 15);

	mod_phy_reg(pi, 0x4d0, (0x1 << 5), (0) << 5);

	mod_phy_reg(pi, 0x4a4, (0x1ff << 0), (0) << 0);

	mod_phy_reg(pi, 0x4a5, (0xff << 0), (255) << 0);

	mod_phy_reg(pi, 0x4a5, (0x7 << 12), (5) << 12);

	mod_phy_reg(pi, 0x4a5, (0x7 << 8), (0) << 8);

	mod_phy_reg(pi, 0x40d, (0xff << 0), (64) << 0);

	mod_phy_reg(pi, 0x40d, (0x7 << 8), (4) << 8);

	mod_phy_reg(pi, 0x4a2, (0xff << 0), (64) << 0);

	mod_phy_reg(pi, 0x4a2, (0x7 << 8), (4) << 8);

	mod_phy_reg(pi, 0x4d0, (0x1ff << 6), (0) << 6);

	mod_phy_reg(pi, 0x4a8, (0xff << 0), (0x1) << 0);

	wlc_lcnphy_clear_tx_power_offsets(pi);

	mod_phy_reg(pi, 0x4a6, (0x1 << 15), (1) << 15);

	mod_phy_reg(pi, 0x4a6, (0x1ff << 0), (0xff) << 0);

	mod_phy_reg(pi, 0x49a, (0x1ff << 0), (0xff) << 0);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		mod_radio_reg(pi, RADIO_2064_REG028, 0xf, 0xe);
		mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
	} else {
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x1, 1);
		mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 1 << 3);
	}

	write_radio_reg(pi, RADIO_2064_REG025, 0xc);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x1, 1);
	} else {
		if (CHSPEC_IS2G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 1 << 1);
		else
			mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 0 << 1);
	}

	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 1 << 1);
	else
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x4, 1 << 2);

	mod_radio_reg(pi, RADIO_2064_REG11A, 0x1, 1 << 0);

	mod_radio_reg(pi, RADIO_2064_REG005, 0x8, 1 << 3);

	if (!wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
		mod_phy_reg(pi, 0x4d7,
			    (0x1 << 3) | (0x7 << 12), 0 << 3 | 2 << 12);
	}

	rfseq = wlc_lcnphy_rfseq_tbl_adc_pwrup(pi);
	tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;
	tab.tbl_ptr = &rfseq;
	tab.tbl_len = 1;
	tab.tbl_offset = 6;
	wlc_lcnphy_write_table(pi, &tab);

	mod_phy_reg(pi, 0x938, (0x1 << 2), (1) << 2);

	mod_phy_reg(pi, 0x939, (0x1 << 2), (1) << 2);

	mod_phy_reg(pi, 0x4a4, (0x1 << 12), (1) << 12);

	mod_phy_reg(pi, 0x4d7, (0x1 << 2), (1) << 2);

	mod_phy_reg(pi, 0x4d7, (0xf << 8), (0) << 8);

	wlc_lcnphy_pwrctrl_rssiparams(pi);
}

void wlc_lcnphy_tx_pwr_update_npt(phy_info_t *pi)
{
	uint16 tx_cnt, tx_total, npt;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	tx_total = wlc_lcnphy_total_tx_frames(pi);
	tx_cnt = tx_total - pi_lcn->lcnphy_tssi_tx_cnt;
	npt = wlc_lcnphy_get_tx_pwr_npt(pi);

	if (tx_cnt > (1 << npt)) {

		pi_lcn->lcnphy_tssi_tx_cnt = tx_total;

		pi_lcn->lcnphy_tssi_idx = wlc_lcnphy_get_current_tx_pwr_idx(pi);
		pi_lcn->lcnphy_tssi_npt = npt;

	}
}

int32 wlc_lcnphy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;

	a = 32768 + (a1 * tssi);
	b = (1024 * b0) + (64 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	return p;
}

static void wlc_lcnphy_txpower_reset_npt(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return;

	pi_lcn->lcnphy_tssi_idx = LCNPHY_TX_PWR_CTRL_START_INDEX_2G_4313;
	pi_lcn->lcnphy_tssi_npt = LCNPHY_TX_PWR_CTRL_START_NPT;
}

void wlc_lcnphy_txpower_recalc_target(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 rate_table[WLC_NUM_RATES_CCK + WLC_NUM_RATES_OFDM +
			  WLC_NUM_RATES_MCS_1_STREAM];
	uint i, j;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return;

	for (i = 0, j = 0; i < ARRAYSIZE(rate_table); i++, j++) {

		if (i == WLC_NUM_RATES_CCK + WLC_NUM_RATES_OFDM)
			j = TXP_FIRST_MCS_20_SISO;

		rate_table[i] = (uint32) ((int32) (-pi->tx_power_offset[j]));
	}

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_len = ARRAYSIZE(rate_table);
	tab.tbl_ptr = rate_table;
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_lcnphy_write_table(pi, &tab);

	if (wlc_lcnphy_get_target_tx_pwr(pi) != pi->tx_power_min) {
		wlc_lcnphy_set_target_tx_pwr(pi, pi->tx_power_min);

		wlc_lcnphy_txpower_reset_npt(pi);
	}
}

static void wlc_lcnphy_set_tx_pwr_soft_ctrl(phy_info_t *pi, int8 index)
{
	uint32 cck_offset[4] = { 22, 22, 22, 22 };
	uint32 ofdm_offset, reg_offset_cck;
	int i;
	uint16 index2;
	phytbl_info_t tab;

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		return;

	mod_phy_reg(pi, 0x4a4, (0x1 << 14), (0x1) << 14);

	mod_phy_reg(pi, 0x4a4, (0x1 << 14), (0x0) << 14);

	or_phy_reg(pi, 0x6da, 0x0040);

	reg_offset_cck = 0;
	for (i = 0; i < 4; i++)
		cck_offset[i] -= reg_offset_cck;
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_len = 4;
	tab.tbl_ptr = cck_offset;
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_lcnphy_write_table(pi, &tab);
	ofdm_offset = 0;
	tab.tbl_len = 1;
	tab.tbl_ptr = &ofdm_offset;
	for (i = 836; i < 862; i++) {
		tab.tbl_offset = i;
		wlc_lcnphy_write_table(pi, &tab);
	}

	mod_phy_reg(pi, 0x4a4, (0x1 << 15), (0x1) << 15);

	mod_phy_reg(pi, 0x4a4, (0x1 << 14), (0x1) << 14);

	mod_phy_reg(pi, 0x4a4, (0x1 << 13), (0x1) << 13);

	mod_phy_reg(pi, 0x4b0, (0x1 << 7), (0) << 7);

	mod_phy_reg(pi, 0x43b, (0x1 << 6), (0) << 6);

	mod_phy_reg(pi, 0x4a9, (0x1 << 15), (1) << 15);

	index2 = (uint16) (index * 2);
	mod_phy_reg(pi, 0x4a9, (0x1ff << 0), (index2) << 0);

	mod_phy_reg(pi, 0x6a3, (0x1 << 4), (0) << 4);

}

static int8 wlc_lcnphy_tempcompensated_txpwrctrl(phy_info_t *pi)
{
	int8 index, delta_brd, delta_temp, new_index, tempcorrx;
	int16 manp, meas_temp, temp_diff;
	bool neg = 0;
	uint16 temp;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		return pi_lcn->lcnphy_current_index;

	index = FIXED_TXPWR;

	if (NORADIO_ENAB(pi->pubpi))
		return index;

	if (pi_lcn->lcnphy_tempsense_slope == 0) {
		return index;
	}
	temp = (uint16) wlc_lcnphy_tempsense(pi, 0);
	meas_temp = LCNPHY_TEMPSENSE(temp);

	if (pi->tx_power_min != 0) {
		delta_brd = (pi_lcn->lcnphy_measPower - pi->tx_power_min);
	} else {
		delta_brd = 0;
	}

	manp = LCNPHY_TEMPSENSE(pi_lcn->lcnphy_rawtempsense);
	temp_diff = manp - meas_temp;
	if (temp_diff < 0) {

		neg = 1;

		temp_diff = -temp_diff;
	}

	delta_temp = (int8) wlc_lcnphy_qdiv_roundup((uint32) (temp_diff * 192),
						    (uint32) (pi_lcn->
							      lcnphy_tempsense_slope
							      * 10), 0);
	if (neg)
		delta_temp = -delta_temp;

	if (pi_lcn->lcnphy_tempsense_option == 3
	    && LCNREV_IS(pi->pubpi.phy_rev, 0))
		delta_temp = 0;
	if (pi_lcn->lcnphy_tempcorrx > 31)
		tempcorrx = (int8) (pi_lcn->lcnphy_tempcorrx - 64);
	else
		tempcorrx = (int8) pi_lcn->lcnphy_tempcorrx;
	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		tempcorrx = 4;
	new_index =
	    index + delta_brd + delta_temp - pi_lcn->lcnphy_bandedge_corr;
	new_index += tempcorrx;

	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		index = 127;
	if (new_index < 0 || new_index > 126) {
		return index;
	}
	return new_index;
}

static uint16 wlc_lcnphy_set_tx_pwr_ctrl_mode(phy_info_t *pi, uint16 mode)
{

	uint16 current_mode = mode;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) &&
	    mode == LCNPHY_TX_PWR_CTRL_HW)
		current_mode = LCNPHY_TX_PWR_CTRL_TEMPBASED;
	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) &&
	    mode == LCNPHY_TX_PWR_CTRL_TEMPBASED)
		current_mode = LCNPHY_TX_PWR_CTRL_HW;
	return current_mode;
}

void wlc_lcnphy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode)
{
	uint16 old_mode = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	int8 index;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ASSERT((LCNPHY_TX_PWR_CTRL_OFF == mode) ||
	       (LCNPHY_TX_PWR_CTRL_SW == mode) ||
	       (LCNPHY_TX_PWR_CTRL_HW == mode) ||
	       (LCNPHY_TX_PWR_CTRL_TEMPBASED == mode));

	mode = wlc_lcnphy_set_tx_pwr_ctrl_mode(pi, mode);
	old_mode = wlc_lcnphy_set_tx_pwr_ctrl_mode(pi, old_mode);

	mod_phy_reg(pi, 0x6da, (0x1 << 6),
		    ((LCNPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0) << 6);

	mod_phy_reg(pi, 0x6a3, (0x1 << 4),
		    ((LCNPHY_TX_PWR_CTRL_HW == mode) ? 0 : 1) << 4);

	if (old_mode != mode) {
		if (LCNPHY_TX_PWR_CTRL_HW == old_mode) {

			wlc_lcnphy_tx_pwr_update_npt(pi);

			wlc_lcnphy_clear_tx_power_offsets(pi);
		}
		if (LCNPHY_TX_PWR_CTRL_HW == mode) {

			wlc_lcnphy_txpower_recalc_target(pi);

			wlc_lcnphy_set_start_tx_pwr_idx(pi,
							pi_lcn->
							lcnphy_tssi_idx);
			wlc_lcnphy_set_tx_pwr_npt(pi, pi_lcn->lcnphy_tssi_npt);
			mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 0);

			pi_lcn->lcnphy_tssi_tx_cnt =
			    wlc_lcnphy_total_tx_frames(pi);

			wlc_lcnphy_disable_tx_gain_override(pi);
			pi_lcn->lcnphy_tx_power_idx_override = -1;
		} else
			wlc_lcnphy_enable_tx_gain_override(pi);

		mod_phy_reg(pi, 0x4a4,
			    ((0x1 << 15) | (0x1 << 14) | (0x1 << 13)), mode);
		if (mode == LCNPHY_TX_PWR_CTRL_TEMPBASED) {
			index = wlc_lcnphy_tempcompensated_txpwrctrl(pi);
			wlc_lcnphy_set_tx_pwr_soft_ctrl(pi, index);
			pi_lcn->lcnphy_current_index = (int8)
			    ((read_phy_reg(pi, 0x4a9) & 0xFF) / 2);
		}
	}
}

static bool wlc_lcnphy_iqcal_wait(phy_info_t *pi)
{
	uint delay_count = 0;

	while (wlc_lcnphy_iqcal_active(pi)) {
		OSL_DELAY(100);
		delay_count++;

		if (delay_count > (10 * 500))
			break;
	}

	return (0 == wlc_lcnphy_iqcal_active(pi));
}

static void
wlc_lcnphy_tx_iqlo_cal(phy_info_t *pi,
		       lcnphy_txgains_t *target_gains,
		       lcnphy_cal_mode_t cal_mode, bool keep_tone)
{

	lcnphy_txgains_t cal_gains, temp_gains;
	uint16 hash;
	uint8 band_idx;
	int j;
	uint16 ncorr_override[5];
	uint16 syst_coeffs[] = { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000
	};

	uint16 commands_fullcal[] = {
		0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };

	uint16 commands_recal[] = {
		0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };

	uint16 command_nums_fullcal[] = {
		0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };

	uint16 command_nums_recal[] = {
		0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };
	uint16 *command_nums = command_nums_fullcal;

	uint16 *start_coeffs = NULL, *cal_cmds = NULL, cal_type, diq_start;
	uint16 tx_pwr_ctrl_old, save_txpwrctrlrfctrl2;
	uint16 save_sslpnCalibClkEnCtrl, save_sslpnRxFeClkEnCtrl;
	bool tx_gain_override_old;
	lcnphy_txgains_t old_gains;
	uint i, n_cal_cmds = 0, n_cal_start = 0;
	uint16 *values_to_save;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	values_to_save = MALLOC(pi->sh->osh, sizeof(uint16) * 20);
	if (NULL == values_to_save) {
		return;
	}

	save_sslpnRxFeClkEnCtrl = read_phy_reg(pi, 0x6db);
	save_sslpnCalibClkEnCtrl = read_phy_reg(pi, 0x6da);

	or_phy_reg(pi, 0x6da, 0x40);
	or_phy_reg(pi, 0x6db, 0x3);

	switch (cal_mode) {
	case LCNPHY_CAL_FULL:
		start_coeffs = syst_coeffs;
		cal_cmds = commands_fullcal;
		n_cal_cmds = ARRAYSIZE(commands_fullcal);
		break;

	case LCNPHY_CAL_RECAL:
		ASSERT(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs_valid);

		start_coeffs = syst_coeffs;

		cal_cmds = commands_recal;
		n_cal_cmds = ARRAYSIZE(commands_recal);
		command_nums = command_nums_recal;
		break;
	default:
		ASSERT(FALSE);
	}

	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				      start_coeffs, 11, 16, 64);

	write_phy_reg(pi, 0x6da, 0xffff);
	mod_phy_reg(pi, 0x503, (0x1 << 3), (1) << 3);

	tx_pwr_ctrl_old = wlc_lcnphy_get_tx_pwr_ctrl(pi);

	mod_phy_reg(pi, 0x4a4, (0x1 << 12), (1) << 12);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	save_txpwrctrlrfctrl2 = read_phy_reg(pi, 0x4db);

	mod_phy_reg(pi, 0x4db, (0x3ff << 0), (0x2a6) << 0);

	mod_phy_reg(pi, 0x4db, (0x7 << 12), (2) << 12);

	wlc_lcnphy_tx_iqlo_loopback(pi, values_to_save);

	tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_lcnphy_get_tx_gain(pi, &old_gains);

	if (!target_gains) {
		if (!tx_gain_override_old)
			wlc_lcnphy_set_tx_pwr_by_index(pi,
						       pi_lcn->lcnphy_tssi_idx);
		wlc_lcnphy_get_tx_gain(pi, &temp_gains);
		target_gains = &temp_gains;
	}

	hash = (target_gains->gm_gain << 8) |
	    (target_gains->pga_gain << 4) | (target_gains->pad_gain);

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	cal_gains = *target_gains;
	bzero(ncorr_override, sizeof(ncorr_override));
	for (j = 0; j < iqcal_gainparams_numgains_lcnphy[band_idx]; j++) {
		if (hash == tbl_iqcal_gainparams_lcnphy[band_idx][j][0]) {
			cal_gains.gm_gain =
			    tbl_iqcal_gainparams_lcnphy[band_idx][j][1];
			cal_gains.pga_gain =
			    tbl_iqcal_gainparams_lcnphy[band_idx][j][2];
			cal_gains.pad_gain =
			    tbl_iqcal_gainparams_lcnphy[band_idx][j][3];
			bcopy(&tbl_iqcal_gainparams_lcnphy[band_idx][j][3],
			      ncorr_override, sizeof(ncorr_override));
			break;
		}
	}

	wlc_lcnphy_set_tx_gain(pi, &cal_gains);

	write_phy_reg(pi, 0x453, 0xaa9);
	write_phy_reg(pi, 0x93d, 0xc0);

	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				      (CONST void *)
				      lcnphy_iqcal_loft_gainladder,
				      ARRAYSIZE(lcnphy_iqcal_loft_gainladder),
				      16, 0);

	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				      (CONST void *)lcnphy_iqcal_ir_gainladder,
				      ARRAYSIZE(lcnphy_iqcal_ir_gainladder), 16,
				      32);

	if (pi->phy_tx_tone_freq) {

		wlc_lcnphy_stop_tx_tone(pi);
		OSL_DELAY(5);
		wlc_lcnphy_start_tx_tone(pi, 3750, 88, 1);
	} else {
		wlc_lcnphy_start_tx_tone(pi, 3750, 88, 1);
	}

	write_phy_reg(pi, 0x6da, 0xffff);

	for (i = n_cal_start; i < n_cal_cmds; i++) {
		uint16 zero_diq = 0;
		uint16 best_coeffs[11];
		uint16 command_num;

		cal_type = (cal_cmds[i] & 0x0f00) >> 8;

		command_num = command_nums[i];
		if (ncorr_override[cal_type])
			command_num =
			    ncorr_override[cal_type] << 8 | (command_num &
							     0xff);

		write_phy_reg(pi, 0x452, command_num);

		if ((cal_type == 3) || (cal_type == 4)) {

			wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
						     &diq_start, 1, 16, 69);

			wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
						      &zero_diq, 1, 16, 69);
		}

		write_phy_reg(pi, 0x451, cal_cmds[i]);

		if (!wlc_lcnphy_iqcal_wait(pi)) {

			goto cleanup;
		}

		wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
					     best_coeffs,
					     ARRAYSIZE(best_coeffs), 16, 96);
		wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
					      best_coeffs,
					      ARRAYSIZE(best_coeffs), 16, 64);

		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
						      &diq_start, 1, 16, 69);
		}
		wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
					     pi_lcn->lcnphy_cal_results.
					     txiqlocal_bestcoeffs,
					     ARRAYSIZE(pi_lcn->
						       lcnphy_cal_results.
						       txiqlocal_bestcoeffs),
					     16, 96);
	}

	wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				     pi_lcn->lcnphy_cal_results.
				     txiqlocal_bestcoeffs,
				     ARRAYSIZE(pi_lcn->lcnphy_cal_results.
					       txiqlocal_bestcoeffs), 16, 96);
	pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs_valid = TRUE;

	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				      &pi_lcn->lcnphy_cal_results.
				      txiqlocal_bestcoeffs[0], 4, 16, 80);

	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				      &pi_lcn->lcnphy_cal_results.
				      txiqlocal_bestcoeffs[5], 2, 16, 85);

 cleanup:
	wlc_lcnphy_tx_iqlo_loopback_cleanup(pi, values_to_save);
	MFREE(pi->sh->osh, values_to_save, 20 * sizeof(uint16));

	if (!keep_tone)
		wlc_lcnphy_stop_tx_tone(pi);

	write_phy_reg(pi, 0x4db, save_txpwrctrlrfctrl2);

	write_phy_reg(pi, 0x453, 0);

	if (tx_gain_override_old)
		wlc_lcnphy_set_tx_gain(pi, &old_gains);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);

	write_phy_reg(pi, 0x6da, save_sslpnCalibClkEnCtrl);
	write_phy_reg(pi, 0x6db, save_sslpnRxFeClkEnCtrl);

}

static void wlc_lcnphy_idle_tssi_est(wlc_phy_t *ppi)
{
	bool suspend, tx_gain_override_old;
	lcnphy_txgains_t old_gains;
	phy_info_t *pi = (phy_info_t *) ppi;
	uint16 idleTssi, idleTssi0_2C, idleTssi0_OB, idleTssi0_regvalue_OB,
	    idleTssi0_regvalue_2C;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	uint16 SAVE_lpfgain = read_radio_reg(pi, RADIO_2064_REG112);
	uint16 SAVE_jtag_bb_afe_switch =
	    read_radio_reg(pi, RADIO_2064_REG007) & 1;
	uint16 SAVE_jtag_auxpga = read_radio_reg(pi, RADIO_2064_REG0FF) & 0x10;
	uint16 SAVE_iqadc_aux_en = read_radio_reg(pi, RADIO_2064_REG11F) & 4;
	idleTssi = read_phy_reg(pi, 0x4ab);
	suspend =
	    (0 ==
	     (R_REG(pi->sh->osh, &((phy_info_t *) pi)->regs->maccontrol) &
	      MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
	wlc_lcnphy_get_tx_gain(pi, &old_gains);

	wlc_lcnphy_enable_tx_gain_override(pi);
	wlc_lcnphy_set_tx_pwr_by_index(pi, 127);
	write_radio_reg(pi, RADIO_2064_REG112, 0x6);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, 1);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, 1 << 4);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 1 << 2);
	wlc_lcnphy_tssi_setup(pi);
	wlc_phy_do_dummy_tx(pi, TRUE, OFF);
	idleTssi = ((read_phy_reg(pi, 0x4ab) & (0x1ff << 0))
		    >> 0);

	idleTssi0_2C = ((read_phy_reg(pi, 0x63e) & (0x1ff << 0))
			>> 0);

	if (idleTssi0_2C >= 256)
		idleTssi0_OB = idleTssi0_2C - 256;
	else
		idleTssi0_OB = idleTssi0_2C + 256;

	idleTssi0_regvalue_OB = idleTssi0_OB;
	if (idleTssi0_regvalue_OB >= 256)
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB - 256;
	else
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB + 256;
	mod_phy_reg(pi, 0x4a6, (0x1ff << 0), (idleTssi0_regvalue_2C) << 0);

	mod_phy_reg(pi, 0x44c, (0x1 << 12), (0) << 12);

	wlc_lcnphy_set_tx_gain_override(pi, tx_gain_override_old);
	wlc_lcnphy_set_tx_gain(pi, &old_gains);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);

	write_radio_reg(pi, RADIO_2064_REG112, SAVE_lpfgain);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, SAVE_jtag_bb_afe_switch);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, SAVE_jtag_auxpga);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, SAVE_iqadc_aux_en);
	mod_radio_reg(pi, RADIO_2064_REG112, 0x80, 1 << 7);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

static void wlc_lcnphy_vbat_temp_sense_setup(phy_info_t *pi, uint8 mode)
{
	bool suspend;
	uint16 save_txpwrCtrlEn;
	uint8 auxpga_vmidcourse, auxpga_vmidfine, auxpga_gain;
	uint16 auxpga_vmid;
	phytbl_info_t tab;
	uint32 val;
	uint8 save_reg007, save_reg0FF, save_reg11F, save_reg005, save_reg025,
	    save_reg112;
	uint16 values_to_save[14];
	int8 index;
	int i;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	OSL_DELAY(999);

	save_reg007 = (uint8) read_radio_reg(pi, RADIO_2064_REG007);
	save_reg0FF = (uint8) read_radio_reg(pi, RADIO_2064_REG0FF);
	save_reg11F = (uint8) read_radio_reg(pi, RADIO_2064_REG11F);
	save_reg005 = (uint8) read_radio_reg(pi, RADIO_2064_REG005);
	save_reg025 = (uint8) read_radio_reg(pi, RADIO_2064_REG025);
	save_reg112 = (uint8) read_radio_reg(pi, RADIO_2064_REG112);

	for (i = 0; i < 14; i++)
		values_to_save[i] = read_phy_reg(pi, tempsense_phy_regs[i]);
	suspend =
	    (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	save_txpwrCtrlEn = read_radio_reg(pi, 0x4a4);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
	index = pi_lcn->lcnphy_current_index;
	wlc_lcnphy_set_tx_pwr_by_index(pi, 127);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, 0x1);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, 0x1 << 4);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 0x1 << 2);
	mod_phy_reg(pi, 0x503, (0x1 << 0), (0) << 0);

	mod_phy_reg(pi, 0x503, (0x1 << 2), (0) << 2);

	mod_phy_reg(pi, 0x4a4, (0x1 << 14), (0) << 14);

	mod_phy_reg(pi, 0x4a4, (0x1 << 15), (0) << 15);

	mod_phy_reg(pi, 0x4d0, (0x1 << 5), (0) << 5);

	mod_phy_reg(pi, 0x4a5, (0xff << 0), (255) << 0);

	mod_phy_reg(pi, 0x4a5, (0x7 << 12), (5) << 12);

	mod_phy_reg(pi, 0x4a5, (0x7 << 8), (0) << 8);

	mod_phy_reg(pi, 0x40d, (0xff << 0), (64) << 0);

	mod_phy_reg(pi, 0x40d, (0x7 << 8), (6) << 8);

	mod_phy_reg(pi, 0x4a2, (0xff << 0), (64) << 0);

	mod_phy_reg(pi, 0x4a2, (0x7 << 8), (6) << 8);

	mod_phy_reg(pi, 0x4d9, (0x7 << 4), (2) << 4);

	mod_phy_reg(pi, 0x4d9, (0x7 << 8), (3) << 8);

	mod_phy_reg(pi, 0x4d9, (0x7 << 12), (1) << 12);

	mod_phy_reg(pi, 0x4da, (0x1 << 12), (0) << 12);

	mod_phy_reg(pi, 0x4da, (0x1 << 13), (1) << 13);

	mod_phy_reg(pi, 0x4a6, (0x1 << 15), (1) << 15);

	write_radio_reg(pi, RADIO_2064_REG025, 0xC);

	mod_radio_reg(pi, RADIO_2064_REG005, 0x8, 0x1 << 3);

	mod_phy_reg(pi, 0x938, (0x1 << 2), (1) << 2);

	mod_phy_reg(pi, 0x939, (0x1 << 2), (1) << 2);

	mod_phy_reg(pi, 0x4a4, (0x1 << 12), (1) << 12);

	val = wlc_lcnphy_rfseq_tbl_adc_pwrup(pi);
	tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;
	tab.tbl_len = 1;
	tab.tbl_ptr = &val;
	tab.tbl_offset = 6;
	wlc_lcnphy_write_table(pi, &tab);
	if (mode == TEMPSENSE) {
		mod_phy_reg(pi, 0x4d7, (0x1 << 3), (1) << 3);

		mod_phy_reg(pi, 0x4d7, (0x7 << 12), (1) << 12);

		auxpga_vmidcourse = 8;
		auxpga_vmidfine = 0x4;
		auxpga_gain = 2;
		mod_radio_reg(pi, RADIO_2064_REG082, 0x20, 1 << 5);
	} else {
		mod_phy_reg(pi, 0x4d7, (0x1 << 3), (1) << 3);

		mod_phy_reg(pi, 0x4d7, (0x7 << 12), (3) << 12);

		auxpga_vmidcourse = 7;
		auxpga_vmidfine = 0xa;
		auxpga_gain = 2;
	}
	auxpga_vmid =
	    (uint16) ((2 << 8) | (auxpga_vmidcourse << 4) | auxpga_vmidfine);
	mod_phy_reg(pi, 0x4d8, (0x1 << 0), (1) << 0);

	mod_phy_reg(pi, 0x4d8, (0x3ff << 2), (auxpga_vmid) << 2);

	mod_phy_reg(pi, 0x4d8, (0x1 << 1), (1) << 1);

	mod_phy_reg(pi, 0x4d8, (0x7 << 12), (auxpga_gain) << 12);

	mod_phy_reg(pi, 0x4d0, (0x1 << 5), (1) << 5);

	write_radio_reg(pi, RADIO_2064_REG112, 0x6);

	wlc_phy_do_dummy_tx(pi, TRUE, OFF);
	if (!tempsense_done(pi))
		OSL_DELAY(10);

	write_radio_reg(pi, RADIO_2064_REG007, (uint16) save_reg007);
	write_radio_reg(pi, RADIO_2064_REG0FF, (uint16) save_reg0FF);
	write_radio_reg(pi, RADIO_2064_REG11F, (uint16) save_reg11F);
	write_radio_reg(pi, RADIO_2064_REG005, (uint16) save_reg005);
	write_radio_reg(pi, RADIO_2064_REG025, (uint16) save_reg025);
	write_radio_reg(pi, RADIO_2064_REG112, (uint16) save_reg112);
	for (i = 0; i < 14; i++)
		write_phy_reg(pi, tempsense_phy_regs[i], values_to_save[i]);
	wlc_lcnphy_set_tx_pwr_by_index(pi, (int)index);

	write_radio_reg(pi, 0x4a4, save_txpwrCtrlEn);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
	OSL_DELAY(999);
}

void WLBANDINITFN(wlc_lcnphy_tx_pwr_ctrl_init) (wlc_phy_t *ppi)
{
	lcnphy_txgains_t tx_gains;
	uint8 bbmult;
	phytbl_info_t tab;
	int32 a1, b0, b1;
	int32 tssi, pwr, maxtargetpwr, mintargetpwr;
	bool suspend;
	phy_info_t *pi = (phy_info_t *) ppi;

	suspend =
	    (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (NORADIO_ENAB(pi->pubpi)) {
		wlc_lcnphy_set_bbmult(pi, 0x30);
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
		return;
	}

	if (!pi->hwpwrctrl_capable) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tx_gains.gm_gain = 4;
			tx_gains.pga_gain = 12;
			tx_gains.pad_gain = 12;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		} else {
			tx_gains.gm_gain = 7;
			tx_gains.pga_gain = 15;
			tx_gains.pad_gain = 14;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		}
		wlc_lcnphy_set_tx_gain(pi, &tx_gains);
		wlc_lcnphy_set_bbmult(pi, bbmult);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	} else {

		wlc_lcnphy_idle_tssi_est(ppi);

		wlc_lcnphy_clear_tx_power_offsets(pi);

		b0 = pi->txpa_2g[0];
		b1 = pi->txpa_2g[1];
		a1 = pi->txpa_2g[2];
		maxtargetpwr = wlc_lcnphy_tssi2dbm(10, a1, b0, b1);
		mintargetpwr = wlc_lcnphy_tssi2dbm(125, a1, b0, b1);

		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;
		tab.tbl_ptr = &pwr;
		tab.tbl_len = 1;
		tab.tbl_offset = 0;
		for (tssi = 0; tssi < 128; tssi++) {
			pwr = wlc_lcnphy_tssi2dbm(tssi, a1, b0, b1);

			pwr = (pwr < mintargetpwr) ? mintargetpwr : pwr;
			wlc_lcnphy_write_table(pi, &tab);
			tab.tbl_offset++;
		}

		mod_phy_reg(pi, 0x410, (0x1 << 7), (0) << 7);

		write_phy_reg(pi, 0x4a8, 10);

		wlc_lcnphy_set_target_tx_pwr(pi, LCN_TARGET_PWR);

		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
	}
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

static uint8 wlc_lcnphy_get_bbmult(phy_info_t *pi)
{
	uint16 m0m1;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1;
	tab.tbl_len = 1;
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;
	tab.tbl_offset = 87;
	tab.tbl_width = 16;
	wlc_lcnphy_read_table(pi, &tab);

	return (uint8) ((m0m1 & 0xff00) >> 8);
}

static void wlc_lcnphy_set_pa_gain(phy_info_t *pi, uint16 gain)
{
	mod_phy_reg(pi, 0x4fb,
		    LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK,
		    gain << LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT);
	mod_phy_reg(pi, 0x4fd,
		    LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK,
		    gain << LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT);
}

void
wlc_lcnphy_get_radio_loft(phy_info_t *pi,
			  uint8 *ei0, uint8 *eq0, uint8 *fi0, uint8 *fq0)
{
	*ei0 = LCNPHY_IQLOCC_READ(read_radio_reg(pi, RADIO_2064_REG089));
	*eq0 = LCNPHY_IQLOCC_READ(read_radio_reg(pi, RADIO_2064_REG08A));
	*fi0 = LCNPHY_IQLOCC_READ(read_radio_reg(pi, RADIO_2064_REG08B));
	*fq0 = LCNPHY_IQLOCC_READ(read_radio_reg(pi, RADIO_2064_REG08C));
}

static void wlc_lcnphy_get_tx_gain(phy_info_t *pi, lcnphy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = read_phy_reg(pi, 0x439) >> 0;
	gains->dac_gain = (dac_gain & 0x380) >> 7;

	{
		uint16 rfgain0, rfgain1;

		rfgain0 = (read_phy_reg(pi, 0x4b5) & (0xffff << 0)) >> 0;
		rfgain1 = (read_phy_reg(pi, 0x4fb) & (0x7fff << 0)) >> 0;

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

void wlc_lcnphy_set_tx_iqcc(phy_info_t *pi, uint16 a, uint16 b)
{
	phytbl_info_t tab;
	uint16 iqcc[2];

	iqcc[0] = a;
	iqcc[1] = b;

	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;
	tab.tbl_width = 16;
	tab.tbl_ptr = iqcc;
	tab.tbl_len = 2;
	tab.tbl_offset = 80;
	wlc_lcnphy_write_table(pi, &tab);
}

void wlc_lcnphy_set_tx_locc(phy_info_t *pi, uint16 didq)
{
	phytbl_info_t tab;

	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;
	tab.tbl_width = 16;
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_lcnphy_write_table(pi, &tab);
}

void wlc_lcnphy_set_tx_pwr_by_index(phy_info_t *pi, int index)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
	lcnphy_txgains_t gains;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ASSERT(index <= LCNPHY_MAX_TX_POWER_INDEX);

	pi_lcn->lcnphy_tx_power_idx_override = (int8) index;
	pi_lcn->lcnphy_current_index = (uint8) index;

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_len = 1;

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + index;
	tab.tbl_ptr = &bbmultiqcomp;
	wlc_lcnphy_read_table(pi, &tab);

	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + index;
	tab.tbl_width = 32;
	tab.tbl_ptr = &txgain;
	wlc_lcnphy_read_table(pi, &tab);

	gains.gm_gain = (uint16) (txgain & 0xff);
	gains.pga_gain = (uint16) (txgain >> 8) & 0xff;
	gains.pad_gain = (uint16) (txgain >> 16) & 0xff;
	gains.dac_gain = (uint16) (bbmultiqcomp >> 28) & 0x07;
	wlc_lcnphy_set_tx_gain(pi, &gains);
	wlc_lcnphy_set_pa_gain(pi, (uint16) (txgain >> 24) & 0x7f);

	bb_mult = (uint8) ((bbmultiqcomp >> 20) & 0xff);
	wlc_lcnphy_set_bbmult(pi, bb_mult);

	wlc_lcnphy_enable_tx_gain_override(pi);

	if (!wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {

		a = (uint16) ((bbmultiqcomp >> 10) & 0x3ff);
		b = (uint16) (bbmultiqcomp & 0x3ff);
		wlc_lcnphy_set_tx_iqcc(pi, a, b);

		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_LO_OFFSET + index;
		tab.tbl_ptr = &locoeffs;
		wlc_lcnphy_read_table(pi, &tab);

		wlc_lcnphy_set_tx_locc(pi, (uint16) locoeffs);

		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_PWR_OFFSET + index;
		tab.tbl_ptr = &rfpower;
		wlc_lcnphy_read_table(pi, &tab);
		mod_phy_reg(pi, 0x6a6, (0x1fff << 0), (rfpower * 8) << 0);

	}
}

static void wlc_lcnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx)
{

	mod_phy_reg(pi, 0x44d,
		    (0x1 << 1) |
		    (0x1 << 0), (tx ? (0x1 << 1) : 0) | (rx ? (0x1 << 0) : 0));

	or_phy_reg(pi, 0x44c, (0x1 << 1) | (0x1 << 0));
}

static void wlc_lcnphy_clear_papd_comptable(phy_info_t *pi)
{
	uint32 j;
	phytbl_info_t tab;
	uint32 temp_offset[128];
	tab.tbl_ptr = temp_offset;
	tab.tbl_len = 128;
	tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_width = 32;
	tab.tbl_offset = 0;

	bzero(temp_offset, sizeof(temp_offset));
	for (j = 1; j < 128; j += 2)
		temp_offset[j] = 0x80000;

	wlc_lcnphy_write_table(pi, &tab);
	return;
}

static void
wlc_lcnphy_set_rx_gain_by_distribution(phy_info_t *pi,
				       uint16 trsw,
				       uint16 ext_lna,
				       uint16 biq2,
				       uint16 biq1,
				       uint16 tia, uint16 lna2, uint16 lna1)
{
	uint16 gain0_15, gain16_19;

	gain16_19 = biq2 & 0xf;
	gain0_15 = ((biq1 & 0xf) << 12) |
	    ((tia & 0xf) << 8) |
	    ((lna2 & 0x3) << 6) |
	    ((lna2 & 0x3) << 4) | ((lna1 & 0x3) << 2) | ((lna1 & 0x3) << 0);

	mod_phy_reg(pi, 0x4b6, (0xffff << 0), gain0_15 << 0);
	mod_phy_reg(pi, 0x4b7, (0xf << 0), gain16_19 << 0);
	mod_phy_reg(pi, 0x4b1, (0x3 << 11), lna1 << 11);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, 0x4b1, (0x1 << 9), ext_lna << 9);
		mod_phy_reg(pi, 0x4b1, (0x1 << 10), ext_lna << 10);
	} else {
		mod_phy_reg(pi, 0x4b1, (0x1 << 10), 0 << 10);

		mod_phy_reg(pi, 0x4b1, (0x1 << 15), 0 << 15);

		mod_phy_reg(pi, 0x4b1, (0x1 << 9), ext_lna << 9);
	}

	mod_phy_reg(pi, 0x44d, (0x1 << 0), (!trsw) << 0);

}

static void wlc_lcnphy_rx_gain_override_enable(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;

	mod_phy_reg(pi, 0x4b0, (0x1 << 8), ebit << 8);

	mod_phy_reg(pi, 0x44c, (0x1 << 0), ebit << 0);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, 0x44c, (0x1 << 4), ebit << 4);
		mod_phy_reg(pi, 0x44c, (0x1 << 6), ebit << 6);
		mod_phy_reg(pi, 0x4b0, (0x1 << 5), ebit << 5);
		mod_phy_reg(pi, 0x4b0, (0x1 << 6), ebit << 6);
	} else {
		mod_phy_reg(pi, 0x4b0, (0x1 << 12), ebit << 12);
		mod_phy_reg(pi, 0x4b0, (0x1 << 13), ebit << 13);
		mod_phy_reg(pi, 0x4b0, (0x1 << 5), ebit << 5);
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, 0x4b0, (0x1 << 10), ebit << 10);
		mod_phy_reg(pi, 0x4e5, (0x1 << 3), ebit << 3);
	}
}

void wlc_lcnphy_tx_pu(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {

		and_phy_reg(pi, 0x43b, ~(uint16) ((0x1 << 1) | (0x1 << 4)));

		mod_phy_reg(pi, 0x43c, (0x1 << 1), 1 << 1);

		and_phy_reg(pi, 0x44c,
			    ~(uint16) ((0x1 << 3) |
				       (0x1 << 5) |
				       (0x1 << 12) |
				       (0x1 << 0) | (0x1 << 1) | (0x1 << 2)));

		and_phy_reg(pi, 0x44d,
			    ~(uint16) ((0x1 << 3) | (0x1 << 5) | (0x1 << 14)));
		mod_phy_reg(pi, 0x44d, (0x1 << 2), 1 << 2);

		mod_phy_reg(pi, 0x44d, (0x1 << 1) | (0x1 << 0), (0x1 << 0));

		and_phy_reg(pi, 0x4f9,
			    ~(uint16) ((0x1 << 0) | (0x1 << 1) | (0x1 << 2)));

		and_phy_reg(pi, 0x4fa,
			    ~(uint16) ((0x1 << 0) | (0x1 << 1) | (0x1 << 2)));
	} else {

		mod_phy_reg(pi, 0x43b, (0x1 << 1), 1 << 1);
		mod_phy_reg(pi, 0x43c, (0x1 << 1), 0 << 1);

		mod_phy_reg(pi, 0x43b, (0x1 << 4), 1 << 4);
		mod_phy_reg(pi, 0x43c, (0x1 << 6), 0 << 6);

		mod_phy_reg(pi, 0x44c, (0x1 << 12), 1 << 12);
		mod_phy_reg(pi, 0x44d, (0x1 << 14), 1 << 14);

		wlc_lcnphy_set_trsw_override(pi, TRUE, FALSE);

		mod_phy_reg(pi, 0x44d, (0x1 << 2), 0 << 2);
		mod_phy_reg(pi, 0x44c, (0x1 << 2), 1 << 2);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {

			mod_phy_reg(pi, 0x44c, (0x1 << 3), 1 << 3);
			mod_phy_reg(pi, 0x44d, (0x1 << 3), 1 << 3);

			mod_phy_reg(pi, 0x44c, (0x1 << 5), 1 << 5);
			mod_phy_reg(pi, 0x44d, (0x1 << 5), 0 << 5);

			mod_phy_reg(pi, 0x4f9, (0x1 << 1), 1 << 1);
			mod_phy_reg(pi, 0x4fa, (0x1 << 1), 1 << 1);

			mod_phy_reg(pi, 0x4f9, (0x1 << 2), 1 << 2);
			mod_phy_reg(pi, 0x4fa, (0x1 << 2), 1 << 2);

			mod_phy_reg(pi, 0x4f9, (0x1 << 0), 1 << 0);
			mod_phy_reg(pi, 0x4fa, (0x1 << 0), 1 << 0);
		} else {

			mod_phy_reg(pi, 0x44c, (0x1 << 3), 1 << 3);
			mod_phy_reg(pi, 0x44d, (0x1 << 3), 0 << 3);

			mod_phy_reg(pi, 0x44c, (0x1 << 5), 1 << 5);
			mod_phy_reg(pi, 0x44d, (0x1 << 5), 1 << 5);

			mod_phy_reg(pi, 0x4f9, (0x1 << 1), 1 << 1);
			mod_phy_reg(pi, 0x4fa, (0x1 << 1), 0 << 1);

			mod_phy_reg(pi, 0x4f9, (0x1 << 2), 1 << 2);
			mod_phy_reg(pi, 0x4fa, (0x1 << 2), 0 << 2);

			mod_phy_reg(pi, 0x4f9, (0x1 << 0), 1 << 0);
			mod_phy_reg(pi, 0x4fa, (0x1 << 0), 0 << 0);
		}
	}
}

static void
wlc_lcnphy_run_samples(phy_info_t *pi,
		       uint16 num_samps,
		       uint16 num_loops, uint16 wait, bool iqcalmode)
{

	or_phy_reg(pi, 0x6da, 0x8080);

	mod_phy_reg(pi, 0x642, (0x7f << 0), (num_samps - 1) << 0);
	if (num_loops != 0xffff)
		num_loops--;
	mod_phy_reg(pi, 0x640, (0xffff << 0), num_loops << 0);

	mod_phy_reg(pi, 0x641, (0xffff << 0), wait << 0);

	if (iqcalmode) {

		and_phy_reg(pi, 0x453, (uint16) ~(0x1 << 15));
		or_phy_reg(pi, 0x453, (0x1 << 15));
	} else {
		write_phy_reg(pi, 0x63f, 1);
		wlc_lcnphy_tx_pu(pi, 1);
	}

	or_radio_reg(pi, RADIO_2064_REG112, 0x6);
}

void wlc_lcnphy_deaf_mode(phy_info_t *pi, bool mode)
{

	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, 0x4b0, (0x1 << 5), (mode) << 5);
		mod_phy_reg(pi, 0x4b1, (0x1 << 9), 0 << 9);
	} else {
		mod_phy_reg(pi, 0x4b0, (0x1 << 5), (mode) << 5);
		mod_phy_reg(pi, 0x4b1, (0x1 << 9), 0 << 9);
	}

	if (phybw40 == 0) {
		mod_phy_reg((pi), 0x410,
			    (0x1 << 6) |
			    (0x1 << 5),
			    ((CHSPEC_IS2G(pi->radio_chanspec)) ? (!mode) : 0) <<
			    6 | (!mode) << 5);
		mod_phy_reg(pi, 0x410, (0x1 << 7), (mode) << 7);
	}
}

void
wlc_lcnphy_start_tx_tone(phy_info_t *pi, int32 f_kHz, uint16 max_val,
			 bool iqcalmode)
{
	uint8 phy_bw;
	uint16 num_samps, t, k;
	uint32 bw;
	fixed theta = 0, rot = 0;
	cint32 tone_samp;
	uint32 data_buf[64];
	uint16 i_samp, q_samp;
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi->phy_tx_tone_freq = f_kHz;

	wlc_lcnphy_deaf_mode(pi, TRUE);

	phy_bw = 40;
	if (pi_lcn->lcnphy_spurmod) {
		write_phy_reg(pi, 0x942, 0x2);
		write_phy_reg(pi, 0x93b, 0x0);
		write_phy_reg(pi, 0x93c, 0x0);
		wlc_lcnphy_txrx_spur_avoidance_mode(pi, FALSE);
	}

	if (f_kHz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k;
			num_samps = bw / ABS(f_kHz);
			ASSERT(num_samps <= ARRAYSIZE(data_buf));
			k++;
		} while ((num_samps * (uint32) (ABS(f_kHz))) != bw);
	} else
		num_samps = 2;

	rot = FIXED((f_kHz * 36) / phy_bw) / 100;
	theta = 0;

	for (t = 0; t < num_samps; t++) {

		wlc_phy_cordic(theta, &tone_samp);

		theta += rot;

		i_samp = (uint16) (FLOAT(tone_samp.i * max_val) & 0x3ff);
		q_samp = (uint16) (FLOAT(tone_samp.q * max_val) & 0x3ff);
		data_buf[t] = (i_samp << 10) | q_samp;
	}

	mod_phy_reg(pi, 0x6d6, (0x3 << 0), 0 << 0);

	mod_phy_reg(pi, 0x6da, (0x1 << 3), 1 << 3);

	tab.tbl_ptr = data_buf;
	tab.tbl_len = num_samps;
	tab.tbl_id = LCNPHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	wlc_lcnphy_write_table(pi, &tab);

	wlc_lcnphy_run_samples(pi, num_samps, 0xffff, 0, iqcalmode);
}

void wlc_lcnphy_stop_tx_tone(phy_info_t *pi)
{
	int16 playback_status;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi->phy_tx_tone_freq = 0;
	if (pi_lcn->lcnphy_spurmod) {
		write_phy_reg(pi, 0x942, 0x7);
		write_phy_reg(pi, 0x93b, 0x2017);
		write_phy_reg(pi, 0x93c, 0x27c5);
		wlc_lcnphy_txrx_spur_avoidance_mode(pi, TRUE);
	}

	playback_status = read_phy_reg(pi, 0x644);
	if (playback_status & (0x1 << 0)) {
		wlc_lcnphy_tx_pu(pi, 0);
		mod_phy_reg(pi, 0x63f, (0x1 << 1), 1 << 1);
	} else if (playback_status & (0x1 << 1))
		mod_phy_reg(pi, 0x453, (0x1 << 15), 0 << 15);

	mod_phy_reg(pi, 0x6d6, (0x3 << 0), 1 << 0);

	mod_phy_reg(pi, 0x6da, (0x1 << 3), 0 << 3);

	mod_phy_reg(pi, 0x6da, (0x1 << 7), 0 << 7);

	and_radio_reg(pi, RADIO_2064_REG112, 0xFFF9);

	wlc_lcnphy_deaf_mode(pi, FALSE);
}

static void wlc_lcnphy_clear_trsw_override(phy_info_t *pi)
{

	and_phy_reg(pi, 0x44c, (uint16) ~((0x1 << 1) | (0x1 << 0)));
}

void wlc_lcnphy_get_tx_iqcc(phy_info_t *pi, uint16 *a, uint16 *b)
{
	uint16 iqcc[2];
	phytbl_info_t tab;

	tab.tbl_ptr = iqcc;
	tab.tbl_len = 2;
	tab.tbl_id = 0;
	tab.tbl_offset = 80;
	tab.tbl_width = 16;
	wlc_lcnphy_read_table(pi, &tab);

	*a = iqcc[0];
	*b = iqcc[1];
}

uint16 wlc_lcnphy_get_tx_locc(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint16 didq;

	tab.tbl_id = 0;
	tab.tbl_width = 16;
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_lcnphy_read_table(pi, &tab);

	return didq;
}

static void wlc_lcnphy_txpwrtbl_iqlo_cal(phy_info_t *pi)
{

	lcnphy_txgains_t target_gains, old_gains;
	uint8 save_bb_mult;
	uint16 a, b, didq, save_pa_gain = 0;
	uint idx, SAVE_txpwrindex = 0xFF;
	uint32 val;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phytbl_info_t tab;
	uint8 ei0, eq0, fi0, fq0;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	wlc_lcnphy_get_tx_gain(pi, &old_gains);
	save_pa_gain = wlc_lcnphy_get_pa_gain(pi);

	save_bb_mult = wlc_lcnphy_get_bbmult(pi);

	if (SAVE_txpwrctrl == LCNPHY_TX_PWR_CTRL_OFF)
		SAVE_txpwrindex = wlc_lcnphy_get_current_tx_pwr_idx(pi);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	target_gains.gm_gain = 7;
	target_gains.pga_gain = 0;
	target_gains.pad_gain = 21;
	target_gains.dac_gain = 0;
	wlc_lcnphy_set_tx_gain(pi, &target_gains);
	wlc_lcnphy_set_tx_pwr_by_index(pi, 16);

	if (LCNREV_IS(pi->pubpi.phy_rev, 1) || pi_lcn->lcnphy_hw_iqcal_en) {

		wlc_lcnphy_set_tx_pwr_by_index(pi, 30);

		wlc_lcnphy_tx_iqlo_cal(pi, &target_gains,
				       (pi_lcn->
					lcnphy_recal ? LCNPHY_CAL_RECAL :
					LCNPHY_CAL_FULL), FALSE);
	} else {

		wlc_lcnphy_tx_iqlo_soft_cal_full(pi);
	}

	wlc_lcnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);
	if ((ABS((int8) fi0) == 15) && (ABS((int8) fq0) == 15)) {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			target_gains.gm_gain = 255;
			target_gains.pga_gain = 255;
			target_gains.pad_gain = 0xf0;
			target_gains.dac_gain = 0;
		} else {
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 45;
			target_gains.pad_gain = 186;
			target_gains.dac_gain = 0;
		}

		if (LCNREV_IS(pi->pubpi.phy_rev, 1)
		    || pi_lcn->lcnphy_hw_iqcal_en) {

			target_gains.pga_gain = 0;
			target_gains.pad_gain = 30;
			wlc_lcnphy_set_tx_pwr_by_index(pi, 16);
			wlc_lcnphy_tx_iqlo_cal(pi, &target_gains,
					       LCNPHY_CAL_FULL, FALSE);
		} else {

			wlc_lcnphy_tx_iqlo_soft_cal_full(pi);
		}

	}

	wlc_lcnphy_get_tx_iqcc(pi, &a, &b);

	didq = wlc_lcnphy_get_tx_locc(pi);

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_ptr = &val;

	tab.tbl_len = 1;
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;

	for (idx = 0; idx < 128; idx++) {
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + idx;

		wlc_lcnphy_read_table(pi, &tab);
		val = (val & 0xfff00000) |
		    ((uint32) (a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_lcnphy_write_table(pi, &tab);

		val = didq;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_LO_OFFSET + idx;
		wlc_lcnphy_write_table(pi, &tab);
	}

	pi_lcn->lcnphy_cal_results.txiqlocal_a = a;
	pi_lcn->lcnphy_cal_results.txiqlocal_b = b;
	pi_lcn->lcnphy_cal_results.txiqlocal_didq = didq;
	pi_lcn->lcnphy_cal_results.txiqlocal_ei0 = ei0;
	pi_lcn->lcnphy_cal_results.txiqlocal_eq0 = eq0;
	pi_lcn->lcnphy_cal_results.txiqlocal_fi0 = fi0;
	pi_lcn->lcnphy_cal_results.txiqlocal_fq0 = fq0;

	wlc_lcnphy_set_bbmult(pi, save_bb_mult);
	wlc_lcnphy_set_pa_gain(pi, save_pa_gain);
	wlc_lcnphy_set_tx_gain(pi, &old_gains);

	if (SAVE_txpwrctrl != LCNPHY_TX_PWR_CTRL_OFF)
		wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
	else
		wlc_lcnphy_set_tx_pwr_by_index(pi, SAVE_txpwrindex);
}

int16 wlc_lcnphy_tempsense_new(phy_info_t *pi, bool mode)
{
	uint16 tempsenseval1, tempsenseval2;
	int16 avg = 0;
	bool suspend = 0;

	if (NORADIO_ENAB(pi->pubpi))
		return -1;

	if (mode == 1) {
		suspend =
		    (0 ==
		     (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	}
	tempsenseval1 = read_phy_reg(pi, 0x476) & 0x1FF;
	tempsenseval2 = read_phy_reg(pi, 0x477) & 0x1FF;

	if (tempsenseval1 > 255)
		avg = (int16) (tempsenseval1 - 512);
	else
		avg = (int16) tempsenseval1;

	if (tempsenseval2 > 255)
		avg += (int16) (tempsenseval2 - 512);
	else
		avg += (int16) tempsenseval2;

	avg /= 2;

	if (mode == 1) {

		mod_phy_reg(pi, 0x448, (0x1 << 14), (1) << 14);

		OSL_DELAY(100);
		mod_phy_reg(pi, 0x448, (0x1 << 14), (0) << 14);

		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return avg;
}

uint16 wlc_lcnphy_tempsense(phy_info_t *pi, bool mode)
{
	uint16 tempsenseval1, tempsenseval2;
	int32 avg = 0;
	bool suspend = 0;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return -1;

	if (mode == 1) {
		suspend =
		    (0 ==
		     (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	}
	tempsenseval1 = read_phy_reg(pi, 0x476) & 0x1FF;
	tempsenseval2 = read_phy_reg(pi, 0x477) & 0x1FF;

	if (tempsenseval1 > 255)
		avg = (int)(tempsenseval1 - 512);
	else
		avg = (int)tempsenseval1;

	if (pi_lcn->lcnphy_tempsense_option == 1 || pi->hwpwrctrl_capable) {
		if (tempsenseval2 > 255)
			avg = (int)(avg - tempsenseval2 + 512);
		else
			avg = (int)(avg - tempsenseval2);
	} else {
		if (tempsenseval2 > 255)
			avg = (int)(avg + tempsenseval2 - 512);
		else
			avg = (int)(avg + tempsenseval2);
		avg = avg / 2;
	}
	if (avg < 0)
		avg = avg + 512;

	if (pi_lcn->lcnphy_tempsense_option == 2)
		avg = tempsenseval1;

	if (mode)
		wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);

	if (mode == 1) {

		mod_phy_reg(pi, 0x448, (0x1 << 14), (1) << 14);

		OSL_DELAY(100);
		mod_phy_reg(pi, 0x448, (0x1 << 14), (0) << 14);

		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return (uint16) avg;
}

int8 wlc_lcnphy_tempsense_degree(phy_info_t *pi, bool mode)
{
	int32 degree = wlc_lcnphy_tempsense_new(pi, mode);
	degree =
	    ((degree << 10) + LCN_TEMPSENSE_OFFSET + (LCN_TEMPSENSE_DEN >> 1))
	    / LCN_TEMPSENSE_DEN;
	return (int8) degree;
}

int8 wlc_lcnphy_vbatsense(phy_info_t *pi, bool mode)
{
	uint16 vbatsenseval;
	int32 avg = 0;
	bool suspend = 0;

	if (NORADIO_ENAB(pi->pubpi))
		return -1;

	if (mode == 1) {
		suspend =
		    (0 ==
		     (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, VBATSENSE);
	}

	vbatsenseval = read_phy_reg(pi, 0x475) & 0x1FF;

	if (vbatsenseval > 255)
		avg = (int32) (vbatsenseval - 512);
	else
		avg = (int32) vbatsenseval;

	avg =
	    (avg * LCN_VBAT_SCALE_NOM +
	     (LCN_VBAT_SCALE_DEN >> 1)) / LCN_VBAT_SCALE_DEN;

	if (mode == 1) {
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return (int8) avg;
}

static void wlc_lcnphy_afe_clk_init(phy_info_t *pi, uint8 mode)
{
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	mod_phy_reg(pi, 0x6d1, (0x1 << 7), (1) << 7);

	if (((mode == AFE_CLK_INIT_MODE_PAPD) && (phybw40 == 0)) ||
	    (mode == AFE_CLK_INIT_MODE_TXRX2X))
		write_phy_reg(pi, 0x6d0, 0x7);

	wlc_lcnphy_toggle_afe_pwdn(pi);
}

static bool
wlc_lcnphy_rx_iq_est(phy_info_t *pi,
		     uint16 num_samps,
		     uint8 wait_time, lcnphy_iq_est_t *iq_est)
{
	int wait_count = 0;
	bool result = TRUE;
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	mod_phy_reg(pi, 0x6da, (0x1 << 5), (1) << 5);

	mod_phy_reg(pi, 0x410, (0x1 << 3), (0) << 3);

	mod_phy_reg(pi, 0x482, (0xffff << 0), (num_samps) << 0);

	mod_phy_reg(pi, 0x481, (0xff << 0), ((uint16) wait_time) << 0);

	mod_phy_reg(pi, 0x481, (0x1 << 8), (0) << 8);

	mod_phy_reg(pi, 0x481, (0x1 << 9), (1) << 9);

	while (read_phy_reg(pi, 0x481) & (0x1 << 9)) {

		if (wait_count > (10 * 500)) {
			result = FALSE;
			goto cleanup;
		}
		OSL_DELAY(100);
		wait_count++;
	}

	iq_est->iq_prod = ((uint32) read_phy_reg(pi, 0x483) << 16) |
	    (uint32) read_phy_reg(pi, 0x484);
	iq_est->i_pwr = ((uint32) read_phy_reg(pi, 0x485) << 16) |
	    (uint32) read_phy_reg(pi, 0x486);
	iq_est->q_pwr = ((uint32) read_phy_reg(pi, 0x487) << 16) |
	    (uint32) read_phy_reg(pi, 0x488);

 cleanup:
	mod_phy_reg(pi, 0x410, (0x1 << 3), (1) << 3);

	mod_phy_reg(pi, 0x6da, (0x1 << 5), (0) << 5);

	return result;
}

static bool wlc_lcnphy_calc_rx_iq_comp(phy_info_t *pi, uint16 num_samps)
{
#define LCNPHY_MIN_RXIQ_PWR 2
	bool result;
	uint16 a0_new, b0_new;
	lcnphy_iq_est_t iq_est = { 0, 0, 0 };
	int32 a, b, temp;
	int16 iq_nbits, qq_nbits, arsh, brsh;
	int32 iq;
	uint32 ii, qq;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	a0_new = ((read_phy_reg(pi, 0x645) & (0x3ff << 0)) >> 0);
	b0_new = ((read_phy_reg(pi, 0x646) & (0x3ff << 0)) >> 0);
	mod_phy_reg(pi, 0x6d1, (0x1 << 2), (0) << 2);

	mod_phy_reg(pi, 0x64b, (0x1 << 6), (1) << 6);

	wlc_lcnphy_set_rx_iq_comp(pi, 0, 0);

	result = wlc_lcnphy_rx_iq_est(pi, num_samps, 32, &iq_est);
	if (!result)
		goto cleanup;

	iq = (int32) iq_est.iq_prod;
	ii = iq_est.i_pwr;
	qq = iq_est.q_pwr;

	if ((ii + qq) < LCNPHY_MIN_RXIQ_PWR) {
		result = FALSE;
		goto cleanup;
	}

	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);

	arsh = 10 - (30 - iq_nbits);
	if (arsh >= 0) {
		a = (-(iq << (30 - iq_nbits)) + (ii >> (1 + arsh)));
		temp = (int32) (ii >> arsh);
		if (temp == 0) {
			return FALSE;
		}
	} else {
		a = (-(iq << (30 - iq_nbits)) + (ii << (-1 - arsh)));
		temp = (int32) (ii << -arsh);
		if (temp == 0) {
			return FALSE;
		}
	}
	a /= temp;
	brsh = qq_nbits - 31 + 20;
	if (brsh >= 0) {
		b = (qq << (31 - qq_nbits));
		temp = (int32) (ii >> brsh);
		if (temp == 0) {
			return FALSE;
		}
	} else {
		b = (qq << (31 - qq_nbits));
		temp = (int32) (ii << -brsh);
		if (temp == 0) {
			return FALSE;
		}
	}
	b /= temp;
	b -= a * a;
	b = (int32) wlc_phy_sqrt_int((uint32) b);
	b -= (1 << 10);
	a0_new = (uint16) (a & 0x3ff);
	b0_new = (uint16) (b & 0x3ff);
 cleanup:

	wlc_lcnphy_set_rx_iq_comp(pi, a0_new, b0_new);

	mod_phy_reg(pi, 0x64b, (0x1 << 0), (1) << 0);

	mod_phy_reg(pi, 0x64b, (0x1 << 3), (1) << 3);

	pi_lcn->lcnphy_cal_results.rxiqcal_coeff_a0 = a0_new;
	pi_lcn->lcnphy_cal_results.rxiqcal_coeff_b0 = b0_new;

	return result;
}

static bool
wlc_lcnphy_rx_iq_cal(phy_info_t *pi, const lcnphy_rx_iqcomp_t *iqcomp,
		     int iqcomp_sz, bool tx_switch, bool rx_switch, int module,
		     int tx_gain_idx)
{
	lcnphy_txgains_t old_gains;
	uint16 tx_pwr_ctrl;
	uint8 tx_gain_index_old = 0;
	bool result = FALSE, tx_gain_override_old = FALSE;
	uint16 i, Core1TxControl_old, RFOverride0_old,
	    RFOverrideVal0_old, rfoverride2_old, rfoverride2val_old,
	    rfoverride3_old, rfoverride3val_old, rfoverride4_old,
	    rfoverride4val_old, afectrlovr_old, afectrlovrval_old;
	int tia_gain;
	uint32 received_power, rx_pwr_threshold;
	uint16 old_sslpnCalibClkEnCtrl, old_sslpnRxFeClkEnCtrl;
	uint16 values_to_save[11];
	int16 *ptr;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ptr = MALLOC(pi->sh->osh, sizeof(int16) * 131);
	if (NULL == ptr) {
		return FALSE;
	}
	if (module == 2) {
		ASSERT(iqcomp_sz);

		while (iqcomp_sz--) {
			if (iqcomp[iqcomp_sz].chan ==
			    CHSPEC_CHANNEL(pi->radio_chanspec)) {

				wlc_lcnphy_set_rx_iq_comp(pi,
							  (uint16)
							  iqcomp[iqcomp_sz].a,
							  (uint16)
							  iqcomp[iqcomp_sz].b);
				result = TRUE;
				break;
			}
		}
		ASSERT(result);
		goto cal_done;
	}

	if (module == 1) {

		tx_pwr_ctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

		for (i = 0; i < 11; i++) {
			values_to_save[i] =
			    read_radio_reg(pi, rxiq_cal_rf_reg[i]);
		}
		Core1TxControl_old = read_phy_reg(pi, 0x631);

		or_phy_reg(pi, 0x631, 0x0015);

		RFOverride0_old = read_phy_reg(pi, 0x44c);
		RFOverrideVal0_old = read_phy_reg(pi, 0x44d);
		rfoverride2_old = read_phy_reg(pi, 0x4b0);
		rfoverride2val_old = read_phy_reg(pi, 0x4b1);
		rfoverride3_old = read_phy_reg(pi, 0x4f9);
		rfoverride3val_old = read_phy_reg(pi, 0x4fa);
		rfoverride4_old = read_phy_reg(pi, 0x938);
		rfoverride4val_old = read_phy_reg(pi, 0x939);
		afectrlovr_old = read_phy_reg(pi, 0x43b);
		afectrlovrval_old = read_phy_reg(pi, 0x43c);
		old_sslpnCalibClkEnCtrl = read_phy_reg(pi, 0x6da);
		old_sslpnRxFeClkEnCtrl = read_phy_reg(pi, 0x6db);

		tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
		if (tx_gain_override_old) {
			wlc_lcnphy_get_tx_gain(pi, &old_gains);
			tx_gain_index_old = pi_lcn->lcnphy_current_index;
		}

		wlc_lcnphy_set_tx_pwr_by_index(pi, tx_gain_idx);

		mod_phy_reg(pi, 0x4f9, (0x1 << 0), 1 << 0);
		mod_phy_reg(pi, 0x4fa, (0x1 << 0), 0 << 0);

		mod_phy_reg(pi, 0x43b, (0x1 << 1), 1 << 1);
		mod_phy_reg(pi, 0x43c, (0x1 << 1), 0 << 1);

		write_radio_reg(pi, RADIO_2064_REG116, 0x06);
		write_radio_reg(pi, RADIO_2064_REG12C, 0x07);
		write_radio_reg(pi, RADIO_2064_REG06A, 0xd3);
		write_radio_reg(pi, RADIO_2064_REG098, 0x03);
		write_radio_reg(pi, RADIO_2064_REG00B, 0x7);
		mod_radio_reg(pi, RADIO_2064_REG113, 1 << 4, 1 << 4);
		write_radio_reg(pi, RADIO_2064_REG01D, 0x01);
		write_radio_reg(pi, RADIO_2064_REG114, 0x01);
		write_radio_reg(pi, RADIO_2064_REG02E, 0x10);
		write_radio_reg(pi, RADIO_2064_REG12A, 0x08);

		mod_phy_reg(pi, 0x938, (0x1 << 0), 1 << 0);
		mod_phy_reg(pi, 0x939, (0x1 << 0), 0 << 0);
		mod_phy_reg(pi, 0x938, (0x1 << 1), 1 << 1);
		mod_phy_reg(pi, 0x939, (0x1 << 1), 1 << 1);
		mod_phy_reg(pi, 0x938, (0x1 << 2), 1 << 2);
		mod_phy_reg(pi, 0x939, (0x1 << 2), 1 << 2);
		mod_phy_reg(pi, 0x938, (0x1 << 3), 1 << 3);
		mod_phy_reg(pi, 0x939, (0x1 << 3), 1 << 3);
		mod_phy_reg(pi, 0x938, (0x1 << 5), 1 << 5);
		mod_phy_reg(pi, 0x939, (0x1 << 5), 0 << 5);

		mod_phy_reg(pi, 0x43b, (0x1 << 0), 1 << 0);
		mod_phy_reg(pi, 0x43c, (0x1 << 0), 0 << 0);

		wlc_lcnphy_start_tx_tone(pi, 2000, 120, 0);
		write_phy_reg(pi, 0x6da, 0xffff);
		or_phy_reg(pi, 0x6db, 0x3);
		wlc_lcnphy_set_trsw_override(pi, tx_switch, rx_switch);
		wlc_lcnphy_rx_gain_override_enable(pi, TRUE);

		tia_gain = 8;
		rx_pwr_threshold = 950;
		while (tia_gain > 0) {
			tia_gain -= 1;
			wlc_lcnphy_set_rx_gain_by_distribution(pi,
							       0, 0, 2, 2,
							       (uint16)
							       tia_gain, 1, 0);
			OSL_DELAY(500);

			received_power =
			    wlc_lcnphy_measure_digital_power(pi, 2000);
			if (received_power < rx_pwr_threshold)
				break;
		}
		result = wlc_lcnphy_calc_rx_iq_comp(pi, 0xffff);

		wlc_lcnphy_stop_tx_tone(pi);

		write_phy_reg(pi, 0x631, Core1TxControl_old);

		write_phy_reg(pi, 0x44c, RFOverrideVal0_old);
		write_phy_reg(pi, 0x44d, RFOverrideVal0_old);
		write_phy_reg(pi, 0x4b0, rfoverride2_old);
		write_phy_reg(pi, 0x4b1, rfoverride2val_old);
		write_phy_reg(pi, 0x4f9, rfoverride3_old);
		write_phy_reg(pi, 0x4fa, rfoverride3val_old);
		write_phy_reg(pi, 0x938, rfoverride4_old);
		write_phy_reg(pi, 0x939, rfoverride4val_old);
		write_phy_reg(pi, 0x43b, afectrlovr_old);
		write_phy_reg(pi, 0x43c, afectrlovrval_old);
		write_phy_reg(pi, 0x6da, old_sslpnCalibClkEnCtrl);
		write_phy_reg(pi, 0x6db, old_sslpnRxFeClkEnCtrl);

		wlc_lcnphy_clear_trsw_override(pi);

		mod_phy_reg(pi, 0x44c, (0x1 << 2), 0 << 2);

		for (i = 0; i < 11; i++) {
			write_radio_reg(pi, rxiq_cal_rf_reg[i],
					values_to_save[i]);
		}

		if (tx_gain_override_old) {
			wlc_lcnphy_set_tx_pwr_by_index(pi, tx_gain_index_old);
		} else
			wlc_lcnphy_disable_tx_gain_override(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

		wlc_lcnphy_rx_gain_override_enable(pi, FALSE);
	}

 cal_done:
	MFREE(pi->sh->osh, ptr, 131 * sizeof(int16));
	return result;
}

static void wlc_lcnphy_temp_adj(phy_info_t *pi)
{
	if (NORADIO_ENAB(pi->pubpi))
		return;
}

static void wlc_lcnphy_glacial_timer_based_cal(phy_info_t *pi)
{
	bool suspend;
	int8 index;
	uint16 SAVE_pwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	suspend =
	    (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_lcnphy_deaf_mode(pi, TRUE);
	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	index = pi_lcn->lcnphy_current_index;

	wlc_lcnphy_txpwrtbl_iqlo_cal(pi);

	wlc_lcnphy_set_tx_pwr_by_index(pi, index);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_pwrctrl);
	wlc_lcnphy_deaf_mode(pi, FALSE);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

}

static void wlc_lcnphy_periodic_cal(phy_info_t *pi)
{
	bool suspend, full_cal;
	const lcnphy_rx_iqcomp_t *rx_iqcomp;
	int rx_iqcomp_sz;
	uint16 SAVE_pwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	int8 index;
	phytbl_info_t tab;
	int32 a1, b0, b1;
	int32 tssi, pwr, maxtargetpwr, mintargetpwr;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	full_cal =
	    (pi_lcn->lcnphy_full_cal_channel !=
	     CHSPEC_CHANNEL(pi->radio_chanspec));
	pi_lcn->lcnphy_full_cal_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	index = pi_lcn->lcnphy_current_index;

	suspend =
	    (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend) {

		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}
	wlc_lcnphy_deaf_mode(pi, TRUE);

	wlc_lcnphy_txpwrtbl_iqlo_cal(pi);

	rx_iqcomp = lcnphy_rx_iqcomp_table_rev0;
	rx_iqcomp_sz = ARRAYSIZE(lcnphy_rx_iqcomp_table_rev0);

	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		wlc_lcnphy_rx_iq_cal(pi, NULL, 0, TRUE, FALSE, 1, 40);
	else
		wlc_lcnphy_rx_iq_cal(pi, NULL, 0, TRUE, FALSE, 1, 127);

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi)) {

		wlc_lcnphy_idle_tssi_est((wlc_phy_t *) pi);

		b0 = pi->txpa_2g[0];
		b1 = pi->txpa_2g[1];
		a1 = pi->txpa_2g[2];
		maxtargetpwr = wlc_lcnphy_tssi2dbm(10, a1, b0, b1);
		mintargetpwr = wlc_lcnphy_tssi2dbm(125, a1, b0, b1);

		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;
		tab.tbl_ptr = &pwr;
		tab.tbl_len = 1;
		tab.tbl_offset = 0;
		for (tssi = 0; tssi < 128; tssi++) {
			pwr = wlc_lcnphy_tssi2dbm(tssi, a1, b0, b1);
			pwr = (pwr < mintargetpwr) ? mintargetpwr : pwr;
			wlc_lcnphy_write_table(pi, &tab);
			tab.tbl_offset++;
		}
	}

	wlc_lcnphy_set_tx_pwr_by_index(pi, index);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_pwrctrl);
	wlc_lcnphy_deaf_mode(pi, FALSE);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

void wlc_lcnphy_calib_modes(phy_info_t *pi, uint mode)
{
	uint16 temp_new;
	int temp1, temp2, temp_diff;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	switch (mode) {
	case PHY_PERICAL_CHAN:

		break;
	case PHY_FULLCAL:
		wlc_lcnphy_periodic_cal(pi);
		break;
	case PHY_PERICAL_PHYINIT:
		wlc_lcnphy_periodic_cal(pi);
		break;
	case PHY_PERICAL_WATCHDOG:
		if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
			temp_new = wlc_lcnphy_tempsense(pi, 0);
			temp1 = LCNPHY_TEMPSENSE(temp_new);
			temp2 = LCNPHY_TEMPSENSE(pi_lcn->lcnphy_cal_temper);
			temp_diff = temp1 - temp2;
			if ((pi_lcn->lcnphy_cal_counter > 90) ||
			    (temp_diff > 60) || (temp_diff < -60)) {
				wlc_lcnphy_glacial_timer_based_cal(pi);
				wlc_2064_vco_cal(pi);
				pi_lcn->lcnphy_cal_temper = temp_new;
				pi_lcn->lcnphy_cal_counter = 0;
			} else
				pi_lcn->lcnphy_cal_counter++;
		}
		break;
	case LCNPHY_PERICAL_TEMPBASED_TXPWRCTRL:
		if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
			wlc_lcnphy_tx_power_adjustment((wlc_phy_t *) pi);
		break;
	default:
		ASSERT(0);
		break;
	}
}

void wlc_lcnphy_get_tssi(phy_info_t *pi, int8 *ofdm_pwr, int8 *cck_pwr)
{
	int8 cck_offset;
	uint16 status;
	status = (read_phy_reg(pi, 0x4ab));
	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) &&
	    (status  & (0x1 << 15))) {
		*ofdm_pwr = (int8) (((read_phy_reg(pi, 0x4ab) & (0x1ff << 0))
				     >> 0) >> 1);

		if (wlc_phy_tpc_isenabled_lcnphy(pi))
			cck_offset = pi->tx_power_offset[TXP_FIRST_CCK];
		else
			cck_offset = 0;

		*cck_pwr = *ofdm_pwr + cck_offset;
	} else {
		*cck_pwr = 0;
		*ofdm_pwr = 0;
	}
}

void WLBANDINITFN(wlc_phy_cal_init_lcnphy) (phy_info_t *pi)
{
	return;

}

static void wlc_lcnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	if (channel == 14) {
		mod_phy_reg(pi, 0x448, (0x3 << 8), (2) << 8);

	} else {
		mod_phy_reg(pi, 0x448, (0x3 << 8), (1) << 8);

	}
	pi_lcn->lcnphy_bandedge_corr = 2;
	if (channel == 1)
		pi_lcn->lcnphy_bandedge_corr = 4;

	if (channel == 1 || channel == 2 || channel == 3 ||
	    channel == 4 || channel == 9 ||
	    channel == 10 || channel == 11 || channel == 12) {
		si_pmu_pllcontrol(pi->sh->sih, 0x2, 0xffffffff, 0x03000c04);
		si_pmu_pllcontrol(pi->sh->sih, 0x3, 0xffffff, 0x0);
		si_pmu_pllcontrol(pi->sh->sih, 0x4, 0xffffffff, 0x200005c0);

		si_pmu_pllupd(pi->sh->sih);
		write_phy_reg(pi, 0x942, 0);
		wlc_lcnphy_txrx_spur_avoidance_mode(pi, FALSE);
		pi_lcn->lcnphy_spurmod = 0;
		mod_phy_reg(pi, 0x424, (0xff << 8), (0x1b) << 8);

		write_phy_reg(pi, 0x425, 0x5907);
	} else {
		si_pmu_pllcontrol(pi->sh->sih, 0x2, 0xffffffff, 0x03140c04);
		si_pmu_pllcontrol(pi->sh->sih, 0x3, 0xffffff, 0x333333);
		si_pmu_pllcontrol(pi->sh->sih, 0x4, 0xffffffff, 0x202c2820);

		si_pmu_pllupd(pi->sh->sih);
		write_phy_reg(pi, 0x942, 0);
		wlc_lcnphy_txrx_spur_avoidance_mode(pi, TRUE);

		pi_lcn->lcnphy_spurmod = 0;
		mod_phy_reg(pi, 0x424, (0xff << 8), (0x1f) << 8);

		write_phy_reg(pi, 0x425, 0x590a);
	}

	or_phy_reg(pi, 0x44a, 0x44);
	write_phy_reg(pi, 0x44a, 0x80);
}

void
wlc_lcnphy_pktengtx(wlc_phy_t *ppi, wl_pkteng_t *pkteng, uint8 rate,
		    struct ether_addr *sa, uint32 wait_delay)
{
}

void wlc_lcnphy_tx_power_adjustment(wlc_phy_t *ppi)
{
	int8 index;
	uint16 index2;
	phy_info_t *pi = (phy_info_t *) ppi;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) && SAVE_txpwrctrl) {
		index = wlc_lcnphy_tempcompensated_txpwrctrl(pi);
		index2 = (uint16) (index * 2);
		mod_phy_reg(pi, 0x4a9, (0x1ff << 0), (index2) << 0);

		pi_lcn->lcnphy_current_index = (int8)
		    ((read_phy_reg(pi, 0x4a9) & 0xFF) / 2);
	}
}

static void wlc_lcnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a, uint16 b)
{
	mod_phy_reg(pi, 0x645, (0x3ff << 0), (a) << 0);

	mod_phy_reg(pi, 0x646, (0x3ff << 0), (b) << 0);

	mod_phy_reg(pi, 0x647, (0x3ff << 0), (a) << 0);

	mod_phy_reg(pi, 0x648, (0x3ff << 0), (b) << 0);

	mod_phy_reg(pi, 0x649, (0x3ff << 0), (a) << 0);

	mod_phy_reg(pi, 0x64a, (0x3ff << 0), (b) << 0);

}

void WLBANDINITFN(wlc_phy_init_lcnphy) (phy_info_t *pi)
{
	uint8 phybw40;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	pi_lcn->lcnphy_cal_counter = 0;
	pi_lcn->lcnphy_cal_temper = pi_lcn->lcnphy_rawtempsense;

	or_phy_reg(pi, 0x44a, 0x80);
	and_phy_reg(pi, 0x44a, 0x7f);

	wlc_lcnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);

	write_phy_reg(pi, 0x60a, 160);

	write_phy_reg(pi, 0x46a, 25);

	wlc_lcnphy_baseband_init(pi);

	wlc_lcnphy_radio_init(pi);

	if (CHSPEC_IS2G(pi->radio_chanspec))
		wlc_lcnphy_tx_pwr_ctrl_init((wlc_phy_t *) pi);

	wlc_phy_chanspec_set((wlc_phy_t *) pi, pi->radio_chanspec);

	si_pmu_regcontrol(pi->sh->sih, 0, 0xf, 0x9);

	si_pmu_chipcontrol(pi->sh->sih, 0, 0xffffffff, 0x03CDDDDD);

	if ((pi->sh->boardflags & BFL_FEM)
	    && wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		wlc_lcnphy_set_tx_pwr_by_index(pi, FIXED_TXPWR);

	wlc_lcnphy_agc_temp_init(pi);

	wlc_lcnphy_temp_adj(pi);

	mod_phy_reg(pi, 0x448, (0x1 << 14), (1) << 14);

	OSL_DELAY(100);
	mod_phy_reg(pi, 0x448, (0x1 << 14), (0) << 14);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
	pi_lcn->lcnphy_noise_samples = LCNPHY_NOISE_SAMPLES_DEFAULT;
	wlc_lcnphy_calib_modes(pi, PHY_PERICAL_PHYINIT);
}

static void
wlc_lcnphy_tx_iqlo_loopback(phy_info_t *pi, uint16 *values_to_save)
{
	uint16 vmid;
	int i;
	for (i = 0; i < 20; i++) {
		values_to_save[i] =
		    read_radio_reg(pi, iqlo_loopback_rf_regs[i]);
	}

	mod_phy_reg(pi, 0x44c, (0x1 << 12), 1 << 12);
	mod_phy_reg(pi, 0x44d, (0x1 << 14), 1 << 14);

	mod_phy_reg(pi, 0x44c, (0x1 << 11), 1 << 11);
	mod_phy_reg(pi, 0x44d, (0x1 << 13), 0 << 13);

	mod_phy_reg(pi, 0x43b, (0x1 << 1), 1 << 1);
	mod_phy_reg(pi, 0x43c, (0x1 << 1), 0 << 1);

	mod_phy_reg(pi, 0x43b, (0x1 << 0), 1 << 0);
	mod_phy_reg(pi, 0x43c, (0x1 << 0), 0 << 0);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		and_radio_reg(pi, RADIO_2064_REG03A, 0xFD);
	else
		and_radio_reg(pi, RADIO_2064_REG03A, 0xF9);
	or_radio_reg(pi, RADIO_2064_REG11A, 0x1);

	or_radio_reg(pi, RADIO_2064_REG036, 0x01);
	or_radio_reg(pi, RADIO_2064_REG11A, 0x18);
	OSL_DELAY(20);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0);
		else
			or_radio_reg(pi, RADIO_2064_REG03A, 1);
	} else {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 3, 1);
		else
			or_radio_reg(pi, RADIO_2064_REG03A, 0x3);
	}

	OSL_DELAY(20);

	write_radio_reg(pi, RADIO_2064_REG025, 0xF);
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG028, 0xF, 0x4);
		else
			mod_radio_reg(pi, RADIO_2064_REG028, 0xF, 0x6);
	} else {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1e, 0x4 << 1);
		else
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1e, 0x6 << 1);
	}

	OSL_DELAY(20);

	write_radio_reg(pi, RADIO_2064_REG005, 0x8);
	or_radio_reg(pi, RADIO_2064_REG112, 0x80);
	OSL_DELAY(20);

	or_radio_reg(pi, RADIO_2064_REG0FF, 0x10);
	or_radio_reg(pi, RADIO_2064_REG11F, 0x44);
	OSL_DELAY(20);

	or_radio_reg(pi, RADIO_2064_REG00B, 0x7);
	or_radio_reg(pi, RADIO_2064_REG113, 0x10);
	OSL_DELAY(20);

	write_radio_reg(pi, RADIO_2064_REG007, 0x1);
	OSL_DELAY(20);

	vmid = 0x2A6;
	mod_radio_reg(pi, RADIO_2064_REG0FC, 0x3 << 0, (vmid >> 8) & 0x3);
	write_radio_reg(pi, RADIO_2064_REG0FD, (vmid & 0xff));
	or_radio_reg(pi, RADIO_2064_REG11F, 0x44);
	OSL_DELAY(20);

	or_radio_reg(pi, RADIO_2064_REG0FF, 0x10);
	OSL_DELAY(20);
	write_radio_reg(pi, RADIO_2064_REG012, 0x02);
	or_radio_reg(pi, RADIO_2064_REG112, 0x06);
	write_radio_reg(pi, RADIO_2064_REG036, 0x11);
	write_radio_reg(pi, RADIO_2064_REG059, 0xcc);
	write_radio_reg(pi, RADIO_2064_REG05C, 0x2e);
	write_radio_reg(pi, RADIO_2064_REG078, 0xd7);
	write_radio_reg(pi, RADIO_2064_REG092, 0x15);
}

static void
wlc_lcnphy_samp_cap(phy_info_t *pi, int clip_detect_algo, uint16 thresh,
		    int16 *ptr, int mode)
{
	uint32 curval1, curval2, stpptr, curptr, strptr, val;
	uint16 sslpnCalibClkEnCtrl, timer;
	uint16 old_sslpnCalibClkEnCtrl;
	int16 imag, real;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	timer = 0;
	old_sslpnCalibClkEnCtrl = read_phy_reg(pi, 0x6da);

	curval1 = R_REG(pi->sh->osh, &pi->regs->psm_corectlsts);
	ptr[130] = 0;
	W_REG(pi->sh->osh, &pi->regs->psm_corectlsts, ((1 << 6) | curval1));

	W_REG(pi->sh->osh, &pi->regs->smpl_clct_strptr, 0x7E00);
	W_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr, 0x8000);
	OSL_DELAY(20);
	curval2 = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, curval2 | 0x30);

	write_phy_reg(pi, 0x555, 0x0);
	write_phy_reg(pi, 0x5a6, 0x5);

	write_phy_reg(pi, 0x5a2, (uint16) (mode | mode << 6));
	write_phy_reg(pi, 0x5cf, 3);
	write_phy_reg(pi, 0x5a5, 0x3);
	write_phy_reg(pi, 0x583, 0x0);
	write_phy_reg(pi, 0x584, 0x0);
	write_phy_reg(pi, 0x585, 0x0fff);
	write_phy_reg(pi, 0x586, 0x0000);

	write_phy_reg(pi, 0x580, 0x4501);

	sslpnCalibClkEnCtrl = read_phy_reg(pi, 0x6da);
	write_phy_reg(pi, 0x6da, (uint32) (sslpnCalibClkEnCtrl | 0x2008));
	stpptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr);
	curptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
	do {
		OSL_DELAY(10);
		curptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
		timer++;
	} while ((curptr != stpptr) && (timer < 500));

	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, 0x2);
	strptr = 0x7E00;
	W_REG(pi->sh->osh, &pi->regs->tplatewrptr, strptr);
	while (strptr < 0x8000) {
		val = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
		imag = ((val >> 16) & 0x3ff);
		real = ((val) & 0x3ff);
		if (imag > 511) {
			imag -= 1024;
		}
		if (real > 511) {
			real -= 1024;
		}
		if (pi_lcn->lcnphy_iqcal_swp_dis)
			ptr[(strptr - 0x7E00) / 4] = real;
		else
			ptr[(strptr - 0x7E00) / 4] = imag;
		if (clip_detect_algo) {
			if (imag > thresh || imag < -thresh) {
				strptr = 0x8000;
				ptr[130] = 1;
			}
		}
		strptr += 4;
	}

	write_phy_reg(pi, 0x6da, old_sslpnCalibClkEnCtrl);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, curval2);
	W_REG(pi->sh->osh, &pi->regs->psm_corectlsts, curval1);
}

static void wlc_lcnphy_tx_iqlo_soft_cal_full(phy_info_t *pi)
{
	lcnphy_unsign16_struct iqcc0, locc2, locc3, locc4;

	wlc_lcnphy_set_cc(pi, 0, 0, 0);
	wlc_lcnphy_set_cc(pi, 2, 0, 0);
	wlc_lcnphy_set_cc(pi, 3, 0, 0);
	wlc_lcnphy_set_cc(pi, 4, 0, 0);

	wlc_lcnphy_a1(pi, 4, 0, 0);
	wlc_lcnphy_a1(pi, 3, 0, 0);
	wlc_lcnphy_a1(pi, 2, 3, 2);
	wlc_lcnphy_a1(pi, 0, 5, 8);
	wlc_lcnphy_a1(pi, 2, 2, 1);
	wlc_lcnphy_a1(pi, 0, 4, 3);

	iqcc0 = wlc_lcnphy_get_cc(pi, 0);
	locc2 = wlc_lcnphy_get_cc(pi, 2);
	locc3 = wlc_lcnphy_get_cc(pi, 3);
	locc4 = wlc_lcnphy_get_cc(pi, 4);
}

static void
wlc_lcnphy_set_cc(phy_info_t *pi, int cal_type, int16 coeff_x, int16 coeff_y)
{
	uint16 di0dq0;
	uint16 x, y, data_rf;
	int k;
	switch (cal_type) {
	case 0:
		wlc_lcnphy_set_tx_iqcc(pi, coeff_x, coeff_y);
		break;
	case 2:
		di0dq0 = (coeff_x & 0xff) << 8 | (coeff_y & 0xff);
		wlc_lcnphy_set_tx_locc(pi, di0dq0);
		break;
	case 3:
		k = wlc_lcnphy_calc_floor(coeff_x, 0);
		y = 8 + k;
		k = wlc_lcnphy_calc_floor(coeff_x, 1);
		x = 8 - k;
		data_rf = (x * 16 + y);
		write_radio_reg(pi, RADIO_2064_REG089, data_rf);
		k = wlc_lcnphy_calc_floor(coeff_y, 0);
		y = 8 + k;
		k = wlc_lcnphy_calc_floor(coeff_y, 1);
		x = 8 - k;
		data_rf = (x * 16 + y);
		write_radio_reg(pi, RADIO_2064_REG08A, data_rf);
		break;
	case 4:
		k = wlc_lcnphy_calc_floor(coeff_x, 0);
		y = 8 + k;
		k = wlc_lcnphy_calc_floor(coeff_x, 1);
		x = 8 - k;
		data_rf = (x * 16 + y);
		write_radio_reg(pi, RADIO_2064_REG08B, data_rf);
		k = wlc_lcnphy_calc_floor(coeff_y, 0);
		y = 8 + k;
		k = wlc_lcnphy_calc_floor(coeff_y, 1);
		x = 8 - k;
		data_rf = (x * 16 + y);
		write_radio_reg(pi, RADIO_2064_REG08C, data_rf);
		break;
	}
}

static lcnphy_unsign16_struct wlc_lcnphy_get_cc(phy_info_t *pi, int cal_type)
{
	uint16 a, b, didq;
	uint8 di0, dq0, ei, eq, fi, fq;
	lcnphy_unsign16_struct cc;
	cc.re = 0;
	cc.im = 0;
	switch (cal_type) {
	case 0:
		wlc_lcnphy_get_tx_iqcc(pi, &a, &b);
		cc.re = a;
		cc.im = b;
		break;
	case 2:
		didq = wlc_lcnphy_get_tx_locc(pi);
		di0 = (((didq & 0xff00) << 16) >> 24);
		dq0 = (((didq & 0x00ff) << 24) >> 24);
		cc.re = (uint16) di0;
		cc.im = (uint16) dq0;
		break;
	case 3:
		wlc_lcnphy_get_radio_loft(pi, &ei, &eq, &fi, &fq);
		cc.re = (uint16) ei;
		cc.im = (uint16) eq;
		break;
	case 4:
		wlc_lcnphy_get_radio_loft(pi, &ei, &eq, &fi, &fq);
		cc.re = (uint16) fi;
		cc.im = (uint16) fq;
		break;
	}
	return cc;
}

static void
wlc_lcnphy_a1(phy_info_t *pi, int cal_type, int num_levels, int step_size_lg2)
{
	const lcnphy_spb_tone_t *phy_c1;
	lcnphy_spb_tone_t phy_c2;
	lcnphy_unsign16_struct phy_c3;
	int phy_c4, phy_c5, k, l, j, phy_c6;
	uint16 phy_c7, phy_c8, phy_c9;
	int16 phy_c10, phy_c11, phy_c12, phy_c13, phy_c14, phy_c15, phy_c16;
	int16 *ptr, phy_c17;
	int32 phy_c18, phy_c19;
	uint32 phy_c20, phy_c21;
	bool phy_c22, phy_c23, phy_c24, phy_c25;
	uint16 phy_c26, phy_c27;
	uint16 phy_c28, phy_c29, phy_c30;
	uint16 phy_c31;
	uint16 *phy_c32;
	phy_c21 = 0;
	phy_c10 = phy_c13 = phy_c14 = phy_c8 = 0;
	ptr = MALLOC(pi->sh->osh, sizeof(int16) * 131);
	if (NULL == ptr) {
		return;
	}

	phy_c32 = MALLOC(pi->sh->osh, sizeof(uint16) * 20);
	if (NULL == phy_c32) {
		return;
	}
	phy_c26 = read_phy_reg(pi, 0x6da);
	phy_c27 = read_phy_reg(pi, 0x6db);
	phy_c31 = read_radio_reg(pi, RADIO_2064_REG026);
	write_phy_reg(pi, 0x93d, 0xC0);

	wlc_lcnphy_start_tx_tone(pi, 3750, 88, 0);
	write_phy_reg(pi, 0x6da, 0xffff);
	or_phy_reg(pi, 0x6db, 0x3);

	wlc_lcnphy_tx_iqlo_loopback(pi, phy_c32);
	OSL_DELAY(500);
	phy_c28 = read_phy_reg(pi, 0x938);
	phy_c29 = read_phy_reg(pi, 0x4d7);
	phy_c30 = read_phy_reg(pi, 0x4d8);
	or_phy_reg(pi, 0x938, 0x1 << 2);
	or_phy_reg(pi, 0x4d7, 0x1 << 2);
	or_phy_reg(pi, 0x4d7, 0x1 << 3);
	mod_phy_reg(pi, 0x4d7, (0x7 << 12), 0x2 << 12);
	or_phy_reg(pi, 0x4d8, 1 << 0);
	or_phy_reg(pi, 0x4d8, 1 << 1);
	mod_phy_reg(pi, 0x4d8, (0x3ff << 2), 0x23A << 2);
	mod_phy_reg(pi, 0x4d8, (0x7 << 12), 0x7 << 12);
	phy_c1 = &lcnphy_spb_tone_3750[0];
	phy_c4 = 32;

	if (num_levels == 0) {
		if (cal_type != 0) {
			num_levels = 4;
		} else {
			num_levels = 9;
		}
	}
	if (step_size_lg2 == 0) {
		if (cal_type != 0) {
			step_size_lg2 = 3;
		} else {
			step_size_lg2 = 8;
		}
	}

	phy_c7 = (1 << step_size_lg2);
	phy_c3 = wlc_lcnphy_get_cc(pi, cal_type);
	phy_c15 = (int16) phy_c3.re;
	phy_c16 = (int16) phy_c3.im;
	if (cal_type == 2) {
		if (phy_c3.re > 127)
			phy_c15 = phy_c3.re - 256;
		if (phy_c3.im > 127)
			phy_c16 = phy_c3.im - 256;
	}
	wlc_lcnphy_set_cc(pi, cal_type, phy_c15, phy_c16);
	OSL_DELAY(20);
	for (phy_c8 = 0; phy_c7 != 0 && phy_c8 < num_levels; phy_c8++) {
		phy_c23 = 1;
		phy_c22 = 0;
		switch (cal_type) {
		case 0:
			phy_c10 = 511;
			break;
		case 2:
			phy_c10 = 127;
			break;
		case 3:
			phy_c10 = 15;
			break;
		case 4:
			phy_c10 = 15;
			break;
		}

		phy_c9 = read_phy_reg(pi, 0x93d);
		phy_c9 = 2 * phy_c9;
		phy_c24 = 0;
		phy_c5 = 7;
		phy_c25 = 1;
		while (1) {
			write_radio_reg(pi, RADIO_2064_REG026,
					(phy_c5 & 0x7) | ((phy_c5 & 0x7) << 4));
			OSL_DELAY(50);
			phy_c22 = 0;
			ptr[130] = 0;
			wlc_lcnphy_samp_cap(pi, 1, phy_c9, &ptr[0], 2);
			if (ptr[130] == 1)
				phy_c22 = 1;
			if (phy_c22)
				phy_c5 -= 1;
			if ((phy_c22 != phy_c24) && (!phy_c25))
				break;
			if (!phy_c22)
				phy_c5 += 1;
			if (phy_c5 <= 0 || phy_c5 >= 7)
				break;
			phy_c24 = phy_c22;
			phy_c25 = 0;
		}

		if (phy_c5 < 0)
			phy_c5 = 0;
		else if (phy_c5 > 7)
			phy_c5 = 7;

		for (k = -phy_c7; k <= phy_c7; k += phy_c7) {
			for (l = -phy_c7; l <= phy_c7; l += phy_c7) {
				phy_c11 = phy_c15 + k;
				phy_c12 = phy_c16 + l;

				if (phy_c11 < -phy_c10)
					phy_c11 = -phy_c10;
				else if (phy_c11 > phy_c10)
					phy_c11 = phy_c10;
				if (phy_c12 < -phy_c10)
					phy_c12 = -phy_c10;
				else if (phy_c12 > phy_c10)
					phy_c12 = phy_c10;
				wlc_lcnphy_set_cc(pi, cal_type, phy_c11,
						  phy_c12);
				OSL_DELAY(20);
				wlc_lcnphy_samp_cap(pi, 0, 0, ptr, 2);

				phy_c18 = 0;
				phy_c19 = 0;
				for (j = 0; j < 128; j++) {
					if (cal_type != 0) {
						phy_c6 = j % phy_c4;
					} else {
						phy_c6 = (2 * j) % phy_c4;
					}
					phy_c2.re = phy_c1[phy_c6].re;
					phy_c2.im = phy_c1[phy_c6].im;
					phy_c17 = ptr[j];
					phy_c18 = phy_c18 + phy_c17 * phy_c2.re;
					phy_c19 = phy_c19 + phy_c17 * phy_c2.im;
				}

				phy_c18 = phy_c18 >> 10;
				phy_c19 = phy_c19 >> 10;
				phy_c20 =
				    ((phy_c18 * phy_c18) + (phy_c19 * phy_c19));

				if (phy_c23 || phy_c20 < phy_c21) {
					phy_c21 = phy_c20;
					phy_c13 = phy_c11;
					phy_c14 = phy_c12;
				}
				phy_c23 = 0;
			}
		}
		phy_c23 = 1;
		phy_c15 = phy_c13;
		phy_c16 = phy_c14;
		phy_c7 = phy_c7 >> 1;
		wlc_lcnphy_set_cc(pi, cal_type, phy_c15, phy_c16);
		OSL_DELAY(20);
	}
	goto cleanup;
 cleanup:
	wlc_lcnphy_tx_iqlo_loopback_cleanup(pi, phy_c32);
	wlc_lcnphy_stop_tx_tone(pi);
	write_phy_reg(pi, 0x6da, phy_c26);
	write_phy_reg(pi, 0x6db, phy_c27);
	write_phy_reg(pi, 0x938, phy_c28);
	write_phy_reg(pi, 0x4d7, phy_c29);
	write_phy_reg(pi, 0x4d8, phy_c30);
	write_radio_reg(pi, RADIO_2064_REG026, phy_c31);

	MFREE(pi->sh->osh, phy_c32, 20 * sizeof(uint16));
	MFREE(pi->sh->osh, ptr, 131 * sizeof(int16));
}

static void
wlc_lcnphy_tx_iqlo_loopback_cleanup(phy_info_t *pi, uint16 *values_to_save)
{
	int i;

	and_phy_reg(pi, 0x44c, 0x0 >> 11);

	and_phy_reg(pi, 0x43b, 0xC);

	for (i = 0; i < 20; i++) {
		write_radio_reg(pi, iqlo_loopback_rf_regs[i],
				values_to_save[i]);
	}
}

static void
WLBANDINITFN(wlc_lcnphy_load_tx_gain_table) (phy_info_t *pi,
					     const lcnphy_tx_gain_tbl_entry *
					     gain_table) {
	uint32 j;
	phytbl_info_t tab;
	uint32 val;
	uint16 pa_gain;
	uint16 gm_gain;

	if (CHSPEC_IS5G(pi->radio_chanspec))
		pa_gain = 0x70;
	else
		pa_gain = 0x70;

	if (pi->sh->boardflags & BFL_FEM)
		pa_gain = 0x10;
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_len = 1;
	tab.tbl_ptr = &val;

	for (j = 0; j < 128; j++) {
		gm_gain = gain_table[j].gm;
		val = (((uint32) pa_gain << 24) |
		       (gain_table[j].pad << 16) |
		       (gain_table[j].pga << 8) | gm_gain);

		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + j;
		wlc_lcnphy_write_table(pi, &tab);

		val = (gain_table[j].dac << 28) | (gain_table[j].bb_mult << 20);
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + j;
		wlc_lcnphy_write_table(pi, &tab);
	}
}

static void wlc_lcnphy_load_rfpower(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 val, bbmult, rfgain;
	uint8 index;
	uint8 scale_factor = 1;
	int16 temp, temp1, temp2, qQ, qQ1, qQ2, shift;

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;
	tab.tbl_len = 1;

	for (index = 0; index < 128; index++) {
		tab.tbl_ptr = &bbmult;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + index;
		wlc_lcnphy_read_table(pi, &tab);
		bbmult = bbmult >> 20;

		tab.tbl_ptr = &rfgain;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + index;
		wlc_lcnphy_read_table(pi, &tab);

		qm_log10((int32) (bbmult), 0, &temp1, &qQ1);
		qm_log10((int32) (1 << 6), 0, &temp2, &qQ2);

		if (qQ1 < qQ2) {
			temp2 = qm_shr16(temp2, qQ2 - qQ1);
			qQ = qQ1;
		} else {
			temp1 = qm_shr16(temp1, qQ1 - qQ2);
			qQ = qQ2;
		}
		temp = qm_sub16(temp1, temp2);

		if (qQ >= 4)
			shift = qQ - 4;
		else
			shift = 4 - qQ;

		val = (((index << shift) + (5 * temp) +
			(1 << (scale_factor + shift - 3))) >> (scale_factor +
							       shift - 2));

		tab.tbl_ptr = &val;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_PWR_OFFSET + index;
		wlc_lcnphy_write_table(pi, &tab);
	}
}

static void WLBANDINITFN(wlc_lcnphy_tbl_init) (phy_info_t *pi)
{
	uint idx;
	uint8 phybw40;
	phytbl_info_t tab;
	uint32 val;

	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	for (idx = 0; idx < dot11lcnphytbl_info_sz_rev0; idx++) {
		wlc_lcnphy_write_table(pi, &dot11lcnphytbl_info_rev0[idx]);
	}

	if (pi->sh->boardflags & BFL_FEM_BT) {
		tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
		tab.tbl_width = 16;
		tab.tbl_ptr = &val;
		tab.tbl_len = 1;
		val = 100;
		tab.tbl_offset = 4;
		wlc_lcnphy_write_table(pi, &tab);
	}

	tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;
	tab.tbl_ptr = &val;
	tab.tbl_len = 1;

	val = 114;
	tab.tbl_offset = 0;
	wlc_lcnphy_write_table(pi, &tab);

	val = 130;
	tab.tbl_offset = 1;
	wlc_lcnphy_write_table(pi, &tab);

	val = 6;
	tab.tbl_offset = 8;
	wlc_lcnphy_write_table(pi, &tab);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		if (pi->sh->boardflags & BFL_FEM)
			wlc_lcnphy_load_tx_gain_table(pi,
						      dot11lcnphy_2GHz_extPA_gaintable_rev0);
		else
			wlc_lcnphy_load_tx_gain_table(pi,
						      dot11lcnphy_2GHz_gaintable_rev0);
	}

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			for (idx = 0;
			     idx < dot11lcnphytbl_rx_gain_info_2G_rev2_sz;
			     idx++)
				if (pi->sh->boardflags & BFL_EXTLNA)
					wlc_lcnphy_write_table(pi,
							       &dot11lcnphytbl_rx_gain_info_extlna_2G_rev2
							       [idx]);
				else
					wlc_lcnphy_write_table(pi,
							       &dot11lcnphytbl_rx_gain_info_2G_rev2
							       [idx]);
		} else {
			for (idx = 0;
			     idx < dot11lcnphytbl_rx_gain_info_5G_rev2_sz;
			     idx++)
				if (pi->sh->boardflags & BFL_EXTLNA_5GHz)
					wlc_lcnphy_write_table(pi,
							       &dot11lcnphytbl_rx_gain_info_extlna_5G_rev2
							       [idx]);
				else
					wlc_lcnphy_write_table(pi,
							       &dot11lcnphytbl_rx_gain_info_5G_rev2
							       [idx]);
		}
	}

	if ((pi->sh->boardflags & BFL_FEM)
	    && !(pi->sh->boardflags & BFL_FEM_BT))
		wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4313_epa);
	else if (pi->sh->boardflags & BFL_FEM_BT) {
		if (pi->sh->boardrev < 0x1250)
			wlc_lcnphy_write_table(pi,
					       &dot11lcn_sw_ctrl_tbl_info_4313_bt_epa);
		else
			wlc_lcnphy_write_table(pi,
					       &dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250);
	} else
		wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4313);

	wlc_lcnphy_load_rfpower(pi);

	wlc_lcnphy_clear_papd_comptable(pi);
}

static void WLBANDINITFN(wlc_lcnphy_rev0_baseband_init) (phy_info_t *pi)
{
	uint16 afectrl1;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	write_radio_reg(pi, RADIO_2064_REG11C, 0x0);

	write_phy_reg(pi, 0x43b, 0x0);
	write_phy_reg(pi, 0x43c, 0x0);
	write_phy_reg(pi, 0x44c, 0x0);
	write_phy_reg(pi, 0x4e6, 0x0);
	write_phy_reg(pi, 0x4f9, 0x0);
	write_phy_reg(pi, 0x4b0, 0x0);
	write_phy_reg(pi, 0x938, 0x0);
	write_phy_reg(pi, 0x4b0, 0x0);
	write_phy_reg(pi, 0x44e, 0);

	or_phy_reg(pi, 0x567, 0x03);

	or_phy_reg(pi, 0x44a, 0x44);
	write_phy_reg(pi, 0x44a, 0x80);

	if (!(pi->sh->boardflags & BFL_FEM))
		wlc_lcnphy_set_tx_pwr_by_index(pi, 52);

	if (0) {
		afectrl1 = 0;
		afectrl1 = (uint16) ((pi_lcn->lcnphy_rssi_vf) |
				     (pi_lcn->lcnphy_rssi_vc << 4) | (pi_lcn->
								      lcnphy_rssi_gs
								      << 10));
		write_phy_reg(pi, 0x43e, afectrl1);
	}

	mod_phy_reg(pi, 0x634, (0xff << 0), 0xC << 0);
	if (pi->sh->boardflags & BFL_FEM) {
		mod_phy_reg(pi, 0x634, (0xff << 0), 0xA << 0);

		write_phy_reg(pi, 0x910, 0x1);
	}

	mod_phy_reg(pi, 0x448, (0x3 << 8), 1 << 8);
	mod_phy_reg(pi, 0x608, (0xff << 0), 0x17 << 0);
	mod_phy_reg(pi, 0x604, (0x7ff << 0), 0x3EA << 0);

}

static void WLBANDINITFN(wlc_lcnphy_rev2_baseband_init) (phy_info_t *pi)
{
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		mod_phy_reg(pi, 0x416, (0xff << 0), 80 << 0);

		mod_phy_reg(pi, 0x416, (0xff << 8), 80 << 8);
	}
}

static void wlc_lcnphy_agc_temp_init(phy_info_t *pi)
{
	int16 temp;
	phytbl_info_t tab;
	uint32 tableBuffer[2];
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	temp = (int16) read_phy_reg(pi, 0x4df);
	pi_lcn->lcnphy_ofdmgainidxtableoffset = (temp & (0xff << 0)) >> 0;

	if (pi_lcn->lcnphy_ofdmgainidxtableoffset > 127)
		pi_lcn->lcnphy_ofdmgainidxtableoffset -= 256;

	pi_lcn->lcnphy_dsssgainidxtableoffset = (temp & (0xff << 8)) >> 8;

	if (pi_lcn->lcnphy_dsssgainidxtableoffset > 127)
		pi_lcn->lcnphy_dsssgainidxtableoffset -= 256;

	tab.tbl_ptr = tableBuffer;
	tab.tbl_len = 2;
	tab.tbl_id = 17;
	tab.tbl_offset = 59;
	tab.tbl_width = 32;
	wlc_lcnphy_read_table(pi, &tab);

	if (tableBuffer[0] > 63)
		tableBuffer[0] -= 128;
	pi_lcn->lcnphy_tr_R_gain_val = tableBuffer[0];

	if (tableBuffer[1] > 63)
		tableBuffer[1] -= 128;
	pi_lcn->lcnphy_tr_T_gain_val = tableBuffer[1];

	temp = (int16) (read_phy_reg(pi, 0x434)
			& (0xff << 0));
	if (temp > 127)
		temp -= 256;
	pi_lcn->lcnphy_input_pwr_offset_db = (int8) temp;

	pi_lcn->lcnphy_Med_Low_Gain_db = (read_phy_reg(pi, 0x424)
					  & (0xff << 8))
	    >> 8;
	pi_lcn->lcnphy_Very_Low_Gain_db = (read_phy_reg(pi, 0x425)
					   & (0xff << 0))
	    >> 0;

	tab.tbl_ptr = tableBuffer;
	tab.tbl_len = 2;
	tab.tbl_id = LCNPHY_TBL_ID_GAIN_IDX;
	tab.tbl_offset = 28;
	tab.tbl_width = 32;
	wlc_lcnphy_read_table(pi, &tab);

	pi_lcn->lcnphy_gain_idx_14_lowword = tableBuffer[0];
	pi_lcn->lcnphy_gain_idx_14_hiword = tableBuffer[1];

}

static void WLBANDINITFN(wlc_lcnphy_bu_tweaks) (phy_info_t *pi)
{
	if (NORADIO_ENAB(pi->pubpi))
		return;

	or_phy_reg(pi, 0x805, 0x1);

	mod_phy_reg(pi, 0x42f, (0x7 << 0), (0x3) << 0);

	mod_phy_reg(pi, 0x030, (0x7 << 0), (0x3) << 0);

	write_phy_reg(pi, 0x414, 0x1e10);
	write_phy_reg(pi, 0x415, 0x0640);

	mod_phy_reg(pi, 0x4df, (0xff << 8), -9 << 8);

	or_phy_reg(pi, 0x44a, 0x44);
	write_phy_reg(pi, 0x44a, 0x80);
	mod_phy_reg(pi, 0x434, (0xff << 0), (0xFD) << 0);

	mod_phy_reg(pi, 0x420, (0xff << 0), (16) << 0);

	if (!(pi->sh->boardrev < 0x1204))
		mod_radio_reg(pi, RADIO_2064_REG09B, 0xF0, 0xF0);

	write_phy_reg(pi, 0x7d6, 0x0902);
	mod_phy_reg(pi, 0x429, (0xf << 0), (0x9) << 0);

	mod_phy_reg(pi, 0x429, (0x3f << 4), (0xe) << 4);

	if (LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		mod_phy_reg(pi, 0x423, (0xff << 0), (0x46) << 0);

		mod_phy_reg(pi, 0x411, (0xff << 0), (1) << 0);

		mod_phy_reg(pi, 0x434, (0xff << 0), (0xFF) << 0);

		mod_phy_reg(pi, 0x656, (0xf << 0), (2) << 0);

		mod_phy_reg(pi, 0x44d, (0x1 << 2), (1) << 2);

		mod_radio_reg(pi, RADIO_2064_REG0F7, 0x4, 0x4);
		mod_radio_reg(pi, RADIO_2064_REG0F1, 0x3, 0);
		mod_radio_reg(pi, RADIO_2064_REG0F2, 0xF8, 0x90);
		mod_radio_reg(pi, RADIO_2064_REG0F3, 0x3, 0x2);
		mod_radio_reg(pi, RADIO_2064_REG0F3, 0xf0, 0xa0);

		mod_radio_reg(pi, RADIO_2064_REG11F, 0x2, 0x2);

		wlc_lcnphy_clear_tx_power_offsets(pi);
		mod_phy_reg(pi, 0x4d0, (0x1ff << 6), (10) << 6);

	}
}

static void WLBANDINITFN(wlc_lcnphy_baseband_init) (phy_info_t *pi)
{

	wlc_lcnphy_tbl_init(pi);
	wlc_lcnphy_rev0_baseband_init(pi);
	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		wlc_lcnphy_rev2_baseband_init(pi);
	wlc_lcnphy_bu_tweaks(pi);
}

static void WLBANDINITFN(wlc_radio_2064_init) (phy_info_t *pi)
{
	uint32 i;
	lcnphy_radio_regs_t *lcnphyregs = NULL;

	lcnphyregs = lcnphy_radio_regs_2064;

	for (i = 0; lcnphyregs[i].address != 0xffff; i++)
		if (CHSPEC_IS5G(pi->radio_chanspec) && lcnphyregs[i].do_init_a)
			write_radio_reg(pi,
					((lcnphyregs[i].address & 0x3fff) |
					 RADIO_DEFAULT_CORE),
					(uint16) lcnphyregs[i].init_a);
		else if (lcnphyregs[i].do_init_g)
			write_radio_reg(pi,
					((lcnphyregs[i].address & 0x3fff) |
					 RADIO_DEFAULT_CORE),
					(uint16) lcnphyregs[i].init_g);

	write_radio_reg(pi, RADIO_2064_REG032, 0x62);
	write_radio_reg(pi, RADIO_2064_REG033, 0x19);

	write_radio_reg(pi, RADIO_2064_REG090, 0x10);

	write_radio_reg(pi, RADIO_2064_REG010, 0x00);

	if (LCNREV_IS(pi->pubpi.phy_rev, 1)) {

		write_radio_reg(pi, RADIO_2064_REG060, 0x7f);
		write_radio_reg(pi, RADIO_2064_REG061, 0x72);
		write_radio_reg(pi, RADIO_2064_REG062, 0x7f);
	}

	write_radio_reg(pi, RADIO_2064_REG01D, 0x02);
	write_radio_reg(pi, RADIO_2064_REG01E, 0x06);

	mod_phy_reg(pi, 0x4ea, (0x7 << 0), 0 << 0);

	mod_phy_reg(pi, 0x4ea, (0x7 << 3), 1 << 3);

	mod_phy_reg(pi, 0x4ea, (0x7 << 6), 2 << 6);

	mod_phy_reg(pi, 0x4ea, (0x7 << 9), 3 << 9);

	mod_phy_reg(pi, 0x4ea, (0x7 << 12), 4 << 12);

	write_phy_reg(pi, 0x4ea, 0x4688);

	mod_phy_reg(pi, 0x4eb, (0x7 << 0), 2 << 0);

	mod_phy_reg(pi, 0x4eb, (0x7 << 6), 0 << 6);

	mod_phy_reg(pi, 0x46a, (0xffff << 0), 25 << 0);

	wlc_lcnphy_set_tx_locc(pi, 0);

	wlc_lcnphy_rcal(pi);

	wlc_lcnphy_rc_cal(pi);
}

static void WLBANDINITFN(wlc_lcnphy_radio_init) (phy_info_t *pi)
{
	if (NORADIO_ENAB(pi->pubpi))
		return;

	wlc_radio_2064_init(pi);
}

static void wlc_lcnphy_rcal(phy_info_t *pi)
{
	uint8 rcal_value;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	and_radio_reg(pi, RADIO_2064_REG05B, 0xfD);

	or_radio_reg(pi, RADIO_2064_REG004, 0x40);
	or_radio_reg(pi, RADIO_2064_REG120, 0x10);

	or_radio_reg(pi, RADIO_2064_REG078, 0x80);
	or_radio_reg(pi, RADIO_2064_REG129, 0x02);

	or_radio_reg(pi, RADIO_2064_REG057, 0x01);

	or_radio_reg(pi, RADIO_2064_REG05B, 0x02);
	OSL_DELAY(5000);
	SPINWAIT(!wlc_radio_2064_rcal_done(pi), 10 * 1000 * 1000);

	if (wlc_radio_2064_rcal_done(pi)) {
		rcal_value = (uint8) read_radio_reg(pi, RADIO_2064_REG05C);
		rcal_value = rcal_value & 0x1f;
	}

	and_radio_reg(pi, RADIO_2064_REG05B, 0xfD);

	and_radio_reg(pi, RADIO_2064_REG057, 0xFE);
}

static void wlc_lcnphy_rc_cal(phy_info_t *pi)
{
	uint8 dflt_rc_cal_val;
	uint16 flt_val;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	dflt_rc_cal_val = 7;
	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		dflt_rc_cal_val = 11;
	flt_val =
	    (dflt_rc_cal_val << 10) | (dflt_rc_cal_val << 5) |
	    (dflt_rc_cal_val);
	write_phy_reg(pi, 0x933, flt_val);
	write_phy_reg(pi, 0x934, flt_val);
	write_phy_reg(pi, 0x935, flt_val);
	write_phy_reg(pi, 0x936, flt_val);
	write_phy_reg(pi, 0x937, (flt_val & 0x1FF));

	return;
}

static bool BCMATTACHFN(wlc_phy_txpwr_srom_read_lcnphy) (phy_info_t *pi)
{
	int8 txpwr = 0;
	int i;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		uint16 cckpo = 0;
		uint32 offset_ofdm, offset_mcs;

		pi_lcn->lcnphy_tr_isolation_mid =
		    (uint8) PHY_GETINTVAR(pi, "triso2g");

		pi_lcn->lcnphy_rx_power_offset =
		    (uint8) PHY_GETINTVAR(pi, "rxpo2g");

		pi->txpa_2g[0] = (int16) PHY_GETINTVAR(pi, "pa0b0");
		pi->txpa_2g[1] = (int16) PHY_GETINTVAR(pi, "pa0b1");
		pi->txpa_2g[2] = (int16) PHY_GETINTVAR(pi, "pa0b2");

		pi_lcn->lcnphy_rssi_vf = (uint8) PHY_GETINTVAR(pi, "rssismf2g");
		pi_lcn->lcnphy_rssi_vc = (uint8) PHY_GETINTVAR(pi, "rssismc2g");
		pi_lcn->lcnphy_rssi_gs = (uint8) PHY_GETINTVAR(pi, "rssisav2g");

		{
			pi_lcn->lcnphy_rssi_vf_lowtemp = pi_lcn->lcnphy_rssi_vf;
			pi_lcn->lcnphy_rssi_vc_lowtemp = pi_lcn->lcnphy_rssi_vc;
			pi_lcn->lcnphy_rssi_gs_lowtemp = pi_lcn->lcnphy_rssi_gs;

			pi_lcn->lcnphy_rssi_vf_hightemp =
			    pi_lcn->lcnphy_rssi_vf;
			pi_lcn->lcnphy_rssi_vc_hightemp =
			    pi_lcn->lcnphy_rssi_vc;
			pi_lcn->lcnphy_rssi_gs_hightemp =
			    pi_lcn->lcnphy_rssi_gs;
		}

		txpwr = (int8) PHY_GETINTVAR(pi, "maxp2ga0");
		pi->tx_srom_max_2g = txpwr;

		for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
			pi->txpa_2g_low_temp[i] = pi->txpa_2g[i];
			pi->txpa_2g_high_temp[i] = pi->txpa_2g[i];
		}

		cckpo = (uint16) PHY_GETINTVAR(pi, "cck2gpo");
		if (cckpo) {
			uint max_pwr_chan = txpwr;

			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = max_pwr_chan -
				    ((cckpo & 0xf) * 2);
				cckpo >>= 4;
			}

			offset_ofdm = (uint32) PHY_GETINTVAR(pi, "ofdm2gpo");
			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
				pi->tx_srom_max_rate_2g[i] = max_pwr_chan -
				    ((offset_ofdm & 0xf) * 2);
				offset_ofdm >>= 4;
			}
		} else {
			uint8 opo = 0;

			opo = (uint8) PHY_GETINTVAR(pi, "opo");

			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr;
			}

			offset_ofdm = (uint32) PHY_GETINTVAR(pi, "ofdm2gpo");

			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr -
				    ((offset_ofdm & 0xf) * 2);
				offset_ofdm >>= 4;
			}
			offset_mcs =
			    ((uint16) PHY_GETINTVAR(pi, "mcs2gpo1") << 16) |
			    (uint16) PHY_GETINTVAR(pi, "mcs2gpo0");
			pi_lcn->lcnphy_mcs20_po = offset_mcs;
			for (i = TXP_FIRST_SISO_MCS_20;
			     i <= TXP_LAST_SISO_MCS_20; i++) {
				pi->tx_srom_max_rate_2g[i] =
				    txpwr - ((offset_mcs & 0xf) * 2);
				offset_mcs >>= 4;
			}
		}

		pi_lcn->lcnphy_rawtempsense =
		    (uint16) PHY_GETINTVAR(pi, "rawtempsense");
		pi_lcn->lcnphy_measPower =
		    (uint8) PHY_GETINTVAR(pi, "measpower");
		pi_lcn->lcnphy_tempsense_slope =
		    (uint8) PHY_GETINTVAR(pi, "tempsense_slope");
		pi_lcn->lcnphy_hw_iqcal_en =
		    (bool) PHY_GETINTVAR(pi, "hw_iqcal_en");
		pi_lcn->lcnphy_iqcal_swp_dis =
		    (bool) PHY_GETINTVAR(pi, "iqcal_swp_dis");
		pi_lcn->lcnphy_tempcorrx =
		    (uint8) PHY_GETINTVAR(pi, "tempcorrx");
		pi_lcn->lcnphy_tempsense_option =
		    (uint8) PHY_GETINTVAR(pi, "tempsense_option");
		pi_lcn->lcnphy_freqoffset_corr =
		    (uint8) PHY_GETINTVAR(pi, "freqoffset_corr");
		if ((uint8) getintvar(pi->vars, "aa2g") > 1)
			wlc_phy_ant_rxdiv_set((wlc_phy_t *) pi,
					      (uint8) getintvar(pi->vars,
								"aa2g"));
	}
	pi_lcn->lcnphy_cck_dig_filt_type = -1;
	if (PHY_GETVAR(pi, "cckdigfilttype")) {
		int16 temp;
		temp = (int16) PHY_GETINTVAR(pi, "cckdigfilttype");
		if (temp >= 0) {
			pi_lcn->lcnphy_cck_dig_filt_type = temp;
		}
	}

	return TRUE;
}

void wlc_2064_vco_cal(phy_info_t *pi)
{
	uint8 calnrst;

	mod_radio_reg(pi, RADIO_2064_REG057, 1 << 3, 1 << 3);
	calnrst = (uint8) read_radio_reg(pi, RADIO_2064_REG056) & 0xf8;
	write_radio_reg(pi, RADIO_2064_REG056, calnrst);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2064_REG056, calnrst | 0x03);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2064_REG056, calnrst | 0x07);
	OSL_DELAY(300);
	mod_radio_reg(pi, RADIO_2064_REG057, 1 << 3, 0);
}

static void
wlc_lcnphy_radio_2064_channel_tune_4313(phy_info_t *pi, uint8 channel)
{
	uint i;
	const chan_info_2064_lcnphy_t *ci;
	uint8 rfpll_doubler = 0;
	uint8 pll_pwrup, pll_pwrup_ovr;
	fixed qFxtal, qFref, qFvco, qFcal;
	uint8 d15, d16, f16, e44, e45;
	uint32 div_int, div_frac, fvco3, fpfd, fref3, fcal_div;
	uint16 loop_bw, d30, setCount;
	if (NORADIO_ENAB(pi->pubpi))
		return;
	ci = &chan_info_2064_lcnphy[0];
	rfpll_doubler = 1;

	mod_radio_reg(pi, RADIO_2064_REG09D, 0x4, 0x1 << 2);

	write_radio_reg(pi, RADIO_2064_REG09E, 0xf);
	if (!rfpll_doubler) {
		loop_bw = PLL_2064_LOOP_BW;
		d30 = PLL_2064_D30;
	} else {
		loop_bw = PLL_2064_LOOP_BW_DOUBLER;
		d30 = PLL_2064_D30_DOUBLER;
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2064_lcnphy); i++)
			if (chan_info_2064_lcnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2064_lcnphy)) {
			return;
		}

		ci = &chan_info_2064_lcnphy[i];
	}

	write_radio_reg(pi, RADIO_2064_REG02A, ci->logen_buftune);

	mod_radio_reg(pi, RADIO_2064_REG030, 0x3, ci->logen_rccr_tx);

	mod_radio_reg(pi, RADIO_2064_REG091, 0x3, ci->txrf_mix_tune_ctrl);

	mod_radio_reg(pi, RADIO_2064_REG038, 0xf, ci->pa_input_tune_g);

	mod_radio_reg(pi, RADIO_2064_REG030, 0x3 << 2,
		      (ci->logen_rccr_rx) << 2);

	mod_radio_reg(pi, RADIO_2064_REG05E, 0xf, ci->pa_rxrf_lna1_freq_tune);

	mod_radio_reg(pi, RADIO_2064_REG05E, (0xf) << 4,
		      (ci->pa_rxrf_lna2_freq_tune) << 4);

	write_radio_reg(pi, RADIO_2064_REG06C, ci->rxrf_rxrf_spare1);

	pll_pwrup = (uint8) read_radio_reg(pi, RADIO_2064_REG044);
	pll_pwrup_ovr = (uint8) read_radio_reg(pi, RADIO_2064_REG12B);

	or_radio_reg(pi, RADIO_2064_REG044, 0x07);

	or_radio_reg(pi, RADIO_2064_REG12B, (0x07) << 1);
	e44 = 0;
	e45 = 0;

	fpfd = rfpll_doubler ? (pi->xtalfreq << 1) : (pi->xtalfreq);
	if (pi->xtalfreq > 26000000)
		e44 = 1;
	if (pi->xtalfreq > 52000000)
		e45 = 1;
	if (e44 == 0)
		fcal_div = 1;
	else if (e45 == 0)
		fcal_div = 2;
	else
		fcal_div = 4;
	fvco3 = (ci->freq * 3);
	fref3 = 2 * fpfd;

	qFxtal = wlc_lcnphy_qdiv_roundup(pi->xtalfreq, PLL_2064_MHZ, 16);
	qFref = wlc_lcnphy_qdiv_roundup(fpfd, PLL_2064_MHZ, 16);
	qFcal = pi->xtalfreq * fcal_div / PLL_2064_MHZ;
	qFvco = wlc_lcnphy_qdiv_roundup(fvco3, 2, 16);

	write_radio_reg(pi, RADIO_2064_REG04F, 0x02);

	d15 = (pi->xtalfreq * fcal_div * 4 / 5) / PLL_2064_MHZ - 1;
	write_radio_reg(pi, RADIO_2064_REG052, (0x07 & (d15 >> 2)));
	write_radio_reg(pi, RADIO_2064_REG053, (d15 & 0x3) << 5);

	d16 = (qFcal * 8 / (d15 + 1)) - 1;
	write_radio_reg(pi, RADIO_2064_REG051, d16);

	f16 = ((d16 + 1) * (d15 + 1)) / qFcal;
	setCount = f16 * 3 * (ci->freq) / 32 - 1;
	mod_radio_reg(pi, RADIO_2064_REG053, (0x0f << 0),
		      (uint8) (setCount >> 8));

	or_radio_reg(pi, RADIO_2064_REG053, 0x10);
	write_radio_reg(pi, RADIO_2064_REG054, (uint8) (setCount & 0xff));

	div_int = ((fvco3 * (PLL_2064_MHZ >> 4)) / fref3) << 4;

	div_frac = ((fvco3 * (PLL_2064_MHZ >> 4)) % fref3) << 4;
	while (div_frac >= fref3) {
		div_int++;
		div_frac -= fref3;
	}
	div_frac = wlc_lcnphy_qdiv_roundup(div_frac, fref3, 20);

	mod_radio_reg(pi, RADIO_2064_REG045, (0x1f << 0),
		      (uint8) (div_int >> 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x1f << 4),
		      (uint8) (div_int << 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x0f << 0),
		      (uint8) (div_frac >> 16));
	write_radio_reg(pi, RADIO_2064_REG047, (uint8) (div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2064_REG048, (uint8) div_frac & 0xff);

	write_radio_reg(pi, RADIO_2064_REG040, 0xfb);

	write_radio_reg(pi, RADIO_2064_REG041, 0x9A);
	write_radio_reg(pi, RADIO_2064_REG042, 0xA3);
	write_radio_reg(pi, RADIO_2064_REG043, 0x0C);

	{
		uint8 h29, h23, c28, d29, h28_ten, e30, h30_ten, cp_current;
		uint16 c29, c38, c30, g30, d28;
		c29 = loop_bw;
		d29 = 200;
		c38 = 1250;
		h29 = d29 / c29;
		h23 = 1;
		c28 = 30;
		d28 = (((PLL_2064_HIGH_END_KVCO - PLL_2064_LOW_END_KVCO) *
			(fvco3 / 2 - PLL_2064_LOW_END_VCO)) /
		       (PLL_2064_HIGH_END_VCO - PLL_2064_LOW_END_VCO))
		    + PLL_2064_LOW_END_KVCO;
		h28_ten = (d28 * 10) / c28;
		c30 = 2640;
		e30 = (d30 - 680) / 490;
		g30 = 680 + (e30 * 490);
		h30_ten = (g30 * 10) / c30;
		cp_current = ((c38 * h29 * h23 * 100) / h28_ten) / h30_ten;
		mod_radio_reg(pi, RADIO_2064_REG03C, 0x3f, cp_current);
	}
	if (channel >= 1 && channel <= 5)
		write_radio_reg(pi, RADIO_2064_REG03C, 0x8);
	else
		write_radio_reg(pi, RADIO_2064_REG03C, 0x7);
	write_radio_reg(pi, RADIO_2064_REG03D, 0x3);

	mod_radio_reg(pi, RADIO_2064_REG044, 0x0c, 0x0c);
	OSL_DELAY(1);

	wlc_2064_vco_cal(pi);

	write_radio_reg(pi, RADIO_2064_REG044, pll_pwrup);
	write_radio_reg(pi, RADIO_2064_REG12B, pll_pwrup_ovr);
	if (LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		write_radio_reg(pi, RADIO_2064_REG038, 3);
		write_radio_reg(pi, RADIO_2064_REG091, 7);
	}
}

bool wlc_phy_tpc_isenabled_lcnphy(phy_info_t *pi)
{
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return 0;
	else
		return (LCNPHY_TX_PWR_CTRL_HW ==
			wlc_lcnphy_get_tx_pwr_ctrl((pi)));
}

void wlc_phy_txpower_recalc_target_lcnphy(phy_info_t *pi)
{
	uint16 pwr_ctrl;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
		wlc_lcnphy_calib_modes(pi, LCNPHY_PERICAL_TEMPBASED_TXPWRCTRL);
	} else if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi)) {

		pwr_ctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
		wlc_lcnphy_txpower_recalc_target(pi);

		wlc_lcnphy_set_tx_pwr_ctrl(pi, pwr_ctrl);
	} else
		return;
}

void wlc_phy_detach_lcnphy(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_lcnphy, sizeof(phy_info_lcnphy_t));
}

bool wlc_phy_attach_lcnphy(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn;

	pi->u.pi_lcnphy =
	    (phy_info_lcnphy_t *) MALLOC(pi->sh->osh,
					 sizeof(phy_info_lcnphy_t));
	if (pi->u.pi_lcnphy == NULL) {
		return FALSE;
	}
	bzero((char *)pi->u.pi_lcnphy, sizeof(phy_info_lcnphy_t));

	pi_lcn = pi->u.pi_lcnphy;

	if ((0 == (pi->sh->boardflags & BFL_NOPA)) && !NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	pi->xtalfreq = si_alp_clock(pi->sh->sih);
	ASSERT(0 == (pi->xtalfreq % 1000));

	pi_lcn->lcnphy_papd_rxGnCtrl_init = 0;

	pi->pi_fptr.init = wlc_phy_init_lcnphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_lcnphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_lcnphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_lcnphy;
	pi->pi_fptr.txiqccget = wlc_lcnphy_get_tx_iqcc;
	pi->pi_fptr.txiqccset = wlc_lcnphy_set_tx_iqcc;
	pi->pi_fptr.txloccget = wlc_lcnphy_get_tx_locc;
	pi->pi_fptr.radioloftget = wlc_lcnphy_get_radio_loft;
	pi->pi_fptr.detach = wlc_phy_detach_lcnphy;

	if (!wlc_phy_txpwr_srom_read_lcnphy(pi))
		return FALSE;

	if ((pi->sh->boardflags & BFL_FEM) && (LCNREV_IS(pi->pubpi.phy_rev, 1))) {
		if (pi_lcn->lcnphy_tempsense_option == 3) {
			pi->hwpwrctrl = TRUE;
			pi->hwpwrctrl_capable = TRUE;
			pi->temppwrctrl_capable = FALSE;
		} else {
			pi->hwpwrctrl = FALSE;
			pi->hwpwrctrl_capable = FALSE;
			pi->temppwrctrl_capable = TRUE;
		}
	}

	return TRUE;
}

static void wlc_lcnphy_set_rx_gain(phy_info_t *pi, uint32 gain)
{
	uint16 trsw, ext_lna, lna1, lna2, tia, biq0, biq1, gain0_15, gain16_19;

	trsw = (gain & ((uint32) 1 << 28)) ? 0 : 1;
	ext_lna = (uint16) (gain >> 29) & 0x01;
	lna1 = (uint16) (gain >> 0) & 0x0f;
	lna2 = (uint16) (gain >> 4) & 0x0f;
	tia = (uint16) (gain >> 8) & 0xf;
	biq0 = (uint16) (gain >> 12) & 0xf;
	biq1 = (uint16) (gain >> 16) & 0xf;

	gain0_15 = (uint16) ((lna1 & 0x3) | ((lna1 & 0x3) << 2) |
			     ((lna2 & 0x3) << 4) | ((lna2 & 0x3) << 6) |
			     ((tia & 0xf) << 8) | ((biq0 & 0xf) << 12));
	gain16_19 = biq1;

	mod_phy_reg(pi, 0x44d, (0x1 << 0), trsw << 0);
	mod_phy_reg(pi, 0x4b1, (0x1 << 9), ext_lna << 9);
	mod_phy_reg(pi, 0x4b1, (0x1 << 10), ext_lna << 10);
	mod_phy_reg(pi, 0x4b6, (0xffff << 0), gain0_15 << 0);
	mod_phy_reg(pi, 0x4b7, (0xf << 0), gain16_19 << 0);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, 0x4b1, (0x3 << 11), lna1 << 11);
		mod_phy_reg(pi, 0x4e6, (0x3 << 3), lna1 << 3);
	}
	wlc_lcnphy_rx_gain_override_enable(pi, TRUE);
}

static uint32 wlc_lcnphy_get_receive_power(phy_info_t *pi, int32 *gain_index)
{
	uint32 received_power = 0;
	int32 max_index = 0;
	uint32 gain_code = 0;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	max_index = 36;
	if (*gain_index >= 0)
		gain_code = lcnphy_23bitgaincode_table[*gain_index];

	if (-1 == *gain_index) {
		*gain_index = 0;
		while ((*gain_index <= (int32) max_index)
		       && (received_power < 700)) {
			wlc_lcnphy_set_rx_gain(pi,
					       lcnphy_23bitgaincode_table
					       [*gain_index]);
			received_power =
			    wlc_lcnphy_measure_digital_power(pi,
							     pi_lcn->
							     lcnphy_noise_samples);
			(*gain_index)++;
		}
		(*gain_index)--;
	} else {
		wlc_lcnphy_set_rx_gain(pi, gain_code);
		received_power =
		    wlc_lcnphy_measure_digital_power(pi,
						     pi_lcn->
						     lcnphy_noise_samples);
	}

	return received_power;
}

int32 wlc_lcnphy_rx_signal_power(phy_info_t *pi, int32 gain_index)
{
	int32 gain = 0;
	int32 nominal_power_db;
	int32 log_val, gain_mismatch, desired_gain, input_power_offset_db,
	    input_power_db;
	int32 received_power, temperature;
	uint freq;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	received_power = wlc_lcnphy_get_receive_power(pi, &gain_index);

	gain = lcnphy_gain_table[gain_index];

	nominal_power_db = read_phy_reg(pi, 0x425) >> 8;

	{
		uint32 power = (received_power * 16);
		uint32 msb1, msb2, val1, val2, diff1, diff2;
		msb1 = find_msbit(power);
		msb2 = msb1 + 1;
		val1 = 1 << msb1;
		val2 = 1 << msb2;
		diff1 = (power - val1);
		diff2 = (val2 - power);
		if (diff1 < diff2)
			log_val = msb1;
		else
			log_val = msb2;
	}

	log_val = log_val * 3;

	gain_mismatch = (nominal_power_db / 2) - (log_val);

	desired_gain = gain + gain_mismatch;

	input_power_offset_db = read_phy_reg(pi, 0x434) & 0xFF;

	if (input_power_offset_db > 127)
		input_power_offset_db -= 256;

	input_power_db = input_power_offset_db - desired_gain;

	input_power_db =
	    input_power_db + lcnphy_gain_index_offset_for_rssi[gain_index];

	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((freq > 2427) && (freq <= 2467))
		input_power_db = input_power_db - 1;

	temperature = pi_lcn->lcnphy_lastsensed_temperature;

	if ((temperature - 15) < -30) {
		input_power_db =
		    input_power_db + (((temperature - 10 - 25) * 286) >> 12) -
		    7;
	} else if ((temperature - 15) < 4) {
		input_power_db =
		    input_power_db + (((temperature - 10 - 25) * 286) >> 12) -
		    3;
	} else {
		input_power_db =
		    input_power_db + (((temperature - 10 - 25) * 286) >> 12);
	}

	wlc_lcnphy_rx_gain_override_enable(pi, 0);

	return input_power_db;
}

static int
wlc_lcnphy_load_tx_iir_filter(phy_info_t *pi, bool is_ofdm, int16 filt_type)
{
	int16 filt_index = -1;
	int j;

	uint16 addr[] = {
		0x910,
		0x91e,
		0x91f,
		0x924,
		0x925,
		0x926,
		0x920,
		0x921,
		0x927,
		0x928,
		0x929,
		0x922,
		0x923,
		0x930,
		0x931,
		0x932
	};

	uint16 addr_ofdm[] = {
		0x90f,
		0x900,
		0x901,
		0x906,
		0x907,
		0x908,
		0x902,
		0x903,
		0x909,
		0x90a,
		0x90b,
		0x904,
		0x905,
		0x90c,
		0x90d,
		0x90e
	};

	if (!is_ofdm) {
		for (j = 0; j < LCNPHY_NUM_TX_DIG_FILTERS_CCK; j++) {
			if (filt_type == LCNPHY_txdigfiltcoeffs_cck[j][0]) {
				filt_index = (int16) j;
				break;
			}
		}

		if (filt_index == -1) {
			ASSERT(FALSE);
		} else {
			for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
				write_phy_reg(pi, addr[j],
					      LCNPHY_txdigfiltcoeffs_cck
					      [filt_index][j + 1]);
			}
		}
	} else {
		for (j = 0; j < LCNPHY_NUM_TX_DIG_FILTERS_OFDM; j++) {
			if (filt_type == LCNPHY_txdigfiltcoeffs_ofdm[j][0]) {
				filt_index = (int16) j;
				break;
			}
		}

		if (filt_index == -1) {
			ASSERT(FALSE);
		} else {
			for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
				write_phy_reg(pi, addr_ofdm[j],
					      LCNPHY_txdigfiltcoeffs_ofdm
					      [filt_index][j + 1]);
			}
		}
	}

	return (filt_index != -1) ? 0 : -1;
}
