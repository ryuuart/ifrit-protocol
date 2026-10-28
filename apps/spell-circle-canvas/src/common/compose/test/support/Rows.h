#pragma once
// A table of named rows for a case that states one promise: each row is
// one instance of the promise, checked under its own name, so a failure
// reads as the promise and the row that broke it and every other row
// still runs.

#include <gtest/gtest.h>

#include <functional>
#include <initializer_list>

namespace sigil::compose::test {

/** One instance of a promise whose check is its own: the row's
 *  expectations run inside @p check. An `ASSERT_*` inside a check ends
 *  that row alone. */
struct Row {
  const char* name;
  std::function<void()> check;
};

/** Runs every row under its name. */
inline void checkRows(std::initializer_list<Row> rows) {
  for (const Row& row : rows) {
    SCOPED_TRACE(row.name);
    row.check();
  }
}

/** Runs @p check on every row of a data table under the row's `name`. */
template <class Table, class Check>
void forEachRow(const Table& rows, Check&& check) {
  for (const auto& row : rows) {
    SCOPED_TRACE(row.name);
    check(row);
  }
}

}  // namespace sigil::compose::test
