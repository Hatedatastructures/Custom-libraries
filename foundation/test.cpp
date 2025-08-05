#include "Template.hpp"
int main()
{
  con::hash_map<con::string,int> hash_table;
  hash_table.push({"hello", 1});
  hash_table.push({"world", 2});
  hash_table.push({"hello", 3});
  hash_table.push({"world", 4});
  hash_table.push({"hello", 5});
  for(auto &i : hash_table)
    std::cout << i.first << " " << i.second << std::endl;
  return 0;
}