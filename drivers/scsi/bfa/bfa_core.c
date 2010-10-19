/*
 * Copyright (c) 2005-2010 Brocade Communications Systems, Inc.
 * All rights reserved
 * www.brocade.com
 *
 * Linux driver for Brocade Fibre Channel Host Bus Adapter.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (GPL) Version 2 as
 * published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "bfa_modules.h"
#include "bfi_ctreg.h"
#include "bfad_drv.h"

BFA_TRC_FILE(HAL, CORE);

/**
 * BFA IOC FC related definitions
 */

/**
 * IOC local definitions
 */
#define BFA_IOCFC_TOV		5000	/* msecs */

enum {
	BFA_IOCFC_ACT_NONE	= 0,
	BFA_IOCFC_ACT_INIT	= 1,
	BFA_IOCFC_ACT_STOP	= 2,
	BFA_IOCFC_ACT_DISABLE	= 3,
};

#define DEF_CFG_NUM_FABRICS		1
#define DEF_CFG_NUM_LPORTS		256
#define DEF_CFG_NUM_CQS			4
#define DEF_CFG_NUM_IOIM_REQS		(BFA_IOIM_MAX)
#define DEF_CFG_NUM_TSKIM_REQS		128
#define DEF_CFG_NUM_FCXP_REQS		64
#define DEF_CFG_NUM_UF_BUFS		64
#define DEF_CFG_NUM_RPORTS		1024
#define DEF_CFG_NUM_ITNIMS		(DEF_CFG_NUM_RPORTS)
#define DEF_CFG_NUM_TINS		256

#define DEF_CFG_NUM_SGPGS		2048
#define DEF_CFG_NUM_REQQ_ELEMS		256
#define DEF_CFG_NUM_RSPQ_ELEMS		64
#define DEF_CFG_NUM_SBOOT_TGTS		16
#define DEF_CFG_NUM_SBOOT_LUNS		16

/**
 * forward declaration for IOC FC functions
 */
static void bfa_iocfc_enable_cbfn(void *bfa_arg, enum bfa_status status);
static void bfa_iocfc_disable_cbfn(void *bfa_arg);
static void bfa_iocfc_hbfail_cbfn(void *bfa_arg);
static void bfa_iocfc_reset_cbfn(void *bfa_arg);
static struct bfa_ioc_cbfn_s bfa_iocfc_cbfn;

/**
 * BFA Interrupt handling functions
 */
static void
bfa_msix_errint(struct bfa_s *bfa, u32 intr)
{
	bfa_ioc_error_isr(&bfa->ioc);
}

static void
bfa_msix_lpu(struct bfa_s *bfa)
{
	bfa_ioc_mbox_isr(&bfa->ioc);
}

static void
bfa_reqq_resume(struct bfa_s *bfa, int qid)
{
	struct list_head *waitq, *qe, *qen;
	struct bfa_reqq_wait_s *wqe;

	waitq = bfa_reqq(bfa, qid);
	list_for_each_safe(qe, qen, waitq) {
		/**
		 * Callback only as long as there is room in request queue
		 */
		if (bfa_reqq_full(bfa, qid))
			break;

		list_del(qe);
		wqe = (struct bfa_reqq_wait_s *) qe;
		wqe->qresume(wqe->cbarg);
	}
}

void
bfa_msix_all(struct bfa_s *bfa, int vec)
{
	bfa_intx(bfa);
}

/**
 *  hal_intr_api
 */
bfa_boolean_t
bfa_intx(struct bfa_s *bfa)
{
	u32 intr, qintr;
	int queue;

	intr = bfa_reg_read(bfa->iocfc.bfa_regs.intr_status);
	if (!intr)
		return BFA_FALSE;

	/**
	 * RME completion queue interrupt
	 */
	qintr = intr & __HFN_INT_RME_MASK;
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_status, qintr);

	for (queue = 0; queue < BFI_IOC_MAX_CQS_ASIC; queue++) {
		if (intr & (__HFN_INT_RME_Q0 << queue))
			bfa_msix_rspq(bfa, queue & (BFI_IOC_MAX_CQS - 1));
	}
	intr &= ~qintr;
	if (!intr)
		return BFA_TRUE;

	/**
	 * CPE completion queue interrupt
	 */
	qintr = intr & __HFN_INT_CPE_MASK;
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_status, qintr);

	for (queue = 0; queue < BFI_IOC_MAX_CQS_ASIC; queue++) {
		if (intr & (__HFN_INT_CPE_Q0 << queue))
			bfa_msix_reqq(bfa, queue & (BFI_IOC_MAX_CQS - 1));
	}
	intr &= ~qintr;
	if (!intr)
		return BFA_TRUE;

	bfa_msix_lpu_err(bfa, intr);

	return BFA_TRUE;
}

void
bfa_intx_enable(struct bfa_s *bfa)
{
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_mask, bfa->iocfc.intr_mask);
}

void
bfa_intx_disable(struct bfa_s *bfa)
{
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_mask, -1L);
}

void
bfa_isr_enable(struct bfa_s *bfa)
{
	u32 intr_unmask;
	int pci_func = bfa_ioc_pcifn(&bfa->ioc);

	bfa_trc(bfa, pci_func);

	bfa_msix_install(bfa);
	intr_unmask = (__HFN_INT_ERR_EMC | __HFN_INT_ERR_LPU0 |
		       __HFN_INT_ERR_LPU1 | __HFN_INT_ERR_PSS |
		       __HFN_INT_LL_HALT);

	if (pci_func == 0)
		intr_unmask |= (__HFN_INT_CPE_Q0 | __HFN_INT_CPE_Q1 |
				__HFN_INT_CPE_Q2 | __HFN_INT_CPE_Q3 |
				__HFN_INT_RME_Q0 | __HFN_INT_RME_Q1 |
				__HFN_INT_RME_Q2 | __HFN_INT_RME_Q3 |
				__HFN_INT_MBOX_LPU0);
	else
		intr_unmask |= (__HFN_INT_CPE_Q4 | __HFN_INT_CPE_Q5 |
				__HFN_INT_CPE_Q6 | __HFN_INT_CPE_Q7 |
				__HFN_INT_RME_Q4 | __HFN_INT_RME_Q5 |
				__HFN_INT_RME_Q6 | __HFN_INT_RME_Q7 |
				__HFN_INT_MBOX_LPU1);

	bfa_reg_write(bfa->iocfc.bfa_regs.intr_status, intr_unmask);
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_mask, ~intr_unmask);
	bfa->iocfc.intr_mask = ~intr_unmask;
	bfa_isr_mode_set(bfa, bfa->msix.nvecs != 0);
}

