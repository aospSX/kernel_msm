/*
 * Copyright © 2008-2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Zou Nan hai <nanhai.zou@intel.com>
 *    Xiang Hai hao<haihao.xiang@intel.com>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "i915_drm.h"
#include "i915_trace.h"
#include "intel_drv.h"

static u32 i915_gem_get_seqno(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 seqno;

	seqno = dev_priv->next_seqno;

	/* reserve 0 for non-seqno */
	if (++dev_priv->next_seqno == 0)
		dev_priv->next_seqno = 1;

	return seqno;
}

static void
render_ring_flush(struct intel_ring_buffer *ring,
		  u32	invalidate_domains,
		  u32	flush_domains)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 cmd;

#if WATCH_EXEC
	DRM_INFO("%s: invalidate %08x flush %08x\n", __func__,
		  invalidate_domains, flush_domains);
#endif

	trace_i915_gem_request_flush(dev, dev_priv->next_seqno,
				     invalidate_domains, flush_domains);

	if ((invalidate_domains | flush_domains) & I915_GEM_GPU_DOMAINS) {
		/*
		 * read/write caches:
		 *
		 * I915_GEM_DOMAIN_RENDER is always invalidated, but is
		 * only flushed if MI_NO_WRITE_FLUSH is unset.  On 965, it is
		 * also flushed at 2d versus 3d pipeline switches.
		 *
		 * read-only caches:
		 *
		 * I915_GEM_DOMAIN_SAMPLER is flushed on pre-965 if
		 * MI_READ_FLUSH is set, and is always flushed on 965.
		 *
		 * I915_GEM_DOMAIN_COMMAND may not exist?
		 *
		 * I915_GEM_DOMAIN_INSTRUCTION, which exists on 965, is
		 * invalidated when MI_EXE_FLUSH is set.
		 *
		 * I915_GEM_DOMAIN_VERTEX, which exists on 965, is
		 * invalidated with every MI_FLUSH.
		 *
		 * TLBs:
		 *
		 * On 965, TLBs associated with I915_GEM_DOMAIN_COMMAND
		 * and I915_GEM_DOMAIN_CPU in are invalidated at PTE write and
		 * I915_GEM_DOMAIN_RENDER and I915_GEM_DOMAIN_SAMPLER
		 * are flushed at any MI_FLUSH.
		 */

		cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
		if ((invalidate_domains|flush_domains) &
		    I915_GEM_DOMAIN_RENDER)
			cmd &= ~MI_NO_WRITE_FLUSH;
		if (INTEL_INFO(dev)->gen < 4) {
			/*
			 * On the 965, the sampler cache always gets flushed
			 * and this bit is reserved.
			 */
			if (invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
				cmd |= MI_READ_FLUSH;
		}
		if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
			cmd |= MI_EXE_FLUSH;

#if WATCH_EXEC
		DRM_INFO("%s: queue flush %08x to ring\n", __func__, cmd);
#endif
		if (intel_ring_begin(ring, 2) == 0) {
			intel_ring_emit(ring, cmd);
			intel_ring_emit(ring, MI_NOOP);
			intel_ring_advance(ring);
		}
	}
}

static void ring_write_tail(struct intel_ring_buffer *ring,
			    u32 value)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	I915_WRITE_TAIL(ring, value);
}

u32 intel_ring_get_active_head(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 acthd_reg = INTEL_INFO(ring->dev)->gen >= 4 ?
			RING_ACTHD(ring->mmio_base) : ACTHD;

	return I915_READ(acthd_reg);
}

