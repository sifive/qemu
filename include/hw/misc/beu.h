/*
 * Bus Error Unit interface
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

#ifndef HW_MISC_BEU_H
#define HW_MISC_BEU_H

#include <stdbool.h>
#include "exec/hwaddr.h"
#include "exec/memattrs.h"
#include "hw/core/cpu.h"
#include "qom/object.h"

#define TYPE_BEU_INTERFACE "beu-interface"
#define BEU_INTERFACE(obj) \
    INTERFACE_CHECK(BEUInterface, (obj), TYPE_BEU_INTERFACE)
typedef struct BEUInterfaceClass BEUInterfaceClass;
DECLARE_CLASS_CHECKERS(BEUInterfaceClass, BEU_INTERFACE,
                       TYPE_BEU_INTERFACE)

typedef struct BEUInterface BEUInterface;

struct BEUInterfaceClass {
    InterfaceClass parent;

    bool (*handle_bus_error)(BEUInterface *bi, MMUAccessType access_type,
                             MemTxResult response, hwaddr physaddr);
};

#endif /* HW_MISC_BEU_H */
