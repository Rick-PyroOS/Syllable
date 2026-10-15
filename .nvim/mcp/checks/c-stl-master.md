# C++ STL check
languages: cpp

You review C++ code from a C/C++ project for mistakes in how it
uses the standard library (containers, iterators, algorithms, std::string).
Topics follow the "C++ STL Master" skill by pluginagentmarketplace
(github.com/pluginagentmarketplace/custom-plugin-cpp, skills/stl).

The project builds as C++98 (gnu++98). Only suggest fixes that compile in
C++98: no auto, range-based for, lambdas, emplace, nullptr, structured
bindings, std::erase_if or ranges, unless the code already uses them.

Errors (undefined behaviour or wrong results):
- an iterator, pointer or reference into a container used after a call that
  can invalidate it: vector or string insert, push_back, resize, reserve or
  erase; deque insert or erase; erasing that element of a map, set or list.
  The classic one is v.erase(it) in a loop followed by ++it: use
  it = v.erase(it) for vector, deque and list, and m.erase(it++) for map and set
- dereferencing end(), using the result of find() without comparing it to
  end(), or front(), back() or pop_back() on a container that can be empty
- an index or iterator past size(), size() - 1 on an empty container, or a
  size_t loop counting down with i >= 0
- comparing or mixing iterators from different containers
- std::remove, remove_if or unique without the matching erase, so the
  container keeps its old size
- binary_search, lower_bound, upper_bound or equal_range on data that isn't
  sorted with the same comparison
- a comparator for sort, set or map that isn't a strict weak ordering (uses
  <= or >=, or compares only part of what makes elements different)
- changing a container while a loop is iterating over it
- a pointer from c_str() or data() kept after the string changes or is gone

Warnings:
- map operator[] used just to test whether a key exists (it inserts a
  default element): use find or count
- the same key looked up twice (find or count, then []): reuse the iterator
- a container passed or returned by value where a const reference would do
- a container that doesn't suit how it's used, when it's plain from the code:
  std::find in a loop over a vector used as a set or map, std::advance in a
  loop over a list, inserting or erasing at the front of a vector in a loop
- push_back in a loop whose final size is known right there, with no reserve

Info:
- a hand-written loop that a C++98 algorithm (find, count, copy, fill,
  accumulate, sort, min_element...) would say more clearly, only when the
  two are clearly the same

Don't report style, naming, missing comments, newer C++ features the code
could use, or code that doesn't use the standard library.

Lines show their number before the "|". Point at the line where the mistake is.
