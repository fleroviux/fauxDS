/*
 * Copyright (C) 2020 fleroviux
 */

#pragma once

#include <common/integer.hpp>
#include <fstream>
#include <string>

namespace fauxDS::core {

struct Cartridge {
  Cartridge() { Reset(); }

  void Reset();
  void Load(std::string const& path);

  struct ROMCTRL {
    ROMCTRL(Cartridge& cart) : cart(cart) {}

    auto ReadByte (uint offset) -> u8;
    void WriteByte(uint offset, u8 value);

  private:
    friend struct fauxDS::core::Cartridge;

    // TODO: implement the remaining data fields.
    // http://problemkaputt.de/gbatek.htm#dscartridgeioports
    bool data_ready = false;
    int  data_block_size = 0;
    bool busy = false;

    Cartridge& cart;
  } romctrl { *this };

  struct CARDCMD {
    auto ReadByte (uint offset) -> u8;
    void WriteByte(uint offset, u8 value);

  private:
    friend struct fauxDS::core::Cartridge;

    u8 buffer[8] {0};
  } cardcmd;

private:
  void OnCommandStart();

  bool loaded = false;
  std::fstream file;
};

} // namespace fauxDS::core