static int init_ring_common(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_object *obj_priv = to_intel_bo(ring->gem_object);
	u32 head;

	/* Stop the ring if it's running. */
	I915_WRITE_CTL(ring, 0);
	I915_WRITE_HEAD(ring, 0);
	ring->write_tail(ring, 0);

	/* Initialize the ring. */
	I915_WRITE_START(ring, obj_priv->gtt_offset);
	head = I915_READ_HEAD(ring) & HEAD_ADDR;

	/* G45 ring initialization fails to reset head to zero */
	if (head != 0) {
		DRM_ERROR("%s head not reset to zero "
				"ctl %08x head %08x tail %08x start %08x\n",
				ring->name,
				I915_READ_CTL(ring),
				I915_READ_HEAD(ring),
				I915_READ_TAIL(ring),
				I915_READ_START(ring));

		I915_WRITE_HEAD(ring, 0);

		DRM_ERROR("%s head forced to zero "
				"ctl %08x head %08x tail %08x start %08x\n",
				ring->name,
				I915_READ_CTL(ring),
				I915_READ_HEAD(ring),
				I915_READ_TAIL(ring),
				I915_READ_START(ring));
	}

	I915_WRITE_CTL(ring,
			((ring->gem_object->size - PAGE_SIZE) & RING_NR_PAGES)
			| RING_REPORT_64K | RING_VALID);

	/* If the head is still not zero, the ring is dead */
	if ((I915_READ_CTL(ring) & RING_VALID) == 0 ||
	    I915_READ_START(ring) != obj_priv->gtt_offset ||
	    (I915_READ_HEAD(ring) & HEAD_ADDR) != 0) {
		DRM_ERROR("%s initialization failed "
				"ctl %08x head %08x tail %08x start %08x\n",
				ring->name,
				I915_READ_CTL(ring),
				I915_READ_HEAD(ring),
				I915_READ_TAIL(ring),
				I915_READ_START(ring));
		return -EIO;
	}

	if (!drm_core_check_feature(ring->dev, DRIVER_MODESET))
		i915_kernel_lost_context(ring->dev);
	else {
		ring->head = I915_READ_HEAD(ring) & HEAD_ADDR;
		ring->tail = I915_READ_TAIL(ring) & TAIL_ADDR;
		ring->space = ring->head - (ring->tail + 8);
		if (ring->space < 0)
			ring->space += ring->size;
	}
	return 0;
}

static int init_render_ring(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	int ret = init_ring_common(ring);

	if (INTEL_INFO(dev)->gen > 3) {
		drm_i915_private_t *dev_priv = dev->dev_private;
		int mode = VS_TIMER_DISPATCH << 16 | VS_TIMER_DISPATCH;
		if (IS_GEN6(dev))
			mode |= MI_FLUSH_ENABLE << 16 | MI_FLUSH_ENABLE;
		I915_WRITE(MI_MODE, mode);
	}

	return ret;
}

#define PIPE_CONTROL_FLUSH(ring__, addr__)					\
do {									\
	intel_ring_emit(ring__, GFX_OP_PIPE_CONTROL | PIPE_CONTROL_QW_WRITE |		\
		 PIPE_CONTROL_DEPTH_STALL | 2);				\
	intel_ring_emit(ring__, (addr__) | PIPE_CONTROL_GLOBAL_GTT);			\
	intel_ring_emit(ring__, 0);							\
	intel_ring_emit(ring__, 0);							\
} while (0)

/**
 * Creates a new sequence number, emitting a write of it to the status page
 * plus an interrupt, which will trigger i915_user_interrupt_handler.
 *
 * Must be called with struct_lock held.
 *
 * Returned sequence numbers are nonzero on success.
 */
static int
render_ring_add_request(struct intel_ring_buffer *ring,
			u32 *result)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	u32 seqno = i915_gem_get_seqno(dev);
	int ret;

	if (IS_GEN6(dev)) {
		ret = intel_ring_begin(ring, 6);
		if (ret)
		    return ret;

		intel_ring_emit(ring, GFX_OP_PIPE_CONTROL | 3);
		intel_ring_emit(ring, PIPE_CONTROL_QW_WRITE |
				PIPE_CONTROL_WC_FLUSH | PIPE_CONTROL_IS_FLUSH |
				PIPE_CONTROL_NOTIFY);
		intel_ring_emit(ring, dev_priv->seqno_gfx_addr | PIPE_CONTROL_GLOBAL_GTT);
		intel_ring_emit(ring, seqno);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, 0);
	} else if (HAS_PIPE_CONTROL(dev)) {
		u32 scratch_addr = dev_priv->seqno_gfx_addr + 128;

		/*
		 * Workaround qword write incoherence by flushing the
		 * PIPE_NOTIFY buffers out to memory before requesting
		 * an interrupt.
		 */
		ret = intel_ring_begin(ring, 32);
		if (ret)
			return ret;

		intel_ring_emit(ring, GFX_OP_PIPE_CONTROL | PIPE_CONTROL_QW_WRITE |
				PIPE_CONTROL_WC_FLUSH | PIPE_CONTROL_TC_FLUSH);
		intel_ring_emit(ring, dev_priv->seqno_gfx_addr | PIPE_CONTROL_GLOBAL_GTT);
		intel_ring_emit(ring, seqno);
		intel_ring_emit(ring, 0);
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		scratch_addr += 128; /* write to separate cachelines */
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		scratch_addr += 128;
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		scratch_addr += 128;
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		scratch_addr += 128;
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		scratch_addr += 128;
		PIPE_CONTROL_FLUSH(ring, scratch_addr);
		intel_ring_emit(ring, GFX_OP_PIPE_CONTROL | PIPE_CONTROL_QW_WRITE |
				PIPE_CONTROL_WC_FLUSH | PIPE_CONTROL_TC_FLUSH |
				PIPE_CONTROL_NOTIFY);
		intel_ring_emit(ring, dev_priv->seqno_gfx_addr | PIPE_CONTROL_GLOBAL_GTT);
		intel_ring_emit(ring, seqno);
		intel_ring_emit(ring, 0);
	} else {
		ret = intel_ring_begin(ring, 4);
		if (ret)
		    return ret;

		intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
		intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
		intel_ring_emit(ring, seqno);

		intel_ring_emit(ring, MI_USER_INTERRUPT);
	}

	intel_ring_advance(ring);
	*result = seqno;
	return 0;
}

