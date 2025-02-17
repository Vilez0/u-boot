// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for Qualcomm sm8250
 *
 * (C) Copyright 2023 Linaro Ltd.
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dt-bindings/clock/qcom,gcc-sm8250.h>

#include "clock-qcom.h"

#define GCC_SE12_UART_RCG_REG 0x184D0
#define GCC_SDCC2_APPS_CLK_SRC_REG 0x1400c

#define APCS_GPLL0_ENA_VOTE 0x79000
#define APCS_GPLL9_STATUS 0x1c000
#define APCS_GPLLX_ENA_REG 0x52018

#define USB30_SEC_GDSCR 0x10004

static const struct freq_tbl ftbl_gcc_qupv3_wrap1_s4_clk_src[] = {
	F(7372800, CFG_CLK_SRC_GPLL0_EVEN, 1, 384, 15625),
	F(14745600, CFG_CLK_SRC_GPLL0_EVEN, 1, 768, 15625),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(29491200, CFG_CLK_SRC_GPLL0_EVEN, 1, 1536, 15625),
	F(32000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 8, 75),
	F(48000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 4, 25),
	F(50000000, CFG_CLK_SRC_GPLL0_EVEN, 6, 0, 0),
	F(64000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 16, 75),
	F(75000000, CFG_CLK_SRC_GPLL0_EVEN, 4, 0, 0),
	F(80000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 4, 15),
	F(96000000, CFG_CLK_SRC_GPLL0_EVEN, 1, 8, 25),
	F(100000000, CFG_CLK_SRC_GPLL0, 6, 0, 0),
	{ }
};

static const struct freq_tbl ftbl_gcc_sdcc2_apps_clk_src[] = {
	F(400000, CFG_CLK_SRC_CXO, 12, 1, 4),
	F(19200000, CFG_CLK_SRC_CXO, 1, 0, 0),
	F(25000000, CFG_CLK_SRC_GPLL0_EVEN, 12, 0, 0),
	F(50000000, CFG_CLK_SRC_GPLL0_EVEN, 6, 0, 0),
	F(100000000, CFG_CLK_SRC_GPLL0, 6, 0, 0),
	F(202000000, CFG_CLK_SRC_GPLL9, 4, 0, 0),
	{ }
};

static struct pll_vote_clk gpll9_vote_clk = {
	.status = APCS_GPLL9_STATUS,
	.status_bit = BIT(31),
	.ena_vote = APCS_GPLLX_ENA_REG,
	.vote_bit = BIT(9),
};

static ulong sm8250_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	const struct freq_tbl *freq;

	if (clk->id < priv->data->num_clks)
		debug("%s: %s, requested rate=%ld\n", __func__, priv->data->clks[clk->id].name, rate);

	switch (clk->id) {
	case GCC_QUPV3_WRAP1_S4_CLK: /*UART2*/
		freq = qcom_find_freq(ftbl_gcc_qupv3_wrap1_s4_clk_src, rate);
		clk_rcg_set_rate_mnd(priv->base, GCC_SE12_UART_RCG_REG,
						freq->pre_div, freq->m, freq->n, freq->src, 16);

		return freq->freq;
	case GCC_SDCC2_APPS_CLK:
		/* Enable GPLL9 so that we can point SDCC2_APPS_CLK_SRC at it */
		clk_enable_gpll0(priv->base, &gpll9_vote_clk);
		freq = qcom_find_freq(ftbl_gcc_sdcc2_apps_clk_src, rate);
		printf("%s: got freq %u\n", __func__, freq->freq);
		WARN(freq->src != CFG_CLK_SRC_GPLL9, "SDCC2_APPS_CLK_SRC not set to GPLL9, requested rate %lu\n", rate);
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC2_APPS_CLK_SRC_REG,
						freq->pre_div, freq->m, freq->n, CFG_CLK_SRC_GPLL9, 8);

		return rate;
	default:
		return 0;
	}
}