void
bfa_isr_disable(struct bfa_s *bfa)
{
	bfa_isr_mode_set(bfa, BFA_FALSE);
	bfa_reg_write(bfa->iocfc.bfa_regs.intr_mask, -1L);
	bfa_msix_uninstall(bfa);
}

void
bfa_msix_reqq(struct bfa_s *bfa, int qid)
{
	struct list_head *waitq;

	qid &= (BFI_IOC_MAX_CQS - 1);

	bfa->iocfc.hwif.hw_reqq_ack(bfa, qid);

	/**
	 * Resume any pending requests in the corresponding reqq.
	 */
	waitq = bfa_reqq(bfa, qid);
	if (!list_empty(waitq))
		bfa_reqq_resume(bfa, qid);
}

void
bfa_isr_unhandled(struct bfa_s *bfa, struct bfi_msg_s *m)
{
	bfa_trc(bfa, m->mhdr.msg_class);
	bfa_trc(bfa, m->mhdr.msg_id);
	bfa_trc(bfa, m->mhdr.mtag.i2htok);
	bfa_assert(0);
	bfa_trc_stop(bfa->trcmod);
}

void
bfa_msix_rspq(struct bfa_s *bfa, int qid)
{
	struct bfi_msg_s *m;
	u32 pi, ci;
	struct list_head *waitq;

	bfa_trc_fp(bfa, qid);

	qid &= (BFI_IOC_MAX_CQS - 1);

	bfa->iocfc.hwif.hw_rspq_ack(bfa, qid);

	ci = bfa_rspq_ci(bfa, qid);
	pi = bfa_rspq_pi(bfa, qid);

	bfa_trc_fp(bfa, ci);
	bfa_trc_fp(bfa, pi);

	if (bfa->rme_process) {
		while (ci != pi) {
			m = bfa_rspq_elem(bfa, qid, ci);
			bfa_assert_fp(m->mhdr.msg_class < BFI_MC_MAX);

			bfa_isrs[m->mhdr.msg_class] (bfa, m);

			CQ_INCR(ci, bfa->iocfc.cfg.drvcfg.num_rspq_elems);
		}
	}

	/**
	 * update CI
	 */
	bfa_rspq_ci(bfa, qid) = pi;
	bfa_reg_write(bfa->iocfc.bfa_regs.rme_q_ci[qid], pi);
	mmiowb();

	/**
	 * Resume any pending requests in the corresponding reqq.
	 */
	waitq = bfa_reqq(bfa, qid);
	if (!list_empty(waitq))
		bfa_reqq_resume(bfa, qid);
}

void
bfa_msix_lpu_err(struct bfa_s *bfa, int vec)
{
	u32 intr, curr_value;

	intr = bfa_reg_read(bfa->iocfc.bfa_regs.intr_status);

	if (intr & (__HFN_INT_MBOX_LPU0 | __HFN_INT_MBOX_LPU1))
		bfa_msix_lpu(bfa);

	intr &= (__HFN_INT_ERR_EMC | __HFN_INT_ERR_LPU0 |
		__HFN_INT_ERR_LPU1 | __HFN_INT_ERR_PSS | __HFN_INT_LL_HALT);

	if (intr) {
		if (intr & __HFN_INT_LL_HALT) {
			/**
			 * If LL_HALT bit is set then FW Init Halt LL Port
			 * Register needs to be cleared as well so Interrupt
			 * Status Register will be cleared.
			 */
			curr_value = bfa_reg_read(bfa->ioc.ioc_regs.ll_halt);
			curr_value &= ~__FW_INIT_HALT_P;
			bfa_reg_write(bfa->ioc.ioc_regs.ll_halt, curr_value);
		}

		if (intr & __HFN_INT_ERR_PSS) {
			/**
			 * ERR_PSS bit needs to be cleared as well in case
			 * interrups are shared so driver's interrupt handler is
			 * still called eventhough it is already masked out.
			 */
			curr_value = bfa_reg_read(
					bfa->ioc.ioc_regs.pss_err_status_reg);
			curr_value &= __PSS_ERR_STATUS_SET;
			bfa_reg_write(bfa->ioc.ioc_regs.pss_err_status_reg,
					curr_value);
		}

		bfa_reg_write(bfa->iocfc.bfa_regs.intr_status, intr);
		bfa_msix_errint(bfa, intr);
	}
}

void
bfa_isr_bind(enum bfi_mclass mc, bfa_isr_func_t isr_func)
{
	bfa_isrs[mc] = isr_func;
}

/**
 * BFA IOC FC related functions
 */

/**
 *  hal_ioc_pvt BFA IOC private functions
 */

static void
bfa_iocfc_cqs_sz(struct bfa_iocfc_cfg_s *cfg, u32 *dm_len)
{
	int		i, per_reqq_sz, per_rspq_sz;

	per_reqq_sz = BFA_ROUNDUP((cfg->drvcfg.num_reqq_elems * BFI_LMSG_SZ),
				  BFA_DMA_ALIGN_SZ);
	per_rspq_sz = BFA_ROUNDUP((cfg->drvcfg.num_rspq_elems * BFI_LMSG_SZ),
				  BFA_DMA_ALIGN_SZ);

	/*
	 * Calculate CQ size
	 */
	for (i = 0; i < cfg->fwcfg.num_cqs; i++) {
		*dm_len = *dm_len + per_reqq_sz;
		*dm_len = *dm_len + per_rspq_sz;
	}

	/*
	 * Calculate Shadow CI/PI size
	 */
	for (i = 0; i < cfg->fwcfg.num_cqs; i++)
		*dm_len += (2 * BFA_CACHELINE_SZ);
}

static void
bfa_iocfc_fw_cfg_sz(struct bfa_iocfc_cfg_s *cfg, u32 *dm_len)
{
	*dm_len +=
		BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfg_s), BFA_CACHELINE_SZ);
	*dm_len +=
		BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfgrsp_s),
			    BFA_CACHELINE_SZ);
}

/**
 * Use the Mailbox interface to send BFI_IOCFC_H2I_CFG_REQ
 */
