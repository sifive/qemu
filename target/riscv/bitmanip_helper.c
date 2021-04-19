/*
 * RISC-V Bitmanip Extension Helpers for QEMU.
 *
 * Copyright (c) 2020 Kito Cheng, kito.cheng@sifive.com
 * Copyright (c) 2020 Frank Chang, frank.chang@sifive.com
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
#include "qemu/host-utils.h"
#include "exec/exec-all.h"
#include "exec/helper-proto.h"
#include "tcg/tcg.h"

static const uint64_t adjacent_masks[] = {
    dup_const(MO_8, 0x55),
    dup_const(MO_8, 0x33),
    dup_const(MO_8, 0x0f),
    dup_const(MO_16, 0xff),
    dup_const(MO_32, 0xffff),
#ifdef TARGET_RISCV64
    UINT32_MAX
#endif
};

static inline target_ulong do_swap(target_ulong x, uint64_t mask, int shift)
{
    return ((x & mask) << shift) | ((x & ~mask) >> shift);
}

static target_ulong do_grev(target_ulong rs1,
                            target_ulong rs2,
                            int bits)
{
    target_ulong x = rs1;
    int i, shift;

    for (i = 0, shift = 1; shift < bits; i++, shift <<= 1) {
        if (rs2 & shift) {
            x = do_swap(x, adjacent_masks[i], shift);
        }
    }

    return x;
}

target_ulong HELPER(grev)(target_ulong rs1, target_ulong rs2)
{
    return do_grev(rs1, rs2, TARGET_LONG_BITS);
}

/* RV64-only instruction */
#ifdef TARGET_RISCV64

target_ulong HELPER(grevw)(target_ulong rs1, target_ulong rs2)
{
    return do_grev(rs1, rs2, 32);
}

#endif

static target_ulong do_gorc(target_ulong rs1,
                            target_ulong rs2,
                            int bits)
{
    target_ulong x = rs1;
    int i, shift;

    for (i = 0, shift = 1; shift < bits; i++, shift <<= 1) {
        if (rs2 & shift) {
            x |= do_swap(x, adjacent_masks[i], shift);
        }
    }

    return x;
}

target_ulong HELPER(gorc)(target_ulong rs1, target_ulong rs2)
{
    return do_gorc(rs1, rs2, TARGET_LONG_BITS);
}

/* RV64-only instruction */
#ifdef TARGET_RISCV64

target_ulong HELPER(gorcw)(target_ulong rs1, target_ulong rs2)
{
    return do_gorc(rs1, rs2, 32);
}

#endif

static target_ulong do_clmul(target_long rs1,
                             target_ulong rs2,
                             int bits)
{
    target_ulong x = 0;
    int i;

    for (i = 0; i < bits; i++) {
        if ((rs2 >> i) & 1) {
            x ^= rs1 << i;
        }
    }

    return x;
}

static target_ulong do_clmulh(target_long rs1,
                              target_ulong rs2,
                              int bits)
{
    target_ulong x = 0;
    int i;

    for (i = 0; i < bits; i++) {
        if ((rs2 >> i) & 1) {
            x ^= rs1 >> (bits - i);
        }
    }

    return x;
}

static target_ulong do_clmulr(target_long rs1,
                              target_ulong rs2,
                              int bits)
{
    target_ulong x = 0;
    int i;

    for (i = 0; i < bits; i++) {
        if ((rs2 >> i) & 1) {
            x ^= rs1 >> (bits - i - 1);
        }
    }

    return x;
}

target_ulong HELPER(clmul)(target_ulong rs1, target_ulong rs2)
{
    return do_clmul(rs1, rs2, TARGET_LONG_BITS);
}

target_ulong HELPER(clmulh)(target_ulong rs1, target_ulong rs2)
{
    return do_clmulh(rs1, rs2, TARGET_LONG_BITS);
}

target_ulong HELPER(clmulr)(target_ulong rs1, target_ulong rs2)
{
    return do_clmulr(rs1, rs2, TARGET_LONG_BITS);
}

/* RV64-only instructions */
#ifdef TARGET_RISCV64

target_ulong HELPER(clmulw)(target_ulong rs1, target_ulong rs2)
{
    return do_clmul(rs1, rs2, 32);
}

target_ulong HELPER(clmulhw)(target_ulong rs1, target_ulong rs2)
{
    return do_clmulh(rs1, rs2, 32);
}

target_ulong HELPER(clmulrw)(target_ulong rs1, target_ulong rs2)
{
    return do_clmulr(rs1, rs2, 32);
}

#endif
