#include "document.hpp"

#include <sstream>
#include <iostream>

using std::endl;

string to_string(const document& doc) {
  std::stringstream ss;
  auto print_keyval = [&](const section::value_type& keyval) {
    ss << keyval.first.to_string() << " = ";
    if (keyval.second.empty())
      ss << endl;
    else if (keyval.second.front() == ' ' || keyval.second.back() == ' ')
      ss << '"' << keyval.second.to_string() << '"' << endl;
    else 
      ss << keyval.second.to_string() << endl;
  };
  if(doc.find(fixed_string()) != doc.end())
    for(auto& keyval : doc.at(fixed_string()))
      print_keyval(keyval);

  for(auto& sec : doc) {
    if(sec.first.empty()) continue;
    ss << endl << '[' << sec.first.to_string() << ']' << endl;
    for(auto& keyval : sec.second)
      print_keyval(keyval);
  }
  return ss.str();
}
