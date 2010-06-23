/*
 * resourcecleanup.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <dspbridge/nodepriv.h>
#include <dspbridge/drv.h>

extern int drv_get_proc_ctxt_list(struct process_context **pPctxt,
					 struct drv_object *hdrv_obj);

extern int drv_insert_proc_context(struct drv_object *hDrVObject,
					  void *hPCtxt);

extern int drv_remove_all_dmm_res_elements(void *ctxt);

extern int drv_remove_all_node_res_elements(void *ctxt);

extern int drv_proc_set_pid(void *ctxt, s32 process);

extern int drv_remove_all_resources(void *pPctxt);

extern int drv_remove_proc_context(struct drv_object *hDRVObject,
					  void *pr_ctxt);

extern int drv_get_node_res_element(void *hnode, void *node_res,
					   void *ctxt);

extern int drv_insert_node_res_element(void *hnode, void *node_res,
					      void *ctxt);

extern void drv_proc_node_update_heap_status(void *hNodeRes, s32 status);

extern int drv_remove_node_res_element(void *node_res, void *status);

extern void drv_proc_node_update_status(void *hNodeRes, s32 status);

extern int drv_proc_update_strm_res(u32 num_bufs, void *strm_res);

extern int drv_proc_insert_strm_res_element(void *hStrm,
						   void *strm_res,
						   void *pPctxt);

extern int drv_get_strm_res_element(void *hStrm, void *strm_res,
					   void *ctxt);

extern int drv_proc_remove_strm_res_element(void *strm_res,
						   void *ctxt);

extern int drv_remove_all_strm_res_elements(void *ctxt);

extern enum node_state node_get_state(void *hnode);
