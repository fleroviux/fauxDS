/*
 * Copyright (C) 2020 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

enum class DataOp {
  AND = 0,
  EOR = 1,
  SUB = 2,
  RSB = 3,
  ADD = 4,
  ADC = 5,
  SBC = 6,
  RSC = 7,
  TST = 8,
  TEQ = 9,
  CMP = 10,
  CMN = 11,
  ORR = 12,
  MOV = 13,
  BIC = 14,
  MVN = 15
};

template <bool immediate, int opcode, bool _set_flags, int field4>
void ARM_DataProcessing(std::uint32_t instruction) {
  bool set_flags = _set_flags;

  int reg_dst = (instruction >> 12) & 0xF;
  int reg_op1 = (instruction >> 16) & 0xF;
  int reg_op2 = (instruction >>  0) & 0xF;

  std::uint32_t op2 = 0;
  std::uint32_t op1 = state.reg[reg_op1];

  int carry = state.cpsr.f.c;

  if (immediate) {
    int value = instruction & 0xFF;
    int shift = ((instruction >> 8) & 0xF) * 2;

    if (shift != 0) {
      carry = (value >> (shift - 1)) & 1;
      op2   = (value >> shift) | (value << (32 - shift));
    } else {
      op2 = value;
    }
  } else {
    std::uint32_t shift;
    int  shift_type = ( field4 >> 1) & 3;
    bool shift_imm  = (~field4 >> 0) & 1;

    op2 = state.reg[reg_op2];

    if (shift_imm) {
      shift = (instruction >> 7) & 0x1F;
    } else {
      shift = state.reg[(instruction >> 8) & 0xF];

      if (reg_op1 == 15) op1 += 4;
      if (reg_op2 == 15) op2 += 4;
    }

    DoShift(shift_type, op2, shift, carry, shift_imm);
  }

  if (reg_dst == 15 && set_flags) {
    auto spsr = *p_spsr;

    SwitchMode(spsr.f.mode);
    state.cpsr.v = spsr.v;
    set_flags = false;
  }

  auto& cpsr = state.cpsr;
  auto& result = state.reg[reg_dst];

  switch (static_cast<DataOp>(opcode)) {
    case DataOp::AND:
      result = op1 & op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case DataOp::EOR:
      result = op1 ^ op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case DataOp::SUB:
      result = SUB(op1, op2, set_flags);
      break;
    case DataOp::RSB:
      result = SUB(op2, op1, set_flags);
      break;
    case DataOp::ADD:
      result = ADD(op1, op2, set_flags);
      break;
    case DataOp::ADC:
      result = ADC(op1, op2, set_flags);
      break;
    case DataOp::SBC:
      result = SBC(op1, op2, set_flags);
      break;
    case DataOp::RSC:
      result = SBC(op2, op1, set_flags);
      break;
    case DataOp::TST: {
      std::uint32_t result = op1 & op2;
      SetZeroAndSignFlag(result);
      cpsr.f.c = carry;
      break;
    }
    case DataOp::TEQ: {
      std::uint32_t result = op1 ^ op2;
      SetZeroAndSignFlag(result);
      cpsr.f.c = carry;
      break;
    }
    case DataOp::CMP:
      SUB(op1, op2, true);
      break;
    case DataOp::CMN:
      ADD(op1, op2, true);
      break;
    case DataOp::ORR:
      result = op1 | op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case DataOp::MOV:
      result = op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case DataOp::BIC:
      result = op1 & ~op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case DataOp::MVN:
      result = ~op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
  }

  if (reg_dst == 15) {
    if (state.cpsr.f.thumb) {
      ReloadPipeline16();
    } else {
      ReloadPipeline32();
    }
  } else {
    state.r15 += 4;
  }
}

template <bool immediate, bool use_spsr, bool to_status>
void ARM_StatusTransfer(std::uint32_t instruction) {
  if (to_status) {
    std::uint32_t op;
    std::uint32_t mask = 0;
    std::uint8_t  fsxc = (instruction >> 16) & 0xF;

    if (immediate && fsxc == 0) {
      /* Hint instructions are implemented as move immediate to
       * status register instructions with zero mask.
       */
      ARM_Hint(instruction);
      return; 
    }

    /* Create mask based on fsxc-bits. */
    if (fsxc & 1) mask |= 0x000000FF;
    if (fsxc & 2) mask |= 0x0000FF00;
    if (fsxc & 4) mask |= 0x00FF0000;
    if (fsxc & 8) mask |= 0xFF000000;

    /* Decode source operand. */
    if (immediate) {
      int value = instruction & 0xFF;
      int shift = ((instruction >> 8) & 0xF) * 2;

      op = (value >> shift) | (value << (32 - shift));
    } else {
      op = state.reg[instruction & 0xF];
    }

    std::uint32_t value = op & mask;

    if (!use_spsr) {
      if (mask & 0xFF) {
        SwitchMode(static_cast<Mode>(value & 0x1F));
      }
      state.cpsr.v = (state.cpsr.v & ~mask) | value;
    } else {
      p_spsr->v = (p_spsr->v & ~mask) | value;
    }

    /* TODO: Handle case where Thumb-bit was set. */
  } else {
    int dst = (instruction >> 12) & 0xF;

    if (use_spsr) {
      state.reg[dst] = p_spsr->v;
    } else {
      state.reg[dst] = state.cpsr.v;
    }
  }

  state.r15 += 4;
}

