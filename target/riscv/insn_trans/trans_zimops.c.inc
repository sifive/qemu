/*
 * RISC-V translation routines for the Control-Flow Integrity Extension
 *
 * Copyright (c) 2022-2023 Rivos Inc.
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

 static bool trans_zimops_r(DisasContext *ctx, arg_zimops_r * a)
 {
      /* zimops not implemented, raise illegal instruction & return true */
      if (!ctx->cfg_ptr->ext_zimops) {
            gen_exception_illegal(ctx);
            return true;
      }

      /*
       * zimops implemented, simply grab destination and mov zero.
       * return true
       */
      TCGv dest = dest_gpr(ctx, a->rd);
      dest = tcg_constant_tl(0);
      gen_set_gpr(ctx, a->rd, dest);
      return true;
 }

 static bool trans_zimops_rr(DisasContext *ctx, arg_zimops_r * a)
 {
      /* zimops not implemented, raise illegal instruction & return true */
      if (!ctx->cfg_ptr->ext_zimops) {
            gen_exception_illegal(ctx);
            return true;
      }

      /*
       * zimops implemented, simply grab destination and mov zero.
       * return true
       */
      TCGv dest = dest_gpr(ctx, a->rd);
      dest = tcg_constant_tl(0);
      gen_set_gpr(ctx, a->rd, dest);
      return true;
 }
