#include "wrapper.hpp"
#include "common.hpp"
#include "tstring.hpp"
#include "token_iterator.hpp"

#include <sstream>
#include <iostream>

NAMESPACE(lini::node)

base_pp wrapper::get_child_ptr(tstring path) const {
  if (auto immediate_path = cut_front(trim(path), '.'); !immediate_path.untouched()) {
    if (auto iterator = map.find(immediate_path); iterator != map.end() && iterator->second)
      if (auto child = dynamic_cast<container*>(iterator->second->get()); child)
        return child->get_child_ptr(path);
  } else if (auto iterator = map.find(path); iterator != map.end())
    return iterator->second;
  return {};
}

base_pp wrapper::add(tstring path, base_p&& value) {
  // Check path for invalid characters
  trim(path);
  for(char c : path)
    if(auto invalid = strchr(" #$\"'(){}[]", c); invalid)
      throw error("Invalid character '" + string{*invalid} + "' in path: " + path);

  if (auto immediate_path = cut_front(path, '.'); !immediate_path.untouched()) {
    // This isn't the final part of the path
    auto& ptr = map[immediate_path];
    if (!ptr) {
      ptr = std::make_shared<base_p>(std::make_unique<wrapper>());
    }
    auto& child_ptr = *ptr;
    if (!child_ptr) {
      auto tmp = std::make_unique<wrapper>();
      auto res = tmp->add(path, move(value));
      child_ptr = move(tmp);
      return res;
    } else if (auto child = dynamic_cast<addable*>(child_ptr.get()); child) {
      return child->add(path, move(value));
    } else {
      auto node = new wrapper();
      node->value = move(child_ptr);
      child_ptr = base_p(node);
      return node->add(path, move(value));
    }
  } else {
    // This is the final part of the path
    auto& place = map[path];
    if (!place) {
      place = std::make_shared<base_p>(move(value));
    } else if (*place) {
      if (auto node = dynamic_cast<wrapper*>(place->get()); node) {
        node->value = move(value);
      } else {
        throw error("Duplicate key: " + static_cast<string>(path));
      }
    } else
      *place = move(value);
    return place;
  }
}

void wrapper::iterate_children(std::function<void(const string&, const base&)> processor) const {
  for(auto pair : map) {
    if (pair.second && *pair.second)
    processor(pair.first, **pair.second);
  }
}

string wrapper::get() const {
  return value ? value->get() : "";
}

NAMESPACE_END
