/*
 * Copyright (C) 2017, Emilio G. Cota <cota@braap.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 *
 * plugin-gen.h - TCG-dependent definitions for generating plugin code
 *
 * This header should be included only from plugin.c and C files that emit
 * TCG code.
 */
#ifndef QEMU_PLUGIN_GEN_H
#define QEMU_PLUGIN_GEN_H

#include "qemu/plugin.h"
#include "tcg/tcg.h"

struct DisasContextBase;

#ifdef CONFIG_PLUGIN

bool plugin_gen_tb_start(CPUState *cpu, const TranslationBlock *tb);
void plugin_gen_tb_end(CPUState *cpu);
void plugin_gen_insn_start(CPUState *cpu, const struct DisasContextBase *db);
// Changed by Kaifeng Xu
void plugin_gen_insn_end(CPUState *cpu, const struct DisasContextBase *db);

void plugin_gen_disable_mem_helpers(void);
void plugin_gen_empty_mem_callback(TCGv addr, uint32_t info);

static inline void plugin_insn_append(const void *from, size_t size)
{
    struct qemu_plugin_insn *insn = tcg_ctx->plugin_insn;

    if (insn == NULL) {
        return;
    }

    insn->data = g_byte_array_append(insn->data, from, size);
}

#else /* !CONFIG_PLUGIN */

static inline
bool plugin_gen_tb_start(CPUState *cpu, const TranslationBlock *tb)
{
    return false;
}

static inline
void plugin_gen_insn_start(CPUState *cpu, const struct DisasContextBase *db)
{ }

// Changed by Kaifeng Xu
static inline void plugin_gen_insn_end(CPUState *cpu, const struct DisasContextBase *db)
{ }

static inline void plugin_gen_tb_end(CPUState *cpu)
{ }

static inline void plugin_gen_disable_mem_helpers(void)
{ }

static inline void plugin_gen_empty_mem_callback(TCGv addr, uint32_t info)
{ }

static inline void plugin_insn_append(const void *from, size_t size)
{ }

#endif /* CONFIG_PLUGIN */

#endif /* QEMU_PLUGIN_GEN_H */