template <bool accumulate, bool set_flags>
void ARM_Multiply(std::uint32_t instruction) {
  std::uint32_t result;

  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  result = state.reg[op1] * state.reg[op2];

  if (accumulate) {
    result += state.reg[op3];
  }

  if (set_flags) {
    SetZeroAndSignFlag(result);
  }

  state.reg[dst] = result;
  state.r15 += 4;
}

template <bool sign_extend, bool accumulate, bool set_flags>
void ARM_MultiplyLong(std::uint32_t instruction) {
  int op1 = (instruction >> 0) & 0xF;
  int op2 = (instruction >> 8) & 0xF;

  int dst_lo = (instruction >> 12) & 0xF;
  int dst_hi = (instruction >> 16) & 0xF;

  std::int64_t result;

  if (sign_extend) {
    std::int64_t a = state.reg[op1];
    std::int64_t b = state.reg[op2];

    /* Sign-extend operands */
    if (a & 0x80000000) a |= 0xFFFFFFFF00000000;
    if (b & 0x80000000) b |= 0xFFFFFFFF00000000;

    result = a * b;
  } else {
    std::uint64_t uresult = (std::uint64_t)state.reg[op1] * (std::uint64_t)state.reg[op2];

    result = (std::int64_t)uresult;
  }

  if (accumulate) {
    std::int64_t value = state.reg[dst_hi];

    /* Workaround x86 shift limitations. */
    value <<= 16;
    value <<= 16;
    value  |= state.reg[dst_lo];

    result += value;
  }

  std::uint32_t result_hi = result >> 32;

  state.reg[dst_lo] = result & 0xFFFFFFFF;
  state.reg[dst_hi] = result_hi;

  if (set_flags) {
    state.cpsr.f.n = result_hi >> 31;
    state.cpsr.f.z = result == 0;
  }

  state.r15 += 4;
}

void ARM_UnsignedMultiplyLongAccumulateAccumulate(std::uint32_t instruction) {
  int op1 = (instruction >> 0) & 0xF;
  int op2 = (instruction >> 8) & 0xF;
  int dst_lo = (instruction >> 12) & 0xF;
  int dst_hi = (instruction >> 16) & 0xF;
  
  LOG_WARN("Hit UMAAL instruction - implementation is experimental. @ r15 = 0x{0:08X}", state.r15);

  std::uint64_t result = (std::uint64_t)state.reg[op1] * (std::uint64_t)state.reg[op2];
  
  result += state.reg[dst_lo];
  result += state.reg[dst_hi];
  
  state.reg[dst_lo] = result & 0xFFFFFFFF;
  state.reg[dst_hi] = result >> 32;

  state.r15 += 4;
}

