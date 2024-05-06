#include "tracereader.h"
#include "qemutrace.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

tracereader::tracereader(uint8_t cpu, std::string _ts) : cpu(cpu), trace_string(_ts)
{
  std::string last_dot = trace_string.substr(trace_string.find_last_of("."));

  if (trace_string.substr(0, 4) == "http") {
    // Check file exists
    char testfile_command[4096];
    sprintf(testfile_command, "wget -q --spider %s", trace_string.c_str());
    FILE* testfile = popen(testfile_command, "r");
    if (pclose(testfile)) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "wget -qO- -o /dev/null %2$s | %1$s -dc";
  } else {
    std::ifstream testfile(trace_string);
    if (!testfile.good()) {
      std::cerr << "TRACE FILE NOT FOUND" << std::endl;
      assert(0);
    }
    cmd_fmtstr = "%1$s -dc %2$s";
  }

  if (last_dot[1] == 'g') // gzip format
    decomp_program = "gzip";
  else if (last_dot[1] == 'x') // xz
    decomp_program = "xz";
  else if (last_dot[1] == 't') // QEMU trace format, added by Kaifeng Xu
    decomp_program = "trace";
  else {
    std::cout << "ChampSim does not support traces other than gz or xz compression!" << std::endl;
    assert(0);
  }

  open(trace_string);
}

tracereader::~tracereader() { close(); }

template <typename T>
ooo_model_instr tracereader::read_single_instr()
{
  T trace_read_instr;

  while (!fread(&trace_read_instr, sizeof(T), 1, trace_file)) {
    // reached end of file for this trace
    std::cout << "*** Reached end of trace: " << trace_string << std::endl;

    // close the trace file and re-open it
    close();
    open(trace_string);
  }

  // Added by Kaifeng Xu
  // std::cout << "ip: " << std::hex << trace_read_instr.ip << ", is_branch: " << std::dec << trace_read_instr.is_branch << std::endl;
  // copy the instruction into the performance model's instruction format
  ooo_model_instr retval(cpu, trace_read_instr);
  return retval;
}


void tracereader::open(std::string trace_string)
{
  // Changed by Kaifeng Xu
  // Simplified trace format without compression
  if (decomp_program != "trace"){
    char gunzip_command[4096];
    sprintf(gunzip_command, cmd_fmtstr.c_str(), decomp_program.c_str(), trace_string.c_str());
    trace_file = popen(gunzip_command, "r");
  } else {
    trace_file = fopen(trace_string.c_str(), "rb");
  }
  if (trace_file == NULL) {
    std::cerr << std::endl << "*** CANNOT OPEN TRACE FILE: " << trace_string << " ***" << std::endl;
    assert(0);
  }
  if (decomp_program == "trace"){
    QEMU_tracefile_header header;
    fread(&header, sizeof(QEMU_tracefile_header), 1, trace_file);
    std::cout << "Trace Header:" << header.header_event_id << "," << header.header_magic << "," << header.header_version << std::endl;

    // Skip trace event mappings in trace file, totally 134046 bytes, remember to change this if trace events changes
    char event_mappings[134081];
    fread(&event_mappings, 134081, 1, trace_file);
    // Check first event
    QEMU_event_header event_header;
    fread(&event_header, sizeof(QEMU_event_header), 1, trace_file);
    QEMU_trace_nop trace_nop;
    fread(&trace_nop, sizeof(QEMU_trace_nop), 1, trace_file);
    std::cout << "Marker: " << trace_nop.byte0 << " " << trace_nop.byte1 << " " << trace_nop.byte2 << std::endl;
    // first event should be begin marker
    assert((event_header.event == EVENT_ID_NOP) && (trace_nop.byte0 == 0xbe));
    // Follow that begin marker, there should be an instruction event
    fread(&event_header, sizeof(QEMU_event_header), 1, trace_file);
    assert(event_header.event == EVENT_ID_INSN);
  }
  // End Kaifeng Xu
}