static u32
render_ring_get_seqno(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	if (HAS_PIPE_CONTROL(dev))
		return ((volatile u32 *)(dev_priv->seqno_page))[0];
	else
		return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static void
render_ring_get_user_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->user_irq_lock, irqflags);
	if (dev->irq_enabled && (++ring->user_irq_refcount == 1)) {
		if (HAS_PCH_SPLIT(dev))
			ironlake_enable_graphics_irq(dev_priv, GT_PIPE_NOTIFY);
		else
			i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
	}
	spin_unlock_irqrestore(&dev_priv->user_irq_lock, irqflags);
}

static void
render_ring_put_user_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = (drm_i915_private_t *) dev->dev_private;
	unsigned long irqflags;

	spin_lock_irqsave(&dev_priv->user_irq_lock, irqflags);
	BUG_ON(dev->irq_enabled && ring->user_irq_refcount <= 0);
	if (dev->irq_enabled && (--ring->user_irq_refcount == 0)) {
		if (HAS_PCH_SPLIT(dev))
			ironlake_disable_graphics_irq(dev_priv, GT_PIPE_NOTIFY);
		else
			i915_disable_irq(dev_priv, I915_USER_INTERRUPT);
	}
	spin_unlock_irqrestore(&dev_priv->user_irq_lock, irqflags);
}

void intel_ring_setup_status_page(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 mmio = IS_GEN6(ring->dev) ?
		RING_HWS_PGA_GEN6(ring->mmio_base) :
		RING_HWS_PGA(ring->mmio_base);
	I915_WRITE(mmio, (u32)ring->status_page.gfx_addr);
	POSTING_READ(mmio);
}

static void
bsd_ring_flush(struct intel_ring_buffer *ring,
	       u32     invalidate_domains,
	       u32     flush_domains)
{
	if (intel_ring_begin(ring, 2) == 0) {
		intel_ring_emit(ring, MI_FLUSH);
		intel_ring_emit(ring, MI_NOOP);
		intel_ring_advance(ring);
	}
}

static int
ring_add_request(struct intel_ring_buffer *ring,
		 u32 *result)
{
	u32 seqno;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	seqno = i915_gem_get_seqno(ring->dev);

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	DRM_DEBUG_DRIVER("%s %d\n", ring->name, seqno);
	*result = seqno;
	return 0;
}

static void
bsd_ring_get_user_irq(struct intel_ring_buffer *ring)
{
	/* do nothing */
}
static void
bsd_ring_put_user_irq(struct intel_ring_buffer *ring)
{
	/* do nothing */
}

static u32
ring_status_page_get_seqno(struct intel_ring_buffer *ring)
{
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static int
ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
			 struct drm_i915_gem_execbuffer2 *exec,
			 struct drm_clip_rect *cliprects,
			 uint64_t exec_offset)
{
	uint32_t exec_start;
	int ret;

	exec_start = (uint32_t) exec_offset + exec->batch_start_offset;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring,
			MI_BATCH_BUFFER_START |
			(2 << 6) |
			MI_BATCH_NON_SECURE_I965);
	intel_ring_emit(ring, exec_start);
	intel_ring_advance(ring);

	return 0;
}

