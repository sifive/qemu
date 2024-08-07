/*
 * RISC-V translation routines for the Control-Flow Integrity Extension
 *
 * Copyright (c) 2024 Rivos Inc.
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

static MemOp mxl_memop(DisasContext *ctx)
{
    switch (get_xl(ctx)) {
    case MXL_RV32:
        return MO_TEUL;

    case MXL_RV64:
        return MO_TEUQ;

    case MXL_RV128:
        return MO_TEUO;

    default:
        g_assert_not_reached();
    }
}

static bool trans_sspopchk(DisasContext *ctx, arg_sspopchk *a)
{
    /* default for qemu-user, use regular RW memory and thus mmu_idx=0 */
    int ss_mmu_idx = 0;

    /* sspopchk only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi was not enabled, return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    /*
     * sspopchk can only compare with x1 or x5. Everything else defaults to
     * zimops
     */

    if (a->rs1 != 1 && a->rs1 != 5) {
        return false;
    }

    /*
     * get data in TCGv using get_gpr
     * get addr in TCGv using gen_helper_csrr on CSR_SSP
     * use some tcg subtract arithmetic (subtract by XLEN) on addr
     * perform ss store on computed address
     */

    TCGv addr = tcg_temp_new();
    TCGLabel *skip = gen_new_label();
    uint32_t tmp = (get_xl(ctx) == MXL_RV64) ? 8 : 4;
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    TCGv data = tcg_temp_new();
    gen_helper_csrr(addr, tcg_env, ssp_csr);
#ifndef CONFIG_USER_ONLY
    ss_mmu_idx = get_ss_index(ctx);
#endif

    tcg_gen_qemu_ld_tl(data, addr, ss_mmu_idx,
                       mxl_memop(ctx) | MO_ALIGN);
    TCGv rs1 = get_gpr(ctx, a->rs1, EXT_NONE);
    tcg_gen_brcond_tl(TCG_COND_EQ, data, rs1, skip);
    gen_helper_raise_sw_check_excep(tcg_env,
        tcg_constant_tl(RISCV_EXCP_SW_CHECK_BCFI_TVAL), data, rs1);
    gen_set_label(skip);
    tcg_gen_addi_tl(addr, addr, tmp);
    gen_helper_csrw(tcg_env, ssp_csr, addr);

    return true;
}

static bool trans_sspush(DisasContext *ctx, arg_sspush *a)
{
    /* default for qemu-user, use regular RW memory and thus mmu_idx=0 */
    int ss_mmu_idx = 0;

    /* sspush only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi was not enabled, return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    /*
     * sspush can only push from x1 or x5. Everything else defaults to zimop
     */
    if (a->rs2 != 1 && a->rs2 != 5) {
        return false;
    }

    /*
     * get data in TCGv using get_gpr
     * get addr in TCGv using gen_helper_csrr on CSR_SSP
     * use some tcg subtract arithmetic (subtract by XLEN) on addr
     * perform ss store on computed address
     */

    TCGv addr = tcg_temp_new();
    int tmp = (get_xl(ctx) == MXL_RV64) ? -8 : -4;
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    TCGv data = get_gpr(ctx, a->rs2, EXT_NONE);
    gen_helper_csrr(addr, tcg_env, ssp_csr);
#ifndef CONFIG_USER_ONLY
    ss_mmu_idx = get_ss_index(ctx);
#endif

    tcg_gen_addi_tl(addr, addr, tmp);

    tcg_gen_qemu_st_tl(data, addr, ss_mmu_idx,
                       mxl_memop(ctx) | MO_ALIGN);
    gen_helper_csrw(tcg_env, ssp_csr, addr);

    return true;
}

static bool trans_ssrdp(DisasContext *ctx, arg_ssrdp *a)
{
    /* ssrdp only supported on 32bit and 64bit */
    if (get_xl(ctx) != MXL_RV32 && get_xl(ctx) != MXL_RV64) {
        return false;
    }

    /* back cfi was not enabled, return false */
    if (!ctx->bcfi_enabled) {
        return false;
    }

    TCGv dest = get_gpr(ctx, a->rd, EXT_NONE);
    TCGv_i32 ssp_csr = tcg_constant_i32(CSR_SSP);
    gen_helper_csrr(dest, tcg_env, ssp_csr);
    gen_set_gpr(ctx, a->rd, dest);

    return true;
}