static void
bfa_iocfc_send_cfg(void *bfa_arg)
{
	struct bfa_s *bfa = bfa_arg;
	struct bfa_iocfc_s *iocfc = &bfa->iocfc;
	struct bfi_iocfc_cfg_req_s cfg_req;
	struct bfi_iocfc_cfg_s *cfg_info = iocfc->cfginfo;
	struct bfa_iocfc_cfg_s	*cfg = &iocfc->cfg;
	int		i;

	bfa_assert(cfg->fwcfg.num_cqs <= BFI_IOC_MAX_CQS);
	bfa_trc(bfa, cfg->fwcfg.num_cqs);

	bfa_iocfc_reset_queues(bfa);

	/**
	 * initialize IOC configuration info
	 */
	cfg_info->endian_sig = BFI_IOC_ENDIAN_SIG;
	cfg_info->num_cqs = cfg->fwcfg.num_cqs;

	bfa_dma_be_addr_set(cfg_info->cfgrsp_addr, iocfc->cfgrsp_dma.pa);
	/**
	 * dma map REQ and RSP circular queues and shadow pointers
	 */
	for (i = 0; i < cfg->fwcfg.num_cqs; i++) {
		bfa_dma_be_addr_set(cfg_info->req_cq_ba[i],
				    iocfc->req_cq_ba[i].pa);
		bfa_dma_be_addr_set(cfg_info->req_shadow_ci[i],
				    iocfc->req_cq_shadow_ci[i].pa);
		cfg_info->req_cq_elems[i] =
			bfa_os_htons(cfg->drvcfg.num_reqq_elems);

		bfa_dma_be_addr_set(cfg_info->rsp_cq_ba[i],
				    iocfc->rsp_cq_ba[i].pa);
		bfa_dma_be_addr_set(cfg_info->rsp_shadow_pi[i],
				    iocfc->rsp_cq_shadow_pi[i].pa);
		cfg_info->rsp_cq_elems[i] =
			bfa_os_htons(cfg->drvcfg.num_rspq_elems);
	}

	/**
	 * Enable interrupt coalescing if it is driver init path
	 * and not ioc disable/enable path.
	 */
	if (!iocfc->cfgdone)
		cfg_info->intr_attr.coalesce = BFA_TRUE;

	iocfc->cfgdone = BFA_FALSE;

	/**
	 * dma map IOC configuration itself
	 */
	bfi_h2i_set(cfg_req.mh, BFI_MC_IOCFC, BFI_IOCFC_H2I_CFG_REQ,
		    bfa_lpuid(bfa));
	bfa_dma_be_addr_set(cfg_req.ioc_cfg_dma_addr, iocfc->cfg_info.pa);

	bfa_ioc_mbox_send(&bfa->ioc, &cfg_req,
			  sizeof(struct bfi_iocfc_cfg_req_s));
}

static void
bfa_iocfc_init_mem(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		   struct bfa_pcidev_s *pcidev)
{
	struct bfa_iocfc_s	*iocfc = &bfa->iocfc;

	bfa->bfad = bfad;
	iocfc->bfa = bfa;
	iocfc->action = BFA_IOCFC_ACT_NONE;

	iocfc->cfg = *cfg;

	/**
	 * Initialize chip specific handlers.
	 */
	if (bfa_asic_id_ct(bfa_ioc_devid(&bfa->ioc))) {
		iocfc->hwif.hw_reginit = bfa_hwct_reginit;
		iocfc->hwif.hw_reqq_ack = bfa_hwct_reqq_ack;
		iocfc->hwif.hw_rspq_ack = bfa_hwct_rspq_ack;
		iocfc->hwif.hw_msix_init = bfa_hwct_msix_init;
		iocfc->hwif.hw_msix_install = bfa_hwct_msix_install;
		iocfc->hwif.hw_msix_uninstall = bfa_hwct_msix_uninstall;
		iocfc->hwif.hw_isr_mode_set = bfa_hwct_isr_mode_set;
		iocfc->hwif.hw_msix_getvecs = bfa_hwct_msix_getvecs;
		iocfc->hwif.hw_msix_get_rme_range = bfa_hwct_msix_get_rme_range;
	} else {
		iocfc->hwif.hw_reginit = bfa_hwcb_reginit;
		iocfc->hwif.hw_reqq_ack = bfa_hwcb_reqq_ack;
		iocfc->hwif.hw_rspq_ack = bfa_hwcb_rspq_ack;
		iocfc->hwif.hw_msix_init = bfa_hwcb_msix_init;
		iocfc->hwif.hw_msix_install = bfa_hwcb_msix_install;
		iocfc->hwif.hw_msix_uninstall = bfa_hwcb_msix_uninstall;
		iocfc->hwif.hw_isr_mode_set = bfa_hwcb_isr_mode_set;
		iocfc->hwif.hw_msix_getvecs = bfa_hwcb_msix_getvecs;
		iocfc->hwif.hw_msix_get_rme_range = bfa_hwcb_msix_get_rme_range;
	}

	iocfc->hwif.hw_reginit(bfa);
	bfa->msix.nvecs = 0;
}

static void
bfa_iocfc_mem_claim(struct bfa_s *bfa, struct bfa_iocfc_cfg_s *cfg,
		    struct bfa_meminfo_s *meminfo)
{
	u8	       *dm_kva;
	u64	dm_pa;
	int		i, per_reqq_sz, per_rspq_sz;
	struct bfa_iocfc_s  *iocfc = &bfa->iocfc;
	int		dbgsz;

	dm_kva = bfa_meminfo_dma_virt(meminfo);
	dm_pa = bfa_meminfo_dma_phys(meminfo);

	/*
	 * First allocate dma memory for IOC.
	 */
	bfa_ioc_mem_claim(&bfa->ioc, dm_kva, dm_pa);
	dm_kva += bfa_ioc_meminfo();
	dm_pa  += bfa_ioc_meminfo();

	/*
	 * Claim DMA-able memory for the request/response queues and for shadow
	 * ci/pi registers
	 */
	per_reqq_sz = BFA_ROUNDUP((cfg->drvcfg.num_reqq_elems * BFI_LMSG_SZ),
				  BFA_DMA_ALIGN_SZ);
	per_rspq_sz = BFA_ROUNDUP((cfg->drvcfg.num_rspq_elems * BFI_LMSG_SZ),
				  BFA_DMA_ALIGN_SZ);

	for (i = 0; i < cfg->fwcfg.num_cqs; i++) {
		iocfc->req_cq_ba[i].kva = dm_kva;
		iocfc->req_cq_ba[i].pa = dm_pa;
		memset(dm_kva, 0, per_reqq_sz);
		dm_kva += per_reqq_sz;
		dm_pa += per_reqq_sz;

		iocfc->rsp_cq_ba[i].kva = dm_kva;
		iocfc->rsp_cq_ba[i].pa = dm_pa;
		memset(dm_kva, 0, per_rspq_sz);
		dm_kva += per_rspq_sz;
		dm_pa += per_rspq_sz;
	}

