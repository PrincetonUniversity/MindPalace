/*
 * =====================================================================================
 *
 *       Filename:  qemutrace.h
 *
 *    Description:  QEMU trace format
 *
 *        Version:  1.0
 *        Created:  04/23/2023 01:37:39 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Kaifeng Xu (kx), kaifengx@princeton.edu
 *   Organization:  Princeton University
 *
 * =====================================================================================
 */
#ifndef QEMU_TRACE_H
#define QEMU_TRACE_H

#define EVENT_ID_INSN 63
#define EVENT_ID_DATA 64
#define EVENT_ID_NOP 65

struct QEMU_tracefile_header {
    uint64_t header_event_id;
    uint64_t header_magic;
    uint64_t header_version;
};

struct QEMU_event_header {
    uint64_t type;
    uint64_t event;
    uint64_t timestamp_ns;
    uint32_t length;
    uint32_t pid;
};

struct QEMU_trace_insn {
    uint64_t icount;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t seg_states;
    uint64_t cr3;
    uint64_t br_type;
    uint64_t target_vaddr;
};

struct QEMU_trace_data {
    uint64_t icount;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t load_store;
    uint64_t length;
    uint64_t seg_states;
    uint64_t cr3;
};

struct QEMU_trace_nop {
    uint64_t byte0;
    uint64_t byte1;
    uint64_t byte2;
};

// Added by Kaifeng Xu
// branch types from x86 QEMU trace
typedef enum {
    QEMU_OPTYPE_OP  =0,
    QEMU_OPTYPE_BRANCH,  // jump condition
    QEMU_OPTYPE_JMP_DIRECT,
    QEMU_OPTYPE_JMP_INDIRECT,
    QEMU_OPTYPE_CALL_DIRECT,
    QEMU_OPTYPE_CALL_INDIRECT,
    QEMU_OPTYPE_RET,
    QEMU_OPTYPE_OTHERS
} QemuOpType;

#endif
