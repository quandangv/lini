#include "test.hxx"

#include <fstream>

struct parse_test_single {
  string path, value, parsed;
  bool is_fixed{true}, fail{false}, exception{false}, clone_fail{false};
};

bool operator==(const std::pair<string, string>& pair, const parse_test_single& test) {
  return test.path == pair.first;
}

using parse_test = vector<parse_test_single>;

void test_nodes(parse_test testset, int repeat = base_repeat) {
  auto doc = std::make_shared<node::wrapper>();

  node::parse_context context;
  context.root = context.parent = doc;
  // Add keys to doc
  for(auto test : testset) {
    auto last_count = get_test_part_count();
    context.raw = test.value;
    tstring ts(context.raw);
    try {
      doc->add(test.path, context, ts);
    } catch (const std::exception& e) {
      EXPECT_TRUE(test.fail) << "Unexpected exception: " << e.what();
    }
    if (last_count != get_test_part_count())
      cerr << "Key: " << test.path << endl;
  }
  auto test_doc = [&](node::base_s node, node::errorlist& errs) {
    auto doc = std::dynamic_pointer_cast<node::wrapper>(node);
    for (auto test : testset) {
      // Skip key if it is expected to fail in the previous step
      if (test.fail || test.clone_fail) {
        if (auto pos = std::find(errs.begin(), errs.end(), test); pos != errs.end())
          errs.erase(pos);
        //std::erase_if(errs, [&](auto pair) { return pair.first == test.path; });
      } else {
        check_key(*doc, test.path, test.parsed, test.exception, test.is_fixed);
      }
    }
  };
  triple_node_test(doc, test_doc, repeat);

  //Test for wrapper::optimize
  node::clone_context clone_ctx;
  doc->optimize(clone_ctx);
  for (auto test : testset) {
    auto found_error = std::find(clone_ctx.errors.begin(), clone_ctx.errors.end(), test);
    EXPECT_TRUE(found_error == clone_ctx.errors.end() || test.fail || test.clone_fail)
        << "Key: " << test.path;
  }
}

TEST(Node, Simple) {
  test_nodes({
    {"key", "foo", "foo"},
    {"a.ref", "${key}", "foo"},
    {"a.ref-space", "${ key }", "foo"},
    {"newline", "hello\\nworld", "hello\nworld"},
  });
  test_nodes({{"empty", "", ""}});
}

TEST(Node, Cmd) {
  test_nodes({
    {"msg", "1.000", "1.000"},
    {"cmd", "${cmd 'echo ${msg}'}", "1.000", false},
    {"cmd-ref", "${map 1 2 ${cmd}}", "2", false},
    {"cmd-msg", "result is ${cmd-ref}", "result is 2", false},
  }, base_repeat / 100);
  test_nodes({{"cmd1", "${cmd \"echo 'hello  world'\"}", "hello  world", false}}, base_repeat / 100);
  test_nodes({{"cmd2", "${cmd 'echo hello world'}", "hello world", false}}, base_repeat / 100);
  test_nodes({{"cmd3", "${cmd nexist}", "", false, false, true}}, base_repeat / 100);
  test_nodes({{"cmd4", "${cmd nexist ? fail}", "fail", false}}, base_repeat / 100);
  test_nodes({{"cmd4", "${cmd nexist ?}", "", false}}, base_repeat / 100);
  test_nodes({
    {"greeting", "${cmd 'echo Hello ${rel name}'}", "Hello quan", false},
    {"greeting.name", "quan", "quan"},
  }, base_repeat / 100);
}

