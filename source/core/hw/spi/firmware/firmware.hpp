/*
 * Copyright (C) fleroviux
 */

#pragma once

#include <core/hw/spi/spi_device.hpp>
#include <fstream>

namespace fauxDS::core {

/// SPI firmware flash
struct Firmware : SPIDevice {
  void Reset();

  void Select() override;
  void Deselect() override;
  auto Transfer(u8 data) -> u8 override;

private:
  enum class Command : u8 {
    WriteEnable   = 0x06, // WREM
    WriteDisable  = 0x04, // WRDI
    ReadJEDEC     = 0x96, // RDID
    ReadStatus    = 0x05, // RDSR
    ReadData      = 0x03, // READ
    ReadDataFast  = 0x0B, // FAST
    PageWrite     = 0x0A, // PW
    PageProgram   = 0x02, // PP
    PageErase     = 0xDB, // PE
    SectorErase   = 0xD8, // SE
    DeepPowerDown = 0xB9, // DP
    ReleaseDeepPowerDown = 0xAB // RDP
  };

  enum class State {
    ReceiveCommand,
    ReadAddress0,
    ReadAddress1,
    ReadAddress2,
    ReadData,
    Deselected
  };

  void ParseCommand(u8 command);

  State state;
  Command command;
  u32 address;

  std::fstream file;
};

} // namespace fauxDS::core