void tracereader::close()
{
  if (trace_file != NULL) {
    pclose(trace_file);
  }
}

class qemu_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;
  ooo_model_instr read_single_instr_qemutrace();

public:
  qemu_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr_qemutrace();

    if (!initialized) {
      last_instr = trace_read_instr;
      trace_read_instr = read_single_instr_qemutrace();
      initialized = true;
    }

    // placeholder: if branch taken
    // Add translation: Kaifeng Xu
    switch(last_instr.branch_type){
      case QEMU_OPTYPE_OP:
        last_instr.is_branch = 0;
        last_instr.branch_taken = 0;
        last_instr.branch_type = NOT_BRANCH;
        last_instr.branch_target = 0;
        break;
      case QEMU_OPTYPE_BRANCH:
        last_instr.is_branch = 1;
        last_instr.branch_taken = (last_instr.branch_target == trace_read_instr.ip);
        last_instr.branch_type = BRANCH_CONDITIONAL;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.source_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.source_registers[1] = REG_FLAGS;
        break;
      case QEMU_OPTYPE_JMP_DIRECT:
        last_instr.is_branch = 1;
        last_instr.branch_taken = 1;
        last_instr.branch_type = BRANCH_DIRECT_JUMP;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        break;
      case QEMU_OPTYPE_JMP_INDIRECT:
        last_instr.is_branch = 1;
        last_instr.branch_taken = 1;
        last_instr.branch_type = BRANCH_INDIRECT;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.source_registers[0] = 50;
        break;
      case QEMU_OPTYPE_CALL_DIRECT:
        last_instr.is_branch = 1;
        last_instr.branch_taken = 1;
        last_instr.branch_type = BRANCH_DIRECT_CALL;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.destination_registers[1] = REG_STACK_POINTER;
        last_instr.source_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.source_registers[1] = REG_STACK_POINTER;
        break;
      case QEMU_OPTYPE_CALL_INDIRECT:
        last_instr.is_branch = 1;
        last_instr.branch_taken = 1;
        last_instr.branch_type = BRANCH_INDIRECT_CALL;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.destination_registers[1] = REG_STACK_POINTER;
        last_instr.source_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.source_registers[1] = REG_STACK_POINTER;
        last_instr.source_registers[2] = 50;
        break;
      case QEMU_OPTYPE_RET:
        last_instr.is_branch = 1;
        last_instr.branch_taken = 1;
        last_instr.branch_type = BRANCH_RETURN;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        last_instr.destination_registers[1] = REG_STACK_POINTER;
        last_instr.source_registers[0] = REG_STACK_POINTER;
        break;
      case QEMU_OPTYPE_OTHERS:
        last_instr.is_branch = 1;
        last_instr.branch_taken = (last_instr.branch_target == trace_read_instr.ip);
        last_instr.branch_type = BRANCH_OTHER;
        last_instr.destination_registers[0] = REG_INSTRUCTION_POINTER;
        break;
      default:
        last_instr.is_branch = 0;
        last_instr.branch_taken = 0;
        last_instr.branch_type = NOT_BRANCH;
        last_instr.branch_target = 0;
    }
    // last_instr.branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr;

    last_instr = trace_read_instr;
    return retval;
  }
};

