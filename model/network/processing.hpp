#pragma once
#include <iostream>
namespace configuration
{
  static constexpr std::uint8_t  current_version = 0x01;
  static constexpr std::uint64_t request_header_size = 9;
  static constexpr std::uint64_t response_header_size = 16;
  static constexpr std::uint32_t packet_identifier = 0x20250921;
}