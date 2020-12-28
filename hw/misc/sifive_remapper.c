/*
 * QEMU SiFive TileLink Address Remapper
 *
 * Copyright (c) 2020 SiFive, Inc.
 *
 * SiFive TileLink component which supports dynamic translation
 * of addresses from one location to another.
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
#include "qemu/module.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/misc/sifive_remapper.h"

/*
 * Return from/to entry index of given address.
 *
 * If address is out of valid from/to registers address range
 * or current address remapper version is not supported,
 * returns -1.
 *
 * @addr: Address to be converted.
 */
static int from_to_idx(SiFiveRemapperState *s, hwaddr addr)
{
    /* Sanity check. */
    if (s->max_entries == 0) {
        return -1;
    }

    uint32_t offset = addr - SIFIVE_REMAPPER_FROM_BASE;
    uint32_t max_offset = (s->max_entries - 1) * 16 + 8;
    return (offset <= max_offset) ? (offset / 16) : -1;
}

/*
 * Add or update remap entry's MemoryRegion alias on system bus.
 *
 * This function should be called within memory transaction.
 *
 * @e: Remap Entry.
 * @priority: Priority to be set to remap entry's MemoryRegion alias
 *            on system bus.
 */
static void update_remap_mr(SiFiveRemapperState *s, RemapEntry *e, int priority)
{
    MemoryRegion *sys_mem = get_system_memory();

    memory_region_set_address(e->alias, e->from_addr);
    memory_region_set_alias_offset(e->alias, e->to_addr);
    memory_region_set_size(e->alias, e->size);

    if (e->alias->priority != priority) {
        memory_region_del_subregion(sys_mem, e->alias);
        memory_region_add_subregion_overlap(sys_mem, e->from_addr,
                                            e->alias, priority);
    }

    /* MemoryRegion alias may be disabled before, re-enable it. */
    memory_region_set_enabled(e->alias, true);
}

