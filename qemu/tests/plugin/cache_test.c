/*
 * Copyright (C) 2024, Kaifeng Xu <kaifengx@princeton.edu>
 *
 * plugin for MindPalace
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

// Added by Kaifeng Xu
#include "hw/pqii.h"

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;
static bool track_io;
static bool do_inline;
#define MAX_UDATA_BUF_SIZE 10000
uint8_t udata_buf[MAX_UDATA_BUF_SIZE][3];
int idx_udata_buf = 0;

#define WARMUP_THRESHOLD_1 0000000000
#define WARMUP_THRESHOLD_2 1000000000
// Maximum recording of 1B instructions, can be altered

static void plugin_exit(qemu_plugin_id_t id, void *p){
}

static void plugin_init(void)
{
}

static void vcpu_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr, void *udata)
{
    if(g_pqii_data.status){
        if((g_pqii_data.icount > WARMUP_THRESHOLD_1) && (g_pqii_data.icount <= WARMUP_THRESHOLD_2)){
            struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);
            if (track_io) {
                if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
                } else {
                    // Not decide what to do yet
                    return;
                }
            } else {
                if (hwaddr && !qemu_plugin_hwaddr_is_io(hwaddr)) {
                } else {
                    // Not decide what to do yet
                    return;
                }
            }
	    }
    }
}

static void vcpu_insn_exec_before(unsigned int cpu_index, void *udata)
{
    qemu_plugin_nop(udata);
    uint8_t *nop_data = (uint8_t *)udata;
    if (nop_data[0] == (uint8_t)0xbe){
        g_pqii_data.status = 1;
    } else if (nop_data[0] == (uint8_t)0xed){
        g_pqii_data.status = 0;
    }
}

uint64_t tmp_vaddr, tmp_paddr;

static void vcpu_insn_get_vaddr(unsigned int cpu_index, void *udata)
{
	tmp_vaddr = (uint64_t) udata;
}

static void vcpu_insn_get_paddr(unsigned int cpu_index, void *udata)
{
	tmp_paddr = (uint64_t) udata;
}

static void vcpu_insn_exec_normal(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 0, 0, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_br(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 1, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_dir_jmp(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 2, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_indir_jmp(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 3, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_dir_call(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 4, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_indir_call(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 5, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_ret(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 6, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_insn_exec_others(unsigned int cpu_index, void *udata)
{
    if(g_pqii_data.status){
        g_pqii_data.icount ++;
        if(g_pqii_data.icount <= WARMUP_THRESHOLD_2){
            if(g_pqii_data.icount > WARMUP_THRESHOLD_1)
		        qemu_plugin_get_cpuinfo(tmp_vaddr, tmp_paddr, 7, (uint64_t)udata, g_pqii_data.icount);
        } else
            g_pqii_data.status = false;
	}
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        const guint8 *insn_data = qemu_plugin_insn_data(insn);
		if (  ( insn_data[0] == (guint8)0x0f ) 
           && ( insn_data[1] == (guint8)0x1f ) 
           && ( insn_data[2] == (guint8)0x84 )
           && ( (insn_data[3] == (guint8)0xbe) || (insn_data[3] == (guint8)0xed) || (insn_data[3] == (guint8)0xac) ) 
           ) {
            udata_buf[idx_udata_buf][0] = insn_data[3];
            udata_buf[idx_udata_buf][1] = insn_data[6];
            udata_buf[idx_udata_buf][2] = insn_data[7];
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, (void *)(&udata_buf[idx_udata_buf]) );
            idx_udata_buf++;
            if(idx_udata_buf >= MAX_UDATA_BUF_SIZE) idx_udata_buf = 0;
        } else {
            uint64_t vaddr = qemu_plugin_insn_vaddr(insn);
            uint64_t paddr = qemu_plugin_insn_paddr(insn);
            int is_br_jmp = qemu_plugin_insn_is_br_jmp(insn);
            uint64_t target_vaddr = qemu_plugin_insn_target_vaddr(insn);
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_get_vaddr, QEMU_PLUGIN_CB_NO_REGS, (void *)vaddr);
            qemu_plugin_register_vcpu_insn_exec_cb(
                insn, vcpu_insn_get_paddr, QEMU_PLUGIN_CB_NO_REGS, (void *)paddr);
            switch(is_br_jmp){
                case 0:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_normal, QEMU_PLUGIN_CB_NO_REGS, NULL);
                    break;
                case 1:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_br, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 2:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_dir_jmp, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 3:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_indir_jmp, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 4:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_dir_call, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 5:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_indir_call, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 6:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_ret, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                case 7:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_others, QEMU_PLUGIN_CB_NO_REGS, (void *)target_vaddr);
                    break;
                default:
                    qemu_plugin_register_vcpu_insn_exec_cb(
                        insn, vcpu_insn_exec_normal, QEMU_PLUGIN_CB_NO_REGS, NULL);
                    break;
            }
            qemu_plugin_register_vcpu_mem_cb(insn, vcpu_haddr,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, NULL);
        }
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