static const struct gate_clk sm8250_clks[] = {
	GATE_CLK(GCC_AGGRE_UFS_CARD_AXI_CLK, 0x750cc, 0x00000001),
	GATE_CLK(GCC_AGGRE_UFS_PHY_AXI_CLK, 0x770cc, 0x00000001),
	GATE_CLK(GCC_AGGRE_USB3_PRIM_AXI_CLK, 0x0f080, 0x00000001),
	GATE_CLK(GCC_AGGRE_USB3_SEC_AXI_CLK, 0x10080, 0x00000001),
	GATE_CLK(GCC_CFG_NOC_USB3_PRIM_AXI_CLK, 0x0f07c, 0x00000001),
	GATE_CLK(GCC_CFG_NOC_USB3_SEC_AXI_CLK, 0x1007c, 0x00000001),
	GATE_CLK(GCC_QMIP_CAMERA_NRT_AHB_CLK, 0x0b018, 0x00000001),
	GATE_CLK(GCC_QMIP_CAMERA_RT_AHB_CLK, 0x0b01c, 0x00000001),
	GATE_CLK(GCC_QMIP_DISP_AHB_CLK, 0x0b020, 0x00000001),
	GATE_CLK(GCC_QMIP_VIDEO_CVP_AHB_CLK, 0x0b010, 0x00000001),
	GATE_CLK(GCC_QMIP_VIDEO_VCODEC_AHB_CLK, 0x0b014, 0x00000001),
	GATE_CLK(GCC_QUPV3_WRAP0_CORE_2X_CLK, 0x52008, 0x00000200),
	GATE_CLK(GCC_QUPV3_WRAP0_CORE_CLK, 0x52008, 0x00000100),
	GATE_CLK(GCC_QUPV3_WRAP0_S0_CLK, 0x52008, 0x00000400),
	GATE_CLK(GCC_QUPV3_WRAP0_S1_CLK, 0x52008, 0x00000800),
	GATE_CLK(GCC_QUPV3_WRAP0_S2_CLK, 0x52008, 0x00001000),
	GATE_CLK(GCC_QUPV3_WRAP0_S3_CLK, 0x52008, 0x00002000),
	GATE_CLK(GCC_QUPV3_WRAP0_S4_CLK, 0x52008, 0x00004000),
	GATE_CLK(GCC_QUPV3_WRAP0_S5_CLK, 0x52008, 0x00008000),
	GATE_CLK(GCC_QUPV3_WRAP0_S6_CLK, 0x52008, 0x00010000),
	GATE_CLK(GCC_QUPV3_WRAP0_S7_CLK, 0x52008, 0x00020000),
	GATE_CLK(GCC_QUPV3_WRAP1_CORE_2X_CLK, 0x52008, 0x00040000),
	GATE_CLK(GCC_QUPV3_WRAP1_CORE_CLK, 0x52008, 0x00080000),
	GATE_CLK(GCC_QUPV3_WRAP1_S0_CLK, 0x52008, 0x00400000),
	GATE_CLK(GCC_QUPV3_WRAP1_S1_CLK, 0x52008, 0x00800000),
	GATE_CLK(GCC_QUPV3_WRAP1_S2_CLK, 0x52008, 0x01000000),
	GATE_CLK(GCC_QUPV3_WRAP1_S3_CLK, 0x52008, 0x02000000),
	GATE_CLK(GCC_QUPV3_WRAP1_S4_CLK, 0x52008, 0x04000000),
	GATE_CLK(GCC_QUPV3_WRAP1_S5_CLK, 0x52008, 0x08000000),
	GATE_CLK(GCC_QUPV3_WRAP2_CORE_2X_CLK, 0x52010, 0x00000008),
	GATE_CLK(GCC_QUPV3_WRAP2_CORE_CLK, 0x52010, 0x00000001),
	GATE_CLK(GCC_QUPV3_WRAP2_S0_CLK, 0x52010, 0x00000010),
	GATE_CLK(GCC_QUPV3_WRAP2_S1_CLK, 0x52010, 0x00000020),
	GATE_CLK(GCC_QUPV3_WRAP2_S2_CLK, 0x52010, 0x00000040),
	GATE_CLK(GCC_QUPV3_WRAP2_S3_CLK, 0x52010, 0x00000080),
	GATE_CLK(GCC_QUPV3_WRAP2_S4_CLK, 0x52010, 0x00000100),
	GATE_CLK(GCC_QUPV3_WRAP2_S5_CLK, 0x52010, 0x00000200),
	GATE_CLK(GCC_QUPV3_WRAP_0_M_AHB_CLK, 0x52008, 0x00000040),
	GATE_CLK(GCC_QUPV3_WRAP_0_S_AHB_CLK, 0x52008, 0x00000080),
	GATE_CLK(GCC_QUPV3_WRAP_1_M_AHB_CLK, 0x52008, 0x00100000),
	GATE_CLK(GCC_QUPV3_WRAP_1_S_AHB_CLK, 0x52008, 0x00200000),
	GATE_CLK(GCC_QUPV3_WRAP_2_M_AHB_CLK, 0x52010, 0x00000004),
	GATE_CLK(GCC_QUPV3_WRAP_2_S_AHB_CLK, 0x52010, 0x00000002),
	GATE_CLK(GCC_SDCC2_AHB_CLK, 0x14008, 0x00000001),
	GATE_CLK(GCC_SDCC2_APPS_CLK, 0x14004, 0x00000001),
	GATE_CLK(GCC_SDCC4_AHB_CLK, 0x16008, 0x00000001),
	GATE_CLK(GCC_SDCC4_APPS_CLK, 0x16004, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_AHB_CLK, 0x75018, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_AXI_CLK, 0x75010, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_ICE_CORE_CLK, 0x75064, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_PHY_AUX_CLK, 0x7509c, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_RX_SYMBOL_0_CLK, 0x75020, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_RX_SYMBOL_1_CLK, 0x750b8, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_TX_SYMBOL_0_CLK, 0x7501c, 0x00000001),
	GATE_CLK(GCC_UFS_CARD_UNIPRO_CORE_CLK, 0x7505c, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_AHB_CLK, 0x77018, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_AXI_CLK, 0x77010, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_ICE_CORE_CLK, 0x77064, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_PHY_AUX_CLK, 0x7709c, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_RX_SYMBOL_0_CLK, 0x77020, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_RX_SYMBOL_1_CLK, 0x770b8, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_TX_SYMBOL_0_CLK, 0x7701c, 0x00000001),
	GATE_CLK(GCC_UFS_PHY_UNIPRO_CORE_CLK, 0x7705c, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_MASTER_CLK, 0x0f010, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_MOCK_UTMI_CLK, 0x0f01c, 0x00000001),
	GATE_CLK(GCC_USB30_PRIM_SLEEP_CLK, 0x0f018, 0x00000001),
	GATE_CLK(GCC_USB30_SEC_MASTER_CLK, 0x10010, 0x00000001),
	GATE_CLK(GCC_USB30_SEC_MOCK_UTMI_CLK, 0x1001c, 0x00000001),
	GATE_CLK(GCC_USB30_SEC_SLEEP_CLK, 0x10018, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_PHY_AUX_CLK, 0x0f054, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_PHY_COM_AUX_CLK, 0x0f058, 0x00000001),
	GATE_CLK(GCC_USB3_PRIM_PHY_PIPE_CLK, 0x0f05c, 0x00000001),
	GATE_CLK(GCC_USB3_SEC_CLKREF_EN, 0x8c010, 0x00000001),
	GATE_CLK(GCC_USB3_SEC_PHY_AUX_CLK, 0x10054, 0x00000001),
	GATE_CLK(GCC_USB3_SEC_PHY_COM_AUX_CLK, 0x10058, 0x00000001),
	GATE_CLK(GCC_USB3_SEC_PHY_PIPE_CLK, 0x1005c, 0x00000001),
};