/*
 * Create or update memory region aliases to remap memory layouts.
 *
 * According to spec.:
 *  If multiple From[] entries match, then address is remapped to
 *  the bitwise OR of the To[] values of all matching entries.
 *
 * To deal with above case, we have to create MemoryRegion aliases
 * with different priorties for each remap entry so that each
 * remap entry can have its own bitwise OR of the To[] values
 * and overlaps onto other low priority remap entries.
 *
 * E.g. considering the following remap entries:
 *
 * Remap entry 1:
 *  From: 0x2000
 *  To: 0x6000
 *  Range size: 2^12 bytes (0x1000)
 *  => From range: [0x2000, 0x3000)
 *
 * Remap entry 2:
 *  From: 0x2100
 *  To: 0x7000
 *  Range size: 2^8 bytes (0x100)
 *  => From range: [0x2100, 0x2200)
 *
 * Remap entry 3:
 *  From: 0x2100
 *  To: 0x8000
 *  Range size: 2^4 bytes (0x10)
 *  => From range: [0x2100, 0x2110)
 *
 * Remap entry 4:
 *  From: 0x3100
 *  To: 0x9000
 *  Range size: 2^5 bytes (0x20)
 *  => From range: [0x3100, 0x3120)
 *
 * Remap entry 5:
 *  From: 0x2400
 *  To: 0xa000
 *  Range size: 2^8 bytes (0x100)
 *  => From range: [0x2400, 0x2500)
 *
 * Remap entry 6:
 *  From: 0x2420
 *  To: 0xb000
 *  Range size: 2^3 bytes (0x8)
 *  => From range: [0x2420, 0x2428)
 *
 * We will create the MemoryRegion aliases as:
 *
 *                                                                      Priority
 *                                                                       (High)
 *    0x2100    0x2110         0x2420    0x2428
 *    +---------+              +---------+                                 ^
 *    | Entry 3 |              | Entry 6 |                                 |
 *    +---------+              +---------+                                 |
 *                                                                         |
 *    0x2100        0x2200   0x2400        0x2500                          |
 *    +-------------+        +-------------+                               |
 *    |   Entry 2   |        |   Entry 5   |                               |
 *    +-------------+        +-------------+                               |
 *                                                                         |
 * 0x2000                                      0x3000  0x3100      0x3120  |
 * +-------------------------------------------+       +-----------+       |
 * |                  Entry 1                  |       |  Entry 4  |
 * +-------------------------------------------+       +-----------+     (Low)
 *
 * Memory access will be divided into the following ranges:
 *
 * [0x2000, 0x2100): remap to Entry 1 To address
 *                   = 0x6000
 * [0x2100, 0x2110): remap to (Entry 1 | Entry 2 | Entry 3) To address
 *                   = (0x6000 | 0x7000 | 0x8000)
 *                   = 0xf000
 * [0x2110, 0x2200): remap to (Entry 1 | Entry 2) To address
 *                   = (0x6000 | 0x7000)
 *                   = 0x7000
 * [0x2200, 0x2400): remap to Entry 1 To address
 *                   = 0x6000
 * [0x2400, 0x2420): remap to (Entry 1 | Entry 5) To address
 *                   = (0x6000 | 0xa000)
 *                   = 0xe000
 * [0x2420, 0x2428): remap to (Entry 1 | Entry 5 | Entry 6) To address
 *                   = (0x6000 | 0xa000 | 0xb000)
 *                   = 0xf000
 * [0x2428, 0x2500): remap to (Entry 1 | Entry 5) To address
 *                   = (0x6000 | 0xa000)
 *                   = 0xe000
 * [0x2500, 0x3000): remap to Entry 1 To address
 *                   = 0x6000
 * [0x3000, 0x3100): no remap
 * [0x3100, 0x3120): remap to Entry 4 To address
 *                   = 0x9000
 *
 * MemoryRegion aliases are created for each remap entry with its addr
 * set to the overlapped bitwise ORed To address and offset set to
 * remap entry's From address. By giving different priorties,
 * we can divided the memory accesses into different ranges as illustrated
 * above.
 *
 * Also, as the range size information is encoded in From address,
 * range size is limited by its encoding.
 * Therefore, partially overlapped remap entries won't exist.
 *
 * i.e. it's impossible to have the case like:
 *
 *                    +---------------------+
 *                    |       Entry 2       |
 *                    +---------------------+
 * +-----------------------------+
 * |           Entry 1           |
 * +-----------------------------+
 *
 * which makes the thing easier.
 *
 * A remap entries list is created and managed for the convenience
 * to find out the overlapping remap entries (i.e. multiple From[]
 * entries match) and assign the proper bitwised ORed To address
 * to the correspond MemoryRegion alias's addr.
 *
 * The remap entries list is sorted in asending from address order
 * and if there are two or more entries have same from address,
 * then they are sorted in desending range size order.
 *
 * With sorted remap entries list, if more than one remap entries
 * are overlapped, we can assign higher priority to the remap entry
 * with larger from address by simply traversing the list.
 *
 * For the overlapped remap entries with same from address, as we also
 * sort remap entries in desending range size order, we can also
 * guarantee that the remap entry with smaller range size has higher
 * priority than the larger ones.
 */
static void refresh_remaps(SiFiveRemapperState *s)
{
    RemapEntry *e, *next;
    int priority = 1;
    uint64_t ored_to;
    uint64_t current_end, next_end, max_end;

    if (QTAILQ_EMPTY(&s->remaps_head)) {
        return;
    }

    memory_region_transaction_begin();

    /* Note: partially overlapped remap entries won't exist. */
    QTAILQ_FOREACH(e, &s->remaps_head, entry) {
        current_end = (e->from & e->mask) + e->size;

        if (priority == 1) {
            /* Current remap entry does not overlap any other remap entries. */
            ored_to = e->to_addr;
            max_end = current_end;
        } else {
            /* Current remap entry overlaps other remap entries. */
            ored_to |= e->to_addr;
        }

        /* Update current remap entry to address and MemoryRegion alias. */
        e->to_addr = ored_to;
        update_remap_mr(s, e, priority);

        next = QTAILQ_NEXT(e, entry);

        if (!next) {
            /* Current remap entry is the last remap entry. */
            break;
        }

        next_end = (next->from & next->mask) + next->size;

        if (next_end <= current_end) {
            /* Next remap entry overlaps current remap entry. */
            priority++;
        } else if (next->from >= max_end) {
            /* Next remap entry does not overlap any other remap entries. */
            priority = 1;
        } else {
            /*
             * Next remap entry does not overlap current remap entry,
             * but is still overlapping the largest range sized remap entry.
             */
            priority--;
        }
    }

    memory_region_transaction_commit();
}