template <bool accumulate, bool x, bool y>
void ARM_SignedHalfwordMultiply(std::uint32_t instruction) {
  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  std::int16_t value1;
  std::int16_t value2;

  LOG_WARN("Hit SMULxy/SMLAxy instruction - implementation is experimental. @ r15 = 0x{0:08X}", state.r15);

  if (x) {
    value1 = std::int16_t(state.reg[op1] >> 16);
  } else {
    value1 = std::int16_t(state.reg[op1] & 0xFFFF);
  }
  
  if (y) {
    value2 = std::int16_t(state.reg[op2] >> 16);
  } else {
    value2 = std::int16_t(state.reg[op2] & 0xFFFF);
  }
  
  state.reg[dst] = std::uint32_t(value1 * value2);

  if (accumulate) {
    /* Set sticky overflow on accumulation overflow,
     * but do not saturate the result.
     */
    state.reg[dst] = QADD(state.reg[dst], state.reg[op3], false);
  }
  
  state.r15 += 4;
}

template <bool accumulate, bool y>
void ARM_SignedWordHalfwordMultiply(std::uint32_t instruction) {
  /* NOTE: this is probably used as fixed point fractional multiply. */
  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  LOG_WARN("Hit SMULWy/SMLAWy instruction - implementation is experimental. @ r15 = 0x{0:08X}", state.r15);

  std::int32_t value1 = std::int32_t(state.reg[op1]);
  std::int16_t value2;
  
  if (y) {
    value2 = std::int16_t(state.reg[op2] >> 16);
  } else {
    value2 = std::int16_t(state.reg[op2] & 0xFFFF);
  }
  
  state.reg[dst] = std::uint32_t((value1 * value2)/0x10000);

  if (accumulate) {
    /* Set sticky overflow on accumulation overflow,
     * but do not saturate the result.
     */
    state.reg[dst] = QADD(state.reg[dst], state.reg[op3], false);
  }

  state.r15 += 4;
}

template <bool x, bool y>
void ARM_SignedHalfwordMultiplyLongAccumulate(std::uint32_t instruction) {
  int op1 = (instruction >> 0) & 0xF;
  int op2 = (instruction >> 8) & 0xF;
  int dst_lo = (instruction >> 12) & 0xF;
  int dst_hi = (instruction >> 16) & 0xF;

  LOG_WARN("Hit SMLALxy instruction - implementation is experimental. @ r15 = 0x{0:08X}", state.r15);

  std::int16_t value1;
  std::int16_t value2;
  
  if (x) {
    value1 = std::int16_t(state.reg[op1] >> 16);
  } else {
    value1 = std::int16_t(state.reg[op1] & 0xFFFF);
  }
  
  if (y) {
    value2 = std::int16_t(state.reg[op2] >> 16);
  } else {
    value2 = std::int16_t(state.reg[op2] & 0xFFFF);
  }
  
  std::uint64_t result = value1 * value2;
  
  result += state.reg[dst_lo];
  result += std::uint64_t(state.reg[dst_hi]) << 32;
  
  state.reg[dst_lo] = result & 0xFFFFFFFF;
  state.reg[dst_hi] = result >> 32;
  
  state.r15 += 4;
}

template <bool byte>
void ARM_SingleDataSwap(std::uint32_t instruction) {
  int src  = (instruction >>  0) & 0xF;
  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;

  std::uint32_t tmp;

  if (byte) {
    tmp = ReadByte(state.reg[base]);
    WriteByte(state.reg[base], (std::uint8_t)state.reg[src]);
  } else {
    tmp = ReadWordRotate(state.reg[base]);
    WriteWord(state.reg[base], state.reg[src]);
  }

  state.reg[dst] = tmp;
  state.r15 += 4;
}

template <bool link>
void ARM_BranchAndExchangeMaybeLink(std::uint32_t instruction) {
  std::uint32_t address = state.reg[instruction & 0xF];

  if (link) {
    state.r14 = state.r15 - 4;
  }

  if (address & 1) {
    state.r15 = address & ~1;
    state.cpsr.f.thumb = 1;
    ReloadPipeline16();
  } else {
    state.r15 = address & ~3;
    ReloadPipeline32();
  }
}