	for (i = 0; i < cfg->fwcfg.num_cqs; i++) {
		iocfc->req_cq_shadow_ci[i].kva = dm_kva;
		iocfc->req_cq_shadow_ci[i].pa = dm_pa;
		dm_kva += BFA_CACHELINE_SZ;
		dm_pa += BFA_CACHELINE_SZ;

		iocfc->rsp_cq_shadow_pi[i].kva = dm_kva;
		iocfc->rsp_cq_shadow_pi[i].pa = dm_pa;
		dm_kva += BFA_CACHELINE_SZ;
		dm_pa += BFA_CACHELINE_SZ;
	}

	/*
	 * Claim DMA-able memory for the config info page
	 */
	bfa->iocfc.cfg_info.kva = dm_kva;
	bfa->iocfc.cfg_info.pa = dm_pa;
	bfa->iocfc.cfginfo = (struct bfi_iocfc_cfg_s *) dm_kva;
	dm_kva += BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfg_s), BFA_CACHELINE_SZ);
	dm_pa += BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfg_s), BFA_CACHELINE_SZ);

	/*
	 * Claim DMA-able memory for the config response
	 */
	bfa->iocfc.cfgrsp_dma.kva = dm_kva;
	bfa->iocfc.cfgrsp_dma.pa = dm_pa;
	bfa->iocfc.cfgrsp = (struct bfi_iocfc_cfgrsp_s *) dm_kva;

	dm_kva +=
		BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfgrsp_s),
			    BFA_CACHELINE_SZ);
	dm_pa += BFA_ROUNDUP(sizeof(struct bfi_iocfc_cfgrsp_s),
			     BFA_CACHELINE_SZ);


	bfa_meminfo_dma_virt(meminfo) = dm_kva;
	bfa_meminfo_dma_phys(meminfo) = dm_pa;

	dbgsz = bfa_ioc_debug_trcsz(bfa_auto_recover);
	if (dbgsz > 0) {
		bfa_ioc_debug_memclaim(&bfa->ioc, bfa_meminfo_kva(meminfo));
		bfa_meminfo_kva(meminfo) += dbgsz;
	}
}

/**
 * Start BFA submodules.
 */
static void
bfa_iocfc_start_submod(struct bfa_s *bfa)
{
	int		i;

	bfa->rme_process = BFA_TRUE;

	for (i = 0; hal_mods[i]; i++)
		hal_mods[i]->start(bfa);
}

/**
 * Disable BFA submodules.
 */
static void
bfa_iocfc_disable_submod(struct bfa_s *bfa)
{
	int		i;

	for (i = 0; hal_mods[i]; i++)
		hal_mods[i]->iocdisable(bfa);
}

static void
bfa_iocfc_init_cb(void *bfa_arg, bfa_boolean_t complete)
{
	struct bfa_s	*bfa = bfa_arg;

	if (complete) {
		if (bfa->iocfc.cfgdone)
			bfa_cb_init(bfa->bfad, BFA_STATUS_OK);
		else
			bfa_cb_init(bfa->bfad, BFA_STATUS_FAILED);
	} else {
		if (bfa->iocfc.cfgdone)
			bfa->iocfc.action = BFA_IOCFC_ACT_NONE;
	}
}

static void
bfa_iocfc_stop_cb(void *bfa_arg, bfa_boolean_t compl)
{
	struct bfa_s  *bfa = bfa_arg;
	struct bfad_s *bfad = bfa->bfad;

	if (compl)
		complete(&bfad->comp);
	else
		bfa->iocfc.action = BFA_IOCFC_ACT_NONE;
}

static void
bfa_iocfc_disable_cb(void *bfa_arg, bfa_boolean_t compl)
{
	struct bfa_s  *bfa = bfa_arg;
	struct bfad_s *bfad = bfa->bfad;

	if (compl)
		complete(&bfad->disable_comp);
}

/**
 * Update BFA configuration from firmware configuration.
 */
static void
bfa_iocfc_cfgrsp(struct bfa_s *bfa)
{
	struct bfa_iocfc_s		*iocfc	 = &bfa->iocfc;
	struct bfi_iocfc_cfgrsp_s	*cfgrsp	 = iocfc->cfgrsp;
	struct bfa_iocfc_fwcfg_s	*fwcfg	 = &cfgrsp->fwcfg;

	fwcfg->num_cqs	      = fwcfg->num_cqs;
	fwcfg->num_ioim_reqs  = bfa_os_ntohs(fwcfg->num_ioim_reqs);
	fwcfg->num_tskim_reqs = bfa_os_ntohs(fwcfg->num_tskim_reqs);
	fwcfg->num_fcxp_reqs  = bfa_os_ntohs(fwcfg->num_fcxp_reqs);
	fwcfg->num_uf_bufs    = bfa_os_ntohs(fwcfg->num_uf_bufs);
	fwcfg->num_rports     = bfa_os_ntohs(fwcfg->num_rports);

	iocfc->cfgdone = BFA_TRUE;

	/**
	 * Configuration is complete - initialize/start submodules
	 */
	bfa_fcport_init(bfa);

	if (iocfc->action == BFA_IOCFC_ACT_INIT)
		bfa_cb_queue(bfa, &iocfc->init_hcb_qe, bfa_iocfc_init_cb, bfa);
	else
		bfa_iocfc_start_submod(bfa);
}
void
bfa_iocfc_reset_queues(struct bfa_s *bfa)
{
	int		q;

	for (q = 0; q < BFI_IOC_MAX_CQS; q++) {
		bfa_reqq_ci(bfa, q) = 0;
		bfa_reqq_pi(bfa, q) = 0;
		bfa_rspq_ci(bfa, q) = 0;
		bfa_rspq_pi(bfa, q) = 0;
	}
}

/**
 * IOC enable request is complete
 */
static void
bfa_iocfc_enable_cbfn(void *bfa_arg, enum bfa_status status)
{
	struct bfa_s	*bfa = bfa_arg;

	if (status != BFA_STATUS_OK) {
		bfa_isr_disable(bfa);
		if (bfa->iocfc.action == BFA_IOCFC_ACT_INIT)
			bfa_cb_queue(bfa, &bfa->iocfc.init_hcb_qe,
				     bfa_iocfc_init_cb, bfa);
		return;
	}

	bfa_iocfc_send_cfg(bfa);
}

/**
 * IOC disable request is complete
 */