static void _insert_entry(SiFiveRemapperState *s, RemapEntry *e)
{
    RemapEntry *other;

    /*
     * Remap entries are sorted in asending from address order.
     * If two or more entries have same from address,
     * then they are sorted in desending range size order.
     * See refresh_remaps() for details.
     */
    QTAILQ_FOREACH(other, &s->remaps_head, entry) {
        if (e->from == other->from) {
            if (e->size > other->size) {
                QTAILQ_INSERT_BEFORE(other, e, entry);
            }
        } else if (e->from < other->from) {
            QTAILQ_INSERT_BEFORE(other, e, entry);
        }
    }

    QTAILQ_INSERT_TAIL(&s->remaps_head, e, entry);
}

static void insert_entry(SiFiveRemapperState *s, RemapEntry *e)
{
    _insert_entry(s, e);
    refresh_remaps(s);
}

static void _remove_entry(SiFiveRemapperState *s, RemapEntry *e)
{
    QTAILQ_REMOVE(&s->remaps_head, e, entry);
}

static void remove_entry(SiFiveRemapperState *s, RemapEntry *e)
{
    _remove_entry(s, e);
    refresh_remaps(s);
}

/*
 * The remap entry's from/to address and range size may changed.
 * Resort remap entries list by first to remove the entry from
 * the list and then insert the entry back to the list again.
 */
static void resort_entry(SiFiveRemapperState *s, RemapEntry *e)
{
    _remove_entry(s, e);
    _insert_entry(s, e);
    refresh_remaps(s);
}

/*
 * Update remap entry info.
 */
static void update_remap_info(SiFiveRemapperState *s, RemapEntry *e)
{
    int ones = cto64(e->from);

    /*
     * mask is the MSB of the from address to be matched.
     *
     * However, the real remapping region size is also limited by the
     * max entry region address width.
     */
    int mask_shift = MIN(ones + 1, s->max_entry_region_addr_width);
    uint64_t mask = MAKE_64BIT_MASK(mask_shift, 64 - mask_shift);

    /*
     * Do nothing if either remap entry's From or To format is invalid.
     *
     * Invalid formats:
     *  1. Reserved From entry, which LSB is not 1.
     *  2. Number of LSB 1s of From > 62.
     *  3. bit[number LSB 1s of From] is not 0.
     *  4. To is not aligned NAPOT to the size of From[idx].
     */
    if ((ones == 0) ||
        (ones > 62) ||
        (e->from & (1 << ones)) ||
        (e->to & ~mask)) {
        e->valid = false;
        return;
    }

    e->from_addr = e->from & mask;
    e->to_addr = e->to & mask;
    e->size = 1 << mask_shift;
    e->mask = mask;
    e->valid = true;
}

/*
 * Setup remap entries for given remappervalid[] register.
 *
 * @idx: Index of remappervalid[] register.
 * @new_val: New value set to remappervalid[] register.
 */
static void setup_range_remaps(SiFiveRemapperState *s, int idx,
                               uint32_t new_val)
{
    /* Remap entry index. */
    int entry_idx = idx * 32;
    RemapEntry *e;
    uint32_t changes = s->valid[idx] ^ new_val;
    bool valid;
    int mask_len;
    uint32_t mask;

    if ((entry_idx >= s->entries) || !changes) {
        goto done;
    }

    for (int i = 0; i < 32; i++, entry_idx++) {
        if (entry_idx >= s->entries) {
            break;
        }

        if (!(changes & (1 << i))) {
            /* No change, do nothing. */
            continue;
        }

        valid = new_val & (1 << i);
        e = &s->remaps[entry_idx];

        if (!valid) {
            /* Disable remap and remove from remap entries list. */
            memory_region_set_enabled(e->alias, false);
            if (QTAILQ_IN_USE(e, entry)) {
                _remove_entry(s, e);
            }
            continue;
        }

        if (e->valid) {
            /* Add remap into remap entries list. */
            _insert_entry(s, e);
        }
    }

    /* Refresh remaps. */
    refresh_remaps(s);

done:
    mask_len = s->entries - (idx * 32);
    mask = (mask_len > 0) ? MAKE_64BIT_MASK(0, mask_len) : 0;
    s->valid[idx] = new_val & mask;
}

static void flush_all_remaps(SiFiveRemapperState *s)
{
    RemapEntry *e;

    /* Disable all existing remaps. */
    memory_region_transaction_begin();

    for (int i = 0; i < s->entries; i++) {
        if (s->remaps[i].alias) {
            memory_region_set_enabled(s->remaps[i].alias, false);
        }
    }

    memory_region_transaction_commit();

    /* Clear remappervalid[] registers. */
    memset(s->valid, 0x0, sizeof(s->valid));

    /* Remove all remaps from remap entries list. */
    while (!QTAILQ_EMPTY(&s->remaps_head)) {
        e = QTAILQ_LAST(&s->remaps_head);
        QTAILQ_REMOVE(&s->remaps_head, e, entry);
    }
}