template <bool pre, bool add, bool immediate, bool writeback, bool load, int opcode>
void ARM_SingleHalfwordDoubleTransfer(std::uint32_t instruction) {
  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;

  std::uint32_t offset;
  std::uint32_t address = state.reg[base];

  if (immediate) {
    offset = (instruction & 0xF) | ((instruction >> 4) & 0xF0);
  } else {
    offset = state.reg[instruction & 0xF];
  }

  if (pre) {
    address += add ? offset : -offset;
  }

  /* TODO: figure out alignment constraints for LDRD/STRD. */
  if (opcode == 1 && load) {
    state.reg[dst] = ReadHalfRotate(address);
  } else if (opcode == 1) {
    std::uint32_t value = state.reg[dst];

    if (dst == 15) {
      value += 4;
    }

    WriteHalf(address, value);
  } else if (opcode == 2 && load) {
    state.reg[dst] = ReadByteSigned(address);
  } else if (opcode == 3 && load) {
    state.reg[dst] = ReadHalfSigned(address);
  } else if (opcode == 2) {
    /* TODO: check if loading r15 must clear the pipeline. */
    state.reg[dst + 0] = ReadWord(address + 0);
    state.reg[dst + 1] = ReadWord(address + 4);
    
    /* Disable writeback if base is second destination register. */
    if (base == (dst + 1)) {
      state.r15 += 4;
      return;
    }
  } else if (opcode == 3) {
    /* TODO: likely r15 will be advanced before the second write. */
    WriteWord(address + 0, state.reg[dst + 0]);
    WriteWord(address + 4, state.reg[dst + 1]);
  }

  if ((writeback || !pre) && (!load || base != dst)) {
    if (!pre) {
      address += add ? offset : -offset;
    }
    state.reg[base] = address;
  }

  state.r15 += 4;
}

template <bool link>
void ARM_BranchAndLink(std::uint32_t instruction) {
  std::uint32_t offset = instruction & 0xFFFFFF;

  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }

  if (link) {
    state.r14 = state.r15 - 4;
  }

  state.r15 += offset * 4;
  ReloadPipeline32();
}

void ARM_BranchLinkExchangeImm(std::uint32_t instruction) {
  std::uint32_t offset = instruction & 0xFFFFFF;

  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }

  offset = (offset << 2) | ((instruction >> 23) & 2);

  state.r14  = state.r15 - 4;
  state.r15 += offset;
  state.cpsr.f.thumb = 1;
  ReloadPipeline16();
}

template <bool immediate, bool pre, bool add, bool byte, bool writeback, bool load>
void ARM_SingleDataTransfer(std::uint32_t instruction) {
  Mode mode;
  std::uint32_t offset;

  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;
  std::uint32_t address = state.reg[base];

  /* Post-indexing implicitly write back to the base register.
   * In that case user mode registers will be used if the W-bit is set.
   */
  bool user_mode = !pre && writeback;

  if (user_mode) {
    mode = static_cast<Mode>(state.cpsr.f.mode);
    SwitchMode(MODE_USR);
  }

  /* Calculate offset relative to base register. */
  if (immediate) {
    offset = instruction & 0xFFF;
  } else {
    int carry  = state.cpsr.f.c;
    int opcode = (instruction >> 5) & 3;
    int amount = (instruction >> 7) & 0x1F;

    offset = state.reg[instruction & 0xF];
    DoShift(opcode, offset, amount, carry, true);
  }

  if (pre) {
    address += add ? offset : -offset;
  }

  if (load) {
    if (byte) {
      state.reg[dst] = ReadByte(address);
    } else {
      state.reg[dst] = ReadWordRotate(address);
    }
  } else {
    std::uint32_t value = state.reg[dst];

    /* r15 is $+12 now due to internal prefetch cycle. */
    if (dst == 15) value += 4;

    if (byte) {
      WriteByte(address, (std::uint8_t)value);
    } else {
      WriteWord(address, value);
    }
  }

  /* Write address back to the base register.
   * However the destination register must not be overwritten.
   */
  if (!load || base != dst) {
    if (!pre) {
      state.reg[base] += add ? offset : -offset;
    } else if (writeback) {
      state.reg[base] = address;
    }
  }

  /* Restore original mode (if it was changed) */
  if (user_mode) {
    SwitchMode(mode);
  }

  if (load && dst == 15) {
    /* NOTE: ldrb(t) and ldrt to r15 are unpredictable. */
    if ((state.r15 & 1) && !byte && !user_mode) {
      state.cpsr.f.thumb = 1;
      state.r15 &= ~1;
      ReloadPipeline16();
    } else {
      ReloadPipeline32();
    }
  } else {
    state.r15 += 4;
  }
}