static void
bfa_iocfc_disable_cbfn(void *bfa_arg)
{
	struct bfa_s	*bfa = bfa_arg;

	bfa_isr_disable(bfa);
	bfa_iocfc_disable_submod(bfa);

	if (bfa->iocfc.action == BFA_IOCFC_ACT_STOP)
		bfa_cb_queue(bfa, &bfa->iocfc.stop_hcb_qe, bfa_iocfc_stop_cb,
			     bfa);
	else {
		bfa_assert(bfa->iocfc.action == BFA_IOCFC_ACT_DISABLE);
		bfa_cb_queue(bfa, &bfa->iocfc.dis_hcb_qe, bfa_iocfc_disable_cb,
			     bfa);
	}
}

/**
 * Notify sub-modules of hardware failure.
 */
static void
bfa_iocfc_hbfail_cbfn(void *bfa_arg)
{
	struct bfa_s	*bfa = bfa_arg;

	bfa->rme_process = BFA_FALSE;

	bfa_isr_disable(bfa);
	bfa_iocfc_disable_submod(bfa);

	if (bfa->iocfc.action == BFA_IOCFC_ACT_INIT)
		bfa_cb_queue(bfa, &bfa->iocfc.init_hcb_qe, bfa_iocfc_init_cb,
			     bfa);
}

/**
 * Actions on chip-reset completion.
 */
static void
bfa_iocfc_reset_cbfn(void *bfa_arg)
{
	struct bfa_s	*bfa = bfa_arg;

	bfa_iocfc_reset_queues(bfa);
	bfa_isr_enable(bfa);
}

/**
 *  hal_ioc_public
 */

/**
 * Query IOC memory requirement information.
 */
void
bfa_iocfc_meminfo(struct bfa_iocfc_cfg_s *cfg, u32 *km_len,
		  u32 *dm_len)
{
	/* dma memory for IOC */
	*dm_len += bfa_ioc_meminfo();

	bfa_iocfc_fw_cfg_sz(cfg, dm_len);
	bfa_iocfc_cqs_sz(cfg, dm_len);
	*km_len += bfa_ioc_debug_trcsz(bfa_auto_recover);
}

/**
 * Query IOC memory requirement information.
 */
void
bfa_iocfc_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
		 struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	int		i;
	struct bfa_ioc_s *ioc = &bfa->ioc;

	bfa_iocfc_cbfn.enable_cbfn = bfa_iocfc_enable_cbfn;
	bfa_iocfc_cbfn.disable_cbfn = bfa_iocfc_disable_cbfn;
	bfa_iocfc_cbfn.hbfail_cbfn = bfa_iocfc_hbfail_cbfn;
	bfa_iocfc_cbfn.reset_cbfn = bfa_iocfc_reset_cbfn;

	ioc->trcmod = bfa->trcmod;
	bfa_ioc_attach(&bfa->ioc, bfa, &bfa_iocfc_cbfn, &bfa->timer_mod);

	/**
	 * Set FC mode for BFA_PCI_DEVICE_ID_CT_FC.
	 */
	if (pcidev->device_id == BFA_PCI_DEVICE_ID_CT_FC)
		bfa_ioc_set_fcmode(&bfa->ioc);

	bfa_ioc_pci_init(&bfa->ioc, pcidev, BFI_MC_IOCFC);
	bfa_ioc_mbox_register(&bfa->ioc, bfa_mbox_isrs);

	bfa_iocfc_init_mem(bfa, bfad, cfg, pcidev);
	bfa_iocfc_mem_claim(bfa, cfg, meminfo);
	bfa_timer_init(&bfa->timer_mod);

	INIT_LIST_HEAD(&bfa->comp_q);
	for (i = 0; i < BFI_IOC_MAX_CQS; i++)
		INIT_LIST_HEAD(&bfa->reqq_waitq[i]);
}

/**
 * Query IOC memory requirement information.
 */
void
bfa_iocfc_detach(struct bfa_s *bfa)
{
	bfa_ioc_detach(&bfa->ioc);
}

/**
 * Query IOC memory requirement information.
 */
void
bfa_iocfc_init(struct bfa_s *bfa)
{
	bfa->iocfc.action = BFA_IOCFC_ACT_INIT;
	bfa_ioc_enable(&bfa->ioc);
}

/**
 * IOC start called from bfa_start(). Called to start IOC operations
 * at driver instantiation for this instance.
 */
void
bfa_iocfc_start(struct bfa_s *bfa)
{
	if (bfa->iocfc.cfgdone)
		bfa_iocfc_start_submod(bfa);
}

/**
 * IOC stop called from bfa_stop(). Called only when driver is unloaded
 * for this instance.
 */
void
bfa_iocfc_stop(struct bfa_s *bfa)
{
	bfa->iocfc.action = BFA_IOCFC_ACT_STOP;

	bfa->rme_process = BFA_FALSE;
	bfa_ioc_disable(&bfa->ioc);
}

void
bfa_iocfc_isr(void *bfaarg, struct bfi_mbmsg_s *m)
{
	struct bfa_s		*bfa = bfaarg;
	struct bfa_iocfc_s	*iocfc = &bfa->iocfc;
	union bfi_iocfc_i2h_msg_u	*msg;

	msg = (union bfi_iocfc_i2h_msg_u *) m;
	bfa_trc(bfa, msg->mh.msg_id);

	switch (msg->mh.msg_id) {
	case BFI_IOCFC_I2H_CFG_REPLY:
		iocfc->cfg_reply = &msg->cfg_reply;
		bfa_iocfc_cfgrsp(bfa);
		break;
	case BFI_IOCFC_I2H_UPDATEQ_RSP:
		iocfc->updateq_cbfn(iocfc->updateq_cbarg, BFA_STATUS_OK);
		break;
	default:
		bfa_assert(0);
	}
}

void
bfa_adapter_get_attr(struct bfa_s *bfa, struct bfa_adapter_attr_s *ad_attr)
{
	bfa_ioc_get_adapter_attr(&bfa->ioc, ad_attr);
}

u64
bfa_adapter_get_id(struct bfa_s *bfa)
{
	return bfa_ioc_get_adid(&bfa->ioc);
}

void
bfa_iocfc_get_attr(struct bfa_s *bfa, struct bfa_iocfc_attr_s *attr)
{
	struct bfa_iocfc_s	*iocfc = &bfa->iocfc;

	attr->intr_attr.coalesce = iocfc->cfginfo->intr_attr.coalesce;

	attr->intr_attr.delay = iocfc->cfginfo->intr_attr.delay ?
				bfa_os_ntohs(iocfc->cfginfo->intr_attr.delay) :
				bfa_os_ntohs(iocfc->cfgrsp->intr_attr.delay);

	attr->intr_attr.latency = iocfc->cfginfo->intr_attr.latency ?
			bfa_os_ntohs(iocfc->cfginfo->intr_attr.latency) :
			bfa_os_ntohs(iocfc->cfgrsp->intr_attr.latency);

	attr->config	= iocfc->cfg;
}