static uint64_t sifive_remapper_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    SiFiveRemapperState *s = opaque;

    switch (addr) {
    case SIFIVE_REMAPPER_CFG:
        return s->cfg;
    case SIFIVE_REMAPPER_VALID_0:
        return s->valid[0];
    case SIFIVE_REMAPPER_VALID_1:
        return s->valid[1];
    case SIFIVE_REMAPPER_VALID_2:
        return s->valid[2];
    case SIFIVE_REMAPPER_VALID_3:
        return s->valid[3];
    case SIFIVE_REMAPPER_VALID_4:
        return s->valid[4];
    case SIFIVE_REMAPPER_VALID_5:
        return s->valid[5];
    case SIFIVE_REMAPPER_VALID_6:
        return s->valid[6];
    case SIFIVE_REMAPPER_VERSION:
        return s->version;
    case SIFIVE_REMAPPER_ENTRIES:
        /* Exists only after version 1 of remapper. */
        return (s->version >= SIFIVE_REMAPPER_VERSION_REVISITED) ?
            s->entries : 0;
    case SIFIVE_REMAPPER_KEY:
        return s->key;
    }

    if (addr >= SIFIVE_REMAPPER_FROM_BASE) {
        int idx = from_to_idx(s, addr);
        if (idx != -1) {
            return (addr & 0x8) ? s->remaps[idx].to : s->remaps[idx].from;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: read: addr=0x%" HWADDR_PRIx "\n",
                  __func__, addr);
    return 0;
}