template <bool _pre, bool add, bool user_mode, bool _writeback, bool load>
void ARM_BlockDataTransfer(std::uint32_t instruction) {
  bool pre = _pre;
  bool writeback = _writeback;

  int base = (instruction >> 16) & 0xF;

  int bytes = 0;
  int last = 0;
  int list = instruction & 0xFFFF;

  Mode mode;
  bool transfer_pc = list & (1 << 15);
  bool switch_mode = user_mode && (!load || !transfer_pc);

  state.r15 += 4;
  
  for (int i = 0; i <= 15; i++) {
    if (list & (1 << i)) {
      last = i;
      bytes += 4;
    }
  }
  
  std::uint32_t base_new;
  std::uint32_t address = state.reg[base];
  
  if (switch_mode) {
    mode = state.cpsr.f.mode;
    SwitchMode(MODE_USR);
  }

  if (!add) {
    address -= bytes;
    base_new = address;
    pre = !pre;
  } else {
    base_new = address + bytes;
  }

  // TODO: Handle case on the ARM11 where STM w/ writeback
  // and w/ base register in the list apparently stores
  // the final base address unless the base register is
  // the first or second register in the list.
  for (int i = 0; i <= last; i++) {
    if (~list & (1 << i)) {
      continue;
    }
    
    if (pre) {
      address += 4;
    }
    
    if (load) {
      state.reg[i] = ReadWord(address);
      if (i == 15 && user_mode) {
        auto& spsr = *p_spsr;

        SwitchMode(spsr.f.mode);
        state.cpsr.v = spsr.v;
      }
    } else {
      WriteWord(address, state.reg[i]);
    }
    
    if (!pre) {
      address += 4;
    }
  }
  
  if (switch_mode) {
    SwitchMode(mode);
  }

  if (writeback && load) {
    // LDM ARMv5: writeback if base is the only register or not the last register.
    // LDM ARMv6: writeback if base in not in the register list.  
    writeback &= (arch == Architecture::ARMv6K) ? (last != base || list == (1 << base)) : !(list & (1 << base));
  }

  if (writeback) {
    state.reg[base] = base_new;
  }

  if (load && transfer_pc) {
    if ((state.r15 & 1) && !user_mode) {
      state.cpsr.f.thumb = 1;
      state.r15 &= ~1;
    }

    if (state.cpsr.f.thumb) {
      ReloadPipeline16();
    } else {
      ReloadPipeline32();
    }
  }
}

void ARM_Undefined(std::uint32_t instruction) {
  LOG_ERROR("Undefined ARM instruction: 0x{0:08X} @ r15 = 0x{1:08X}", instruction, state.r15);
  hit_unimplemented_or_undefined = true;

  /* Save return address and program status. */
  state.bank[BANK_UND][BANK_R14] = state.r15 - 4;
  state.spsr[BANK_UND].v = state.cpsr.v;

  /* Switch to UND mode and disable interrupts. */
  SwitchMode(MODE_UND);
  state.cpsr.f.mask_irq = 1;

  /* Jump to execution vector */
  state.r15 = ExceptionBase() + 0x04;
  ReloadPipeline32();
}

void ARM_SWI(std::uint32_t instruction) {
  /* Save return address and program status. */
  state.bank[BANK_SVC][BANK_R14] = state.r15 - 4;
  state.spsr[BANK_SVC].v = state.cpsr.v;

  /* Switch to SVC mode and disable interrupts. */
  SwitchMode(MODE_SVC);
  state.cpsr.f.mask_irq = 1;

  /* Jump to execution vector */
  state.r15 = ExceptionBase() + 0x08;
  ReloadPipeline32();
}

