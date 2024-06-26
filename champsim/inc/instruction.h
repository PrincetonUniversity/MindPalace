#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "circular_buffer.hpp"
#include "trace_instruction.h"
#include "qemutrace.h"

// special registers that help us identify branches
#define REG_STACK_POINTER 6
#define REG_FLAGS 25
#define REG_INSTRUCTION_POINTER 26

// branch types
#define NOT_BRANCH 0
#define BRANCH_DIRECT_JUMP 1
#define BRANCH_INDIRECT 2
#define BRANCH_CONDITIONAL 3
#define BRANCH_DIRECT_CALL 4
#define BRANCH_INDIRECT_CALL 5
#define BRANCH_RETURN 6
#define BRANCH_OTHER 7

struct ooo_model_instr {
  uint64_t instr_id = 0, ip = 0, event_cycle = 0;

  bool is_branch = 0, is_memory = 0, branch_taken = 0, branch_mispredicted = 0, source_added[NUM_INSTR_SOURCES] = {},
       destination_added[NUM_INSTR_DESTINATIONS_SPARC] = {};

  // Added by Kaifeng Xu
  bool is_btb_miss = 0;
  bool is_kernel = true;
  // End

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint8_t branch_type = NOT_BRANCH;
  uint64_t branch_target = 0;

  uint8_t translated = 0, fetched = 0, decoded = 0, scheduled = 0, executed = 0;
  int num_reg_ops = 0, num_mem_ops = 0, num_reg_dependent = 0;

  uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers

  uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers

  // these are indices of instructions in the ROB that depend on me
  std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> registers_instrs_depend_on_me, memory_instrs_depend_on_me;

  // memory addresses that may cause dependencies between instructions
  uint64_t instruction_pa = 0;
  uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  uint64_t source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_SOURCES> lq_index = {};
  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_DESTINATIONS_SPARC> sq_index = {};

  ooo_model_instr() = default;

  ooo_model_instr(uint8_t cpu, input_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;
    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    asid[0] = cpu;
    asid[1] = cpu;
  }

  ooo_model_instr(uint8_t cpu, cloudsuite_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;
    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
  }

  ooo_model_instr(uint8_t cpu, QEMU_trace_insn instr)
  {
    this->ip = instr.vaddr;
    this->is_branch = (instr.br_type > 0); // not decided here
    this->branch_type = instr.br_type;
    this->branch_target = instr.target_vaddr;
    // this->branch_taken = ?; not decided here

    this->asid[0] = (instr.cr3 >> 12) & 0xff;
    this->asid[1] = (instr.cr3 >> 20) & 0xff;
    this->is_kernel = (instr.seg_states < 3);
  }

  ooo_model_instr(uint8_t cpu, QEMU_trace_insn instr, QEMU_trace_data trace_data)
  {
    this->ip = instr.vaddr;
    this->is_branch = (instr.br_type > 0); // not decided here
    this->branch_type = instr.br_type;
    this->branch_target = instr.target_vaddr;
    
    // 0 load, 1 store
    if (trace_data.load_store) {
      this->destination_memory[0] = trace_data.vaddr;
    } else {
      this->source_memory[0] = trace_data.vaddr;
    }

    this->asid[0] = (instr.cr3 >> 12) & 0xff;
    this->asid[1] = (instr.cr3 >> 20) & 0xff;
    this->is_kernel = (instr.seg_states < 3);
  }

};

#endif
