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

#ifndef HW_SIFIVE_L2PF_H
#define HW_SIFIVE_L2PF_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_SIFIVE_L2PF "sifive.l2pf"

#define SIFIVE_L2PF_BASIC_CONTROL              0x0000
#define SIFIVE_L2PF_USER_BITS_CONTROL          0x0004

#define SIFIVE_L2PF_REG_SIZE                   0x2000

/* Register masks */

/*
 * v0:
 *  Basic control register:
 *      bit[0]:     en
 *      bit[1]:     crossPageOptmDis
 *      bit[7:2]:   dist
 *      bit[13:8]:  maxAllowedDist
 *      bit[19:14]: linToExpThrd
 *      bit[20]:    ageOutEn
 *      bit[27:21]: numLdsToAgeOut
 *      bit[28]:    crossPageEn
 *      bit[30:29]: reserved
 *
 * L2 user Bits control register:
 *      bit[3:0]:   qFullnessThrd
 *      bit[8:4]:   hitCacheThrd
 *      bit[12:9]:  hitMSHRThrd
 *      bit[18:13]: window
 *      bit[19]:    reserved
 *      bit[20]:    reserved
 *      bit[21]:    reserved
 */
#define SIFIVE_L2PF_BASIC_CTL_MASK_V0       0x1fffffff
#define SIFIVE_L2PF_USER_BITS_CTL_MASK_V0   0x0007ffff

/*
 * v1:
 *  Basic control register:
 *      bit[0]:     scalarLoadSupportEn
 *      bit[1]:     reserved
 *      bit[7:2]:   dist
 *      bit[13:8]:  maxAllowedDist
 *      bit[19:14]: linToExpThrd
 *      bit[20]:    reserved
 *      bit[27:21]: reserved
 *      bit[28]:    crossPageEn
 *      bit[30:29]: forgiveThrd
 *
 * L2 user Bits control register:
 *      bit[3:0]:   qFullnessThrd
 *      bit[8:4]:   hitCacheThrd
 *      bit[12:9]:  hitMSHRThrd
 *      bit[18:13]: window
 *      bit[19]:    scalarStoreSupportEn
 *      bit[20]:    vectorLoadSupportEn
 *      bit[21]:    vectorStoreSupportEn
 */
#define SIFIVE_L2PF_BASIC_CTL_MASK_V1       0x700ffffd
#define SIFIVE_L2PF_USER_BITS_CTL_MASK_V1   0x003fffff

/* Reset values */
#define SIFIVE_L2PF_BASIC_CTL_RST           0x0001430c
#define SIFIVE_L2PF_USER_BITS_CTL_RST       0x0000c45e


typedef struct SiFiveL2PFState SiFiveL2PFState;
DECLARE_INSTANCE_CHECKER(SiFiveL2PFState, SIFIVE_L2PF,
                         TYPE_SIFIVE_L2PF)

struct SiFiveL2PFState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    MemoryRegion mmio;

    uint32_t basic_ctl;
    uint32_t user_bits_ctl;

    uint32_t version;
    uint32_t basic_ctl_reg_mask;
    uint32_t user_bits_ctl_reg_mask;
};

DeviceState *sifive_l2pf_create(hwaddr addr);

#endif /* HW_SIFIVE_L2PF_H */
