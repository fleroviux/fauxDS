/*
 * Copyright (C) 2020 fleroviux
 */

#pragma once

#include <util/integer.hpp>
#include <util/log.hpp>
#include <core/device/input_device.hpp>
#include <functional>
#include <string.h>
#include <vector>

#include "hw/apu/apu.hpp"
#include "hw/cart/cart.hpp"
#include "hw/dma/dma7.hpp"
#include "hw/dma/dma9.hpp"
#include "hw/ipc/ipc.hpp"
#include "hw/math/math_engine.hpp"
#include "hw/irq/irq.hpp"
#include "hw/spi/spi.hpp"
#include "hw/timer/timer.hpp"
#include "hw/video_unit/video_unit.hpp"
#include "scheduler.hpp"

namespace Duality::core {

// TODO: this whole construct really needs to go away :methharold:
struct Interconnect {
  Interconnect()
      : apu(scheduler)
      , irq7(false)
      , irq9(true)
      , cart(irq7, irq9, dma7, dma9) 
      , ipc(irq7, irq9)
      , spi(irq7)
      , timer7(scheduler, irq7)
      , timer9(scheduler, irq9)
      , dma7(irq7)
      , dma9(irq9)
      , video_unit(scheduler, irq7, irq9, dma7, dma9)
      , wramcnt(swram) {
    Reset();
  }

  void Reset() {
    memset(ewram, 0, sizeof(ewram));
    memset(swram.data, 0, sizeof(swram));
    
    scheduler.Reset();
    apu.Reset();
    cart.Reset();
    irq7.Reset();
    irq9.Reset();
    math_engine.Reset();
    ipc.Reset();
    spi.Reset();
    timer7.Reset();
    timer9.Reset();
    dma7.Reset();
    dma9.Reset();
    video_unit.Reset();

    // TODO: this is the value for direct boot,
    // which value is correct for firmware boot?
    wramcnt.WriteByte(3);
  }

  void SetInputDevice(InputDevice& device) {
    keyinput.SetInputDevice(device);
    extkeyinput.SetInputDevice(device);
    spi.tsc.SetInputDevice(device);
  }

  u8 ewram[0x400000];

  struct SWRAM {
    u8 data[0x8000];

    struct Alloc {
      u8* data = nullptr;
      u32 mask = 0;
    } arm9 = {}, arm7 = {};
  } swram;

  Scheduler scheduler;
  APU apu;
  Cartridge cart;
  IRQ irq7;
  IRQ irq9;
  MathEngine math_engine;
  IPC ipc;
  SPI spi;
  Timer timer7;
  Timer timer9;
  DMA7 dma7;
  DMA9 dma9;
  VideoUnit video_unit;

  struct WRAMCNT {
    using Callback = std::function<void(void)>;

    WRAMCNT(SWRAM& swram) : swram(swram) {}

    void AddCallback(Callback callback) {
      callbacks.push_back(callback);
    }

    auto ReadByte() -> u8 {
      return value;
    }

    void WriteByte(u8 value) {
      this->value = value & 3;

      switch (this->value) {
        case 0:
          swram.arm9 = { swram.data, 0x7FFF };
          swram.arm7 = { nullptr, 0 };
          break;
        case 1:
          swram.arm9 = { &swram.data[0x4000], 0x3FFF };
          swram.arm7 = { &swram.data[0x0000], 0x3FFF };
          break;
        case 2:
          swram.arm9 = { &swram.data[0x0000], 0x3FFF };
          swram.arm7 = { &swram.data[0x4000], 0x3FFF };
          break;
        case 3:
          swram.arm9 = { nullptr, 0 };
          swram.arm7 = { swram.data, 0x7FFF };
          break;
      }

      for (auto const& callback : callbacks) callback();
    }

  private:
    int value = 0;
    SWRAM& swram;
    std::vector<Callback> callbacks;
  } wramcnt;

  struct KeyInput {
    void SetInputDevice(InputDevice& device) {
      input_device = &device;
    }

    auto ReadByte(uint offset) -> u8 {
      using Key = InputDevice::Key;

      switch (offset) {
        case 0:
          if (input_device == nullptr) return 0xFF;

          return (input_device->IsKeyDown(Key::A)      ? 0 :   1) |
                 (input_device->IsKeyDown(Key::B)      ? 0 :   2) |
                 (input_device->IsKeyDown(Key::Select) ? 0 :   4) |
                 (input_device->IsKeyDown(Key::Start)  ? 0 :   8) |
                 (input_device->IsKeyDown(Key::Right)  ? 0 :  16) |
                 (input_device->IsKeyDown(Key::Left)   ? 0 :  32) |
                 (input_device->IsKeyDown(Key::Up)     ? 0 :  64) |
                 (input_device->IsKeyDown(Key::Down)   ? 0 : 128);
        case 1:
          if (input_device == nullptr) return 3;

          return (input_device->IsKeyDown(Key::R) ? 0 : 1) |
                 (input_device->IsKeyDown(Key::L) ? 0 : 2);
      }

      UNREACHABLE;
    }

    InputDevice* input_device = nullptr;
  } keyinput = {};

  struct ExtKeyInput {
    void SetInputDevice(InputDevice& device) {
      input_device = &device;
    }

    auto ReadByte() -> u8 {
      using Key = InputDevice::Key;

      if (input_device == nullptr) return 67;

      return (input_device->IsKeyDown(Key::X) ? 0 : 1) |
             (input_device->IsKeyDown(Key::Y) ? 0 : 2) |
             (input_device->IsKeyDown(Key::TouchPen) ? 0 : 64);
    }

    InputDevice* input_device = nullptr;
  } extkeyinput = {};
};

} // namespace Duality::core
