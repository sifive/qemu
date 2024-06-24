#ifndef RISCV_TARGET_PRCTL_H
#define RISCV_TARGET_PRCTL_H

#define PR_SHADOW_STACK_SUPPORTED_STATUS_MASK (PR_SHADOW_STACK_ENABLE)

static abi_long do_prctl_cfi_set(CPUArchState *env, abi_long option, abi_long flag)
{
    if (env_archcpu(env)->cfg.ext_cfi_ss) {
        switch (option) {
        case PR_GET_SHADOW_STACK_STATUS:
            {
                abi_ulong bcfi_status = 0;

                /* this means shadow stack is enabled on the task */
                bcfi_status |= (env->ubcfi_en ? PR_SHADOW_STACK_ENABLE : 0);

                return copy_to_user(flag, &bcfi_status, sizeof(bcfi_status)) ? \
                    -EFAULT : 0;
            }
        case PR_SET_SHADOW_STACK_STATUS:
            {
                bool enable_shstk = false;

                if (env->ubcfi_locked)
                    return -TARGET_EINVAL;

                /* Reject unknown flags */
                if (flag & ~PR_SHADOW_STACK_SUPPORTED_STATUS_MASK)
                    return -TARGET_EINVAL;

                enable_shstk = flag & PR_SHADOW_STACK_ENABLE ? true : false;

                /*
                 * Request is to enable shadow stack and shadow stack is not
                 * enabled already.
                 */
                if (enable_shstk && !env->ubcfi_en) {
                    if (env->ssp != 0)
                        return -TARGET_EINVAL;

                    env->ubcfi_en = true;
                    zicfiss_shadow_stack_alloc(env);
                }

                /*
                 * Request is to disable shadow stack and shadow stack is
                 * enabled already.
                 */
                if (!enable_shstk && env->ubcfi_en) {
                    if (env->ssp == 0)
                        return -TARGET_EINVAL;

                    env->ubcfi_en = false;
                    zicfiss_shadow_stack_release(env);
                }

                return 0;
            }
        case PR_LOCK_SHADOW_STACK_STATUS:
            {
                if (!env->ubcfi_en)
                    return -TARGET_EINVAL;

                env->ubcfi_locked = true;
                return 0;
            }
        }
    }

    if (env_archcpu(env)->cfg.ext_cfi_lp) {
        switch (option) {
        case PR_GET_INDIR_BR_LP_STATUS:
            {
                abi_ulong fcfi_status = 0;

                /* indirect branch tracking is enabled on the task or not */
                fcfi_status |= (env->ufcfi_en ? PR_INDIR_BR_LP_ENABLE : 0);

                return copy_to_user(flag, &fcfi_status, sizeof(fcfi_status)) ? \
                    -EFAULT : 0;
            }
        case PR_SET_INDIR_BR_LP_STATUS:
            {
                bool enable_indir_lp = false;

                if (env->ufcfi_locked)
                    return -TARGET_EINVAL;

                /* Reject random flags */
                if (flag & ~PR_INDIR_BR_LP_ENABLE)
                    return -TARGET_EINVAL;

                enable_indir_lp = flag & PR_INDIR_BR_LP_ENABLE ? true : false;
                env->ufcfi_en = enable_indir_lp;
                return 0;
            }
        case PR_LOCK_INDIR_BR_LP_STATUS:
            {
                if (!env->ufcfi_en)
                    return -TARGET_EINVAL;

                env->ufcfi_locked = true;
                return 0;
            }
        }
    }
    return -TARGET_EINVAL;
}
#define do_prctl_cfi_set do_prctl_cfi_set

#endif
