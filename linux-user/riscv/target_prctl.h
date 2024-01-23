/* No special prctl support required. */
#ifndef RISCV_TARGET_PRCTL_H
#define RISCV_TARGET_PRCTL_H

/* -TARGET_EINVAL: Unsupported/Invalid flag for this architecture
 * -TARGET_EACCES: Try to set an already set CFI feature
 * -TARGET_ENOENT: CFI feature is not supported by CPU
 **/
static abi_long do_prctl_cfi_set(CPUArchState *env, abi_long option, abi_long flag)
{
    if (env_archcpu(env)->cfg.ext_cfi_ss) {
        switch (option) {
        case PR_GET_SHADOW_STACK_STATUS:
            return env->ubcfien;
        case PR_SET_SHADOW_STACK_STATUS:
            {
                if (env->ubcfien)
                    return -TARGET_EACCES;
                if (flag & PR_SHADOW_STACK_ENABLE) {
                    env->ubcfien = true;
                    if (env->ssp == 0)
                        zicfiss_shadow_stack_alloc(env);
                    return 0;
                }
                return -TARGET_EINVAL;
            }
        case PR_LOCK_SHADOW_STACK_STATUS:
            return -TARGET_EINVAL;
        }
    }

    if (env_archcpu(env)->cfg.ext_cfi_lp) {
        switch (option) {
        case PR_GET_INDIR_BR_LP_STATUS:
            return env->ufcfien;
        case PR_SET_INDIR_BR_LP_STATUS:
            {
                if (env->ufcfien)
                    return -TARGET_EACCES;
                if (flag & PR_INDIR_BR_LP_ENABLE) {
                    env->ufcfien = true;
                    return 0;
                }
                return -TARGET_EINVAL;
            }
        case PR_LOCK_INDIR_BR_LP_STATUS:
            return -TARGET_EINVAL;
        }
    }
    return -TARGET_ENOENT;
}
#define do_prctl_cfi_set do_prctl_cfi_set

#endif
