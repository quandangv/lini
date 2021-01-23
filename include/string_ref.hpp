#pragma once

#include "error.hpp"

#include <string>
#include <optional>
#include <memory>
#include <cspace/processor.hpp>

namespace lini {
  struct document;
  struct string_ref;
  using std::string;
  using string_ref_p = std::unique_ptr<string_ref>;

  struct string_ref {
    struct error : error_base { using error_base::error_base; };

    virtual string get() const = 0;
    virtual bool readonly() const { return true; }
    virtual void set(const string&) {}
    virtual ~string_ref() {}
    virtual string_ref_p get_optimized() { return {}; }
  };
  [[nodiscard]] string_ref_p optimize(string_ref_p&);

  struct onetime_ref : public string_ref {
    mutable string val;

    onetime_ref(string&& val) : val(val) {}
    string get() const { return val; }
    string get_onetime() const { return move(val); }
  };
    
  struct const_ref : public string_ref {
    string val;

    const_ref(string&& val) : val(val) {}
    string get() const { return val; }
    bool readonly() const { return false; }
    void set(const string& value) { val = value; }
  };

  struct local_ref : public string_ref {
    const string_ref_p& ref;

    local_ref(const string_ref_p& ref) : ref(ref) {}
    string get() const { return ref->get(); }
    bool readonly() const { return ref->readonly(); }
    void set(const string& value) { ref->set(value); }
  };

  struct soft_local_ref : public string_ref {
    const document& doc;
    string section, key;
    string_ref_p fallback;

    soft_local_ref(string&& section, string&& key, const document& doc, string_ref_p&& fallback)
      : section(section), key(key), doc(doc), fallback(move(fallback)) {}
    string get() const;
    string_ref_p get_optimized();
  };

  struct fallback_ref : public string_ref {
    string_ref_p fallback;

    string use_fallback(const string& error_message) const;
  };

  struct meta_ref : public fallback_ref {
    string_ref_p value;
  };

  struct color_ref : public meta_ref {
    cspace::processor processor;

    string get() const;
  };

  struct env_ref : public meta_ref {
    string get() const;
    bool readonly() const;
    void set(const string& value);
  };

  struct cmd_ref : public meta_ref {
    string get() const;
  };

  struct file_ref : public meta_ref {
    string get() const;
    bool readonly() const;
    void set(const string& value);
    struct error : error_base { using error_base::error_base; };

  };

  struct string_interpolate_ref : public string_ref {
    struct replacement_list : public std::iterator<std::forward_iterator_tag, string> {
      struct iterator;
      std::vector<string_ref_p> list;

      iterator begin() const;
      iterator end() const;
    } replacements;
    string base;
    std::vector<size_t> positions;

    string get() const;
  };
}
