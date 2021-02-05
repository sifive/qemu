/*
 * SiFive Error Device
 *
 * Copyright (c) 2021 Frank Chang, frank.chang@sifive.com
 * Copyright (c) 2021 SiFive, Inc.
 *
 * Error Device is a TileLink slave that responds to all requests with
 * a TileLink error. It's useful for testing software handling bus error.
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
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/misc/sifive_err_dev.h"

static uint64_t sifive_err_dev_read(void *opaque, hwaddr addr,
                                    unsigned int size)
{
    SiFiveErrDevState *s = SIFIVE_ERR_DEV(opaque);
    qemu_set_irq(s->gpio_out, true);
    return 0;
}

static void sifive_err_dev_write(void *opaque, hwaddr addr,
                                 uint64_t val64, unsigned int size)
{
    /* All writes are discarded. */
    SiFiveErrDevState *s = SIFIVE_ERR_DEV(opaque);
    qemu_set_irq(s->gpio_out, true);
}

static const MemoryRegionOps sifive_err_dev_ops = {
    .read = sifive_err_dev_read,
    .write = sifive_err_dev_write,
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

static Property sifive_err_dev_properties[] = {
    DEFINE_PROP_UINT32("mmio-size", SiFiveErrDevState, mmio_size, 0x1000),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_err_dev_realize(DeviceState *dev, Error **errp)
{
    SiFiveErrDevState *s = SIFIVE_ERR_DEV(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &sifive_err_dev_ops, s,
                          TYPE_SIFIVE_ERR_DEV, s->mmio_size);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->gpio_out);
}

static void sifive_err_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, sifive_err_dev_properties);
    dc->realize = sifive_err_dev_realize;
}

static const TypeInfo sifive_err_dev_info = {
    .name          = TYPE_SIFIVE_ERR_DEV,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SiFiveErrDevState),
    .class_init    = sifive_err_dev_class_init,
};

static void sifive_err_dev_register_types(void)
{
    type_register_static(&sifive_err_dev_info);
}

type_init(sifive_err_dev_register_types)


/*
 * Create Error Device.
 */
DeviceState *sifive_err_dev_create(hwaddr addr, uint32_t size)
{
    DeviceState *dev = qdev_new(TYPE_SIFIVE_ERR_DEV);
    qdev_prop_set_uint32(dev, "mmio-size", size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