ooo_model_instr qemu_tracereader::read_single_instr_qemutrace()
{
  QEMU_event_header header;
  QEMU_trace_insn trace_read_instr;

  if (!fread(&trace_read_instr, sizeof(QEMU_trace_insn), 1, trace_file)) {
    // reached end of file for this trace
    std::cout << "*** Reached end of trace: " << trace_string << std::endl;
    exit(1);
  }

  // Read next line and see if this is a r/w
  if (!fread(&header, sizeof(QEMU_event_header), 1, trace_file)) {
    // reached end of file for this trace
    std::cout << "*** Reached end of trace: " << trace_string << std::endl;
    exit(1);
  }

  // if this is a data trace
  if (header.event == EVENT_ID_DATA) {
    QEMU_trace_data trace_data;
    if (!fread(&trace_data, sizeof(QEMU_trace_data), 1, trace_file)) {
      // reached end of file for this trace
      std::cout << "*** Reached end of trace: " << trace_string << std::endl;
      exit(1);
    }

    // Read until next instruction
    while(true){
      if (!fread(&header, sizeof(QEMU_event_header), 1, trace_file)) {
        // reached end of file for this trace
        std::cout << "*** Reached end of trace: " << trace_string << std::endl;
        exit(1);
      }
      if (header.event == EVENT_ID_INSN) {
          break;
      } else if (header.event == EVENT_ID_DATA) {
        QEMU_trace_data trace_data;
        if (!fread(&trace_data, sizeof(QEMU_trace_data), 1, trace_file)) {
          // reached end of file for this trace
          std::cout << "*** Reached end of trace: " << trace_string << std::endl;
          exit(1);
        }
      } else if (header.event == EVENT_ID_NOP) {
        QEMU_trace_nop trace_nop;
        if (!fread(&trace_nop, sizeof(QEMU_trace_nop), 1, trace_file)) {
          // reached end of file for this trace
          std::cout << "*** Reached end of trace: " << trace_string << std::endl;
          exit(1);
        }
        std::cout << "Marker: " << trace_nop.byte0 << " " << trace_nop.byte1 << " " << trace_nop.byte2 << std::endl;
      } else {
        assert(0); // should not reach this
      }
    }

    // Return
    ooo_model_instr retval(cpu, trace_read_instr, trace_data);
    return retval;
  } else if (header.event == EVENT_ID_NOP) {
    // Handle marker instructions
    QEMU_trace_nop trace_nop;
    if (!fread(&trace_nop, sizeof(QEMU_trace_nop), 1, trace_file)) {
      // reached end of file for this trace
      std::cout << "*** Reached end of trace: " << trace_string << std::endl;
      exit(1);
    }
    std::cout << "Marker: " << trace_nop.byte0 << " " << trace_nop.byte1 << " " << trace_nop.byte2 << std::endl;
    // See if next trace line is instruction
    if (!fread(&header, sizeof(QEMU_event_header), 1, trace_file)) {
      // reached end of file for this trace
      std::cout << "*** Reached end of trace: " << trace_string << std::endl;
      exit(1);
    }
    assert(header.event == EVENT_ID_INSN);
  } else {
    assert(header.event == EVENT_ID_INSN); // should be insn, or bug
  }

  // if this is an instruction trace
  ooo_model_instr retval(cpu, trace_read_instr);
  return retval;

}

class cloudsuite_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

public:
  cloudsuite_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<cloudsuite_instr>();

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
    }

    last_instr.branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr;

    last_instr = trace_read_instr;
    return retval;
  }
};

class input_tracereader : public tracereader
{
  ooo_model_instr last_instr;
  bool initialized = false;

public:
  input_tracereader(uint8_t cpu, std::string _tn) : tracereader(cpu, _tn) {}

  ooo_model_instr get()
  {
    ooo_model_instr trace_read_instr = read_single_instr<input_instr>();

    if (!initialized) {
      last_instr = trace_read_instr;
      initialized = true;
    }

    last_instr.branch_target = trace_read_instr.ip;
    ooo_model_instr retval = last_instr;

    last_instr = trace_read_instr;
    return retval;
  }
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
  // Added by Kaifeng Xu
  std::string last_dot = fname.substr(fname.find_last_of("."));
  if (last_dot[1] == 't') // QEMU trace format, added by Kaifeng Xu
      return new qemu_tracereader(cpu, fname);

  if (is_cloudsuite) {
    return new cloudsuite_tracereader(cpu, fname);
  } else {
    return new input_tracereader(cpu, fname);
  }
}
