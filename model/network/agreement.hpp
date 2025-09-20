#include <iostream>
#include <vector>
#include <string>
#include <string>
#include <json/json.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include "encryption.hpp"
namespace agreement
{
  class agreement_internal
  {
  public:
    uint16_t _package_length;
    uint8_t _package_version;
    uint16_t _Package_type;
    std::string _package_data;
  };
} //end agreement