/*
 * Copyright (C) 2020 fleroviux
 */

#pragma once

#include <array>
#include <buildconfig.hpp>
#include <util/integer.hpp>
#include <util/likely.hpp>
#include <util/meta.hpp>
#include <util/punning.hpp>
#include <memory>

#include <util/log.hpp>

namespace Duality::core::arm {

/** Base class that memory systems must implement
  * in order to be connected to an ARM core.
  * Provides an uniform interface for ARM cores to
  * access memory.
  */
struct MemoryBase {
  enum class Bus : int {
    Code,
    Data,
    System
  };

  virtual auto ReadByte(u32 address, Bus bus) ->  u8 = 0;
  virtual auto ReadHalf(u32 address, Bus bus) -> u16 = 0;
  virtual auto ReadWord(u32 address, Bus bus) -> u32 = 0;
  virtual auto ReadQuad(u32 address, Bus bus) -> u64 = 0;
  
  virtual void WriteByte(u32 address, u8  value, Bus bus) = 0;
  virtual void WriteHalf(u32 address, u16 value, Bus bus) = 0;
  virtual void WriteWord(u32 address, u32 value, Bus bus) = 0;
  virtual void WriteQuad(u32 address, u64 value, Bus bus) = 0;

  template<typename T, Bus bus>
  auto FastRead(u32 address) -> T {
    static_assert(common::is_one_of_v<T, u8, u16, u32, u64>);

    address &= ~(sizeof(T) - 1);

    if (address >= dawn_addr && address < (dawn_addr + 0x3438)) {
      LOG_DEBUG("DBG: read dawn[{0}] = 0x{1:08X}, r14=0x{2:08X} r15=0x{3:08X}", address - dawn_addr, ReadWord(address, Bus::Data), r14, r15);
    }

    if constexpr (gEnableFastMemory && bus != Bus::System) {
      if (itcm.config.enable_read &&
          address >= itcm.config.base &&
          address <= itcm.config.limit) {
        return read<T>(itcm.data, (address - itcm.config.base) & itcm.mask);
      }
    }

    if constexpr (gEnableFastMemory && bus == Bus::Data) {
      if (dtcm.config.enable_read && address >= dtcm.config.base && address <= dtcm.config.limit) {
        return read<T>(dtcm.data, (address - dtcm.config.base) & dtcm.mask);
      }
    }

    if (gEnableFastMemory && likely(pagetable != nullptr)) {
      auto page = (*pagetable)[address >> kPageShift];
      if (likely(page != nullptr)) {
        return read<T>(page, address & kPageMask);
      }
    }

    if constexpr (std::is_same_v<T,  u8>) return ReadByte(address, bus);
    if constexpr (std::is_same_v<T, u16>) return ReadHalf(address, bus);
    if constexpr (std::is_same_v<T, u32>) return ReadWord(address, bus);
    if constexpr (std::is_same_v<T, u64>) return ReadQuad(address, bus);
  }

  template<typename T, Bus bus>
  void FastWrite(u32 address, T value) {
    static_assert(common::is_one_of_v<T, u8, u16, u32, u64>);

    address &= ~(sizeof(T) - 1);

    // auto a = ReadWord(0x020cd1ac, Bus::Data) + 0x20;
    // //auto b = ReadWord(a, Bus::Data);
    // if (address == a) {
    //   LOG_DEBUG("DBG: write to target pointer 0x{0:08X}=0x{1:08X}  r14=0x{2:08X} r15=0x{3:08X}", a, value, r14, r15);
    // }

    // if constexpr (std::is_same_v<T, u32>) {
    //   if (param1 != 0xFFFFFFFF && address == (param1 + 0x30)) {
    //     LOG_DEBUG("DBG: set *(0x{0:08X} + 0x30) = 0x{1:08X}, r14=0x{2:08X} r15=0x{3:08X}", param1, value, r14, r15);
    //   }
    // }

    if (address >= dawn_addr && address < (dawn_addr + 0x3438)) {
      LOG_DEBUG("DBG: write dawn[{0}] = 0x{1:08X}", address - dawn_addr, value);
    }

    if ((address >> 24) == 0x06) {
      LOG_DEBUG("DBG: VRAM write 0x{0:08X} = 0x{1:08X}, r14=0x{2:08X}, r15=0x{3:08X}", address, value, r14, r15);
    }


    if constexpr (gEnableFastMemory && bus != Bus::System) {
      if (itcm.config.enable &&
          address >= itcm.config.base &&
          address <= itcm.config.limit) {
        if constexpr (std::is_same_v<T, u32>) {
          // if (value == 0x30585442) {  // BTX0
          //   LOG_DEBUG("DBG: wrote 'BTX0' to ITCM r14=0x{0:08X} r15=0x{1:08X}", r14, r15);
          // }
        }
        write<T>(itcm.data, (address - itcm.config.base) & itcm.mask, value);
        return;
      }

      if (dtcm.config.enable &&
          address >= dtcm.config.base &&
          address <= dtcm.config.limit) {
        write<T>(dtcm.data, (address - dtcm.config.base) & dtcm.mask, value);
        return;
      }
    }

    if (gEnableFastMemory && likely(pagetable != nullptr)) {
      auto page = (*pagetable)[address >> kPageShift];
      if (likely(page != nullptr)) {
        write<T>(page, address & kPageMask, value);
        return;
      }
    }

    if constexpr (std::is_same_v<T,  u8>) WriteByte(address, value, bus);
    if constexpr (std::is_same_v<T, u16>) WriteHalf(address, value, bus);
    if constexpr (std::is_same_v<T, u32>) WriteWord(address, value, bus);
    if constexpr (std::is_same_v<T, u64>) WriteQuad(address, value, bus);
  }

  static constexpr int kPageShift = 12; // 2^12 = 4096
  static constexpr int kPageMask = (1 << kPageShift) - 1;

  std::unique_ptr<std::array<u8*, 1048576>> pagetable = nullptr;

  struct TCM {
    u8* data = nullptr;
    u32 mask = 0;

    struct Config {
      bool enable = false;
      bool enable_read = false;
      u32 base = 0;
      u32 limit = 0;
    } config;
  } itcm, dtcm;

  u32 r14 = 0xDEADBEEF;
  u32 r15 = 0xDEADBEEF;
  u32 param1 = 0xFFFFFFFF;
  u32 dawn_addr = 0xFFFFFFFF;
};

} // namespace Duality::core::arm