static void sifive_remapper_write(void *opaque, hwaddr addr,
                                  uint64_t val64, unsigned int size)
{
    SiFiveRemapperState *s = opaque;
    uint32_t val32 = (uint32_t)val64;

    if (addr == SIFIVE_REMAPPER_KEY) {
        s->key = (val32 == SIFIVE_REMAPPER_UPDATE_KEY);
        return;
    }

    if (!s->key) {
        /*
         * remapperkey state bit is cleared,
         * all writes to registers other than remapperkey are ignored.
         */
        return;
    }

    switch (addr) {
    case SIFIVE_REMAPPER_CFG:
        /* Hard-wired to zero. */
        goto done;
    case SIFIVE_REMAPPER_VALID_0:
        setup_range_remaps(s, 0, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_1:
        setup_range_remaps(s, 1, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_2:
        setup_range_remaps(s, 2, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_3:
        setup_range_remaps(s, 3, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_4:
        setup_range_remaps(s, 4, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_5:
        setup_range_remaps(s, 5, val32);
        goto done;
    case SIFIVE_REMAPPER_VALID_6:
        setup_range_remaps(s, 6, val32);
        goto done;
    case SIFIVE_REMAPPER_FLUSH:
        if ((uint8_t)val64 == 1) {
            flush_all_remaps(s);
        }
        goto done;
    case SIFIVE_REMAPPER_VERSION:
    case SIFIVE_REMAPPER_ENTRIES:
        /* WARL register. */
        goto done;
    }

    if (addr >= SIFIVE_REMAPPER_FROM_BASE) {
        int idx = from_to_idx(s, addr);
        RemapEntry *e = &s->remaps[idx];

        if (idx != -1) {
            /*
             * Value written to From[]/To[] registers is limited by
             * their region address widths.
             */
            if (addr & 0x8) {
                uint64_t mask = (1ULL << s->to_region_addr_width) - 1;
                e->to = s->to_region_base_addr | (val64 & mask);
            } else {
                uint64_t mask = (1ULL << s->from_region_addr_width) - 1;
                e->from = s->from_region_base_addr | (val64 & mask);
            }

            if (idx >= s->entries) {
                goto done;
            }

            /* Update remap entry info. */
            update_remap_info(s, e);

            if (!e->valid) {
                /* Invalid formats of from/to. */
                memory_region_set_enabled(e->alias, false);
                if (QTAILQ_IN_USE(e, entry)) {
                    remove_entry(s, e);
                }
                goto done;
            }

            if (s->valid[idx / 32] & (1 << (idx % 32))) {
                /*
                 * Correspond valid bit is true,
                 * we need to refresh the remaps.
                 */
                if (!QTAILQ_IN_USE(e, entry)) {
                    /*
                     * Remap was removed from the remap entries list
                     * because from/to formats were not valid before.
                     * We need to add it back as form/to formats are valid now.
                     */
                    insert_entry(s, e);
                } else {
                    resort_entry(s, e);
                }
            }

            goto done;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write: addr=0x%" HWADDR_PRIx
                  " v=0x%" HWADDR_PRIx "\n", __func__, addr, val64);
    return;

done:
    /* Clear remapperkey register state bit after each write. */
    s->key = 0;
}

static const MemoryRegionOps sifive_remapper_ops = {
    .read = sifive_remapper_read,
    .write = sifive_remapper_write,
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

static Property sifive_remapper_properties[] = {
    DEFINE_PROP_UINT32("version", SiFiveRemapperState, version,
                       SIFIVE_REMAPPER_VERSION_REVISITED),
    DEFINE_PROP_UINT32("num-entries", SiFiveRemapperState, entries,
                       SIFIVE_REMAPPER_MAX_ENTRIES_REVISED),
    DEFINE_PROP_UINT64("from-region-base-addr",
                       SiFiveRemapperState, from_region_base_addr,
                       SIFIVE_REMAPPER_DEFAULT_FROM_REGION_BASE_ADDR),
    DEFINE_PROP_UINT32("from-region-addr-width",
                       SiFiveRemapperState, from_region_addr_width,
                       SIFIVE_REMAPPER_DEFAULT_FROM_REGION_ADDR_WIDTH),
    DEFINE_PROP_UINT64("to-region-base-addr",
                       SiFiveRemapperState, to_region_base_addr,
                       SIFIVE_REMAPPER_DEFAULT_TO_REGION_BASE_ADDR),
    DEFINE_PROP_UINT32("to-region-add-width",
                       SiFiveRemapperState, to_region_addr_width,
                       SIFIVE_REMAPPER_DEFAULT_TO_REGION_ADDR_WIDTH),
    DEFINE_PROP_UINT32("max-entry-region-addr-width",
                       SiFiveRemapperState, max_entry_region_addr_width,
                       SIFIVE_REMAPPER_DEFAULT_MAX_ENTRY_REGION_ADDR_WIDTH),
    DEFINE_PROP_END_OF_LIST(),
};

static void sifive_remapper_realize(DeviceState *dev, Error **errp)
{
    SiFiveRemapperState *s = SIFIVE_REMAPPER(dev);
    uint64_t mask;

    if (s->from_region_addr_width < 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper from region width" \
                "too small, must >= 2 (4 bytes)\n", \
                __func__);
        exit(1);
    }

    if (s->from_region_addr_width > 63) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper from region width" \
                "too large, must <= 63 (2^63 bytes)\n", \
                __func__);
        exit(1);
    }

    if (s->from_region_addr_width < s->max_entry_region_addr_width) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper from region width: %d"
                "cannot be smaller than max entry region address width: %d\n",
                __func__, s->from_region_addr_width,
                s->max_entry_region_addr_width);
        exit(1);
    }

    mask = (1ULL << s->from_region_addr_width) - 1;

    if (s->from_region_base_addr & mask) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper from region not " \
                "naturally aligned\n", __func__);
        exit(1);
    }

    if (s->to_region_addr_width < 2) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper to region width " \
                "too small, must >= 2 (4 bytes)\n", \
                __func__);
        exit(1);
    }

    if (s->to_region_addr_width > 63) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper to region width" \
                "too large, must <= 63 (2^63 bytes)\n", \
                __func__);
        exit(1);
    }

    if (s->to_region_addr_width < s->max_entry_region_addr_width) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper to region width: %d"
                "cannot be smaller than max entry region address width: %d\n",
                __func__, s->to_region_addr_width,
                s->max_entry_region_addr_width);
        exit(1);
    }

    mask = (1ULL << s->to_region_addr_width) - 1;

    if (s->to_region_base_addr & mask) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper to region not " \
                "naturally aligned\n", __func__);
        exit(1);
    }

    if ((s->max_entry_region_addr_width < 2) ||
            (s->max_entry_region_addr_width > 63)) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Remapper max entry " \
                "region address width must be >= 2 and <= 63\n", __func__);
        exit(1);
    }

    switch (s->version) {
    case SIFIVE_REMAPPER_VERSION_INIT:
        if (s->entries > SIFIVE_REMAPPER_MAX_ENTRIES_INIT) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: version %d: " \
                    "max number of remapper entries is %d\n", \
                    __func__, s->version, SIFIVE_REMAPPER_MAX_ENTRIES_INIT);
            qemu_log_mask(LOG_GUEST_ERROR, "%s: " \
                    "number remapper entries is set to: %d\n", \
                    __func__, SIFIVE_REMAPPER_MAX_ENTRIES_INIT);
            s->entries = SIFIVE_REMAPPER_MAX_ENTRIES_INIT;
        }
        s->max_entries = SIFIVE_REMAPPER_MAX_ENTRIES_INIT;
        break;
    case SIFIVE_REMAPPER_VERSION_REVISITED:
        if (s->entries > SIFIVE_REMAPPER_MAX_ENTRIES_REVISED) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: version %d: " \
                    "max number of remapper entries is %d\n",
                    __func__, s->version, SIFIVE_REMAPPER_MAX_ENTRIES_REVISED);
            qemu_log_mask(LOG_GUEST_ERROR, "%s: " \
                    "number of remapper entries is set to: %d\n",
                    __func__, SIFIVE_REMAPPER_MAX_ENTRIES_REVISED);
            s->entries = SIFIVE_REMAPPER_MAX_ENTRIES_REVISED;
        }
        s->max_entries = SIFIVE_REMAPPER_MAX_ENTRIES_REVISED;
        break;
    default:
        /* Invalid version. */
        error_setg(errp, "unsupported address remapper version: %d",
                   s->version);
        exit(1);
    }

    QTAILQ_INIT(&s->remaps_head);

    memory_region_init_io(&s->mmio, OBJECT(dev), &sifive_remapper_ops, s,
                          TYPE_SIFIVE_REMAPPER, SIFIVE_REMAPPER_REG_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->mmio);
}

