#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/range.h>

/* Check for already reserved areas */
static inline bool __init bad_addr_size(u64 *addrp, u64 *sizep, u64 align)
{
	struct memblock_region *r;
	u64 addr = *addrp, last;
	u64 size = *sizep;
	bool changed = false;

again:
	last = addr + size;
	for_each_memblock(reserved, r) {
		if (last > r->base && addr < r->base) {
			size = r->base - addr;
			changed = true;
			goto again;
		}
		if (last > (r->base + r->size) && addr < (r->base + r->size)) {
			addr = round_up(r->base + r->size, align);
			size = last - addr;
			changed = true;
			goto again;
		}
		if (last <= (r->base + r->size) && addr >= r->base) {
			(*sizep)++;
			return false;
		}
	}
	if (changed) {
		*addrp = addr;
		*sizep = size;
	}
	return changed;
}

static u64 __init __memblock_x86_find_in_range_size(u64 ei_start, u64 ei_last, u64 start,
			 u64 *sizep, u64 align)
{
	u64 addr, last;

	addr = round_up(ei_start, align);
	if (addr < start)
		addr = round_up(start, align);
	if (addr >= ei_last)
		goto out;
	*sizep = ei_last - addr;
	while (bad_addr_size(&addr, sizep, align) && addr + *sizep <= ei_last)
		;
	last = addr + *sizep;
	if (last > ei_last)
		goto out;

	return addr;

out:
	return MEMBLOCK_ERROR;
}

/*
 * Find next free range after start, and size is returned in *sizep
 */
u64 __init memblock_x86_find_in_range_size(u64 start, u64 *sizep, u64 align)
{
	struct memblock_region *r;

	for_each_memblock(memory, r) {
		u64 ei_start = r->base;
		u64 ei_last = ei_start + r->size;
		u64 addr;

		addr = __memblock_x86_find_in_range_size(ei_start, ei_last, start,
					 sizep, align);

		if (addr != MEMBLOCK_ERROR)
			return addr;
	}

	return MEMBLOCK_ERROR;
}