static int
render_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
				struct drm_i915_gem_execbuffer2 *exec,
				struct drm_clip_rect *cliprects,
				uint64_t exec_offset)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	int nbox = exec->num_cliprects;
	uint32_t exec_start, exec_len;
	int i, count, ret;

	exec_start = (uint32_t) exec_offset + exec->batch_start_offset;
	exec_len = (uint32_t) exec->batch_len;

	trace_i915_gem_request_submit(dev, dev_priv->next_seqno + 1);

	count = nbox ? nbox : 1;
	for (i = 0; i < count; i++) {
		if (i < nbox) {
			ret = i915_emit_box(dev, cliprects, i,
					    exec->DR1, exec->DR4);
			if (ret)
				return ret;
		}

		if (IS_I830(dev) || IS_845G(dev)) {
			ret = intel_ring_begin(ring, 4);
			if (ret)
				return ret;

			intel_ring_emit(ring, MI_BATCH_BUFFER);
			intel_ring_emit(ring, exec_start | MI_BATCH_NON_SECURE);
			intel_ring_emit(ring, exec_start + exec_len - 4);
			intel_ring_emit(ring, 0);
		} else {
			ret = intel_ring_begin(ring, 2);
			if (ret)
				return ret;

			if (INTEL_INFO(dev)->gen >= 4) {
				intel_ring_emit(ring,
						MI_BATCH_BUFFER_START | (2 << 6)
						| MI_BATCH_NON_SECURE_I965);
				intel_ring_emit(ring, exec_start);
			} else {
				intel_ring_emit(ring, MI_BATCH_BUFFER_START
						| (2 << 6));
				intel_ring_emit(ring, exec_start |
						MI_BATCH_NON_SECURE);
			}
		}
		intel_ring_advance(ring);
	}

	if (IS_G4X(dev) || IS_GEN5(dev)) {
		if (intel_ring_begin(ring, 2) == 0) {
			intel_ring_emit(ring, MI_FLUSH |
					MI_NO_WRITE_FLUSH |
					MI_INVALIDATE_ISP );
			intel_ring_emit(ring, MI_NOOP);
			intel_ring_advance(ring);
		}
	}
	/* XXX breadcrumb */

	return 0;
}

static void cleanup_status_page(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;

	obj = ring->status_page.obj;
	if (obj == NULL)
		return;
	obj_priv = to_intel_bo(obj);

	kunmap(obj_priv->pages[0]);
	i915_gem_object_unpin(obj);
	drm_gem_object_unreference(obj);
	ring->status_page.obj = NULL;

	memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
}

static int init_status_page(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_gem_object *obj;
	struct drm_i915_gem_object *obj_priv;
	int ret;

	obj = i915_gem_alloc_object(dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate status page\n");
		ret = -ENOMEM;
		goto err;
	}
	obj_priv = to_intel_bo(obj);
	obj_priv->agp_type = AGP_USER_CACHED_MEMORY;

	ret = i915_gem_object_pin(obj, 4096, true, false);
	if (ret != 0) {
		goto err_unref;
	}

	ring->status_page.gfx_addr = obj_priv->gtt_offset;
	ring->status_page.page_addr = kmap(obj_priv->pages[0]);
	if (ring->status_page.page_addr == NULL) {
		memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
		goto err_unpin;
	}
	ring->status_page.obj = obj;
	memset(ring->status_page.page_addr, 0, PAGE_SIZE);

	intel_ring_setup_status_page(ring);
	DRM_DEBUG_DRIVER("%s hws offset: 0x%08x\n",
			ring->name, ring->status_page.gfx_addr);

	return 0;

err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(obj);
err:
	return ret;
}

int intel_init_ring_buffer(struct drm_device *dev,
			   struct intel_ring_buffer *ring)
{
	struct drm_i915_gem_object *obj_priv;
	struct drm_gem_object *obj;
	int ret;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	INIT_LIST_HEAD(&ring->gpu_write_list);

	if (I915_NEED_GFX_HWS(dev)) {
		ret = init_status_page(ring);
		if (ret)
			return ret;
	}

	obj = i915_gem_alloc_object(dev, ring->size);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate ringbuffer\n");
		ret = -ENOMEM;
		goto err_hws;
	}

	ring->gem_object = obj;

	ret = i915_gem_object_pin(obj, PAGE_SIZE, true, false);
	if (ret)
		goto err_unref;

	obj_priv = to_intel_bo(obj);
	ring->map.size = ring->size;
	ring->map.offset = dev->agp->base + obj_priv->gtt_offset;
	ring->map.type = 0;
	ring->map.flags = 0;
	ring->map.mtrr = 0;

	drm_core_ioremap_wc(&ring->map, dev);
	if (ring->map.handle == NULL) {
		DRM_ERROR("Failed to map ringbuffer.\n");
		ret = -EINVAL;
		goto err_unpin;
	}

	ring->virtual_start = ring->map.handle;
	ret = ring->init(ring);
	if (ret)
		goto err_unmap;

	return 0;

