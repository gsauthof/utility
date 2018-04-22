// 2018, Georg Sauthoff <mail@gms.tf>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <iostream>

// oh no! just uses naive implementation:
// https://www.boost.org/doc/libs/1_67_0/doc/html/string_algo/design.html#string_algo.find
// #include <boost/algorithm/string/find.hpp>
// there is also KMP (and BMH) - but no two-way (as of boost 1.64):
// #include <boost/algorithm/searching/knuth_morris_pratt.hpp>
// #include <boost/algorithm/searching/boyer_moore_horspool.hpp>
// better would be: a wrapper that calls different search algorithms
// depending on the sizes of q and t

#include <ixxx/util.hh>

static int real_main(int argc, char **argv)
{
  if (argc != 3) {
    std::cerr << "Call: " << argv[0] << " PATTERN_FILE SRC_FILE\n";
    return 2;
  }
  auto q = ixxx::util::mmap_file(argv[1]);
  auto t = ixxx::util::mmap_file(argv[2]);
  auto r = std::search(t.begin(), t.end(), q.begin(), q.end());
  //auto r = boost::algorithm::boyer_moore_horspool_search(t.begin(), t.end(), q.begin(), q.end()).first;
  //auto r = boost::algorithm::knuth_morris_pratt_search(t.begin(), t.end(), q.begin(), q.end()).first;
  if (r == t.end())
    return 1;
  else
    std::cout << (r - t.begin()) << '\n';
  return 0;
}

int main(int argc, char **argv)
{
  try {
    return real_main(argc, argv);
  } catch (std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
