/*
 * Copyright (C) 2020 fleroviux
 */

#include <algorithm>
#include <stdexcept>

#include "arm.hpp"

namespace Duality::core::arm {

void ARM::Reset() {
  constexpr u32 nop = 0xE320F000;

  state.Reset();
  SwitchMode(MODE_SYS);
  opcode[0] = nop;
  opcode[1] = nop;
  state.r15 = ExceptionBase();
  wait_for_irq = false;
  IRQLine() = false;

  for (auto coprocessor : coprocessors)
    if (coprocessor != nullptr)
      coprocessor->Reset();

  r14_trace.clear();
}

void ARM::Run(int instructions) {
  if (IsWaitingForIRQ() && !IRQLine()) {
    return;
  }

  while (instructions-- > 0) {
    if (IRQLine()) SignalIRQ();

    memory->r14 = state.r14;
    memory->r15 = state.r15;

    // if (state.r15 == (0x021ecf6c + 4)) {
    //   LOG_DEBUG("DBG: called FUN_overlay_5__021ecf6c param_1=0x{0:08X} param_2=0x{1:08X} param_3=0x{2:08X} r14=0x{3:08X}", state.r0, state.r1, state.r2, state.r14);
    // }

    // if (state.r15 == (0x020181f4 + 4)) {
    //   LOG_INFO("DBG: called the allocate wrapper, r0=0x{0:08X}", state.r0);
    // }

    // if (state.r15 == (0x020a5254 + 8)) {
    //   LOG_INFO("DBG: called heap_allocate_1: heap=0x{0:08X} size=0x{1:08X} alignment=0x{2:08X}", state.r0, state.r1, state.r2);
    // }
    // if (state.r15 == (0x020a52ec + 8) || state.r15 == (0x020a5304 + 8)) {
    //   LOG_INFO("DBG: heap_allocate_1 result: 0x{0:08X}", state.r0);
    // }

    // if (state.r15 == (0x01ff85b8 + 8)) {
    //   LOG_INFO("DBG: called dma_immediate_chunked_xfer! r14=0x{0:08X}", state.r14);
    // }

    if (arch == Architecture::ARMv5TE) {
      if (memory->dawn_addr != 0xFFFFFFFF) {
        for (int i = 0; i < 15; i++) {
          if (state.reg[i] == memory->dawn_addr) {
            LOG_INFO("DBG: r{0} contains dawn sprite address @ r15=0x{1:08X}", i, state.r15);
          }
        }  
      }
      

      if (state.r15 == (0x020c4598 + 8)) {
        if (state.r1 >= memory->dawn_addr && state.r1 < (memory->dawn_addr + 0x3438)) {
          auto bitfields_are_gay = state.cpsr.f.mask_irq;
          LOG_INFO("DBG: dma_immediate_xfer with dawn's source address! irq_disable={0}", bitfields_are_gay);
          // for (auto trace : r14_trace) {
          //   fmt::print("\t0x{0:08X} changed @ 0x{1:08X}\n", trace.r14, trace.r15);
          // }
        }
        LOG_INFO("DBG: called dma_immediate_xfer! channel={0} src=0x{1:08X} dst=0x{2:08X} count=0x{3:08X} r14=0x{4:08X}",
          state.r0, state.r1, state.r2, state.r3, state.r14);
      }

      if (state.r15 == (0x01ff8480 + 8)) {
        LOG_INFO("DBG: called dma_set! r14=0x{0:08X}", state.r14);
      }

      static bool is_dawn_sprite = false;
      if (state.r15 == (0x020a5308 + 8)) {
        LOG_INFO("DBG: called heap_allocate_2: heap=0x{0:08X} size=0x{1:08X} alignment=0x{2:08X}", state.r0, state.r1, state.r2);
        is_dawn_sprite = state.r1 == 0x3438;
      }
      if (state.r15 == (0x020a539c + 8) || state.r15 == (0x020a53b4 + 8)) {
        LOG_INFO("DBG: heap_allocate_2 result: 0x{0:08X}", state.r0);
      }

      if (state.r15 == (0x02018214 + 4)) {
        LOG_INFO("DBG: something_allocate result: 0x{0:08X}", state.r0);
        if (is_dawn_sprite) {
          memory->dawn_addr = state.r0;
          LOG_DEBUG("DBG: dawn destination address: 0x{0:08X}", state.r0);
        }
      }

      if (state.r15 == (0x02006d3c + 4)) {
        LOG_DEBUG("DBG: about to set *(0x{1:08X} + 0x30) = 0x{2:08X}. r14=0x{0:08X}", state.r14, state.r0, state.r2);
      }

      if (state.r15 == (0x020c6a40 + 8)) {
        auto addr = ReadWord(state.r0 + 0x30);
        if (addr == 0) {
          LOG_ERROR("DBG: called 0x020c6a40 with *(r0 + 0x30) = 0 r14=0x{0:08X}", state.r14);
        } else {
          //LOG_DEBUG("DBG: *(r0 + 0x30) = 0x{0:08X} r14=0x{1:08X}", addr, state.r14);
        }
        memory->param1 = state.r0;
      }

      if (state.r15 == (0x020c8370 + 8) && state.r1 == 0) {
        LOG_ERROR("DBG: called 0x020c8370 with a null pointer r14=0x{0:08X}", state.r14);
      }

      if (state.r15 == (0x020cd6a4 + 8) && state.r2 == 0) {
        LOG_ERROR("DBG: called 0x020cd6a4 with a null pointer r14=0x{0:08X}", state.r14);
      }
    }

    auto instruction = opcode[0];
    if (state.cpsr.f.thumb) {
      state.r15 &= ~1;

      opcode[0] = opcode[1];
      opcode[1] = ReadHalfCode(state.r15);
      (this->*s_opcode_lut_16[instruction >> 5])(instruction);
    } else {
      state.r15 &= ~3;

      opcode[0] = opcode[1];
      opcode[1] = ReadWordCode(state.r15);
      auto condition = static_cast<Condition>(instruction >> 28);
      if (CheckCondition(condition)) {
        int hash = ((instruction >> 16) & 0xFF0) |
                   ((instruction >>  4) & 0x00F);
        if (condition == COND_NV) {
          hash |= 4096;
        }
        (this->*s_opcode_lut_32[hash])(instruction);

        if (IsWaitingForIRQ()) return;
      } else {
        state.r15 += 4;
      }
    }

    if (state.r14 != r14_old) {
      if (r14_trace.size() == 33) {
        r14_trace.erase(r14_trace.begin());
      }
      r14_trace.push_back({state.r14, state.r15});
      r14_old = state.r14;
    }
  }
}


void ARM::AttachCoprocessor(uint id, Coprocessor* coprocessor) {
  if (id >= 16) { 
    throw std::runtime_error{"Coprocessor ID must be lower or equal to 15"};
  }
  
  coprocessors[id] = coprocessor;
}

void ARM::SignalIRQ() {
  wait_for_irq = false;

  if (state.cpsr.f.mask_irq) {
    return;
  }

  LOG_DEBUG("DBG: dispatch IRQ handler");

  // Save current program status register.
  state.spsr[BANK_IRQ].v = state.cpsr.v;

  // Enter IRQ mode and disable IRQs.
  SwitchMode(MODE_IRQ);
  state.cpsr.f.mask_irq = 1;

  // Save current program counter and disable Thumb.
  if (state.cpsr.f.thumb) {
    state.cpsr.f.thumb = 0;
    state.r14 = state.r15;
  } else {
    state.r14 = state.r15 - 4;
  }
  
  // Jump to IRQ exception vector.
  state.r15 = ExceptionBase() + 0x18;
  ReloadPipeline32();
}

void ARM::ReloadPipeline32() {
  opcode[0] = ReadWordCode(state.r15);
  opcode[1] = ReadWordCode(state.r15 + 4);
  state.r15 += 8;
}

void ARM::ReloadPipeline16() {
  opcode[0] = ReadHalfCode(state.r15);
  opcode[1] = ReadHalfCode(state.r15 + 2);
  state.r15 += 4;
}

void ARM::BuildConditionTable() {
  for (int flags = 0; flags < 16; flags++) {
    bool n = flags & 8;
    bool z = flags & 4;
    bool c = flags & 2;
    bool v = flags & 1;

    condition_table[COND_EQ][flags] = z;
    condition_table[COND_NE][flags] = !z;
    condition_table[COND_CS][flags] =  c;
    condition_table[COND_CC][flags] = !c;
    condition_table[COND_MI][flags] =  n;
    condition_table[COND_PL][flags] = !n;
    condition_table[COND_VS][flags] =  v;
    condition_table[COND_VC][flags] = !v;
    condition_table[COND_HI][flags] =  c && !z;
    condition_table[COND_LS][flags] = !c ||  z;
    condition_table[COND_GE][flags] = n == v;
    condition_table[COND_LT][flags] = n != v;
    condition_table[COND_GT][flags] = !(z || (n != v));
    condition_table[COND_LE][flags] =  (z || (n != v));
    condition_table[COND_AL][flags] = true;
    condition_table[COND_NV][flags] = true;
  }
}

bool ARM::CheckCondition(Condition condition) {
  if (condition == COND_AL) {
    return true;
  }
  return condition_table[condition][state.cpsr.v >> 28];
}

auto ARM::GetRegisterBankByMode(Mode mode) -> Bank {
  switch (mode) {
    case MODE_USR:
    case MODE_SYS:
      return BANK_NONE;
    case MODE_FIQ:
      return BANK_FIQ;
    case MODE_IRQ:
      return BANK_IRQ;
    case MODE_SVC:
      return BANK_SVC;
    case MODE_ABT:
      return BANK_ABT;
    case MODE_UND:
      return BANK_UND;
  }

  UNREACHABLE;
}

void ARM::SwitchMode(Mode new_mode) {
  auto old_bank = GetRegisterBankByMode(state.cpsr.f.mode);
  auto new_bank = GetRegisterBankByMode(new_mode);

  state.cpsr.f.mode = new_mode;
  p_spsr = &state.spsr[new_bank];

  if (old_bank == new_bank) {
    return;
  }

  if (old_bank == BANK_FIQ || new_bank == BANK_FIQ) {
    for (int i = 0; i < 7; i++){
      state.bank[old_bank][i] = state.reg[8 + i];
    }

    for (int i = 0; i < 7; i++) {
      state.reg[8 + i] = state.bank[new_bank][i];
    }
  } else {
    state.bank[old_bank][5] = state.r13;
    state.bank[old_bank][6] = state.r14;

    state.r13 = state.bank[new_bank][5];
    state.r14 = state.bank[new_bank][6];
  }
}

} // namespace Duality::core::arm
