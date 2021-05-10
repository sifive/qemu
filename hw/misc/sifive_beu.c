/*
 * QEMU SiFive Bus Error Unit
 *
 * Copyright (c) 2021 SiFive, Inc.
 *
 * Author:
 *    Frank Chang <frank.chang@sifive.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "hw/core/cpu.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/misc/sifive_beu.h"
#include "hw/misc/beu.h"

static uint64_t sifive_beu_read(void *opaque, hwaddr addr, unsigned int size)
{
    SiFiveBusErrorUnitState *s = opaque;

    switch (addr) {
    case SIFIVE_BEU_CAUSE:
        return s->cause;
    case SIFIVE_BEU_VALUE:
        return s->value;
    case SIFIVE_BEU_ENABLE:
        return s->enable;
    case SIFIVE_BEU_PLIC_INTERRUPT:
        return s->plic_interrupt;
    case SIFIVE_BEU_ACCRUED:
        return s->accrued;
    case SIFIVE_BEU_LOCAL_INTERRUPT:
        return s->local_interrupt;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: read: addr=0x%" HWADDR_PRIx "\n",
                  __func__, addr);
    return 0;
}

static void plic_irq_request(SiFiveBusErrorUnitState *s)
{
    bool level = !!(s->plic_interrupt && s->accrued);

    if (s->plic_irq && (s->plic_irq_level != level)) {
        qemu_set_irq(s->plic_irq, level);
        s->plic_irq_level = level;
    }
}

static void local_irq_request(SiFiveBusErrorUnitState *s)
{
    CPUState *cpu = qemu_get_cpu(s->hartid);
    bool level = !!(s->local_interrupt && s->accrued);

    if (s->local_irq_level != level) {
        if (s->legacy_local) {
            riscv_cpu_set_bus_error(RISCV_CPU(cpu), level);
        } else {
            riscv_cpu_set_rnmi(RISCV_CPU(cpu), s->rnmi, level);
        }

        s->local_irq_level = level;
    }
}

static void sifive_beu_write(void *opaque, hwaddr addr,
                             uint64_t val64, unsigned int size)
{
    SiFiveBusErrorUnitState *s = opaque;
    CPUState *cpu = qemu_get_cpu(s->hartid);
    CPURISCVState *env;

    if (!cpu) {
        error_report("%s:%d: cpu is NULL", __FILE__, __LINE__);
        return;
    }

    env = cpu->env_ptr;

    switch (addr) {
    case SIFIVE_BEU_CAUSE:
        /* Sanity check. */
        if (val64 >= (riscv_cpu_is_32bit(env) ? 32 : 64)) {
            return;
        }

        /*
         * cause register is writable only when either:
         *   1. The written value is 0 to clear cause register.
         *   2. cause register's current value is 0 and event
         *      is enabled in the enable register.
         */
        if (!val64 || (!s->cause && (1ULL << val64) & s->enable)) {
            s->cause = val64;
        }

        /* Clear value register when cause register is set to 0. */
        if (!s->cause) {
            s->value = 0;
        }
        return;
    case SIFIVE_BEU_VALUE+0x0:
    case SIFIVE_BEU_VALUE+0x1:
    case SIFIVE_BEU_VALUE+0x2:
    case SIFIVE_BEU_VALUE+0x3:
    case SIFIVE_BEU_VALUE+0x4:
    case SIFIVE_BEU_VALUE+0x5:
    case SIFIVE_BEU_VALUE+0x6:
    case SIFIVE_BEU_VALUE+0x7:
        s->value = deposit64(s->value, (addr - SIFIVE_BEU_VALUE) * 8,
                             size * 8, val64);
        return;
    case SIFIVE_BEU_ENABLE:
        s->enable = (val64 & s->error_causes);
        return;
    case SIFIVE_BEU_PLIC_INTERRUPT:
        s->plic_interrupt = (val64 & s->error_causes);
        plic_irq_request(s);
        return;
    case SIFIVE_BEU_ACCRUED:
        s->accrued = (val64 & s->error_causes);
        plic_irq_request(s);
        local_irq_request(s);
        return;
    case SIFIVE_BEU_LOCAL_INTERRUPT:
        s->local_interrupt = (val64 & s->error_causes);
        local_irq_request(s);
        return;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%" HWADDR_PRIx
                  " v=0x%" HWADDR_PRIx "\n", __func__, addr, val64);
    return;
}