err_unmap:
	drm_core_ioremapfree(&ring->map, dev);
err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(obj);
	ring->gem_object = NULL;
err_hws:
	cleanup_status_page(ring);
	return ret;
}

void intel_cleanup_ring_buffer(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv;
	int ret;

	if (ring->gem_object == NULL)
		return;

	/* Disable the ring buffer. The ring must be idle at this point */
	dev_priv = ring->dev->dev_private;
	ret = intel_wait_ring_buffer(ring, ring->size - 8);
	I915_WRITE_CTL(ring, 0);

	drm_core_ioremapfree(&ring->map, ring->dev);

	i915_gem_object_unpin(ring->gem_object);
	drm_gem_object_unreference(ring->gem_object);
	ring->gem_object = NULL;

	cleanup_status_page(ring);
}

static int intel_wrap_ring_buffer(struct intel_ring_buffer *ring)
{
	unsigned int *virt;
	int rem;
	rem = ring->size - ring->tail;

	if (ring->space < rem) {
		int ret = intel_wait_ring_buffer(ring, rem);
		if (ret)
			return ret;
	}

	virt = (unsigned int *)(ring->virtual_start + ring->tail);
	rem /= 8;
	while (rem--) {
		*virt++ = MI_NOOP;
		*virt++ = MI_NOOP;
	}

	ring->tail = 0;
	ring->space = ring->head - 8;

	return 0;
}

int intel_wait_ring_buffer(struct intel_ring_buffer *ring, int n)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	unsigned long end;
	u32 head;

	head = intel_read_status_page(ring, 4);
	if (head) {
		ring->head = head & HEAD_ADDR;
		ring->space = ring->head - (ring->tail + 8);
		if (ring->space < 0)
			ring->space += ring->size;
		if (ring->space >= n)
			return 0;
	}

	trace_i915_ring_wait_begin (dev);
	end = jiffies + 3 * HZ;
	do {
		ring->head = I915_READ_HEAD(ring) & HEAD_ADDR;
		ring->space = ring->head - (ring->tail + 8);
		if (ring->space < 0)
			ring->space += ring->size;
		if (ring->space >= n) {
			trace_i915_ring_wait_end(dev);
			return 0;
		}

		if (dev->primary->master) {
			struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
			if (master_priv->sarea_priv)
				master_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;
		}

		msleep(1);
		if (atomic_read(&dev_priv->mm.wedged))
			return -EAGAIN;
	} while (!time_after(jiffies, end));
	trace_i915_ring_wait_end (dev);
	return -EBUSY;
}

int intel_ring_begin(struct intel_ring_buffer *ring,
		     int num_dwords)
{
	int n = 4*num_dwords;
	int ret;

	if (unlikely(ring->tail + n > ring->size)) {
		ret = intel_wrap_ring_buffer(ring);
		if (unlikely(ret))
			return ret;
	}

	if (unlikely(ring->space < n)) {
		ret = intel_wait_ring_buffer(ring, n);
		if (unlikely(ret))
			return ret;
	}

	ring->space -= n;
	return 0;
}

void intel_ring_advance(struct intel_ring_buffer *ring)
{
	ring->tail &= ring->size - 1;
	ring->write_tail(ring, ring->tail);
}

static const struct intel_ring_buffer render_ring = {
	.name			= "render ring",
	.id			= RING_RENDER,
	.mmio_base		= RENDER_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_render_ring,
	.write_tail		= ring_write_tail,
	.flush			= render_ring_flush,
	.add_request		= render_ring_add_request,
	.get_seqno		= render_ring_get_seqno,
	.user_irq_get		= render_ring_get_user_irq,
	.user_irq_put		= render_ring_put_user_irq,
	.dispatch_execbuffer	= render_ring_dispatch_execbuffer,
};

/* ring buffer for bit-stream decoder */

static const struct intel_ring_buffer bsd_ring = {
	.name                   = "bsd ring",
	.id			= RING_BSD,
	.mmio_base		= BSD_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_ring_common,
	.write_tail		= ring_write_tail,
	.flush			= bsd_ring_flush,
	.add_request		= ring_add_request,
	.get_seqno		= ring_status_page_get_seqno,
	.user_irq_get		= bsd_ring_get_user_irq,
	.user_irq_put		= bsd_ring_put_user_irq,
	.dispatch_execbuffer	= ring_dispatch_execbuffer,
};


