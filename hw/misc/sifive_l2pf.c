/*
 * QEMU SiFive Stride Prefetcher
 *
 * SiFive dummy stride prefetcher.
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

#include "qemu/osdep.h"
#include "exec/hwaddr.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/qdev-properties.h"
#include "hw/misc/sifive_l2pf.h"

static uint64_t sifive_l2pf_read(void *opaque, hwaddr addr, unsigned int size)
{
    SiFiveL2PFState *s = opaque;

    switch (addr) {
    case SIFIVE_L2PF_BASIC_CONTROL:
        return s->basic_ctl;
    case SIFIVE_L2PF_USER_BITS_CONTROL:
        return s->user_bits_ctl;
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: read: addr=0x%" HWADDR_PRIx "\n",
                  __func__, addr);

    return 0;
}

static void sifive_l2pf_write(void *opaque, hwaddr addr, uint64_t val64,
                              unsigned size)
{
    SiFiveL2PFState *s = opaque;
    uint32_t val32 = (uint32_t) val64;

    switch (addr) {
    case SIFIVE_L2PF_BASIC_CONTROL:
        s->basic_ctl = (val32 & s->basic_ctl_reg_mask);
        break;
    case SIFIVE_L2PF_USER_BITS_CONTROL:
        s->user_bits_ctl = (val32 & s->user_bits_ctl_reg_mask);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%" HWADDR_PRIx
                      " v=0x%" PRIx64 "\n", __func__, addr, val64);
    }
}

static const MemoryRegionOps sifive_l2pf_ops = {
    .read = sifive_l2pf_read,
    .write = sifive_l2pf_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4
    }
};

static Property sifive_l2pf_properties[] = {
    DEFINE_PROP_UINT32("version", SiFiveL2PFState, version, 1),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_l2pf_realize(DeviceState *dev, Error **errp)
{
    SiFiveL2PFState *s = SIFIVE_L2PF(dev);

    switch (s->version) {
    case 0:
        s->basic_ctl_reg_mask = SIFIVE_L2PF_BASIC_CTL_MASK_V0;
        s->user_bits_ctl_reg_mask = SIFIVE_L2PF_USER_BITS_CTL_MASK_V0;
        break;
    case 1:
        s->basic_ctl_reg_mask = SIFIVE_L2PF_BASIC_CTL_MASK_V1;
        s->user_bits_ctl_reg_mask = SIFIVE_L2PF_USER_BITS_CTL_MASK_V1;
        break;
    default:
        error_report("Unsupported l2pf version: %d.", s->version);
        exit(1);
    }

    memory_region_init_io(&s->mmio, OBJECT(dev), &sifive_l2pf_ops, s,
                          TYPE_SIFIVE_L2PF, SIFIVE_L2PF_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void sifive_l2pf_reset(DeviceState *dev)
{
    SiFiveL2PFState *s = SIFIVE_L2PF(dev);

    s->basic_ctl = SIFIVE_L2PF_BASIC_CTL_RST;
    s->user_bits_ctl = SIFIVE_L2PF_USER_BITS_CTL_RST;
}

static void sifive_l2pf_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sifive_l2pf_realize;
    dc->reset = sifive_l2pf_reset;
    device_class_set_props(dc, sifive_l2pf_properties);
}

static const TypeInfo sifive_l2pf_info = {
    .name          = TYPE_SIFIVE_L2PF,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SiFiveL2PFState),
    .class_init    = sifive_l2pf_class_init,
};

static void sifive_l2pf_register_types(void)
{
    type_register_static(&sifive_l2pf_info);
}

type_init(sifive_l2pf_register_types)


/*
 * Create SiFive Stride Prefetcher.
 */
DeviceState *sifive_l2pf_create(hwaddr addr)
{
    DeviceState *dev = qdev_new(TYPE_SIFIVE_L2PF);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
