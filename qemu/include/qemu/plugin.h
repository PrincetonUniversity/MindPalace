/*
 * Copyright (C) 2017, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#ifndef QEMU_PLUGIN_H
#define QEMU_PLUGIN_H

#include "qemu/config-file.h"
#include "qemu/qemu-plugin.h"
#include "qemu/error-report.h"
#include "qemu/queue.h"
#include "qemu/option.h"

/*
 * Events that plugins can subscribe to.
 */
enum qemu_plugin_event {
    QEMU_PLUGIN_EV_VCPU_INIT,
    QEMU_PLUGIN_EV_VCPU_EXIT,
    QEMU_PLUGIN_EV_VCPU_TB_TRANS,
    QEMU_PLUGIN_EV_VCPU_IDLE,
    QEMU_PLUGIN_EV_VCPU_RESUME,
    QEMU_PLUGIN_EV_VCPU_SYSCALL,
    QEMU_PLUGIN_EV_VCPU_SYSCALL_RET,
    QEMU_PLUGIN_EV_FLUSH,
    QEMU_PLUGIN_EV_ATEXIT,
    QEMU_PLUGIN_EV_MAX, /* total number of plugin events we support */
};

/*
 * Option parsing/processing.
 * Note that we can load an arbitrary number of plugins.
 */
struct qemu_plugin_desc;
typedef QTAILQ_HEAD(, qemu_plugin_desc) QemuPluginList;

#ifdef CONFIG_PLUGIN
extern QemuOptsList qemu_plugin_opts;

static inline void qemu_plugin_add_opts(void)
{
    qemu_add_opts(&qemu_plugin_opts);
}

void qemu_plugin_opt_parse(const char *optarg, QemuPluginList *head);
int qemu_plugin_load_list(QemuPluginList *head);

union qemu_plugin_cb_sig {
    qemu_plugin_simple_cb_t          simple;
    qemu_plugin_udata_cb_t           udata;
    qemu_plugin_vcpu_simple_cb_t     vcpu_simple;
    qemu_plugin_vcpu_udata_cb_t      vcpu_udata;
    qemu_plugin_vcpu_tb_trans_cb_t   vcpu_tb_trans;
    qemu_plugin_vcpu_mem_cb_t        vcpu_mem;
    qemu_plugin_vcpu_syscall_cb_t    vcpu_syscall;
    qemu_plugin_vcpu_syscall_ret_cb_t vcpu_syscall_ret;
    void *generic;
};

enum plugin_dyn_cb_type {
    PLUGIN_CB_INSN,
    PLUGIN_CB_MEM,
    PLUGIN_N_CB_TYPES,
};

enum plugin_dyn_cb_subtype {
    PLUGIN_CB_REGULAR,
    PLUGIN_CB_INLINE,
    PLUGIN_N_CB_SUBTYPES,
};

/*
 * A dynamic callback has an insertion point that is determined at run-time.
 * Usually the insertion point is somewhere in the code cache; think for
 * instance of a callback to be called upon the execution of a particular TB.
 */
struct qemu_plugin_dyn_cb {
    union qemu_plugin_cb_sig f;
    void *userp;
    unsigned tcg_flags;
    enum plugin_dyn_cb_subtype type;
    /* @rw applies to mem callbacks only (both regular and inline) */
    enum qemu_plugin_mem_rw rw;
    /* fields specific to each dyn_cb type go here */
    union {
        struct {
            enum qemu_plugin_op op;
            uint64_t imm;
        } inline_insn;
    };
};

struct qemu_plugin_insn {
    GByteArray *data;
    uint64_t vaddr;
    // Added by Kaifeng Xu
    uint64_t paddr;
    void *haddr;
    GArray *cbs[PLUGIN_N_CB_TYPES][PLUGIN_N_CB_SUBTYPES];
    bool calls_helpers;
    bool mem_helper;
    // Added by Kaifeng Xu
    // 0: not branch or jump
    // 1: branch instruction
    // 2: direct jump instruction 
    // 3: indirect jump instruction
    // 4: direct call
    // 5: indirect call
    // 6: ret
    // 7: others (interrupts, interrupt returns, etc.)
    int is_br_jmp;
    uint64_t target_vaddr;
};

/*
 * qemu_plugin_insn allocate and cleanup functions. We don't expect to
 * cleanup many of these structures. They are reused for each fresh
 * translation.
 */

static inline void qemu_plugin_insn_cleanup_fn(gpointer data)
{
    struct qemu_plugin_insn *insn = (struct qemu_plugin_insn *) data;
    g_byte_array_free(insn->data, true);
}

