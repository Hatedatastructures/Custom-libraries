#include "protocol.hpp"
#include <iostream>
#include <string>

int main()
{
  // protocol::request req;
  // req.header().set_method("GET");
  // protocol::response resp;
  // resp.header().set_status_code(200);
  // resp.header().set_status_message("OK");
  protocol::json object;
  object.set("name", "wang");
  object.set("age", 18);
  std::cout << object.to_string() << std::endl;
  //
  return 0;
}
