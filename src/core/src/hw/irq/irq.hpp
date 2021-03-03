/*
 * Copyright (C) 2020 fleroviux
 */

#pragma once

#include <util/integer.hpp>

#include "arm/arm.hpp"

namespace Duality::core {

/// ARM7 and ARM9 interrupt controller.
struct IRQ {
  enum class Source : u32 {
    VBlank = 1 << 0,
    HBlank = 1 << 1,
    VCount = 1 << 2,
    Timer0 = 1 << 3,
    Timer1 = 1 << 4,
    Timer2 = 1 << 5,
    Timer3 = 1 << 6,
    DMA0 = 1 << 8,
    DMA1 = 1 << 9,
    DMA2 = 1 << 10,
    DMA3 = 1 << 11,
    IPC_Sync = 1 << 16,
    IPC_SendEmpty = 1 << 17,
    IPC_ReceiveNotEmpty = 1 << 18,
    Cart_DataReady = 1 << 19,
    GXFIFO = 1 << 21,
    SPI = 1 << 23
  };

  IRQ(bool arm9);

  void Reset();
  void SetCore(arm::ARM& core) { this->core = &core; UpdateIRQLine(); }
  void Raise(Source source);
  bool IsEnabled();
  bool HasPendingIRQ();

  /// Interrupt Master Enable
  struct IME {
    auto ReadByte (uint offset) -> u8;
    void WriteByte(uint offset, u8 value);

  private:
    friend struct Duality::core::IRQ;

    bool enabled = false;
    IRQ* irq = nullptr;
  } ime;

  /// Interrupt Enable
  struct IE {
    auto ReadByte (uint offset) -> u8;
    void WriteByte(uint offset, u8 value);

  //private:
    friend struct Duality::core::IRQ;

    u32 value = 0;
    IRQ* irq = nullptr;
  } ie;

  /// Interrupt Flag / Acknowledge
  struct IF {
    auto ReadByte (uint offset) -> u8;
    void WriteByte(uint offset, u8 value);

  //private:
    friend struct Duality::core::IRQ;

    u32 value = 0;
    IRQ* irq = nullptr;
  } _if;

private:
  void UpdateIRQLine();

  arm::ARM* core = nullptr;
  bool arm9;
};

} // namespace Duality::core