TEST(Node, Ref) {
  test_nodes({
    {"test2.ref-fake", "{test.key-a}", "{test.key-a}"},
    {"test2.ref-file-default-before", "${file nexist.txt ? ${test3.ref-ref-a}}", "a", false},
    {"test2.ref-before", "${test2.ref-a}", "a"},
    {"test.key-a", "a", "a"},
    {"test2.ref-a", "${test.key-a}", "a"},
    {"test3.ref-ref-a", "${test2.ref-a?failed}", "a"},
    {"test.ref-ref-a", "${test2.ref-a?failed}", "a"},
    {"test2.ref-default-a", "${test.key-nexist?${test.key-a}}", "a", false},
    {"test2.ref-file-default", "${file nexist.txt ? ${test.key-a}}", "a", false},
    {"test2.ref-nexist", "${test.key-nexist? \" f a i l ' }", "\" f a i l '", false},
    {"test2.ref-fail", "${test.key-fail}", "${test.key-fail}", true, false, true, true},
    {"test2.escape", "\\${test.key-a}", "${test.key-a}"},
    {"test2.not-escape", "\\$${test.key-a}", "\\$a"},
  });
  test_nodes({
    {"ref-cyclic-1", "${ref-cyclic-2}", "${ref-cyclic-1}", true, false, true, true},
    {"ref-cyclic-2", "${ref-cyclic-1}", "${ref-cyclic-1}", true, false, true, true},
  });
  test_nodes({
    {"ref-cyclic-1", "${.ref-cyclic-2}", "${ref-cyclic-1}", true, false, true, true},
    {"ref-cyclic-2", "${.ref-cyclic-3}", "${ref-cyclic-1}", true, false, true, true},
    {"ref-cyclic-3", "${.ref-cyclic-1}", "${ref-cyclic-1}", true, false, true, true},
    {"ref-not-cyclic-1", "${ref-not-cyclic-2}", ""},
    {"ref-not-cyclic-2", "", ""}
  });
  test_nodes({
    {"ref-empty", "${empty}", ""},
    {"empty", "", ""},
  });
  test_nodes({{"dep", "${dep fail fail2}", "", false, true}});
  test_nodes({{"dep", "${rel fail fail2}", "", false, true}});
}

TEST(Node, strsub) {
  test_nodes({
    {"key-a", "a", "a"},
    {"strsub", "This is ${key-a} test", "This is a test"},
    {"strsub2", "$ ${key-a}", "$ a"},
    {"strsub3", "} ${key-a}", "} a"},
    {"strsub4", "} ${key-a}", "} a"},
    {"strsub5", "} ${key-a}", "} a"},
    {"strsub6", "} ${key-a}", "} a"},
  }, base_repeat * 10);
}

TEST(Node, strsub_nested) {
  setenv("test_env", "a", true);
  test_nodes({
    {"key-a", "${env test_env}", "a", false},
    {"strsub", "This is ${key-a} test", "This is a test", false},
    {"strsub2", "'${strsub}' is a statement", "'This is a test' is a statement", false},
    {"strsub3", "${strsub2}", "'This is a test' is a statement", false},
    {"strsub4", "${strsub2}", "'This is a test' is a statement", false},
    {"strsub5", "${strsub2}", "'This is a test' is a statement", false},
    {"strsub6", "${strsub2}", "'This is a test' is a statement", false},
  }, base_repeat * 10);
}

TEST(Node, gradient) {
  test_nodes({{"gradient", "${gradient '#000 1:#FFF' ${gradient_var}}", "", false, true}});
  test_nodes({{"gradient", "${gradient '#000 1:#FFF' ${gradient_var} 0}", "", false, true}});
  test_nodes({{"gradient", "${gradient '0:#000 1:#FFF' 0.5}", "#777777"}});
}

TEST(Node, File) {
  test_nodes({
    {"ext", "txt", "txt"},
    {"file", "${file key_file.${ext} ? fail}", "content", false},
    {"file-fail", "${file nexist.${ext} ? Can't find ${ext} file}", "Can't find txt file", false},
  });
  test_nodes({{"file1", "${file key_file.txt }", "content", false}});
  test_nodes({{"file2", "${file key_file.txt?fail}", "content", false}});
  test_nodes({{"file3", "${file nexist.txt ?   ${file key_file.txt}}", "content", false}});
  test_nodes({{"file4", "${file nexist.txt ? \" f a i l ' }", "\" f a i l '", false}});
  test_nodes({{"file5", "${file nexist.txt}", "${file nexist.txt}", false, false, true}});
}

