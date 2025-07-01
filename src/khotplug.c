//-----------------------------------------------------------------------------
// Copyright (c) 2011-2014, Fusion-io, Inc.(acquired by SanDisk Corp. 2014)
// Copyright (c) 2014-2017 SanDisk Corp. and/or all its affiliates. (acquired by Western Digital Corp. 2016)
// Copyright (c) 2016-2017 Western Digital Technologies, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the SanDisk Corp. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
// OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include <linux/types.h>
#include <linux/module.h>
#include <fio/port/dbgset.h>
#include <fio/port/kcpu.h>
#include <fio/port/fio-port.h>
#include <fio/port/kfio_config.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/list.h>
#include <linux/notifier.h>

/**
 * @ingroup PORT_LINUX
 * @{
 */

#ifdef CONFIG_SMP

#ifdef DEFINE_SPINLOCK
static DEFINE_SPINLOCK(hotplug_lock);
#else
static spinlock_t hotplug_lock = SPIN_LOCK_UNLOCKED;
#endif
static int hotplug_initialized;
static LIST_HEAD(notify_list);

struct kfio_cpu_notify
{
    struct list_head list;
    kfio_cpu_notify_fn *func;
};

static void send_event_all(int online_flag, kfio_cpu_t cpu)
{
    struct kfio_cpu_notify *kcn;

    spin_lock_irq(&hotplug_lock);

    list_for_each_entry(kcn, &notify_list, list)
    {
        kcn->func(online_flag, cpu);
    }

    spin_unlock_irq(&hotplug_lock);
}

static int cpuhp_offline_dyn_state; // Global, so we can release the state callback.
static int kfio_cpu_notify_offline(unsigned int cpu)
{
    send_event_all(0, (kfio_cpu_t)cpu);
    return 0;
}

static int cpuhp_online_dyn_state;  // Global, so can release the state callback.
static int kfio_cpu_notify_online(unsigned int cpu)
{
    send_event_all(1, (kfio_cpu_t)cpu);
    return 0;
}

int kfio_register_cpu_notifier(kfio_cpu_notify_fn *func)
{
#ifdef CONFIG_SMP
    struct kfio_cpu_notify *kcn;
    int do_register = 0;

    kcn = kmalloc(sizeof(*kcn), GFP_KERNEL);
    if (!kcn)
        return -ENOMEM;

    spin_lock(&hotplug_lock);
    kcn->func = func;
    list_add_tail(&kcn->list, &notify_list);

    if (!hotplug_initialized)
    {
        do_register = 1;
        hotplug_initialized = 1;
    }
    spin_unlock(&hotplug_lock);

    if (do_register)
    {
        // Fancy footwork to handle various kernel idiosyncracies...
        // Kernels less than 4.8 only use register_cpu_notifier().
        // Kernel 4.8 provides both register_cpu_notifier() and a new function cpuhp_setup_state() for setting
        //  a callback for a cpu coming online. However, our notifier/callback function requires a lower hotplug state
        //  than the provided CPUHP_AP_ONLINE_DYN to work without modification (log messages are wrong).
        //  So we use cpuhp_setup_state() for setting an 'online' callback but use the old register_cpu_notifier()
        //  for 'offline'.
        // Kernel 4.10 and above have symmetrical CPUHP_..._DYN states for online and offline callbacks.
        // Whether kernel has states removal bug or not, now setup our real callback.
        cpuhp_offline_dyn_state =
        cpuhp_setup_state_nocalls(CPUHP_BP_PREPARE_DYN,
                                   "block/iomemory_vsl4:offline",
                                   NULL,
                                   kfio_cpu_notify_offline);
        cpuhp_online_dyn_state =
        cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
                                  "block/iomemory_vsl4:online",
                                  kfio_cpu_notify_online,
                                  NULL);
    }
#endif  /* CONFIG_SMP */
    return 0;
}

void kfio_unregister_cpu_notifier(kfio_cpu_notify_fn *func)
{
#ifdef CONFIG_SMP
    struct kfio_cpu_notify *kcn;
    int do_unregister = 0;

    spin_lock(&hotplug_lock);

    list_for_each_entry(kcn, &notify_list, list)
    {
        if (kcn->func == func)
        {
            list_del(&kcn->list);
            kfree(kcn);
            break;
        }
    }

    if (list_empty(&notify_list))
    {
        hotplug_initialized = 0;
        do_unregister = 1;
    }
    spin_unlock(&hotplug_lock);

    /*
     * The call below aquires sleep mutex, so we need to
     * call it after unlocking hotplug_lock spinlock.
     */
    if (do_unregister != 0)
    {
        cpuhp_remove_state_nocalls(cpuhp_online_dyn_state);
        cpuhp_remove_state_nocalls(cpuhp_offline_dyn_state);
    }

#endif  /* CONFIG_SMP */
}

#endif

/**
 * @}
 */