void ARM_CountLeadingZeros(std::uint32_t instruction) {
  int dst = (instruction >> 12) & 0xF;
  int src =  instruction & 0xF;
  
  std::uint32_t value = state.reg[src];
  
  if (value == 0) {
    state.reg[dst] = 32;
    state.r15 += 4;
    return;
  }
  
  const std::uint32_t mask[] = {
    0xFFFF0000,
    0xFF000000,
    0xF0000000,
    0xC0000000,
    0x80000000 };
  const int shift[] = { 16, 8, 4, 2, 1 };
  
  int result = 0;
  
  for (int i = 0; i < 5; i++) {
    if ((value & mask[i]) == 0) {
      result |= shift[i];
      value <<= shift[i];
    }
  }
  
  state.reg[dst] = result;
  state.r15 += 4;
}

template <int opcode>
void ARM_SaturatingAddSubtract(std::uint32_t instruction) {
  int src1 =  instruction & 0xF;
  int src2 = (instruction >> 16) & 0xF;
  int dst  = (instruction >> 12) & 0xF;
  
  switch (opcode) {
    case 0b0000: {
      state.reg[dst] = QADD(state.reg[src1], state.reg[src2]);
      break;
    }
    case 0b0010: {
      state.reg[dst] = QSUB(state.reg[src1], state.reg[src2]);
      break;
    }
    case 0b0100: {
      state.reg[dst] = QADD(state.reg[src1], state.reg[src2] * 2);
      break;
    }
    case 0b0110: {
      state.reg[dst] = QSUB(state.reg[src1], state.reg[src2] * 2);
      break;
    }
    default: {
      ARM_Undefined(instruction);
      return;
    }
  }
  
  state.r15 += 4;
}

void ARM_Hint(std::uint32_t instruction) {
  std::uint8_t opcode = instruction & 0xFF;

  if (opcode == 3) {
    LOG_INFO("Core #{0} is waiting for an IRQ.", core);
    waiting_for_irq = true;
    state.r15 += 4;
    return;
  }

  if (opcode != 0) {
    ARM_Unimplemented(instruction);
    return;
  }

  state.r15 += 4;
}

void ARM_CoprocessorRegisterTransfer(std::uint32_t instruction) {
  auto dst = (instruction >> 12) & 0xF;
  auto cp_rm = instruction & 0xF;
  auto cp_rn = (instruction >> 16) & 0xF;
  auto opcode1 = (instruction >> 21) & 7;
  auto opcode2 = (instruction >>  5) & 7;
  auto cp_num  = (instruction >>  8) & 0xF;

  if (instruction & (1 << 20)) {
    if (cp_num == 15) {
      state.reg[dst] = cp15.Read(opcode1, cp_rn, cp_rm, opcode2);
    } else {
      LOG_ERROR("mrc p{0}, #{1}, r{2}, C{3}, C{4}, #{5}",
                cp_num,
                opcode1,
                dst,
                cp_rn,
                cp_rm,
                opcode2);
      hit_unimplemented_or_undefined = true;
    }
  } else {
    if (cp_num == 15) {
      cp15.Write(opcode1, cp_rn, cp_rm, opcode2, state.reg[dst]);
    } else {
      LOG_ERROR("mcr p{0}, #{1}, r{2}, C{3}, C{4}, #{5} (rD = 0x{6:08X})",
                cp_num,
                opcode1,
                dst,
                cp_rn,
                cp_rm,
                opcode2,
                state.reg[dst]);
      hit_unimplemented_or_undefined = true;
    }
  }

  state.r15 += 4;
}

void ARM_Unimplemented(std::uint32_t instruction) {
  LOG_ERROR("Unimplemented ARM instruction: 0x{0:08X} @ r15 = 0x{1:08X}", instruction, state.r15);
  hit_unimplemented_or_undefined = true;
  state.r15 += 4;
}
