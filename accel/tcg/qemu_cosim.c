#include "qemu/osdep.h"
#include "qemu/host-utils.h"
#include "qemu/main-loop.h"
#include "cpu.h"
#include "qemu_cosim.h"
#include "cosim.h"
#include "sysemu/runstate.h"
#include <stdlib.h>
#include "exec/log.h"

void qemu_cosim_read_mem(hwaddr addr, void *buf, hwaddr len);
static void pg_walk(uint64_t satp, uint64_t virt, void (*)(hwaddr addr, void *buf, hwaddr len));

void *cosim_handle;

typedef struct {
    uint64_t addr;
    size_t len;
    uint8_t data[8]; // assume maximum 8-byte per load
    int valid;
} mmio_load_t;

#define MMIO_MAX_LOAD 128
int mmio_load_consume = 0;
int mmio_load_produce = 0;
mmio_load_t mmio_load_queue[MMIO_MAX_LOAD] = {0};

void qemu_cosim_mmioload_record(uint64_t addr, size_t len, uint64_t *value)
{
#ifdef DEBUG_MMIO
    qemu_log("[%d]mmioload record %lx %lx %lx\n", mmio_load_produce, addr, len, *value);
#endif
    if (len > 8) {
        qemu_log("mmio load size > 8: %ld at %lx\n", len, addr);
    }
    assert(len <= 8);
    mmio_load_queue[mmio_load_produce].valid = 1;
    mmio_load_queue[mmio_load_produce].addr = addr;
    mmio_load_queue[mmio_load_produce].len = len;
    memcpy(mmio_load_queue[mmio_load_produce].data, value, len);
    mmio_load_produce = (mmio_load_produce + 1) % MMIO_MAX_LOAD;
}

