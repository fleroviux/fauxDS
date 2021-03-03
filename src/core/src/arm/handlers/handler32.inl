/*
 * Copyright (C) 2020 fleroviux
 */

enum class ARMDataOp {
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

template <bool immediate, ARMDataOp opcode, bool set_flags, int field4>
void ARM_DataProcessing(u32 instruction) {
  int reg_dst = (instruction >> 12) & 0xF;
  int reg_op1 = (instruction >> 16) & 0xF;
  int reg_op2 = (instruction >>  0) & 0xF;

  u32 op2 = 0;
  u32 op1 = state.reg[reg_op1];

  int carry = state.cpsr.f.c;

  if constexpr (immediate) {
    int value = instruction & 0xFF;
    int shift = ((instruction >> 8) & 0xF) * 2;

    if (shift != 0) {
      carry = (value >> (shift - 1)) & 1;
      op2   = (value >> shift) | (value << (32 - shift));
    } else {
      op2 = value;
    }
  } else {
    constexpr int  shift_type = ( field4 >> 1) & 3;
    constexpr bool shift_imm  = (~field4 >> 0) & 1;

    u32 shift;

    op2 = state.reg[reg_op2];

    if constexpr (shift_imm) {
      shift = (instruction >> 7) & 0x1F;
    } else {
      shift = state.reg[(instruction >> 8) & 0xF];

      if (reg_op1 == 15) op1 += 4;
      if (reg_op2 == 15) op2 += 4;
    }

    DoShift(shift_type, op2, shift, carry, shift_imm);
  }

  auto& cpsr = state.cpsr;
  auto& result = state.reg[reg_dst];

  switch (opcode) {
    case ARMDataOp::AND:
      result = op1 & op2;
      if constexpr (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case ARMDataOp::EOR:
      result = op1 ^ op2;
      if constexpr (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case ARMDataOp::SUB:
      result = SUB(op1, op2, set_flags);
      break;
    case ARMDataOp::RSB:
      result = SUB(op2, op1, set_flags);
      break;
    case ARMDataOp::ADD:
      result = ADD(op1, op2, set_flags);
      break;
    case ARMDataOp::ADC:
      result = ADC(op1, op2, set_flags);
      break;
    case ARMDataOp::SBC:
      result = SBC(op1, op2, set_flags);
      break;
    case ARMDataOp::RSC:
      result = SBC(op2, op1, set_flags);
      break;
    case ARMDataOp::TST:
      SetZeroAndSignFlag(op1 & op2);
      cpsr.f.c = carry;
      break;
    case ARMDataOp::TEQ:
      SetZeroAndSignFlag(op1 ^ op2);
      cpsr.f.c = carry;
      break;
    case ARMDataOp::CMP:
      SUB(op1, op2, true);
      break;
    case ARMDataOp::CMN:
      ADD(op1, op2, true);
      break;
    case ARMDataOp::ORR:
      result = op1 | op2;
      if (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case ARMDataOp::MOV:
      result = op2;
      if constexpr (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case ARMDataOp::BIC:
      result = op1 & ~op2;
      if constexpr (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
    case ARMDataOp::MVN:
      result = ~op2;
      if constexpr (set_flags) {
        SetZeroAndSignFlag(result);
        cpsr.f.c = carry;
      }
      break;
  }

  if (reg_dst == 15) {
    if constexpr (set_flags) {
      auto spsr = *p_spsr;

      SwitchMode(spsr.f.mode);
      state.cpsr.v = spsr.v;
    }

    if constexpr (opcode != ARMDataOp::TST &&
                  opcode != ARMDataOp::TEQ &&
                  opcode != ARMDataOp::CMP &&
                  opcode != ARMDataOp::CMN) {
      if (state.cpsr.f.thumb) {
        ReloadPipeline16();
      } else {
        ReloadPipeline32();
      }
    }
  } else {
    state.r15 += 4;
  }
}

template <bool immediate, bool use_spsr, bool to_status>
void ARM_StatusTransfer(u32 instruction) {
  // TODO: handle changing the CPSR.T thumb bit.
  if constexpr (to_status) {
    u32 op;
    u32 mask = 0;
    u8  fsxc = (instruction >> 16) & 0xF;

    if (fsxc & 1) mask |= 0x000000FF;
    if (fsxc & 2) mask |= 0x0000FF00;
    if (fsxc & 4) mask |= 0x00FF0000;
    if (fsxc & 8) mask |= 0xFF000000;

    if constexpr (immediate) {
      int value = instruction & 0xFF;
      int shift = ((instruction >> 8) & 0xF) * 2;

      op = (value >> shift) | (value << (32 - shift));
    } else {
      op = state.reg[instruction & 0xF];
    }

    u32 value = op & mask;

    if constexpr (!use_spsr) {
      if (mask & 0xFF) {
        SwitchMode(static_cast<Mode>(value & 0x1F));
      }
      state.cpsr.v = (state.cpsr.v & ~mask) | value;
    } else {
      p_spsr->v = (p_spsr->v & ~mask) | value;
    }
  } else {
    int dst = (instruction >> 12) & 0xF;

    if constexpr (use_spsr) {
      state.reg[dst] = p_spsr->v;
    } else {
      state.reg[dst] = state.cpsr.v;
    }
  }

  state.r15 += 4;
}

template <bool accumulate, bool set_flags>
void ARM_Multiply(u32 instruction) {
  u32 result;

  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  result = state.reg[op1] * state.reg[op2];

  if constexpr (accumulate) {
    result += state.reg[op3];
  }

  if constexpr (set_flags) {
    SetZeroAndSignFlag(result);
  }

  state.reg[dst] = result;
  state.r15 += 4;
}

template <bool sign_extend, bool accumulate, bool set_flags>
void ARM_MultiplyLong(u32 instruction) {
  int op1 = (instruction >> 0) & 0xF;
  int op2 = (instruction >> 8) & 0xF;

  int dst_lo = (instruction >> 12) & 0xF;
  int dst_hi = (instruction >> 16) & 0xF;

  s64 result;

  if constexpr (sign_extend) {
    s64 a = state.reg[op1];
    s64 b = state.reg[op2];

    // Sign-extend operands
    if (a & 0x80000000) a |= 0xFFFFFFFF00000000;
    if (b & 0x80000000) b |= 0xFFFFFFFF00000000;

    result = a * b;
  } else {
    u64 uresult = (u64)state.reg[op1] * (u64)state.reg[op2];

    result = (s64)uresult;
  }

  if constexpr (accumulate) {
    s64 value = state.reg[dst_hi];

    // TODO: in theory we should be able to shift by 32 because value in 64-bit.
    value <<= 16;
    value <<= 16;
    value  |= state.reg[dst_lo];

    result += value;
  }

  u32 result_hi = result >> 32;

  state.reg[dst_lo] = result & 0xFFFFFFFF;
  state.reg[dst_hi] = result_hi;

  if constexpr (set_flags) {
    state.cpsr.f.n = result_hi >> 31;
    state.cpsr.f.z = result == 0;
  }

  state.r15 += 4;
}

template <bool accumulate, bool x, bool y>
void ARM_SignedHalfwordMultiply(u32 instruction) {
  if (arch == Architecture::ARMv4T) {
    // TODO: unclear how this instruction behaves on the ARM7.
    ARM_Undefined(instruction);
    return;
  }

  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  s16 value1;
  s16 value2;

  if constexpr (x) {
    value1 = s16(state.reg[op1] >> 16);
  } else {
    value1 = s16(state.reg[op1] & 0xFFFF);
  }

  if constexpr (y) {
    value2 = s16(state.reg[op2] >> 16);
  } else {
    value2 = s16(state.reg[op2] & 0xFFFF);
  }

  u32 result = u32(value1 * value2);

  if constexpr (accumulate) {
    // Update sticky-flag without saturating the result.
    // TODO: make helper method to detect overflow instead.
    state.reg[dst] = QADD(result, state.reg[op3], false);
  } else {
    state.reg[dst] = result;
  }

  state.r15 += 4;
}

template <bool accumulate, bool y>
void ARM_SignedWordHalfwordMultiply(u32 instruction) {
  if (arch == Architecture::ARMv4T) {
    // TODO: unclear how this instruction behaves on the ARM7.
    ARM_Undefined(instruction);
    return;
  }

  int op1 = (instruction >>  0) & 0xF;
  int op2 = (instruction >>  8) & 0xF;
  int op3 = (instruction >> 12) & 0xF;
  int dst = (instruction >> 16) & 0xF;

  s32 value1 = s32(state.reg[op1]);
  s16 value2;

  if constexpr (y) {
    value2 = s16(state.reg[op2] >> 16);
  } else {
    value2 = s16(state.reg[op2] & 0xFFFF);
  }

  u32 result = u32((value1 * value2) >> 16);

  if constexpr (accumulate) {
    // Update sticky-flag without saturating the result.
    // TODO: make helper method to detect overflow instead.
    state.reg[dst] = QADD(result, state.reg[op3], false);
  } else {
    state.reg[dst] = result;
  }

  state.r15 += 4;
}

template <bool x, bool y>
void ARM_SignedHalfwordMultiplyLongAccumulate(u32 instruction) {
  if (arch == Architecture::ARMv4T) {
    // TODO: unclear how this instruction behaves on the ARM7.
    ARM_Undefined(instruction);
    return;
  }

  int op1 = (instruction >> 0) & 0xF;
  int op2 = (instruction >> 8) & 0xF;
  int dst_lo = (instruction >> 12) & 0xF;
  int dst_hi = (instruction >> 16) & 0xF;

  s16 value1;
  s16 value2;

  if constexpr (x) {
    value1 = s16(state.reg[op1] >> 16);
  } else {
    value1 = s16(state.reg[op1] & 0xFFFF);
  }

  if constexpr (y) {
    value2 = s16(state.reg[op2] >> 16);
  } else {
    value2 = s16(state.reg[op2] & 0xFFFF);
  }

  u64 result = value1 * value2;

  result += state.reg[dst_lo];
  result += u64(state.reg[dst_hi]) << 32;

  state.reg[dst_lo] = result & 0xFFFFFFFF;
  state.reg[dst_hi] = result >> 32;

  state.r15 += 4;
}

template <bool byte>
void ARM_SingleDataSwap(u32 instruction) {
  int src  = (instruction >>  0) & 0xF;
  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;

  u32 tmp;

  if constexpr (byte) {
    tmp = ReadByte(state.reg[base]);
    WriteByte(state.reg[base], (u8)state.reg[src]);
  } else {
    tmp = ReadWordRotate(state.reg[base]);
    WriteWord(state.reg[base], state.reg[src]);
  }

  state.reg[dst] = tmp;
  state.r15 += 4;
}

template <bool link>
void ARM_BranchAndExchangeMaybeLink(u32 instruction) {
  u32 address = state.reg[instruction & 0xF];

  if constexpr (link) {
    if (arch == Architecture::ARMv4T) {
      ARM_Undefined(instruction);
      return;
    }
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
void ARM_HalfDoubleAndSignedTransfer(u32 instruction) {
  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;

  u32 offset;
  u32 address = state.reg[base];

  if constexpr (immediate) {
    offset = (instruction & 0xF) | ((instruction >> 4) & 0xF0);
  } else {
    offset = state.reg[instruction & 0xF];
  }

  state.r15 += 4;

  if constexpr (pre) {
    address += add ? offset : -offset;
  }

  switch (opcode) {
    case 1: {
      if constexpr (load) {
        state.reg[dst] = ReadHalfMaybeRotate(address);
      } else {
        WriteHalf(address, state.reg[dst]);
      }
      break;
    }
    case 2: {
      if constexpr (load) {
        state.reg[dst] = ReadByteSigned(address);
      } else if (arch == Architecture::ARMv5TE) {
        ASSERT((dst & 1) == 0, "ARM: LDRD: using an odd numbered destination register is undefined. r14=0x{0:08X} r15=0x{1:08X}", state.r14, state.r15);
        ASSERT(dst != 14, "ARM: LDRD: loading r14 and r15 is unpredictable. r14=0x{0:08X} r15=0x{1:08X}", state.r14, state.r15);
        ASSERT(!writeback || (dst != base && (dst + 1) != base), "ARM: LDRD: using the base register as a destination is unpredictable r14=0x{0:08X} r15=0x{1:08X}", state.r14, state.r15);

        state.reg[dst + 0] = ReadWord(address + 0);
        state.reg[dst + 1] = ReadWord(address + 4);
      }
      break;
    }
    case 3: {
      if constexpr (load) {
        state.reg[dst] = ReadHalfSigned(address);
      } else if (arch == Architecture::ARMv5TE) {
        if (dst & 1) {
          std::ofstream file{"itcm.bin", std::ios::out | std::ios::binary};
          auto size = memory->itcm.mask + 1;
          for (uint i = 0; i < size; i++) {
            u8 data = memory->itcm.data[i];
            file.write((char*)&data, 1);
          }
          file.close();
          LOG_DEBUG("DBG: IRQ handler address = 0x{0:08X}", memory->FastRead<u32, Bus::Data>(memory->dtcm.config.base + 0x3FFC));
        }

        ASSERT((dst & 1) == 0, "ARM: STRD: using an odd numbered destination register is undefined. r14=0x{0:08X} r15=0x{1:08X}", state.r14, state.r15);

        WriteWord(address + 0, state.reg[dst + 0]);
        WriteWord(address + 4, state.reg[dst + 1]);
      }
      break;
    }
    default:
      UNREACHABLE;
  }

  if (!load || base != dst) {
    if constexpr (!pre) {
      state.reg[base] += add ? offset : -offset;
    } else if (writeback) {
      state.reg[base] = address;
    }
  }
}

template <bool link>
void ARM_BranchAndLink(u32 instruction) {
  u32 offset = instruction & 0xFFFFFF;

  if (offset & 0x800000) {
    offset |= 0xFF000000;
  }

  if constexpr (link) {
    state.r14 = state.r15 - 4;
  }

  state.r15 += offset * 4;
  ReloadPipeline32();
}

void ARM_BranchLinkExchangeImm(u32 instruction) {
  u32 offset = instruction & 0xFFFFFF;

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
void ARM_SingleDataTransfer(u32 instruction) {
  u32 offset;

  int dst  = (instruction >> 12) & 0xF;
  int base = (instruction >> 16) & 0xF;
  u32 address = state.reg[base];

  constexpr bool translation = !pre && writeback;

  ASSERT(!translation, "ARM: unhandled LDRT or STRT instruction");

  // Calculate offset relative to base register.
  if constexpr (immediate) {
    offset = instruction & 0xFFF;
  } else {
    int carry  = state.cpsr.f.c;
    int opcode = (instruction >> 5) & 3;
    int amount = (instruction >> 7) & 0x1F;

    offset = state.reg[instruction & 0xF];
    DoShift(opcode, offset, amount, carry, true);
  }

  state.r15 += 4;

  if constexpr (pre) {
    address += add ? offset : -offset;
  }

  if constexpr (load) {
    if constexpr (byte) {
      state.reg[dst] = ReadByte(address);
    } else {
      state.reg[dst] = ReadWordRotate(address);
    }
  } else {
    if constexpr (byte) {
      WriteByte(address, (u8)state.reg[dst]);
    } else {
      WriteWord(address, state.reg[dst]);
    }
  }

  // Writeback final address to the base register.
  if (!load || base != dst) {
    if constexpr (!pre) {
      state.reg[base] += add ? offset : -offset;
    } else if (writeback) {
      state.reg[base] = address;
    }
  }

  if constexpr (load) {
    if (dst == 15) {
      if ((state.r15 & 1) && arch != Architecture::ARMv4T) {
        ASSERT(!byte && !translation, "ARM: unpredictable LDRB or LDRT with destination = r15.");
        state.cpsr.f.thumb = 1;
        state.r15 &= ~1;
        ReloadPipeline16();
      } else {
        ReloadPipeline32();
      }
    }
  }
}

template <bool pre, bool add, bool user_mode, bool writeback, bool load>
void ARM_BlockDataTransfer(u32 instruction) {
  int list = instruction & 0xFFFF;
  int base = (instruction >> 16) & 0xF;

  Mode mode;
  bool transfer_pc = list & (1 << 15);

  int bytes;
  u32 base_new;
  u32 address = state.reg[base];
  bool base_is_first = false;
  bool base_is_last = false;

  if (list != 0) {
    #if defined(__has_builtin) && __has_builtin(__builtin_popcount)
      bytes = __builtin_popcount(list) * sizeof(u32);
    #else
      bytes = 0;
      for (int i = 0; i <= 15; i++) {
        if (list & (1 << i))
          bytes += sizeof(u32);
      }
    #endif

    #if defined(__has_builtin) && __has_builtin(__builtin_ctz)
      base_is_first = __builtin_ctz(list) == base;
    #else
      base_is_first = (list & ((1 << base) - 1)) == 0;
    #endif

    #if defined(__has_builtin) && __has_builtin(__builtin_clz)
      base_is_last = (31 - __builtin_clz(list)) == base;
    #else
      base_is_last = (list >> base) == 1;
    #endif
  } else {
    bytes = 16 * sizeof(u32);
    if (arch == Architecture::ARMv4T) {
      list = 1 << 15;
      transfer_pc = true;
    }
  }

  if constexpr (!add) {
    address -= bytes;
    base_new = address;
  } else {
    base_new = address + bytes;
  }

  state.r15 += 4;

  // STM ARMv4: store new base if base is not the first register and old base otherwise.
  // STM ARMv5: always store old base.
  if constexpr (writeback && !load) {
    if (arch == Architecture::ARMv4T && !base_is_first) {
      state.reg[base] = base_new;
    }
  }

  if constexpr (user_mode) {
    if (!load || !transfer_pc) {
      mode = state.cpsr.f.mode;
      SwitchMode(MODE_USR);
    }
  }

  int i = 0;
  u32 remaining = list;

  while (remaining != 0) {
    #if defined(__has_builtin) && __has_builtin(__builtin_ctz)
      i = __builtin_ctz(remaining);
    #else
      while ((remaining & (1 << i)) == 0) i++;
    #endif

    if constexpr (pre == add) {
      address += 4;
    }

    if constexpr (load) {
      state.reg[i] = ReadWord(address);
    } else {
      WriteWord(address, state.reg[i]);
    }

    if constexpr (pre != add) {
      address += 4;
    }

    remaining &= ~(1 << i);
  }

  if constexpr (user_mode) {
    if (load && transfer_pc) {
      auto& spsr = *p_spsr;

      SwitchMode(spsr.f.mode);
      state.cpsr.v = spsr.v;
    } else {
      SwitchMode(mode);
    }
  }

  if constexpr (writeback) {
    if constexpr (load) {
      switch (arch) {
        case Architecture::ARMv5TE:
          // LDM ARMv5: writeback if base is the only register or not the last register.
          if (!base_is_last || list == (1 << base))
            state.reg[base] = base_new;
          break;
        case Architecture::ARMv4T:
          // LDM ARMv4: writeback if base in not in the register list.
          if (!(list & (1 << base)))
            state.reg[base] = base_new;
          break;
      }
    } else {
      state.reg[base] = base_new;
    }
  }

  if constexpr (load) {
    if (transfer_pc) {
      if ((state.r15 & 1) && !user_mode && arch != Architecture::ARMv4T) {
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
}

void ARM_Undefined(u32 instruction) {
  LOG_ERROR("undefined instruction: 0x{0:08X} @ r15 = 0x{1:08X}", instruction, state.r15);

  // Save current program status register.
  state.spsr[BANK_UND].v = state.cpsr.v;

  // Enter UND mode and disable IRQs.
  SwitchMode(MODE_UND);
  state.cpsr.f.mask_irq = 1;

  // Save current program counter and jump to UND exception vector.
  state.r14 = state.r15 - 4;
  state.r15 = ExceptionBase() + 0x04;
  ReloadPipeline32();
}

void ARM_SWI(u32 instruction) {
  // Save current program status register.
  state.spsr[BANK_SVC].v = state.cpsr.v;

  // Enter SVC mode and disable IRQs.
  SwitchMode(MODE_SVC);
  state.cpsr.f.mask_irq = 1;

  // Save current program counter and jump to SVC exception vector.
  state.r14 = state.r15 - 4;
  state.r15 = ExceptionBase() + 0x08;
  ReloadPipeline32();
}

void ARM_CountLeadingZeros(u32 instruction) {
  if (arch == Architecture::ARMv4T) {
    ARM_Undefined(instruction);
    return;
  }

  int dst = (instruction >> 12) & 0xF;
  int src =  instruction & 0xF;

  u32 value = state.reg[src];

  if (value == 0) {
    state.reg[dst] = 32;
    state.r15 += 4;
    return;
  }

  #if defined(__has_builtin) && __has_builtin(__builtin_clz)
    state.reg[dst] = __builtin_clz(value);
  #else
    u32 result = 0;

    const u32 mask[] = {
      0xFFFF0000,
      0xFF000000,
      0xF0000000,
      0xC0000000,
      0x80000000 };
    const int shift[] = { 16, 8, 4, 2, 1 };

    for (int i = 0; i < 5; i++) {
      if ((value & mask[i]) == 0) {
        result |= shift[i];
        value <<= shift[i];
      }
    }

    state.reg[dst] = result;
  #endif

  state.r15 += 4;
}

template <int opcode>
void ARM_SaturatingAddSubtract(u32 instruction) {
  if (arch == Architecture::ARMv4T) {
    ARM_Undefined(instruction);
    return;
  }

  int src1 =  instruction & 0xF;
  int src2 = (instruction >> 16) & 0xF;
  int dst  = (instruction >> 12) & 0xF;
  u32 op2  = state.reg[src2];

  if constexpr ((opcode & 0b1001) != 0) {
    ARM_Undefined(instruction);
    return;
  }

  bool subtract = opcode & 2;
  bool double_op2 = opcode & 4;

  if (double_op2) {
    u32 result = op2 + op2;

    if ((op2 ^ result) >> 31) {
      state.cpsr.f.q = 1;
      result = 0x80000000 - (result >> 31);
    }

    op2 = result;
  }

  if (subtract) {
    state.reg[dst] = QSUB(state.reg[src1], op2);
  } else {
    state.reg[dst] = QADD(state.reg[src1], op2);
  }

  state.r15 += 4;
}

void ARM_CoprocessorRegisterTransfer(u32 instruction) {
  auto dst = (instruction >> 12) & 0xF;
  auto cp_rm = instruction & 0xF;
  auto cp_rn = (instruction >> 16) & 0xF;
  auto opcode1 = (instruction >> 21) & 7;
  auto opcode2 = (instruction >>  5) & 7;
  auto cp_num  = (instruction >>  8) & 0xF;

  if (coprocessors[cp_num] == nullptr) {
    ARM_Undefined(instruction);
    return;
  }

  if (instruction & (1 << 20)) {
    state.reg[dst] = coprocessors[cp_num]->Read(opcode1, cp_rn, cp_rm, opcode2);
  } else {
    coprocessors[cp_num]->Write(opcode1, cp_rn, cp_rm, opcode2, state.reg[dst]);
  }

  state.r15 += 4;
}

void ARM_Unimplemented(u32 instruction) {
  ASSERT(false, "unimplemented instruction: 0x{0:08X} @ r15 = 0x{1:08X}", instruction, state.r15);
}