static void gen6_bsd_ring_write_tail(struct intel_ring_buffer *ring,
				     u32 value)
{
       drm_i915_private_t *dev_priv = ring->dev->dev_private;

       /* Every tail move must follow the sequence below */
       I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
	       GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_MODIFY_MASK |
	       GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_DISABLE);
       I915_WRITE(GEN6_BSD_RNCID, 0x0);

       if (wait_for((I915_READ(GEN6_BSD_SLEEP_PSMI_CONTROL) &
                               GEN6_BSD_SLEEP_PSMI_CONTROL_IDLE_INDICATOR) == 0,
                       50))
               DRM_ERROR("timed out waiting for IDLE Indicator\n");

       I915_WRITE_TAIL(ring, value);
       I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
	       GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_MODIFY_MASK |
	       GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_ENABLE);
}

static void gen6_ring_flush(struct intel_ring_buffer *ring,
			    u32 invalidate_domains,
			    u32 flush_domains)
{
	if (intel_ring_begin(ring, 4) == 0) {
		intel_ring_emit(ring, MI_FLUSH_DW);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, 0);
		intel_ring_emit(ring, 0);
		intel_ring_advance(ring);
	}
}

static int
gen6_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
			      struct drm_i915_gem_execbuffer2 *exec,
			      struct drm_clip_rect *cliprects,
			      uint64_t exec_offset)
{
       uint32_t exec_start;
       int ret;

       exec_start = (uint32_t) exec_offset + exec->batch_start_offset;

       ret = intel_ring_begin(ring, 2);
       if (ret)
	       return ret;

       intel_ring_emit(ring, MI_BATCH_BUFFER_START | MI_BATCH_NON_SECURE_I965);
       /* bit0-7 is the length on GEN6+ */
       intel_ring_emit(ring, exec_start);
       intel_ring_advance(ring);

       return 0;
}

/* ring buffer for Video Codec for Gen6+ */
static const struct intel_ring_buffer gen6_bsd_ring = {
       .name			= "gen6 bsd ring",
       .id			= RING_BSD,
       .mmio_base		= GEN6_BSD_RING_BASE,
       .size			= 32 * PAGE_SIZE,
       .init			= init_ring_common,
       .write_tail		= gen6_bsd_ring_write_tail,
       .flush			= gen6_ring_flush,
       .add_request		= ring_add_request,
       .get_seqno		= ring_status_page_get_seqno,
       .user_irq_get		= bsd_ring_get_user_irq,
       .user_irq_put		= bsd_ring_put_user_irq,
       .dispatch_execbuffer	= gen6_ring_dispatch_execbuffer,
};

/* Blitter support (SandyBridge+) */

static void
blt_ring_get_user_irq(struct intel_ring_buffer *ring)
{
	/* do nothing */
}
static void
blt_ring_put_user_irq(struct intel_ring_buffer *ring)
{
	/* do nothing */
}

static const struct intel_ring_buffer gen6_blt_ring = {
       .name			= "blt ring",
       .id			= RING_BLT,
       .mmio_base		= BLT_RING_BASE,
       .size			= 32 * PAGE_SIZE,
       .init			= init_ring_common,
       .write_tail		= ring_write_tail,
       .flush			= gen6_ring_flush,
       .add_request		= ring_add_request,
       .get_seqno		= ring_status_page_get_seqno,
       .user_irq_get		= blt_ring_get_user_irq,
       .user_irq_put		= blt_ring_put_user_irq,
       .dispatch_execbuffer	= gen6_ring_dispatch_execbuffer,
};

int intel_init_render_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	dev_priv->render_ring = render_ring;

	if (!I915_NEED_GFX_HWS(dev)) {
		dev_priv->render_ring.status_page.page_addr
			= dev_priv->status_page_dmah->vaddr;
		memset(dev_priv->render_ring.status_page.page_addr,
				0, PAGE_SIZE);
	}

	return intel_init_ring_buffer(dev, &dev_priv->render_ring);
}

int intel_init_bsd_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (IS_GEN6(dev))
		dev_priv->bsd_ring = gen6_bsd_ring;
	else
		dev_priv->bsd_ring = bsd_ring;

	return intel_init_ring_buffer(dev, &dev_priv->bsd_ring);
}

int intel_init_blt_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;

	dev_priv->blt_ring = gen6_blt_ring;

	return intel_init_ring_buffer(dev, &dev_priv->blt_ring);
}
