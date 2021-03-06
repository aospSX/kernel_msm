/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __SUBSYS_RESTART_H
#define __SUBSYS_RESTART_H

#include <linux/spinlock.h>

#define SUBSYS_NAME_MAX_LENGTH 40

enum {
	RESET_SOC = 1,
	RESET_SUBSYS_COUPLED,
	RESET_SUBSYS_INDEPENDENT,
	RESET_SUBSYS_MIXED = 25,
	RESET_LEVEL_MAX
};

struct subsys_data {
	const char *name;
	int (*shutdown) (const struct subsys_data *);
	int (*powerup) (const struct subsys_data *);
	void (*crash_shutdown) (const struct subsys_data *);
	int (*ramdump) (int, const struct subsys_data *);

	/* Internal use only */
	struct list_head list;
	void *notif_handle;

	struct mutex shutdown_lock;
	struct mutex powerup_lock;

	void *restart_order;
	struct subsys_data *single_restart_list[1];
};

static inline void *subsystem_get(const char *name)
{
	return NULL;
}

static inline void subsystem_put(void *subsystem) { }

#if defined(CONFIG_MSM_SUBSYSTEM_RESTART)

int get_restart_level(void);
int subsystem_restart(const char *subsys_name);
int ssr_register_subsystem(struct subsys_data *subsys);

#else

static inline int get_restart_level(void)
{
	return 0;
}

static inline int subsystem_restart(const char *subsystem_name)
{
	return 0;
}

static inline int ssr_register_subsystem(struct subsys_data *subsys)
{
	return 0;
}

#endif /* CONFIG_MSM_SUBSYSTEM_RESTART */

#endif