static int qemu_cosim_mmioload_replay(uint64_t addr, size_t len, uint8_t *bytes)
{
#if 0
    if ((addr >= (uint64_t) 0x80000000) && (addr < 0x80000000 + (2048 << 48)) {
    }
#endif
#if 1
    if (mmio_load_queue[mmio_load_consume].valid) {
#else
    if (mmio_load_queue[mmio_load_consume].valid
            && (mmio_load_queue[mmio_load_consume].addr == addr)
            && (mmio_load_queue[mmio_load_consume].len == len)) {
#endif

        memcpy(bytes, mmio_load_queue[mmio_load_consume].data, len);
#ifdef DEBUG_MMIO
        qemu_log("[%d]mmioload replay %lx %lx %lx\n", mmio_load_consume, addr, len, *(uint64_t *)bytes);
#endif
        mmio_load_queue[mmio_load_consume].valid = 0;
        mmio_load_consume = (mmio_load_consume + 1) % MMIO_MAX_LOAD;
        return 1;
    }
#ifdef DEBUG_MMIO
    qemu_log("consume %d, produce %d\n", mmio_load_consume, mmio_load_produce);
    qemu_log("mmio load %lx, len %ld\n", addr, len);
#endif
    return 0;
}

static void atexit_dump(void);
void qemu_cosim_init(const char *elffile, int is_bin, void *dtb, int dtb_sz,
                     uint32_t fdt_load_addr)
{
    if (is_bin == 0) {
        //cosim_handle = cosim_init(elffile, 1, qemu_cosim_mmioload_replay);
        cosim_handle = cosim_init(elffile, 0, qemu_cosim_mmioload_replay, is_bin, dtb, dtb_sz,
                                  fdt_load_addr);
    } else {
        cosim_handle = cosim_init(elffile, 0, qemu_cosim_mmioload_replay, is_bin, dtb, dtb_sz,
                                  fdt_load_addr);
    }
    atexit(atexit_dump);
}

#define DUMP_TEMPLATE \
    qemu_log("pc = %lx\n", env->pc); \
    for (int i = 0; i < 32; i++) \
        qemu_log("[%d] = %lx\n", i, env->gpr[i]); \


void dump_qemu_reg(const CPURISCVState *env);
void dump_cosim_reg(const struct RVRegFile *env);
static void dump_reg(const CPURISCVState *qemu_env, const struct RVRegFile *cosim_env);

uint64_t debug_counter = 1;
static void dump_reg(const CPURISCVState *qemu_env, const struct RVRegFile *cosim_env)
{
    qemu_log("Dump Registgers from Qemu & Cosim\n");
    qemu_log("debug_counter = %lx\n", debug_counter);
    qemu_log("last_pc = %lx, %lx\n", qemu_env->pc, cosim_env->pc);
    qemu_log("insn = %lx: DASM(%lx)\n", cosim_env->last_insn, cosim_env->last_insn);
    qemu_log("mstatus = %lx, %lx\n", qemu_env->mstatus, cosim_env->mstatus);
    qemu_log("mcause = %lx, %lx\n", qemu_env->mcause, cosim_env->mcause);
    qemu_log("mie = %lx, %lx\n", qemu_env->mie, cosim_env->mie);
    qemu_log("mip = %lx, %lx\n", qemu_env->mip, cosim_env->mip);
    qemu_log("mbadaddr= %lx, %lx\n", qemu_env->mbadaddr, cosim_env->mtval);
    qemu_log("sepc = %lx, %lx\n", qemu_env->sepc, cosim_env->sepc);
    qemu_log("scause = %lx, %lx\n", qemu_env->scause, cosim_env->scause);
    qemu_log("satp = %lx, %lx\n", qemu_env->satp, cosim_env->satp);
    qemu_log("stvec = %lx, %lx\n", qemu_env->stvec, cosim_env->stvec);
    qemu_log("sbadaddr= %lx, %lx\n", qemu_env->sbadaddr, cosim_env->stval);
    for (int i = 0; i < 32; i++)
        qemu_log("gp[%d] = %lx, %lx\n", i, qemu_env->gpr[i], cosim_env->gpr[i]);
#ifdef CFG_CMP_FPR
    for (int i = 0; i < 32; i++)
        qemu_log("fp[%d] = %lx, %lx\n", i, qemu_env->fpr[i], cosim_env->fprv0[i]);
#endif
}

void dump_qemu_reg(const CPURISCVState *env)
{
    qemu_log("qemu register:\n");
    DUMP_TEMPLATE
}

void dump_cosim_reg(const struct RVRegFile *env)
{
    qemu_log("cosim register:\n");
    DUMP_TEMPLATE
}

int qemu_cosim_sync_ignored = 1;
void qemu_cosim_ignore_next(void)
{
#ifdef DEBUG
    if (debug_counter > DEBUG_COUNTER) {
        qemu_log("step ignore next\n");
    }
#endif
    qemu_cosim_sync_ignored = 1;
}

const char *stage_str(enum stage_t stage)
{
    switch (stage) {
        case ST_IGNORE: return "st_ignore";
        case ST_INSN_START: return "st_insn_start";
        case ST_END_OF_BLOCK: return "st_end_of_block";
        case ST_INTERRUPT: return "st_interrupt";
        case ST_EXCEPTION: return "st_exception";
    };
    return "st_unknown";
}

static void push_queue(int stage);
static void print_queue(void);

static void atexit_dump(void)
{
#if 0
    const struct RVRegFile *regfile = cosim_get_state(cosim_handle);
    int cpu0 = 0;
    RISCVCPU *cpu = RISCV_CPU(qemu_get_cpu(cpu0));
    CPURISCVState *env = &cpu->env;
    dump_reg(env, regfile);
#else
    print_queue();
#endif
}

#define MAX_BACKUP 10

static struct {int stage; struct RVRegFile regfile; CPURISCVState env;} back_queue[MAX_BACKUP];
static int back_i = 0;

static void push_queue(int stage)
{
    const struct RVRegFile *regfile = cosim_get_state(cosim_handle);
    int cpu0 = 0;
    RISCVCPU *cpu = RISCV_CPU(qemu_get_cpu(cpu0));
    CPURISCVState *env = &cpu->env;

    back_i = (back_i + 1) % MAX_BACKUP;
    if (stage == ST_INSN_START || stage == ST_END_OF_BLOCK) {
        memcpy(&back_queue[back_i].regfile, regfile, sizeof(*regfile));
        memcpy(&back_queue[back_i].env, env, sizeof(*env));
    }
    back_queue[back_i].stage = stage;
}

static void cmp_queue(void)
{
    int cur_i = back_i;
    const struct RVRegFile *regfile = &back_queue[cur_i].regfile;
    CPURISCVState *env = &back_queue[cur_i].env;
    static int error = 0;

#ifdef DEBUG
    if (debug_counter > DEBUG_COUNTER) {
        qemu_log("cmp qemu pc = %lx, cosim pc = %lx\n", env->pc, regfile->pc);
    }
#endif

#ifdef CFG_CMP_FPR
    for (int i = 0; i < 32; i++) {
        if (env->fpr[i] != regfile->fprv0[i]) {
            qemu_log("diff: fpr[%d] %lx vs %lx %lx\n", i, env->fpr[i], regfile->fprv0[i], regfile->fprv1[i]);
            error = 1;
        }
    }
#endif

    for (int i = 0; i < 32; i++) {
        if (env->gpr[i] != regfile->gpr[i]) {
            qemu_log("diff: gpr[%d]\n", i);
            error = 1;
        }
    }

    if (env->mstatus != regfile->mstatus) {
        qemu_log("diff: mstatus %lx %lx\n", env->mstatus, regfile->mstatus);
        error = 1;
    }

    if (env->mie != regfile->mie) {
        qemu_log("diff: mie\n");
        error = 1;
    }

    if (env->mip != regfile->mip) {
        qemu_log("diff: mip\n");
        error = 1;
    }

    if (env->mcause != regfile->mcause) {
        qemu_log("diff: mcause: %lx %lx\n", env->mcause, regfile->mcause);
        if (regfile->mtval != 0) {
            qemu_log("check qemu's mbadaddr pte\n");
            pg_walk(regfile->satp, regfile->mtval, cpu_physical_memory_read);
            qemu_log("check cosim's mbadaddr pte\n");
            pg_walk(regfile->satp, regfile->mtval, qemu_cosim_read_mem);
        }
        error = 1;
    }

    if (env->scause != regfile->scause) {
        qemu_log("diff: scause: %lx %lx\n", env->scause, regfile->scause);
        error = 1;
        if (regfile->stval != 0) {
#if 0
            int test;
            qemu_cosim_read_mem(0x80000000, &test, 4);
            qemu_log("check cosim 0x8000-0000, test = %x\n", test);
            cpu_physical_memory_read(0x80000000, &test, 4);
            qemu_log("check qemu 0x8000-0000, test = %x\n", test);

            qemu_cosim_read_mem(0xf9e88000, &test, 4);
            qemu_log("check cosim 0xf9e8-8000, test = %x\n", test);
            cpu_physical_memory_read(0xf9e88000, &test, 4);
            qemu_log("check qemu 0xf9e8-8000, test = %x\n", test);

            pg_walk(regfile->satp, 0xffffffe000000104, qemu_cosim_read_mem);

            qemu_log("check cosim's sepc pte\n");
            pg_walk(regfile->satp, regfile->sepc, qemu_cosim_read_mem);
#endif
            qemu_log("check qemu's sbadaddr pte\n");
            pg_walk(regfile->satp, regfile->stval, cpu_physical_memory_read);
            qemu_log("check cosim's sbadaddr pte\n");
            pg_walk(regfile->satp, regfile->stval, qemu_cosim_read_mem);
        }
    }

    if (env->sbadaddr != regfile->stval) {
        qemu_log("diff: sbadaddr: %lx %lx\n", env->sbadaddr, regfile->stval);
        error = 1;
    }

    if (env->satp != regfile->satp) {
        qemu_log("diff: satp\n");
        error = 1;
    }

    if (env->stvec != regfile->stvec) {
        qemu_log("diff: stvec\n");
        error = 1;
    }

#if 0
    if (qemu_mutex_iothread_locked())
        qemu_mutex_unlock_iothread();
#endif

    if (error) {
        static int retry = 3;
        if (retry > 0) {
            retry--;
            qemu_log("debug_counter = %lx\n", debug_counter);
            return;
        }
        exit(-1);
    }
    return;
}

static void print_queue(void)
{
    for (int i = back_i + 1; i < MAX_BACKUP; i++) {
        qemu_log("[%d]: stage %s\n", i, stage_str(back_queue[i].stage));
        int stage = back_queue[i].stage;
        if (stage == ST_INSN_START || stage == ST_END_OF_BLOCK)
            dump_reg(&back_queue[i].env, &back_queue[i].regfile);
    }

    for (int i = 0; i < back_i + 1; i++) {
        qemu_log("[%d]: stage %s\n", i, stage_str(back_queue[i].stage));
        int stage = back_queue[i].stage;
        if (stage == ST_INSN_START || stage == ST_END_OF_BLOCK)
            dump_reg(&back_queue[i].env, &back_queue[i].regfile);
    }
}

void qemu_cosim_assert(void)
{
}

void qemu_cosim_sync(enum stage_t stage)
{
    if (qemu_cosim_sync_ignored == 0) {
        qemu_mutex_lock_iothread();
        qemu_cosim_step(stage);
        qemu_cosim_cmp();
        qemu_mutex_unlock_iothread();
    } else {
#ifdef DEBUG
        if (debug_counter > DEBUG_COUNTER) {
            qemu_log("sync: ignored\n");
        }
        push_queue(ST_IGNORE);
#endif
    }
    qemu_cosim_sync_ignored = 0;
}

void qemu_cosim_step(enum stage_t stage)
{
#ifdef DEBUG
    if (debug_counter > DEBUG_COUNTER) {
        int cpu0 = 0;
        RISCVCPU *cpu = RISCV_CPU(qemu_get_cpu(cpu0));
        const struct RVRegFile *regfile = cosim_get_state(cosim_handle);
        CPURISCVState *env = &cpu->env;
        qemu_log("[%d] step cosim: qemu pc = %lx %lx\n", stage, env->pc, regfile->pc);
    }
#endif
    debug_counter++;

    cosim_step_inst(cosim_handle, 1);
    push_queue(stage);
}

void qemu_cosim_cmp(void)
{
    cmp_queue();
}

void qemu_cosim_update_mip(uint64_t value)
{
    cosim_update_mip(cosim_handle, value);
}

void qemu_cosim_enter_interrupt(void)
{
    cosim_check_interrupt(cosim_handle);
    push_queue(ST_INTERRUPT);
}

void qemu_cosim_enter_exception(void)
{
    cosim_step_into_exception(cosim_handle);
    push_queue(ST_EXCEPTION);
}

static void pg_walk(uint64_t satp, uint64_t virt, void (*memif)(hwaddr addr, void *buf, hwaddr len))
{
    if ((satp >> 60) == 0x8) {
        uint64_t pg_table = (satp & 0xfffffffff) << 12;
        unsigned int vpn0 = (virt >> 12) & 0x1ff;
        unsigned int vpn1 = (virt >> 21) & 0x1ff;
        unsigned int vpn2 = (virt >> 30) & 0x1ff;
        qemu_log("virt = 0x%lx, pg_table = %lx\n", virt, pg_table);
        qemu_log("vpn[0] = %u, vpn[1] = %u, vpn[2] = %u\n", vpn0, vpn1, vpn2);
        uint64_t page_desc;
        uint64_t pte_addr;
#if 0
        pte_addr = pg_table;
        for (int i = 0; i < 512; i++) {
            memif(pte_addr, &page_desc, 8);
            qemu_log("[%d] addr 0x%lx, lv2 page_desc  = 0x%lx\n", i, pte_addr, page_desc);
            pte_addr += 8;
        }
#endif
        pte_addr = pg_table + (vpn2 << 3);
        memif(pte_addr, &page_desc, 8);
        qemu_log("addr 0x%lx, lv2 page_desc  = 0x%lx\n", pte_addr, page_desc);
        if ((page_desc & 1) == 0)
            return;
        if ((page_desc & 8) || (page_desc & 2)) {
            pte_addr = ((page_desc >> 10) << 12);
            qemu_log("leaf pte: 0x%lx\n", pte_addr);
            return;
        }
        pte_addr = ((page_desc >> 10) << 12) + (vpn1 << 3);
        memif(pte_addr, &page_desc, 8);
        qemu_log("addr 0x%lx, lv1 page_desc  = 0x%lx\n", pte_addr, page_desc);
        if ((page_desc & 1) == 0)
            return;
        if ((page_desc & 8) || (page_desc & 2)) {
            pte_addr = ((page_desc >> 10) << 12);
            qemu_log("leaf pte: 0x%lx\n", pte_addr);
            return;
        }
        pte_addr = ((page_desc >> 10) << 12) + (vpn0 << 3);
        memif(pte_addr, &page_desc, 8);
        qemu_log("addr 0x%lx, lv0 page_desc  = 0x%lx\n", pte_addr, page_desc);
        pte_addr = ((page_desc >> 10) << 12);
        qemu_log("leaf pte: 0x%lx\n", pte_addr);
    }
}

void qemu_cosim_read_mem(hwaddr addr, void *buf, hwaddr len)
{
    uint64_t ret;
    assert(len <= 8);
    ret = cosim_read_mem(cosim_handle, addr, len);
    memcpy(buf, &ret, len);
}
