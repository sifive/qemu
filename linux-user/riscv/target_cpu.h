#ifndef RISCV_TARGET_CPU_H
#define RISCV_TARGET_CPU_H

static inline void cpu_clone_regs_child(CPURISCVState *env, target_ulong newsp,
                                        unsigned flags)
{
    if (newsp) {
        env->gpr[xSP] = newsp;
    }

    env->gpr[xA0] = 0;

    if (env->ubcfien && env->ssp) {
        uintptr_t ssp = 0;
        /* SS page should be surrounded by two guard pages */
        ssp = (uintptr_t) mmap(0, TARGET_PAGE_SIZE * 3, PROT_NONE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((intptr_t)ssp == -1) {
            perror("shadow stack alloc");
            exit(EXIT_FAILURE);
        }
        ssp += TARGET_PAGE_SIZE;
        mprotect((void *)ssp, TARGET_PAGE_SIZE, PROT_READ | PROT_WRITE);
        /* Duplicate the content of parent's SS page */
        memcpy((void *)ssp,
               (void *)ROUND_DOWN((uintptr_t)env->ssp, TARGET_PAGE_SIZE),
               TARGET_PAGE_SIZE);
        env->ssp = ROUND_UP(ssp + 1, TARGET_PAGE_SIZE);
    } else {
        env->ssp = 0;
    }
}

static inline void cpu_clone_regs_parent(CPURISCVState *env, unsigned flags)
{
}

static inline void cpu_set_tls(CPURISCVState *env, target_ulong newtls)
{
    env->gpr[xTP] = newtls;
}

static inline abi_ulong get_sp_from_cpustate(CPURISCVState *state)
{
   return state->gpr[xSP];
}
#endif