static void sifive_remapper_reset(DeviceState *dev)
{
    SiFiveRemapperState *s = SIFIVE_REMAPPER(dev);
    MemoryRegion *sys_mem = get_system_memory();
    RemapEntry *e;

    /* Flush all remaps. */
    flush_all_remaps(s);

    /* Reset remapperkey register state bit. */
    s->key = 0;

    /* Reset from/to entries. */
    for (int i = 0; i < s->entries; i++) {
        e = &s->remaps[i];
        e->from = e->from_addr = s->from_region_base_addr;
        e->to = e->to_addr = s->to_region_base_addr;
        e->mask = 0;
        e->size = 0;
        e->valid = false;

        memory_region_transaction_begin();

        if (!e->alias) {
            /* Create memory region alias. */
            e->name = g_strdup_printf("sifive.remapper.alias[%u]", i);
            e->alias = g_new(MemoryRegion, 1);

            memory_region_init_alias(e->alias, OBJECT(s), e->name,
                                     sys_mem, e->to_addr, e->size);
            memory_region_add_subregion_overlap(sys_mem, e->from_addr,
                                                e->alias, 1);
        } else {
            memory_region_set_address(e->alias, e->from_addr);
            memory_region_set_alias_offset(e->alias, e->to_addr);
            memory_region_set_size(e->alias, e->size);

            if (e->alias->priority != 1) {
                memory_region_del_subregion(sys_mem, e->alias);
                memory_region_add_subregion_overlap(sys_mem, e->from_addr,
                                                    e->alias, 1);
            }
        }

        memory_region_set_enabled(e->alias, false);
        memory_region_transaction_commit();
    }
}

static void sifive_remapper_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, sifive_remapper_properties);
    dc->realize = sifive_remapper_realize;
    dc->reset = sifive_remapper_reset;
}

static const TypeInfo sifive_remapper_info = {
    .name           = TYPE_SIFIVE_REMAPPER,
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(SiFiveRemapperState),
    .class_init     = sifive_remapper_class_init,
};

static void sifive_remapper_register_types(void)
{
    type_register_static(&sifive_remapper_info);
}

type_init(sifive_remapper_register_types)


/*
 * Create TileLink Address Remapper device.
 */
DeviceState *sifive_remapper_create(hwaddr addr,
                                    SiFiveAddrRmprVersion version,
                                    uint32_t num_entries)
{
    DeviceState *dev = qdev_new(TYPE_SIFIVE_REMAPPER);
    qdev_prop_set_uint32(dev, "version", version);
    qdev_prop_set_uint32(dev, "num-entries", num_entries);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, addr);
    return dev;
}
