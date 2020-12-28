/*
 * QEMU SiFive TileLink Address Remapper
 *
 * Copyright (c) 2020 SiFive, Inc.
 *
 * SiFive TileLink component which supports dynamic translation
 * of addresses from one location to another
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

#ifndef HW_SIFIVE_REMAPPER_H
#define HW_SIFIVE_REMAPPER_H

#include "qom/object.h"
#include "hw/sysbus.h"

#define SIFIVE_REMAPPER_CFG                0x000
#define SIFIVE_REMAPPER_VALID_0            0x004
#define SIFIVE_REMAPPER_VALID_1            0x008
#define SIFIVE_REMAPPER_VALID_2            0x00c
#define SIFIVE_REMAPPER_VALID_3            0x010
#define SIFIVE_REMAPPER_VALID_4            0x014
#define SIFIVE_REMAPPER_VALID_5            0x018
#define SIFIVE_REMAPPER_VALID_6            0x01c
#define SIFIVE_REMAPPER_FLUSH              0x020
#define SIFIVE_REMAPPER_VERSION            0x1f4
#define SIFIVE_REMAPPER_ENTRIES            0x1f8
#define SIFIVE_REMAPPER_KEY                0x1fc

#define SIFIVE_REMAPPER_FROM_BASE          0x200
#define SIFIVE_REMAPPER_REG_SIZE           0x1000

#define SIFIVE_REMAPPER_UPDATE_KEY         0x51f15e

typedef enum SiFiveAddrRmprVersion {
    SIFIVE_REMAPPER_VERSION_INIT,
    SIFIVE_REMAPPER_VERSION_REVISITED,
} SiFiveAddrRmprVersion;

#define SIFIVE_REMAPPER_MAX_ENTRIES_INIT       32
#define SIFIVE_REMAPPER_MAX_ENTRIES_REVISED    224

#define SIFIVE_REMAPPER_MAX_ENTRIES    SIFIVE_REMAPPER_MAX_ENTRIES_REVISED

#define SIFIVE_REMAPPER_DEFAULT_FROM_REGION_BASE_ADDR           0x0
#define SIFIVE_REMAPPER_DEFAULT_FROM_REGION_ADDR_WIDTH          63
#define SIFIVE_REMAPPER_DEFAULT_TO_REGION_BASE_ADDR             0x0
#define SIFIVE_REMAPPER_DEFAULT_TO_REGION_ADDR_WIDTH            63
#define SIFIVE_REMAPPER_DEFAULT_MAX_ENTRY_REGION_ADDR_WIDTH     63

#define TYPE_SIFIVE_REMAPPER           "sifive,remapper2"

typedef struct RemapEntry {
    gchar *name;
    /* from/to are the original register values. */
    uint64_t from;
    uint64_t to;
    /* from/to address mask. */
    uint64_t mask;
    /*
     * from_addr/to_addr are the actual addresses for
     * MemoryRegion alias.
     */
    uint64_t from_addr;
    uint64_t to_addr;
    int size;
    MemoryRegion *alias;
    /* flag indicating whether from/to are valid formats. */
    bool valid;
    QTAILQ_ENTRY(RemapEntry) entry;
} RemapEntry;

typedef struct SiFiveRemapperState SiFiveRemapperState;
DECLARE_INSTANCE_CHECKER(SiFiveRemapperState, SIFIVE_REMAPPER,
                         TYPE_SIFIVE_REMAPPER)

struct SiFiveRemapperState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    DeviceState *soc;
    MemoryRegion mmio;

    uint32_t cfg;
    uint32_t valid[7];
    uint8_t  flush;
    uint64_t from_region_base_addr;
    uint32_t from_region_addr_width;
    uint64_t to_region_base_addr;
    uint32_t to_region_addr_width;
    uint32_t max_entry_region_addr_width;
    uint32_t version;
    uint32_t entries;
    uint32_t max_entries;
    uint32_t key;
    RemapEntry remaps[SIFIVE_REMAPPER_MAX_ENTRIES];

    /*
     * Remap entries list sorted in asending from address order,
     * If two or more entries have same from address,
     * then they are sorted in desending range size order.
     * See refresh_remaps() for details.
     */
    QTAILQ_HEAD(, RemapEntry) remaps_head;
};

DeviceState *sifive_remapper_create(hwaddr addr,
                                    SiFiveAddrRmprVersion version,
                                    uint32_t num_entries);

#endif /* HW_SIFIVE_REMAPPER_H */