bfa_status_t
bfa_iocfc_israttr_set(struct bfa_s *bfa, struct bfa_iocfc_intr_attr_s *attr)
{
	struct bfa_iocfc_s		*iocfc = &bfa->iocfc;
	struct bfi_iocfc_set_intr_req_s *m;

	iocfc->cfginfo->intr_attr.coalesce = attr->coalesce;
	iocfc->cfginfo->intr_attr.delay = bfa_os_htons(attr->delay);
	iocfc->cfginfo->intr_attr.latency = bfa_os_htons(attr->latency);

	if (!bfa_iocfc_is_operational(bfa))
		return BFA_STATUS_OK;

	m = bfa_reqq_next(bfa, BFA_REQQ_IOC);
	if (!m)
		return BFA_STATUS_DEVBUSY;

	bfi_h2i_set(m->mh, BFI_MC_IOCFC, BFI_IOCFC_H2I_SET_INTR_REQ,
		    bfa_lpuid(bfa));
	m->coalesce = iocfc->cfginfo->intr_attr.coalesce;
	m->delay    = iocfc->cfginfo->intr_attr.delay;
	m->latency  = iocfc->cfginfo->intr_attr.latency;

	bfa_trc(bfa, attr->delay);
	bfa_trc(bfa, attr->latency);

	bfa_reqq_produce(bfa, BFA_REQQ_IOC);
	return BFA_STATUS_OK;
}

void
bfa_iocfc_set_snsbase(struct bfa_s *bfa, u64 snsbase_pa)
{
	struct bfa_iocfc_s	*iocfc = &bfa->iocfc;

	iocfc->cfginfo->sense_buf_len = (BFI_IOIM_SNSLEN - 1);
	bfa_dma_be_addr_set(iocfc->cfginfo->ioim_snsbase, snsbase_pa);
}
/**
 * Enable IOC after it is disabled.
 */
void
bfa_iocfc_enable(struct bfa_s *bfa)
{
	bfa_plog_str(bfa->plog, BFA_PL_MID_HAL, BFA_PL_EID_MISC, 0,
		     "IOC Enable");
	bfa_ioc_enable(&bfa->ioc);
}

void
bfa_iocfc_disable(struct bfa_s *bfa)
{
	bfa_plog_str(bfa->plog, BFA_PL_MID_HAL, BFA_PL_EID_MISC, 0,
		     "IOC Disable");
	bfa->iocfc.action = BFA_IOCFC_ACT_DISABLE;

	bfa->rme_process = BFA_FALSE;
	bfa_ioc_disable(&bfa->ioc);
}


bfa_boolean_t
bfa_iocfc_is_operational(struct bfa_s *bfa)
{
	return bfa_ioc_is_operational(&bfa->ioc) && bfa->iocfc.cfgdone;
}

/**
 * Return boot target port wwns -- read from boot information in flash.
 */
void
bfa_iocfc_get_bootwwns(struct bfa_s *bfa, u8 *nwwns, wwn_t *wwns)
{
	struct bfa_iocfc_s *iocfc = &bfa->iocfc;
	struct bfi_iocfc_cfgrsp_s *cfgrsp = iocfc->cfgrsp;
	int i;

	if (cfgrsp->pbc_cfg.boot_enabled && cfgrsp->pbc_cfg.nbluns) {
		bfa_trc(bfa, cfgrsp->pbc_cfg.nbluns);
		*nwwns = cfgrsp->pbc_cfg.nbluns;
		for (i = 0; i < cfgrsp->pbc_cfg.nbluns; i++)
			wwns[i] = cfgrsp->pbc_cfg.blun[i].tgt_pwwn;

		return;
	}

	*nwwns = cfgrsp->bootwwns.nwwns;
	memcpy(wwns, cfgrsp->bootwwns.wwn, sizeof(cfgrsp->bootwwns.wwn));
}

void
bfa_iocfc_get_pbc_boot_cfg(struct bfa_s *bfa, struct bfa_boot_pbc_s *pbcfg)
{
	struct bfa_iocfc_s *iocfc = &bfa->iocfc;
	struct bfi_iocfc_cfgrsp_s *cfgrsp = iocfc->cfgrsp;

	pbcfg->enable = cfgrsp->pbc_cfg.boot_enabled;
	pbcfg->nbluns = cfgrsp->pbc_cfg.nbluns;
	pbcfg->speed = cfgrsp->pbc_cfg.port_speed;
	memcpy(pbcfg->pblun, cfgrsp->pbc_cfg.blun, sizeof(pbcfg->pblun));
}

int
bfa_iocfc_get_pbc_vports(struct bfa_s *bfa, struct bfi_pbc_vport_s *pbc_vport)
{
	struct bfa_iocfc_s *iocfc = &bfa->iocfc;
	struct bfi_iocfc_cfgrsp_s *cfgrsp = iocfc->cfgrsp;

	memcpy(pbc_vport, cfgrsp->pbc_cfg.vport, sizeof(cfgrsp->pbc_cfg.vport));
	return cfgrsp->pbc_cfg.nvports;
}

/**
 *  hal_api
 */

/**
 * Use this function query the memory requirement of the BFA library.
 * This function needs to be called before bfa_attach() to get the
 * memory required of the BFA layer for a given driver configuration.
 *
 * This call will fail, if the cap is out of range compared to pre-defined
 * values within the BFA library
 *
 * @param[in] cfg -	pointer to bfa_ioc_cfg_t. Driver layer should indicate
 *			its configuration in this structure.
 *			The default values for struct bfa_iocfc_cfg_s can be
 *			fetched using bfa_cfg_get_default() API.
 *
 *			If cap's boundary check fails, the library will use
 *			the default bfa_cap_t values (and log a warning msg).
 *
 * @param[out] meminfo - pointer to bfa_meminfo_t. This content
 *			indicates the memory type (see bfa_mem_type_t) and
 *			amount of memory required.
 *
 *			Driver should allocate the memory, populate the
 *			starting address for each block and provide the same
 *			structure as input parameter to bfa_attach() call.
 *
 * @return void
 *
 * Special Considerations: @note
 */
