/*
 * QEMU SiFive Bus Error Unit
 *
 * Copyright (c) 2021 SiFive, Inc.
 *
 * Author:
 *  Frank Chang <frank.chang@sifive.com>
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

#ifndef HW_SIFIVE_BEU_H
#define HW_SIFIVE_BEU_H

#include <stdbool.h>
#include "exec/hwaddr.h"
#include "hw/sysbus.h"
#include "qom/object.h"

#define SIFIVE_BEU_CAUSE            0x00
#define SIFIVE_BEU_VALUE            0x08
#define SIFIVE_BEU_ENABLE           0x10
#define SIFIVE_BEU_PLIC_INTERRUPT   0x18
#define SIFIVE_BEU_ACCRUED          0x20
#define SIFIVE_BEU_LOCAL_INTERRUPT  0x28

#define SIFIVE_BEU_NO_ERROR                         0
#define SIFIVE_BEU_INST_REFILL_ERROR                1
#define SIFIVE_BEU_INST_CORRECTABLE_ECC_ERROR       2
#define SIFIVE_BEU_INST_UNCORRECTABLE_ERROR         3
#define SIFIVE_BEU_LOAD_STORE_ERROR                 5
#define SIFIVE_BEU_DATA_CORRECTABLE_ECC_ERROR       6
#define SIFIVE_BEU_DATA_UNCORRECTABLE_ECC_ERROR     7

#define SIFIVE_BEU_NUM_ERRORS                       8

#define TYPE_SIFIVE_BEU         "sifive,buserror0"

typedef struct SiFiveBusErrorUnitState SiFiveBusErrorUnitState;
DECLARE_INSTANCE_CHECKER(SiFiveBusErrorUnitState, SIFIVE_BEU,
                         TYPE_SIFIVE_BEU)

struct SiFiveBusErrorUnitState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;
    uint32_t mmio_size;

    bool legacy_local;
    qemu_irq plic_irq;
    uint32_t rnmi;
    uint32_t hartid;
    uint32_t error_causes;

    uint64_t cause;
    uint64_t value;
    uint64_t enable;
    uint64_t accrued;
    uint64_t plic_interrupt;
    uint64_t local_interrupt;

    bool plic_irq_level;
    bool local_irq_level;
};

DeviceState *sifive_beu_create(hwaddr addr, uint32_t mmio_size,
                               bool legacy_local,
                               qemu_irq plic_irq, uint32_t rnmi,
                               uint32_t hartid);

#endif