static inline struct qemu_plugin_insn *qemu_plugin_insn_alloc(void)
{
    int i, j;
    struct qemu_plugin_insn *insn = g_new0(struct qemu_plugin_insn, 1);
    insn->data = g_byte_array_sized_new(4);

    for (i = 0; i < PLUGIN_N_CB_TYPES; i++) {
        for (j = 0; j < PLUGIN_N_CB_SUBTYPES; j++) {
            insn->cbs[i][j] = g_array_new(false, false,
                                          sizeof(struct qemu_plugin_dyn_cb));
        }
    }
    return insn;
}

struct qemu_plugin_tb {
    GPtrArray *insns;
    size_t n;
    uint64_t vaddr;
    uint64_t vaddr2;
    // Added by Kaifeng Xu
    uint64_t paddr1;
    uint64_t paddr2;
    void *haddr1;
    void *haddr2;
    GArray *cbs[PLUGIN_N_CB_SUBTYPES];
};

/**
 * qemu_plugin_tb_insn_get(): get next plugin record for translation.
 *
 */
static inline
struct qemu_plugin_insn *qemu_plugin_tb_insn_get(struct qemu_plugin_tb *tb)
{
    struct qemu_plugin_insn *insn;
    int i, j;

    if (unlikely(tb->n == tb->insns->len)) {
        struct qemu_plugin_insn *new_insn = qemu_plugin_insn_alloc();
        g_ptr_array_add(tb->insns, new_insn);
    }
    insn = g_ptr_array_index(tb->insns, tb->n++);
    g_byte_array_set_size(insn->data, 0);
    insn->calls_helpers = false;
    insn->mem_helper = false;

    for (i = 0; i < PLUGIN_N_CB_TYPES; i++) {
        for (j = 0; j < PLUGIN_N_CB_SUBTYPES; j++) {
            g_array_set_size(insn->cbs[i][j], 0);
        }
    }

    return insn;
}

void qemu_plugin_vcpu_init_hook(CPUState *cpu);
void qemu_plugin_vcpu_exit_hook(CPUState *cpu);
void qemu_plugin_tb_trans_cb(CPUState *cpu, struct qemu_plugin_tb *tb);
void qemu_plugin_vcpu_idle_cb(CPUState *cpu);
void qemu_plugin_vcpu_resume_cb(CPUState *cpu);
void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1,
                         uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6, uint64_t a7, uint64_t a8);
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret);

void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr, uint32_t meminfo);

void qemu_plugin_flush_cb(void);

void qemu_plugin_atexit_cb(void);

void qemu_plugin_add_dyn_cb_arr(GArray *arr);

void qemu_plugin_disable_mem_helpers(CPUState *cpu);

#else /* !CONFIG_PLUGIN */

static inline void qemu_plugin_add_opts(void)
{ }

static inline void qemu_plugin_opt_parse(const char *optarg,
                                         QemuPluginList *head)
{
    error_report("plugin interface not enabled in this build");
    exit(1);
}

static inline int qemu_plugin_load_list(QemuPluginList *head)
{
    return 0;
}

static inline void qemu_plugin_vcpu_init_hook(CPUState *cpu)
{ }

static inline void qemu_plugin_vcpu_exit_hook(CPUState *cpu)
{ }

static inline void qemu_plugin_tb_trans_cb(CPUState *cpu,
                                           struct qemu_plugin_tb *tb)
{ }

static inline void qemu_plugin_vcpu_idle_cb(CPUState *cpu)
{ }

static inline void qemu_plugin_vcpu_resume_cb(CPUState *cpu)
{ }

static inline void
qemu_plugin_vcpu_syscall(CPUState *cpu, int64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6,
                         uint64_t a7, uint64_t a8)
{ }

static inline
void qemu_plugin_vcpu_syscall_ret(CPUState *cpu, int64_t num, int64_t ret)
{ }

static inline void qemu_plugin_vcpu_mem_cb(CPUState *cpu, uint64_t vaddr,
                                           uint32_t meminfo)
{ }

static inline void qemu_plugin_flush_cb(void)
{ }

static inline void qemu_plugin_atexit_cb(void)
{ }

static inline
void qemu_plugin_add_dyn_cb_arr(GArray *arr)
{ }

static inline void qemu_plugin_disable_mem_helpers(CPUState *cpu)
{ }

#endif /* !CONFIG_PLUGIN */

#endif /* QEMU_PLUGIN_H */