void
bfa_cfg_get_meminfo(struct bfa_iocfc_cfg_s *cfg, struct bfa_meminfo_s *meminfo)
{
	int		i;
	u32	km_len = 0, dm_len = 0;

	bfa_assert((cfg != NULL) && (meminfo != NULL));

	memset((void *)meminfo, 0, sizeof(struct bfa_meminfo_s));
	meminfo->meminfo[BFA_MEM_TYPE_KVA - 1].mem_type =
		BFA_MEM_TYPE_KVA;
	meminfo->meminfo[BFA_MEM_TYPE_DMA - 1].mem_type =
		BFA_MEM_TYPE_DMA;

	bfa_iocfc_meminfo(cfg, &km_len, &dm_len);

	for (i = 0; hal_mods[i]; i++)
		hal_mods[i]->meminfo(cfg, &km_len, &dm_len);

	dm_len += bfa_port_meminfo();

	meminfo->meminfo[BFA_MEM_TYPE_KVA - 1].mem_len = km_len;
	meminfo->meminfo[BFA_MEM_TYPE_DMA - 1].mem_len = dm_len;
}

/**
 * Use this function to do attach the driver instance with the BFA
 * library. This function will not trigger any HW initialization
 * process (which will be done in bfa_init() call)
 *
 * This call will fail, if the cap is out of range compared to
 * pre-defined values within the BFA library
 *
 * @param[out]	bfa	Pointer to bfa_t.
 * @param[in]	bfad	Opaque handle back to the driver's IOC structure
 * @param[in]	cfg	Pointer to bfa_ioc_cfg_t. Should be same structure
 *			that was used in bfa_cfg_get_meminfo().
 * @param[in]	meminfo	Pointer to bfa_meminfo_t. The driver should
 *			use the bfa_cfg_get_meminfo() call to
 *			find the memory blocks required, allocate the
 *			required memory and provide the starting addresses.
 * @param[in]	pcidev	pointer to struct bfa_pcidev_s
 *
 * @return
 * void
 *
 * Special Considerations:
 *
 * @note
 *
 */
void
bfa_attach(struct bfa_s *bfa, void *bfad, struct bfa_iocfc_cfg_s *cfg,
	       struct bfa_meminfo_s *meminfo, struct bfa_pcidev_s *pcidev)
{
	int			i;
	struct bfa_mem_elem_s	*melem;

	bfa->fcs = BFA_FALSE;

	bfa_assert((cfg != NULL) && (meminfo != NULL));

	/**
	 * initialize all memory pointers for iterative allocation
	 */
	for (i = 0; i < BFA_MEM_TYPE_MAX; i++) {
		melem = meminfo->meminfo + i;
		melem->kva_curp = melem->kva;
		melem->dma_curp = melem->dma;
	}

	bfa_iocfc_attach(bfa, bfad, cfg, meminfo, pcidev);

	for (i = 0; hal_mods[i]; i++)
		hal_mods[i]->attach(bfa, bfad, cfg, meminfo, pcidev);

	bfa_com_port_attach(bfa, meminfo);
}

/**
 * Use this function to delete a BFA IOC. IOC should be stopped (by
 * calling bfa_stop()) before this function call.
 *
 * @param[in] bfa - pointer to bfa_t.
 *
 * @return
 * void
 *
 * Special Considerations:
 *
 * @note
 */
void
bfa_detach(struct bfa_s *bfa)
{
	int	i;

	for (i = 0; hal_mods[i]; i++)
		hal_mods[i]->detach(bfa);

	bfa_iocfc_detach(bfa);
}


void
bfa_init_trc(struct bfa_s *bfa, struct bfa_trc_mod_s *trcmod)
{
	bfa->trcmod = trcmod;
}

void
bfa_init_plog(struct bfa_s *bfa, struct bfa_plog_s *plog)
{
	bfa->plog = plog;
}

/**
 * Initialize IOC.
 *
 * This function will return immediately, when the IOC initialization is
 * completed, the bfa_cb_init() will be called.
 *
 * @param[in]	bfa	instance
 *
 * @return void
 *
 * Special Considerations:
 *
 * @note
 * When this function returns, the driver should register the interrupt service
 * routine(s) and enable the device interrupts. If this is not done,
 * bfa_cb_init() will never get called
 */
void
bfa_init(struct bfa_s *bfa)
{
	bfa_iocfc_init(bfa);
}

/**
 * Use this function initiate the IOC configuration setup. This function
 * will return immediately.
 *
 * @param[in]	bfa	instance
 *
 * @return None
 */
void
bfa_start(struct bfa_s *bfa)
{
	bfa_iocfc_start(bfa);
}

/**
 * Use this function quiese the IOC. This function will return immediately,
 * when the IOC is actually stopped, the bfad->comp will be set.
 *
 * @param[in]bfa - pointer to bfa_t.
 *
 * @return None
 *
 * Special Considerations:
 * bfad->comp can be set before or after bfa_stop() returns.
 *
 * @note
 * In case of any failure, we could handle it automatically by doing a
 * reset and then succeed the bfa_stop() call.
 */
void
bfa_stop(struct bfa_s *bfa)
{
	bfa_iocfc_stop(bfa);
}

void
bfa_comp_deq(struct bfa_s *bfa, struct list_head *comp_q)
{
	INIT_LIST_HEAD(comp_q);
	list_splice_tail_init(&bfa->comp_q, comp_q);
}

void
bfa_comp_process(struct bfa_s *bfa, struct list_head *comp_q)
{
	struct list_head		*qe;
	struct list_head		*qen;
	struct bfa_cb_qe_s	*hcb_qe;

	list_for_each_safe(qe, qen, comp_q) {
		hcb_qe = (struct bfa_cb_qe_s *) qe;
		hcb_qe->cbfn(hcb_qe->cbarg, BFA_TRUE);
	}
}

void
bfa_comp_free(struct bfa_s *bfa, struct list_head *comp_q)
{
	struct list_head		*qe;
	struct bfa_cb_qe_s	*hcb_qe;

	while (!list_empty(comp_q)) {
		bfa_q_deq(comp_q, &qe);
		hcb_qe = (struct bfa_cb_qe_s *) qe;
		hcb_qe->cbfn(hcb_qe->cbarg, BFA_FALSE);
	}
}

void
bfa_attach_fcs(struct bfa_s *bfa)
{
	bfa->fcs = BFA_TRUE;
}

/**
 * Periodic timer heart beat from driver
 */
void
bfa_timer_tick(struct bfa_s *bfa)
{
	bfa_timer_beat(&bfa->timer_mod);
}

/**
 * Return the list of PCI vendor/device id lists supported by this
 * BFA instance.
 */
