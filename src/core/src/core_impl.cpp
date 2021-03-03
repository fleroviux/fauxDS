/*
 * Copyright (C) 2021 fleroviux
 */

#include <core/core.hpp>
#include <fstream>
#include <stdexcept>

#include "arm/arm.hpp"
#include "arm7/arm7.hpp"
#include "arm9/arm9.hpp"
#include "interconnect.hpp"

namespace Duality::core {

/// NDS file header
struct Header {
  // TODO: add remaining header fields.
  // This should be enough for basic direct boot for now though.
  // http://problemkaputt.de/gbatek.htm#dscartridgeheader
  u8 game_title[12];
  u8 game_code[4];
  u8 maker_code[2];
  u8 unit_code;
  u8 encryption_seed_select;
  u8 device_capacity;
  u8 reserved0[8];
  u8 nds_region;
  u8 version;
  u8 autostart;
  struct Binary {
    u32 file_address;
    u32 entrypoint;
    u32 load_address;
    u32 size;
  } arm9, arm7;
} __attribute__((packed));

struct CoreImpl {
  CoreImpl(std::string const& rom_path)
      : arm7(interconnect)
      , arm9(interconnect) {
    Load(rom_path);
  }

  void SetAudioDevice(AudioDevice& device) {
    interconnect.apu.SetAudioDevice(device);
  }

  void SetInputDevice(InputDevice& device) {
    interconnect.SetInputDevice(device);
  }

  void SetVideoDevice(VideoDevice& device) {
    interconnect.video_unit.SetVideoDevice(device);
  }

  void Run(uint cycles) {
    auto& scheduler = interconnect.scheduler;
    auto& irq9 = interconnect.irq9;

    auto frame_target = scheduler.GetTimestampNow() + cycles - overshoot;

    LOG_INFO("ARM9 IRQ: IME={0} IE=0x{1:08X} IF=0x{2:08X}", irq9.IsEnabled(), irq9.ie.value, irq9._if.value);

    while (scheduler.GetTimestampNow() < frame_target) {
      uint cycles = 1;

      // Run both CPUs individually for up to 32 cycles, but make sure
      // that we do not run past any hardware event.
      if (gLooselySynchronizeCPUs) {
        u64 target = std::min(frame_target, scheduler.GetTimestampTarget());
        // Run to the next event if both CPUs are halted.
        // Otherwise run each CPU for up to 32 cycles.
        cycles = target - scheduler.GetTimestampNow();
        if (!arm9.IsHalted() || !arm7.IsHalted()) {
          cycles = std::min(32U, cycles);
        }
      }

      arm9.Run(cycles * 2);
      arm7.Run(cycles);

      scheduler.AddCycles(cycles);
      scheduler.Step();
    }

    overshoot = scheduler.GetTimestampNow() - frame_target;
  }

  void Load(std::string const& rom_path) {
    using Bus = arm::MemoryBase::Bus;

    std::ifstream rom { rom_path, std::ios::in | std::ios::binary };
    if (!rom.good()) {
      throw std::runtime_error("failed to open ROM");
    }

    auto header = Header{};
    rom.read((char*)&header, sizeof(Header));
    if (!rom.good()) {
      throw std::runtime_error("failed to read ROM header, not enough data.");
    }

    {
      u8 data;
      u32 dst = header.arm7.load_address;
      rom.seekg(header.arm7.file_address);
      for (u32 i = 0; i < header.arm7.size; i++) {
        rom.read((char*)&data, 1);
        if (!rom.good()) {
          throw std::runtime_error("failed to read ARM7 binary from ROM into ARM7 memory");
        }
        arm7.Bus().WriteByte(dst++, data, Bus::Data);
      }

      arm7.Reset(header.arm7.entrypoint);
    }

    {
      u8 data;
      u32 dst = header.arm9.load_address;
      rom.seekg(header.arm9.file_address);
      for (u32 i = 0; i < header.arm9.size; i++) {
        rom.read((char*)&data, 1);
        if (!rom.good()) {
          throw std::runtime_error("failed to read ARM9 binary from ROM into ARM9 memory");
        }
        arm9.Bus().WriteByte(dst++, data, Bus::Data);
      }

      arm9.Reset(header.arm9.entrypoint);
    }

    u8 data;
    u32 dst = 0x02FFFE00;
    rom.seekg(0);
    for (u32 i = 0; i < 0x170; i++) {
      rom.read((char*)&data, 1);
      if (!rom.good()) {
        throw std::runtime_error("failed to load cartridge header into memory");
      }
      arm9.Bus().WriteByte(dst++, data, Bus::Data);
    }

    rom.close();

    // Load cartridge ROM into Slot 1
    interconnect.cart.Load(rom_path);

    // Huge thanks to Hydr8gon for pointing this out:
    arm9.Bus().WriteWord(0x027FF800, 0x1FC2, Bus::Data); // Chip ID 1
    arm9.Bus().WriteWord(0x027FF804, 0x1FC2, Bus::Data); // Chip ID 2
    arm9.Bus().WriteHalf(0x027FF850, 0x5835, Bus::Data); // ARM7 BIOS CRC
    arm9.Bus().WriteHalf(0x027FF880, 0x0007, Bus::Data); // Message from ARM9 to ARM7
    arm9.Bus().WriteHalf(0x027FF884, 0x0006, Bus::Data); // ARM7 boot task
    arm9.Bus().WriteWord(0x027FFC00, 0x1FC2, Bus::Data); // Copy of chip ID 1
    arm9.Bus().WriteWord(0x027FFC04, 0x1FC2, Bus::Data); // Copy of chip ID 2
    arm9.Bus().WriteHalf(0x027FFC10, 0x5835, Bus::Data); // Copy of ARM7 BIOS CRC
    arm9.Bus().WriteHalf(0x027FFC40, 0x0001, Bus::Data); // Boot indicator
  }

  u64 overshoot = 0;

  Interconnect interconnect;
  ARM7 arm7;
  ARM9 arm9;
};

Core::Core(std::string const& rom_path) {
  pimpl = new CoreImpl(rom_path);
}

Core::~Core() {
  delete pimpl;
}

void Core::SetAudioDevice(AudioDevice& device) {
  pimpl->SetAudioDevice(device);
}

void Core::SetInputDevice(InputDevice& device) {
  pimpl->SetInputDevice(device);
}

void Core::SetVideoDevice(VideoDevice& device) {
  pimpl->SetVideoDevice(device);
}

void Core::Run(uint cycles) {
  pimpl->Run(cycles);
}

} // namespace Duality::core
