BIN=./champsim/bin/ # binary name
TRACE= # PATH/to/Trace
OUTPUT_DIR= # PATH/to/Output
WARMUP_INSN=000000000
SIM_INSN=1000000000 # default 1B instructions

$BIN --warmup_instructions ${WARMUP_INSN} --simulation_instructions ${SIM_INSN} -c ${TRACE}  > log.out
