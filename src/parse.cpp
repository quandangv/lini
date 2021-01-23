#include "parse.hpp"
#include "logger.hpp"
#include "tstring.hpp"
#include "add_key.hpp"

#include <vector>
#include <cstring>
#include <iostream>

GLOBAL_NAMESPACE

using namespace std;

constexpr const char excluded_chars[] = "\t \"'=;#[](){}:.$\\%";
constexpr const char comment_chars[] = ";#";


std::istream& parse(std::istream& is, document& doc, errorlist& err, const string& initial_section) {
  auto report_err_key = [&](const string& section, const string& key, const string& msg) {
    err.emplace_back("key " + section + "." + key, msg);
  };
  // Add initial section
  auto current_sec = &doc.map.emplace(initial_section, document::sec_map{}).first->second;
  string raw, current_section = initial_section;
  // Iterate through lines
  for (int linecount = 1; std::getline(is, raw); linecount++, raw.clear()) {
    tstring line(raw);
    auto report_err_line = [&](const string& msg) {
      err.emplace_back("line " + to_string(linecount), msg);
    };
    auto check_name = [&](const tstring& name) {
      // Determines if the name is valid
      auto invalid = std::find_if(name.begin(), name.end(), [](char ch) { return strchr(excluded_chars, ch); });
      if (invalid == name.end())
        return true;
      report_err_line("Invalid character '" + string{*invalid} + "' in name");
      return false;
    };
    ltrim(line);
    // skip empty and comment lines
    if (!line.empty() && !strchr(comment_chars, line.front())) {
      if (cut_front_back(line, "[", "]")) {
        // detected section header
        if (check_name(line)) {
          current_section = line;
          current_sec = &doc.map[current_section];
        }
      } else if (auto key = cut_front(line, '='); !key.untouched()) {
        // detected key line
        trim(key);
        if (check_name(key)) {
          trim_quotes(line);
          try {
            add_key(doc, current_section, key, raw, line);
          } catch (const exception& err) {
            report_err_key(current_section, key, err.what());
          }
        } else report_err_line("Invalid key");
      } else report_err_line("Unparsed line");
    }
  }
  for(auto& section : doc.map) {
    for(auto& key : section.second) {
      try {
        auto& ref = doc.values[key.second];
        if (auto op = ref->get_optimized(); op) ref = move(op);
      } catch(const exception& e) {
        report_err_key(section.first, key.first, e.what());
      }
    }
  }
  return is;
}

ostream& write(ostream& os, const document& doc) {
  auto print_keyval = [&](const document::sec_map::value_type& keyval) {
    os << keyval.first << " = ";
    auto value = doc.values[keyval.second]->get();
    if (value.empty())
      os << endl;
    else if (value.front() == ' ' || value.back() == ' ')
      os << '"' << value << '"' << endl;
    else {
      if (auto pos = value.find("${"); pos != string::npos)
        value.insert(value.begin() + pos, '\\');
      os << value << endl;
    }
  };
  for(auto& sec : doc.map) {
    if(!sec.first.empty())
      os << endl << '[' << sec.first << ']' << endl;
    for(auto& keyval : sec.second)
      print_keyval(keyval);
  }
  return os;
}

GLOBAL_NAMESPACE_END
