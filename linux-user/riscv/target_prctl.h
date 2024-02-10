/* No special prctl support required. */
#ifndef RISCV_TARGET_PRCTL_H
#define RISCV_TARGET_PRCTL_H

static abi_long do_prctl_cfi_set(CPUArchState *env, abi_long flag)
{
    if (flag & ~PR_CFI_ALL) {
        return -TARGET_EINVAL;
    }
    if (env->ubcfien && (flag & PR_CFI_SHADOW_STACK)) {
        return -TARGET_EACCES;
    }
    if (env->ufcfien && (flag & PR_CFI_LANDING_PAD)) {
        return -TARGET_EACCES;
    }
    if (env_archcpu(env)->cfg.ext_cfi_ss && flag & PR_CFI_SHADOW_STACK) {
        env->ubcfien = true;
        if (env->ssp == 0) {
            uintptr_t ssp = 0;
            ssp = (uintptr_t) mmap(0, TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if ((intptr_t)ssp == -1) {
                perror("shadow stack alloc");
                exit(EXIT_FAILURE);
            }
            env->ssp = ROUND_UP(ssp + 1, TARGET_PAGE_SIZE);
        }
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