void
bfa_get_pciids(struct bfa_pciid_s **pciids, int *npciids)
{
	static struct bfa_pciid_s __pciids[] = {
		{BFA_PCI_VENDOR_ID_BROCADE, BFA_PCI_DEVICE_ID_FC_8G2P},
		{BFA_PCI_VENDOR_ID_BROCADE, BFA_PCI_DEVICE_ID_FC_8G1P},
		{BFA_PCI_VENDOR_ID_BROCADE, BFA_PCI_DEVICE_ID_CT},
		{BFA_PCI_VENDOR_ID_BROCADE, BFA_PCI_DEVICE_ID_CT_FC},
	};

	*npciids = sizeof(__pciids) / sizeof(__pciids[0]);
	*pciids = __pciids;
}

/**
 * Use this function query the default struct bfa_iocfc_cfg_s value (compiled
 * into BFA layer). The OS driver can then turn back and overwrite entries that
 * have been configured by the user.
 *
 * @param[in] cfg - pointer to bfa_ioc_cfg_t
 *
 * @return
 *	void
 *
 * Special Considerations:
 * note
 */
void
bfa_cfg_get_default(struct bfa_iocfc_cfg_s *cfg)
{
	cfg->fwcfg.num_fabrics = DEF_CFG_NUM_FABRICS;
	cfg->fwcfg.num_lports = DEF_CFG_NUM_LPORTS;
	cfg->fwcfg.num_rports = DEF_CFG_NUM_RPORTS;
	cfg->fwcfg.num_ioim_reqs = DEF_CFG_NUM_IOIM_REQS;
	cfg->fwcfg.num_tskim_reqs = DEF_CFG_NUM_TSKIM_REQS;
	cfg->fwcfg.num_fcxp_reqs = DEF_CFG_NUM_FCXP_REQS;
	cfg->fwcfg.num_uf_bufs = DEF_CFG_NUM_UF_BUFS;
	cfg->fwcfg.num_cqs = DEF_CFG_NUM_CQS;

	cfg->drvcfg.num_reqq_elems = DEF_CFG_NUM_REQQ_ELEMS;
	cfg->drvcfg.num_rspq_elems = DEF_CFG_NUM_RSPQ_ELEMS;
	cfg->drvcfg.num_sgpgs = DEF_CFG_NUM_SGPGS;
	cfg->drvcfg.num_sboot_tgts = DEF_CFG_NUM_SBOOT_TGTS;
	cfg->drvcfg.num_sboot_luns = DEF_CFG_NUM_SBOOT_LUNS;
	cfg->drvcfg.path_tov = BFA_FCPIM_PATHTOV_DEF;
	cfg->drvcfg.ioc_recover = BFA_FALSE;
	cfg->drvcfg.delay_comp = BFA_FALSE;

}

void
bfa_cfg_get_min(struct bfa_iocfc_cfg_s *cfg)
{
	bfa_cfg_get_default(cfg);
	cfg->fwcfg.num_ioim_reqs   = BFA_IOIM_MIN;
	cfg->fwcfg.num_tskim_reqs  = BFA_TSKIM_MIN;
	cfg->fwcfg.num_fcxp_reqs   = BFA_FCXP_MIN;
	cfg->fwcfg.num_uf_bufs     = BFA_UF_MIN;
	cfg->fwcfg.num_rports      = BFA_RPORT_MIN;

	cfg->drvcfg.num_sgpgs      = BFA_SGPG_MIN;
	cfg->drvcfg.num_reqq_elems = BFA_REQQ_NELEMS_MIN;
	cfg->drvcfg.num_rspq_elems = BFA_RSPQ_NELEMS_MIN;
	cfg->drvcfg.min_cfg	   = BFA_TRUE;
}

void
bfa_get_attr(struct bfa_s *bfa, struct bfa_ioc_attr_s *ioc_attr)
{
	bfa_ioc_get_attr(&bfa->ioc, ioc_attr);
}

/**
 * Retrieve firmware trace information on IOC failure.
 */
bfa_status_t
bfa_debug_fwsave(struct bfa_s *bfa, void *trcdata, int *trclen)
{
	return bfa_ioc_debug_fwsave(&bfa->ioc, trcdata, trclen);
}

/**
 * Clear the saved firmware trace information of an IOC.
 */
void
bfa_debug_fwsave_clear(struct bfa_s *bfa)
{
	bfa_ioc_debug_fwsave_clear(&bfa->ioc);
}

/**
 * Fetch firmware trace data.
 *
 * @param[in]		bfa			BFA instance
 * @param[out]		trcdata		Firmware trace buffer
 * @param[in,out]	trclen		Firmware trace buffer len
 *
 * @retval BFA_STATUS_OK			Firmware trace is fetched.
 * @retval BFA_STATUS_INPROGRESS	Firmware trace fetch is in progress.
 */
bfa_status_t
bfa_debug_fwtrc(struct bfa_s *bfa, void *trcdata, int *trclen)
{
	return bfa_ioc_debug_fwtrc(&bfa->ioc, trcdata, trclen);
}

/**
 * Dump firmware memory.
 *
 * @param[in]		bfa		BFA instance
 * @param[out]		buf		buffer for dump
 * @param[in,out]	offset		smem offset to start read
 * @param[in,out]	buflen		length of buffer
 *
 * @retval BFA_STATUS_OK		Firmware memory is dumped.
 * @retval BFA_STATUS_INPROGRESS	Firmware memory dump is in progress.
 */
bfa_status_t
bfa_debug_fwcore(struct bfa_s *bfa, void *buf, u32 *offset, int *buflen)
{
	return bfa_ioc_debug_fwcore(&bfa->ioc, buf, offset, buflen);
}
/**
 * Reset hw semaphore & usage cnt regs and initialize.
 */
void
bfa_chip_reset(struct bfa_s *bfa)
{
	bfa_ioc_ownership_reset(&bfa->ioc);
	bfa_ioc_pll_init(&bfa->ioc);
}

/**
 * Fetch firmware statistics data.
 *
 * @param[in]		bfa		BFA instance
 * @param[out]		data		Firmware stats buffer
 *
 * @retval BFA_STATUS_OK		Firmware trace is fetched.
 */
bfa_status_t
bfa_fw_stats_get(struct bfa_s *bfa, void *data)
{
	return bfa_ioc_fw_stats_get(&bfa->ioc, data);
}

bfa_status_t
bfa_fw_stats_clear(struct bfa_s *bfa)
{
	return bfa_ioc_fw_stats_clear(&bfa->ioc);
}
