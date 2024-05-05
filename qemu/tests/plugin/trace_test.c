/*
 * Copyright (C) 2019, Alex Bennée <alex.bennee@linaro.org>
 *
 * Hot Pages - show which pages saw the most memory accesses.
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

// Kaifeng Xu
#include "hw/pqii.h"
// #include "qemu/plugin.h"
// #include "qemu/config-file.h"
// #include "qemu/log.h";

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

struct qemu_plugin_insn {
    GByteArray *data;
    uint64_t vaddr;
		uint64_t paddr;
    void *haddr;
    GArray *cbs[PLUGIN_N_CB_TYPES][PLUGIN_N_CB_SUBTYPES];
    bool calls_helpers;
    bool mem_helper;
};

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;
static bool track_io;
static bool do_inline;
#define MAX_UDATA_BUF_SIZE 10000
uint8_t udata_buf[MAX_UDATA_BUF_SIZE][2];
int idx_udata_buf = 0;
#define WARMUP_THRESHOLD_1 0
#define WARMUP_THRESHOLD_2 1000000000

static void plugin_exit(qemu_plugin_id_t id, void *p){
}

static void plugin_init(void)
{
}

static void vcpu_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr, void *udata)
{
    if(g_pqii_data.status && (g_pqii_data.icount > WARMUP_THRESHOLD_1) && (g_pqii_data.icount <= WARMUP_THRESHOLD_2)){
        struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);

        /* We only get a hwaddr for system emulation */
        if (track_io) {
            if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
                // Not decide what to do yet
            } else {
                return;
            }
        }
	}
}

static void vcpu_insn_exec_before(unsigned int cpu_index, void *udata)
{
    g_autoptr(GString) res = g_string_new("*** Kaifeng:,I5,");
    g_string_append_printf(res, "val,%x,%x\n", *(uint8_t *)udata, ((uint8_t *)udata)[1] );
    qemu_log_plugin( res->str );
    // g_string_append_printf(res, "val,%x\n", (unsigned int)(((GByteArray *)udata)->data)[3] );
    // g_string_append_printf(res, "val,%x,%x,%x,%x,%x,%x\n", (unsigned int)(insn->data->data)[0], (unsigned int)(insn->data->data)[1],(unsigned int)(insn->data->data)[2],
    //						                                            (unsigned int)(insn->data->data)[3], (unsigned int)(insn->data->data)[4], (unsigned int)(insn->data->data)[5]);
    // qemu_plugin_outs( res->str );
    // if(g_pqii_data.status){
    //     g_pqii_data.icount++;
    // }
    // qemu_log("*** Kaifeng:,I5,val,%x\n", (unsigned int)(((GByteArray *)udata)->data)[0]);
}

uint64_t tmp_vaddr;
static void vcpu_insn_get_vaddr(unsigned int cpu_index, void *udata)
{
	tmp_vaddr = (uint64_t) udata;
}

static void vcpu_insn_exec_normal(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if((g_pqii_data.icount > WARMUP_THRESHOLD_1) && (g_pqii_data.icount <= WARMUP_THRESHOLD_2)){
            uint64_t insn_paddr = (uint64_t) udata; 
	        // qemu_plugin_meminfo_t meminfo = 0;
		    qemu_plugin_get_cpuinfo(tmp_vaddr, insn_paddr, g_pqii_data.icount);
            // g_autoptr(GString) res = g_string_new("*** Kaifeng:,I6,paddr,");
            // g_string_append_printf(res, "%#016"PRIx64"\n", insn_paddr);
            // qemu_log_plugin( res->str );
        }
	}
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
		if( ( (insn->data->data)[0] == (guint8)0x0f ) 
           && ( (insn->data->data)[1] == (guint8)0x1f ) 
           && ( (insn->data->data)[2] == (guint8)0x84 )
           && ( (insn->data->data)[3] == (guint8)0xac ) ) {
              udata_buf[idx_udata_buf][0] = (insn->data->data)[6];
              udata_buf[idx_udata_buf][1] = (insn->data->data)[7];
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, (void *)(&udata_buf[idx_udata_buf]) );
            idx_udata_buf++;
            if(idx_udata_buf >= MAX_UDATA_BUF_SIZE) idx_udata_buf = 0;
        } else {
            //qemu_plugin_register_vcpu_insn_exec_inline(
            //    insn, QEMU_PLUGIN_INLINE_ADD_U64, &g_pqii_data.icount, 1);
            //
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_get_vaddr, QEMU_PLUGIN_CB_NO_REGS, (void *)(insn->vaddr));
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_exec_normal, QEMU_PLUGIN_CB_NO_REGS, (void *)(insn->paddr));
            qemu_plugin_register_vcpu_mem_cb(insn, vcpu_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, NULL);
        }
/*        if (do_inline) {
            qemu_plugin_register_vcpu_insn_exec_inline(
                insn, QEMU_PLUGIN_INLINE_ADD_U64, &g_pqii_data.icount, 1);
        } else {
            qemu_plugin_register_vcpu_mem_cb(insn, vcpu_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, NULL);
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, NULL);
        }*/
    }
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i;

    for (i = 0; i < argc; i++) {
        char *opt = argv[i];
        if (g_strcmp0(opt, "io") == 0) {
            track_io = true;
        } else if (g_strcmp0(opt, "inline") == 0) {
            do_inline = true;
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

    plugin_init();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