static int sm8250_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks < clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, sm8250_clks[clk->id].name);

	switch (clk->id) {
	case GCC_USB30_SEC_MASTER_CLK:
		gdsc_enable(priv->base + USB30_SEC_GDSCR);
		qcom_gate_clk_en(priv, GCC_USB3_SEC_PHY_AUX_CLK);
		qcom_gate_clk_en(priv, GCC_USB3_SEC_PHY_COM_AUX_CLK);
		break;
	}

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map sm8250_gcc_resets[] = {
	[GCC_GPU_BCR] = { 0x71000 },
	[GCC_MMSS_BCR] = { 0xb000 },
	[GCC_NPU_BWMON_BCR] = { 0x73000 },
	[GCC_NPU_BCR] = { 0x4d000 },
	[GCC_PCIE_0_BCR] = { 0x6b000 },
	[GCC_PCIE_0_LINK_DOWN_BCR] = { 0x6c014 },
	[GCC_PCIE_0_NOCSR_COM_PHY_BCR] = { 0x6c020 },
	[GCC_PCIE_0_PHY_BCR] = { 0x6c01c },
	[GCC_PCIE_0_PHY_NOCSR_COM_PHY_BCR] = { 0x6c028 },
	[GCC_PCIE_1_BCR] = { 0x8d000 },
	[GCC_PCIE_1_LINK_DOWN_BCR] = { 0x8e014 },
	[GCC_PCIE_1_NOCSR_COM_PHY_BCR] = { 0x8e020 },
	[GCC_PCIE_1_PHY_BCR] = { 0x8e01c },
	[GCC_PCIE_1_PHY_NOCSR_COM_PHY_BCR] = { 0x8e000 },
	[GCC_PCIE_2_BCR] = { 0x6000 },
	[GCC_PCIE_2_LINK_DOWN_BCR] = { 0x1f014 },
	[GCC_PCIE_2_NOCSR_COM_PHY_BCR] = { 0x1f020 },
	[GCC_PCIE_2_PHY_BCR] = { 0x1f01c },
	[GCC_PCIE_2_PHY_NOCSR_COM_PHY_BCR] = { 0x1f028 },
	[GCC_PCIE_PHY_BCR] = { 0x6f000 },
	[GCC_PCIE_PHY_CFG_AHB_BCR] = { 0x6f00c },
	[GCC_PCIE_PHY_COM_BCR] = { 0x6f010 },
	[GCC_PDM_BCR] = { 0x33000 },
	[GCC_PRNG_BCR] = { 0x34000 },
	[GCC_QUPV3_WRAPPER_0_BCR] = { 0x17000 },
	[GCC_QUPV3_WRAPPER_1_BCR] = { 0x18000 },
	[GCC_QUPV3_WRAPPER_2_BCR] = { 0x1e000 },
	[GCC_QUSB2PHY_PRIM_BCR] = { 0x12000 },
	[GCC_QUSB2PHY_SEC_BCR] = { 0x12004 },
	[GCC_SDCC2_BCR] = { 0x14000 },
	[GCC_SDCC4_BCR] = { 0x16000 },
	[GCC_TSIF_BCR] = { 0x36000 },
	[GCC_UFS_CARD_BCR] = { 0x75000 },
	[GCC_UFS_PHY_BCR] = { 0x77000 },
	[GCC_USB30_PRIM_BCR] = { 0xf000 },
	[GCC_USB30_SEC_BCR] = { 0x10000 },
	[GCC_USB3_DP_PHY_PRIM_BCR] = { 0x50008 },
	[GCC_USB3_DP_PHY_SEC_BCR] = { 0x50014 },
	[GCC_USB3_PHY_PRIM_BCR] = { 0x50000 },
	[GCC_USB3_PHY_SEC_BCR] = { 0x5000c },
	[GCC_USB3PHY_PHY_PRIM_BCR] = { 0x50004 },
	[GCC_USB3PHY_PHY_SEC_BCR] = { 0x50010 },
	[GCC_USB_PHY_CFG_AHB2PHY_BCR] = { 0x6a000 },
};

static struct msm_clk_data qcs404_gcc_data = {
	.resets = sm8250_gcc_resets,
	.num_resets = ARRAY_SIZE(sm8250_gcc_resets),
	.clks = sm8250_clks,
	.num_clks = ARRAY_SIZE(sm8250_clks),

	.enable = sm8250_enable,
	.set_rate = sm8250_set_rate,
};


static const struct udevice_id gcc_sm8250_of_match[] = {
	{
		.compatible = "qcom,gcc-sm8250",
		.data = (ulong)&qcs404_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_sm8250) = {
	.name		= "gcc_sm8250",
	.id		= UCLASS_NOP,
	.of_match	= gcc_sm8250_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC,
};
