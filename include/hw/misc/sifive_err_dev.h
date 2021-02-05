/*
 * SiFive Error Device
 *
 * Copyright (c) 2021 Frank Chang, frank.chang@sifive.com
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

#ifndef HW_SIFIVE_ERR_DEV_H
#define HW_SIFIVE_ERR_DEV_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SIFIVE_ERR_DEV "riscv.sifive.error.device"

typedef struct SiFiveErrDevState SiFiveErrDevState;
DECLARE_INSTANCE_CHECKER(SiFiveErrDevState, SIFIVE_ERR_DEV,
                         TYPE_SIFIVE_ERR_DEV)

struct SiFiveErrDevState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    uint32_t mmio_size;
    qemu_irq gpio_out;
};

DeviceState *sifive_err_dev_create(hwaddr addr, uint32_t size);

#endif
