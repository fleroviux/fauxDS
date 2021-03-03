/*
 * Copyright (C) 2020 fleroviux
 */

#pragma once

#include <array>
#include <util/log.hpp>
#include <fstream>
#include <vector>

#include "coprocessor.hpp"
#include "state.hpp"
#include "memory.hpp"

namespace Duality::core::arm {

struct ARM {
  // TODO: it would be more appropriate to have this be the processor model.
  enum class Architecture {
    ARMv4T,
    ARMv5TE
  };

  ARM(Architecture arch, MemoryBase* memory)
      : arch(arch)
      , memory(memory) {
    BuildConditionTable();
    Reset();
  }

  auto ExceptionBase() -> u32 const { return exception_base; }
  void ExceptionBase(u32 base) { exception_base = base; } 

  void Reset();
  void Run(int instructions);
  void AttachCoprocessor(uint id, Coprocessor* coprocessor);
  auto IRQLine() -> bool& { return irq_line; }
  void WaitForIRQ() { wait_for_irq = true; }
  bool IsWaitingForIRQ() { return wait_for_irq; }

  // TODO: implement a cleaner interface to modify the execution state.
  auto GetState() -> State& { return state; }
  void SetPC(u32 value) {
    state.r15 = value;
    if (state.cpsr.f.thumb) {
      ReloadPipeline16();
    } else {
      ReloadPipeline32();
    }
  }

  typedef void (ARM::*Handler16)(u16);
  typedef void (ARM::*Handler32)(u32);
private:
  friend struct TableGen;
  
  static auto GetRegisterBankByMode(Mode mode) -> Bank;

  void SignalIRQ();
  void SwitchMode(Mode new_mode);
  void ReloadPipeline16();
  void ReloadPipeline32();
  void BuildConditionTable();
  bool CheckCondition(Condition condition);

  #include "handlers/arithmetic.inl"
  #include "handlers/handler16.inl"
  #include "handlers/handler32.inl"
  #include "handlers/memory.inl"

  Architecture arch;
  u32 exception_base = 0;
  bool wait_for_irq = false;
  MemoryBase* memory;
  Coprocessor* coprocessors[16] { nullptr };

  State state;
  State::StatusRegister* p_spsr;
  bool irq_line;

  u32 opcode[2];

  bool condition_table[16][16];

  struct Trace {
    u32 r14;
    u32 r15;
  };

  std::vector<Trace> r14_trace;
  u32 r14_old = 0xFFFFFFFF;
  
  static std::array<Handler16, 2048> s_opcode_lut_16;
  static std::array<Handler32, 8192> s_opcode_lut_32;
};

} // namespace Duality::core::arm