static const MemoryRegionOps sifive_beu_ops = {
    .read = sifive_beu_read,
    .write = sifive_beu_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 8
    }
};

static bool sifive_beu_handle_error(BEUInterface *bi,
                                    MMUAccessType access_type,
                                    MemTxResult response, hwaddr physaddr)
{
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(bi);
    uint32_t mask;
    int error;

    switch (response) {
        case MEMTX_ERROR:
            if (access_type != MMU_INST_FETCH) {
                error = SIFIVE_BEU_LOAD_STORE_ERROR;
            } else {
                error = SIFIVE_BEU_INST_REFILL_ERROR;
            }
            break;
        default:
            return false;
    }

    mask = 1U << error;

    if ((s->enable & mask) && !s->cause) {
        s->cause = error;
        s->value = physaddr;
    }

    s->accrued |= mask;

    plic_irq_request(s);
    local_irq_request(s);

    return true;
}

static Property sifive_beu_properties[] = {
    DEFINE_PROP_UINT32("mmio-size", SiFiveBusErrorUnitState,
                       mmio_size, 0x1000),
    DEFINE_PROP_BOOL("legacy-local", SiFiveBusErrorUnitState,
                     legacy_local, false),
    DEFINE_PROP_UINT32("rnmi", SiFiveBusErrorUnitState,
                       rnmi, 0),
    DEFINE_PROP_UINT32("hartid", SiFiveBusErrorUnitState,
                       hartid, 0),
    /* Supported error causes, bit 0: no error, bit 4: reserved. */
    DEFINE_PROP_UINT32("error-causes", SiFiveBusErrorUnitState,
                       error_causes, 0xEE),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_beu_realize(DeviceState *dev, Error **errp)
{
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(dev);
    memory_region_init_io(&s->mmio, OBJECT(dev), &sifive_beu_ops, s,
                          TYPE_SIFIVE_BEU, s->mmio_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void sifive_beu_reset(DeviceState *dev)
{
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(dev);

    s->cause = 0;
    s->value = 0;
    s->enable = 0;
    s->accrued = 0;
    s->plic_interrupt = 0;
    s->local_interrupt = 0;

    s->plic_irq_level = false;
    s->local_irq_level = false;
}

static void sifive_beu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    BEUInterfaceClass *bic = BEU_INTERFACE_CLASS(klass);

    device_class_set_props(dc, sifive_beu_properties);
    dc->realize = sifive_beu_realize;
    dc->reset = sifive_beu_reset;
    bic->handle_bus_error = sifive_beu_handle_error;
}

static const TypeInfo sifive_beu_info = {
    .name          = TYPE_SIFIVE_BEU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SiFiveBusErrorUnitState),
    .class_init = sifive_beu_class_init,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_BEU_INTERFACE },
        { }
    }
};

static const TypeInfo beu_interface_type_info = {
    .name = TYPE_BEU_INTERFACE,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(BEUInterfaceClass),
};

static void sifive_beu_register_types(void)
{
    type_register_static(&beu_interface_type_info);
    type_register_static(&sifive_beu_info);
}

type_init(sifive_beu_register_types)


/*
 * Create Bus Error Unit.
 */
DeviceState *sifive_beu_create(hwaddr addr, uint32_t mmio_size,
                               bool legacy_local,
                               qemu_irq plic_irq, uint32_t rnmi,
                               uint32_t hartid)
{
    DeviceState *dev = qdev_new(TYPE_SIFIVE_BEU);
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(dev);
    qdev_prop_set_uint32(dev, "mmio-size", mmio_size);
    qdev_prop_set_bit(dev, "legacy-local", legacy_local);
    qdev_prop_set_uint32(dev, "rnmi", rnmi);
    qdev_prop_set_uint32(dev, "hartid", hartid);
    s->plic_irq = plic_irq;
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
