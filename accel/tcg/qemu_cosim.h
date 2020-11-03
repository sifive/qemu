#ifndef QEMU_COSIM_H
#define QEMU_COSIM_H

//#define DEBUG
//#define DEBUG_COUNTER (0xabd74b0)
//#define DEBUG_COUNTER (0xac47000)
#define DEBUG_COUNTER (0)
//#define DEBUG_COUNTER (0x33070)
//#define DEBUG_COUNTER (0x27b9400)
//#define DEBUG_COUNTER (0x5608600)
//#define DEBUG_COUNTER (0x27a7600)
extern uint64_t debug_counter;
extern int qemu_cosim_sync_ignored;

enum stage_t {
    ST_IGNORE,
    ST_INSN_START,
    ST_END_OF_BLOCK,
    ST_INTERRUPT,
    ST_EXCEPTION,
};
const char *stage_str(enum stage_t stage);

void qemu_cosim_assert(void);
void qemu_cosim_init(const char *elffile, int is_bin, void *dtb, int dtb_sz);
void qemu_cosim_step(enum stage_t stage);
void qemu_cosim_cmp(void);
void qemu_cosim_ignore_next(void);
void qemu_cosim_sync(enum stage_t stage);
void qemu_cosim_mmioload_record(uint64_t addr, size_t len, uint64_t *value);
void qemu_cosim_update_mip(uint64_t value);
void qemu_cosim_enter_interrupt(void);
void qemu_cosim_enter_exception(void);
#endif // QEMU_COSIM_H
