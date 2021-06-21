/*
 * SiFive E series machine interface (Customer Ver.)
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

#ifndef HW_SIFIVE_E_CUSTOMER_H
#define HW_SIFIVE_E_CUSTOMER_H

#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/sifive_cpu.h"
#include "hw/gpio/sifive_gpio.h"

#define TYPE_RISCV_E_CUSTOMER_SOC "riscv.sifive.e.customer.soc"
#define RISCV_E_CUSTOMER_SOC(obj) \
    OBJECT_CHECK(SiFiveECustomerSoCState, (obj), TYPE_RISCV_E_CUSTOMER_SOC)

typedef struct SiFiveECustomerSoCState {
    /*< private >*/
    DeviceState parent_obj;

    /*< public >*/
    RISCVHartArrayState cpus;
    DeviceState *plic;
    SIFIVEGPIOState gpio;
    MemoryRegion xip_mem;
    MemoryRegion mask_rom;
} SiFiveECustomerSoCState;

typedef struct SiFiveECustomerState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    SiFiveECustomerSoCState soc;
    bool revb;
} SiFiveECustomerState;

#define TYPE_RISCV_E_CUSTOMER_MACHINE MACHINE_TYPE_NAME("sifive_e_customer")
#define RISCV_E_CUSTOMER_MACHINE(obj) \
    OBJECT_CHECK(SiFiveECustomerState, (obj), TYPE_RISCV_E_CUSTOMER_MACHINE)

enum {
    SIFIVE_E_CUSTOMER_DEV_DEBUG,
    SIFIVE_E_CUSTOMER_DEV_MROM,
    SIFIVE_E_CUSTOMER_DEV_OTP,
    SIFIVE_E_CUSTOMER_DEV_TEST,
    SIFIVE_E_CUSTOMER_DEV_CLINT,
    SIFIVE_E_CUSTOMER_DEV_L2PF,
    SIFIVE_E_CUSTOMER_DEV_REMAPPER,
    SIFIVE_E_CUSTOMER_DEV_PLIC,
    SIFIVE_E_CUSTOMER_DEV_AON,
    SIFIVE_E_CUSTOMER_DEV_PRCI,
    SIFIVE_E_CUSTOMER_DEV_OTP_CTRL,
    SIFIVE_E_CUSTOMER_DEV_GPIO0,
    SIFIVE_E_CUSTOMER_DEV_UART0,
    SIFIVE_E_CUSTOMER_DEV_QSPI0,
    SIFIVE_E_CUSTOMER_DEV_PWM0,
    SIFIVE_E_CUSTOMER_DEV_UART1,
    SIFIVE_E_CUSTOMER_DEV_QSPI1,
    SIFIVE_E_CUSTOMER_DEV_PWM1,
    SIFIVE_E_CUSTOMER_DEV_QSPI2,
    SIFIVE_E_CUSTOMER_DEV_PWM2,
    SIFIVE_E_CUSTOMER_DEV_XIP,
    SIFIVE_E_CUSTOMER_DEV_DTIM
};

enum {
    SIFIVE_E_CUSTOMER_UART0_IRQ  = 3,
    SIFIVE_E_CUSTOMER_UART1_IRQ  = 4,
    SIFIVE_E_CUSTOMER_GPIO0_IRQ0 = 8
};

#define SIFIVE_E_CUSTOMER_CPUS_MAX  8

#define SIFIVE_E_CUSTOMER_PLIC_HART_CONFIG "M"
#define SIFIVE_E_CUSTOMER_PLIC_NUM_SOURCES 127
#define SIFIVE_E_CUSTOMER_PLIC_NUM_PRIORITIES 7
#define SIFIVE_E_CUSTOMER_PLIC_PRIORITY_BASE 0x04
#define SIFIVE_E_CUSTOMER_PLIC_PENDING_BASE 0x1000
#define SIFIVE_E_CUSTOMER_PLIC_ENABLE_BASE 0x2000
#define SIFIVE_E_CUSTOMER_PLIC_ENABLE_STRIDE 0x80
#define SIFIVE_E_CUSTOMER_PLIC_CONTEXT_BASE 0x200000
#define SIFIVE_E_CUSTOMER_PLIC_CONTEXT_STRIDE 0x1000

#define SIFIVE_E_CUSTOMER_L2PF_STRIDE  0x2000

#endif
