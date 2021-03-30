/*
 * QEMU SiFive Bus Error Unit
 *
 * Copyright (c) 2021 SiFive, Inc.
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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "cpu.h"
#include "exec/address-spaces.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/misc/sifive_beu.h"

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

static void sifive_beu_write(void *opaque, hwaddr addr,
                             uint64_t val64, unsigned int size)
{
    SiFiveBusErrorUnitState *s = opaque;
    CPUState *cpu = qemu_get_cpu(s->hartid);
    CPURISCVState *env;

    if (!cpu) {
        error_report("cpu is NULL");
        return;
    }

    env = cpu->env_ptr;

    switch (addr) {
    case SIFIVE_BEU_CAUSE:
        s->cause = deposit64(s->cause, 0, size * 8, val64);

        /* Enable interrupt */
        env->bus_errore = true;
        return;
    case SIFIVE_BEU_VALUE:
        s->value = deposit64(s->value, 0, size * 8, val64);
        return;
    case SIFIVE_BEU_ENABLE:
        s->enable = deposit64(s->enable, 0, size * 8, val64);
        return;
    case SIFIVE_BEU_PLIC_INTERRUPT:
        s->plic_interrupt = deposit64(s->plic_interrupt, 0, size * 8, val64);
        return;
    case SIFIVE_BEU_ACCRUED:
        s->accrued = deposit64(s->accrued, 0, size * 8, val64);

        /* Clear interrupt */
        if (!s->accrued) {
            switch (s->irq_mode) {
            case BEU_IRQ_PLATFORM:
                /* Platform-level interrupt */
                qemu_set_irq(s->irq , 0);
                break;
            case BEU_IRQ_LOCAL:
                /* Hart-local interrupt */
                env->bus_errorp = false;
                cpu_reset_interrupt(cpu, CPU_INTERRUPT_BUS_ERROR);
                break;
            case BEU_IRQ_RNMI:
                /* RNMI */
                qemu_set_irq(s->rnmi, 0);
                break;
            default:
                g_assert_not_reached();
            }
        }
        return;
    case SIFIVE_BEU_LOCAL_INTERRUPT:
        s->local_interrupt = deposit64(s->local_interrupt, 0, size * 8, val64);
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

static void bus_error_handler(void *opaque, int n, int level)
{
    SiFiveBusErrorUnitState *s = opaque;
    CPUState *cpu = qemu_get_cpu(s->hartid);
    CPURISCVState *env;
    uint32_t mask;

    if (!level) {
        return;
    }

    if (!cpu) {
        error_report("cpu is NULL");
        return;
    }

    env = cpu->env_ptr;
    mask = 1 << n;

    if ((s->enable & mask) && !s->cause) {
        s->cause = n;
        /* Unknown address */
        s->value = 0;
    }

    s->accrued |= mask;

    switch (s->irq_mode) {
    case BEU_IRQ_PLATFORM:
        /* Platform-level interrupt */
        if (s->irq && (s->plic_interrupt & mask)) {
            qemu_set_irq(s->irq, 1);
        }
        break;
    case BEU_IRQ_LOCAL:
        /* Hart-local interrupt */
        if (s->local_interrupt & mask) {
            env->bus_errorp = true;
            cpu_interrupt(cpu, CPU_INTERRUPT_BUS_ERROR);
        }
        break;
    case BEU_IRQ_RNMI:
        /* RNMI */
        qemu_set_irq(s->rnmi, 1);
        break;
    default:
        g_assert_not_reached();
    }
}

static Property sifive_beu_properties[] = {
    DEFINE_PROP_UINT32("mmio-size", SiFiveBusErrorUnitState,
                       mmio_size, 0x1000),
    DEFINE_PROP_UINT32("irq_mode", SiFiveBusErrorUnitState,
                       irq_mode, BEU_IRQ_LOCAL),
    DEFINE_PROP_UINT32("hartid", SiFiveBusErrorUnitState,
                       hartid, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_beu_realize(DeviceState *dev, Error **errp)
{
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(dev);
    memory_region_init_io(&s->mmio, OBJECT(dev), &sifive_beu_ops, s,
                          TYPE_SIFIVE_BEU, s->mmio_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
    qdev_init_gpio_in(dev, bus_error_handler, SIFIVE_BEU_NUM_ERRORS);
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
}

static void sifive_beu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, sifive_beu_properties);
    dc->realize = sifive_beu_realize;
    dc->reset = sifive_beu_reset;
}

static const TypeInfo sifive_beu_info = {
    .name          = TYPE_SIFIVE_BEU,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SiFiveBusErrorUnitState),
    .class_init = sifive_beu_class_init,
};

static void sifive_beu_register_types(void)
{
    type_register_static(&sifive_beu_info);
}

type_init(sifive_beu_register_types)


/*
 * Create Bus Error Unit.
 */
DeviceState *sifive_beu_create(hwaddr addr, uint32_t size, qemu_irq irq,
                               SiFiveBusErrorUnitIRQMode irq_mode,
                               uint32_t hartid)
{
    DeviceState *dev = qdev_new(TYPE_SIFIVE_BEU);
    SiFiveBusErrorUnitState *s = SIFIVE_BEU(dev);
    qdev_prop_set_uint32(dev, "mmio-size", size);
    qdev_prop_set_uint32(dev, "irq_mode", irq_mode);
    qdev_prop_set_uint32(dev, "hartid", hartid);
    s->irq = irq;
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
