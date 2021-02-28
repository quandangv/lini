#include "wrapper.hpp"
#include "common.hpp"
#include "tstring.hpp"
#include "token_iterator.hpp"

#include <sstream>
#include <iostream>

NAMESPACE(lini::node)

void wrapper::iterate_children(std::function<void(const string&, const base&)> processor) const {
  iterate_children([&](const string& name, const base_p& child) {
    if (child)
      processor(name, *child);
  });
}

base_p wrapper::get_child_ptr(tstring path) const {
  if (auto immediate_path = cut_front(trim(path), '.'); !immediate_path.untouched()) {
    if (auto iterator = map.find(immediate_path); iterator != map.end())
      if (auto child = dynamic_cast<wrapper*>(iterator->second.get()); child)
        return child->get_child_ptr(path);
  } else if (auto iterator = map.find(path); iterator != map.end())
    return iterator->second;
  return {};
}

std::optional<string> wrapper::get_child(const tstring& path) const {
  auto ptr = get_child_ptr(path);
  return ptr ? ptr->get() : std::optional<string>{};
}

string wrapper::get_child(const tstring& path, string&& fallback) const {
  auto result = get_child(path);
  return result ? *result : move(fallback);
}

base_p& wrapper::get_child_ref(tstring path) {
  if (auto immediate_path = cut_front(trim(path), '.'); !immediate_path.untouched()) {
    if (auto iterator = map.find(immediate_path); iterator != map.end())
      if (auto child = dynamic_cast<wrapper*>(iterator->second.get()); child)
        return child->get_child_ref(path);
  } else if (auto iterator = map.find(path); iterator != map.end())
    return iterator->second;
  throw base::error("Key is empty");
}

bool wrapper::set(const tstring& path, const string& value) {
  auto target = dynamic_cast<settable*>(get_child_ptr(path).get());
  return target ? target->set(value) : false;
}

base_p& wrapper::add(tstring path, ancestor_processor* processor) {
  trim(path);
  for(char c : path)
    if(auto invalid = strchr(" #$\"'(){}[]", c); invalid)
      throw error("Invalid character '" + string{*invalid} + "' in path: " + path);

  if (auto immediate_path = cut_front(path, '.'); !immediate_path.untouched()) {
    // This isn't the final part of the path
    auto& ptr = map[immediate_path];
    auto ancestor = !ptr ? assign(ptr, std::make_shared<wrapper>())
        : dynamic_cast<wrapper*>(ptr.get()) ?: &wrap(ptr);
    if (processor)
      processor->operator()(immediate_path, ancestor);
    return ancestor->add(path);
  } else {
    // This is the final part of the path
    auto& place = map[path];
    if (!place)
      return place;
    wrapper* wrpr = dynamic_cast<wrapper*>(place.get());
    return wrpr ? wrpr->value : place;
  }
}

base_p& wrapper::add(tstring path, const base_p& value) {
  auto& place = add(path);
  return place ? throw error("Duplicate key") : (place = value);
}

base_p& wrapper::add(tstring path, string& raw, tstring value) {
  return add(path, parse_string(raw, value, [&](tstring& ts, const base_p& fallback) { return make_address_ref(ts, fallback); }));
}

base_p& wrapper::add(tstring path, string raw) {
  return add(path, raw, tstring(raw));
}

std::shared_ptr<address_ref> wrapper::make_address_ref(const tstring& ts, const base_p& fallback) {
  return std::make_unique<address_ref>(*this, ts, move(fallback));
}

wrapper& wrapper::wrap(base_p& place) {
  return *assign(place, std::make_shared<wrapper>(place));
}

void wrapper::iterate_children(std::function<void(const string&, const base_p&)> processor) const {
  for(auto& pair : map)
    processor(pair.first, pair.second);
}

string wrapper::get() const {
  return value ? value->get() : "";
}

base_p wrapper::clone(clone_context& context) const {
  auto result = std::make_shared<wrapper>();
  context.ancestors.emplace_back(this, result.get());
  LG_DBUG("Value clone");
  try {
    if (value)
      result->value = value->clone(context);
    LG_DBUG("End Value clone");
  } catch (const std::exception& e) {
    context.report_error(e.what());
    LG_DBUG("End Value clone (catch)");
  }
  for(auto& pair : map) {
    auto last_path = context.current_path;
    if (context.current_path.empty())
      context.current_path = pair.first;
    else
      context.current_path += "." + pair.first;
    LG_DBUG("Key clone");
    try {
      if (pair.second)
        if (auto& place = result->add(pair.first); !place)
          place = pair.second->clone(context);
      LG_DBUG("end Key clone");
    } catch (const std::exception& e) {
      context.report_error(e.what());
      LG_DBUG("end Key clone (catch)");
    }
    context.current_path = last_path;
  }
  context.ancestors.pop_back();
  return result;
}

NAMESPACE_END
