/* No special prctl support required. */
#ifndef RISCV_TARGET_PRCTL_H
#define RISCV_TARGET_PRCTL_H

static abi_long do_prctl_cfi_set(CPUArchState *env, abi_long flag)
{
    if (flag & ~PR_CFI_ALL) {
        return -TARGET_EINVAL;
    }
    if (env->ubcfien && !(flag && PR_CFI_SHADOW_STACK)) {
        return -TARGET_EACCES;
    }
    if (env->ufcfien && !(flag && PR_CFI_LANDING_PAD)) {
        return -TARGET_EACCES;
    }
    if (env_archcpu(env)->cfg.ext_cfi_ss && flag & PR_CFI_SHADOW_STACK) {
        env->ubcfien = true;
        return 0;
    }
    if (env_archcpu(env)->cfg.ext_cfi_lp && flag & PR_CFI_LANDING_PAD) {
        env->ufcfien = true;
        return 0;
    }
    return -TARGET_ENOENT;
}
#define do_prctl_cfi_set do_prctl_cfi_set

#endif