TEST(Node, Color) {
  test_nodes({
    {"color", "${color #123456 }", "#123456"},
    {"color-fallback", "${color nexist(1) ? #ffffff }", "#ffffff"},
    {"color-hsv", "${color hsv(180, 1, 0.75)}", "#00BFBF"},
    {"color-ref", "${color ${color}}", "#123456"},
    {"color-mod", "${color cielch 'lum * 1.5, hue + 60' ${color}}", "#633E5C"},
  });
  test_nodes({
    {"clone5.stat", "60", "60"},
    {"color-fail", "${color ${clone clone5}}", "", false, true},
  });
}

TEST(Node, Clone) {
  test_nodes({
    {"clone_source", "${color #123456 }", "#123456"},
    {"clone_source.lv1", "abc", "abc"},
    {"clone_source.lv1.lv2", "abc", "abc"},
    {"clone_source.dumb", "${nexist}", "", true, false, true, true},
    {"clone", "${clone clone_source }", "#123456"},
    {"clone.lv1", "def", "def", false, true},
    {"clone.lv1.lv2", "def", "def", false, true},
    {"clone.lv1.dumb", "abc", "abc"},
    {"clone_merge", "${clone clone_source clone}", "#123456"},
    {"clone_merge.lv1.dumb", "def", "def", false, true},
  });
  test_nodes({{"clone2", "${clone nexist nexist2 }", "", false, true}});
  test_nodes({{"clone3", "${clone nexist}", "", false, true}});
  test_nodes({
    {"base", "${map 100 1 ${rel stat}}", "", true, false, true, true},
    {"clone4.stat", "60", "60"},
    {"clone4", "${clone base}", "0.6"},
    {"clone4.stat", "60", "60", false, true},
  });
  test_nodes({
    {"src1.key1", "a", "a"},
    {"src2.key2.b", "b", "b"},
    {"src3", "c", "c"},
    {"merge", "${clone src1 src2 src3}", "c"},
    {"merge-fail", "${clone src3 src2 src1}", "", false, true},
    {"merge-wrap.key2", "a", "a"},
    {"merge-wrap", "${clone src2 src3}", "c"},
  });
}

TEST(Node, Other) {
  setenv("test_env", "test_env", true);
  unsetenv("nexist");
  test_nodes({{"strsub", "%{${color hsv(0, 1, 0.5)}}", "%{#800000}"}});
  test_nodes({{"dumb1", "${dumb nexist.txt}", "${dumb nexist.txt}", false, true}});
  test_nodes({{"dumb2", "", ""}});
  test_nodes({{"dumb3", "${}", "", false, true}});
  test_nodes({{"env0", "${env 'test_env' ? fail}", "test_env", false}});
  test_nodes({{"env1", "${env ${nexist ? test_env}}", "test_env", false}});
  test_nodes({{"env2", "${env nexist? \" f a i l \" }", " f a i l ", false}});
  test_nodes({{"env3", "${env nexist test_env }", "", false, true}});
  test_nodes({{"map", "${map 5:10 0:2 7.5}", "1"}});
  test_nodes({{"map", "${map 5:10 0:2 20}", "2"}});
  test_nodes({{"map", "${map 5:10 2 7.5 ? -1}", "1"}});
  test_nodes({{"map", "${map 5:10 7.5}", "1", false, true}});
  test_nodes({
    {"source", "60", "60"},
    {"cache", "${cache ${source} hello}", "hello", false},
  });
  test_nodes({
    {"source", "${var float 100.25}", "100.25", false},
    {"cache", "${cache ${cache 1000 ${source}} 420}", "420", false},
  });
  test_nodes({{"cache", "${cache 123 hello 456}", "hello", false, true}});
  test_nodes({{"cache", "${cache abf hello}", "hello", false, true}});
}
