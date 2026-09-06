#include <sigildata/decode/Csv.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <string>
#include <vector>

namespace sigil::data {

namespace {

/** The days from 1970-01-01 to a civil date, for every proleptic
 *  Gregorian date in the era arithmetic covers. March is taken as the
 *  first month so that the leap day falls at the end of a year and the
 *  four-century cycle is one closed form with no case in it. */
long daysFromCivil(long year, unsigned month, unsigned day) {
  year -= month <= 2;
  const long era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy =
      (153u * (month + (month > 2 ? -3 : 9)) + 2u) / 5u + day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return era * 146097L + static_cast<long>(doe) - 719468L;
}

bool digits(std::string_view text, size_t at, size_t count) {
  if (at + count > text.size()) return false;
  for (size_t i = 0; i < count; ++i)
    if (!std::isdigit(static_cast<unsigned char>(text[at + i]))) return false;
  return true;
}

unsigned number(std::string_view text, size_t at, size_t count) {
  unsigned value = 0;
  for (size_t i = 0; i < count; ++i)
    value = value * 10u + static_cast<unsigned>(text[at + i] - '0');
  return value;
}

std::string_view trimmed(std::string_view text) {
  while (!text.empty() &&
         (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
    text.remove_prefix(1);
  while (!text.empty() &&
         (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
    text.remove_suffix(1);
  return text;
}

/** Whether @p field is a number written with thousands separators —
 *  digits in groups of three after a leading group of one to three, and
 *  nothing else but a sign and a fractional part. Only that exact shape
 *  is read, because a lone comma between digits is a decimal point in
 *  half the world and guessing which would silently multiply a value by
 *  a thousand. */
bool grouped(std::string_view field) {
  if (!field.empty() && (field.front() == '-' || field.front() == '+'))
    field.remove_prefix(1);
  const size_t point = field.find('.');
  std::string_view whole = field.substr(0, point);
  if (point != std::string_view::npos) {
    const std::string_view fraction = field.substr(point + 1);
    if (fraction.empty() || fraction.find(',') != std::string_view::npos)
      return false;
  }
  const size_t first = whole.find(',');
  if (first == std::string_view::npos || first == 0 || first > 3) return false;
  for (size_t i = 0; i < first; ++i)
    if (!std::isdigit(static_cast<unsigned char>(whole[i]))) return false;
  for (size_t at = first; at < whole.size(); at += 4) {
    if (whole[at] != ',' || at + 4 > whole.size()) return false;
    if (!digits(whole, at + 1, 3)) return false;
  }
  return true;
}

std::optional<double> asNumber(std::string_view field) {
  if (field.empty()) return std::nullopt;
  std::string plain;
  if (grouped(field)) {
    for (char c : field)
      if (c != ',') plain.push_back(c);
    field = plain;
  }
  const char* first = field.data();
  const char* last = first + field.size();
  double value = 0;
  const auto [stopped, error] = std::from_chars(first, last, value);
  if (error != std::errc{} || stopped != last) return std::nullopt;
  return value;
}

std::optional<Flag> asFlag(std::string_view text) {
  std::string word;
  for (char c : text)
    word.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  if (word == "true" || word == "yes") return Flag(true);
  if (word == "false" || word == "no") return Flag(false);
  return std::nullopt;
}

/** Which of the delimiters cuts @p line into the most fields, ties going
 *  to the earlier one in this list. Quoting is honoured, so a comma
 *  inside a quoted field does not vote for the comma. */
char inferredDelimiter(std::string_view line) {
  constexpr std::array<char, 3> candidates = {',', '\t', ';'};
  char best = ',';
  size_t most = 0;
  for (char candidate : candidates) {
    size_t fields = 1;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '"') {
        if (quoted && i + 1 < line.size() && line[i + 1] == '"')
          ++i;
        else
          quoted = !quoted;
      } else if (!quoted && line[i] == candidate) {
        ++fields;
      }
    }
    if (fields > most) {
      most = fields;
      best = candidate;
    }
  }
  return best;
}

/** Every row of @p text, each row a vector of fields, with the quoting
 *  rule applied: a quoted field may hold the delimiter, a newline and a
 *  quote written twice. */
std::vector<std::vector<std::string>> rowsOf(std::string_view text,
                                             char delimiter, char comment) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false;
  bool wasQuoted = false;
  bool started = false;  // whether anything of this row has been seen

  // An unquoted field's surrounding space is layout — `a, b` is two
  // fields, not one of them beginning with a space. A quoted field's is
  // its own, and is kept.
  auto endField = [&] {
    if (!wasQuoted) field = std::string(trimmed(field));
    row.push_back(std::move(field));
    field.clear();
    wasQuoted = false;
  };
  auto endRow = [&] {
    endField();
    rows.push_back(std::move(row));
    row.clear();
    started = false;
  };

  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field.push_back('"');
          ++i;
        } else {
          quoted = false;
        }
      } else {
        field.push_back(c);
      }
      continue;
    }
    if (c == '"' && field.empty()) {
      quoted = true;
      wasQuoted = true;
      started = true;
      continue;
    }
    if (c == delimiter) {
      started = true;
      endField();
      continue;
    }
    if (c == '\n' || c == '\r') {
      if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
      if (started || !row.empty() || !field.empty()) endRow();
      continue;
    }
    started = true;
    field.push_back(c);
  }
  if (started || !row.empty() || !field.empty()) endRow();

  if (comment != '\0')
    std::erase_if(rows, [comment](const std::vector<std::string>& line) {
      return !line.empty() && !line.front().empty() &&
             line.front().front() == comment;
    });
  return rows;
}

/** One column built from the cells at @p index of every row, typed by
 *  what every non-empty cell of it turns out to be. */
Column columnOf(const std::vector<std::vector<std::string>>& rows, size_t index,
                std::string name) {
  const size_t count = rows.size();
  bool everyNumber = true, everyFlag = true, everyInstant = true;
  bool anyValue = false;
  std::vector<bool> absent(count, false);

  for (size_t row = 0; row < count; ++row) {
    const std::string_view cell = index < rows[row].size()
                                      ? std::string_view(rows[row][index])
                                      : std::string_view{};
    if (cell.empty()) {
      absent[row] = true;
      continue;
    }
    anyValue = true;
    if (everyNumber && !asNumber(cell)) everyNumber = false;
    if (everyFlag && !asFlag(cell)) everyFlag = false;
    if (everyInstant && !decodeInstant(cell)) everyInstant = false;
  }

  auto cellAt = [&](size_t row) -> std::string_view {
    return index < rows[row].size() ? std::string_view(rows[row][index])
                                    : std::string_view{};
  };

  Column column;
  if (anyValue && everyFlag) {
    std::vector<Flag> cells(count);
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) cells[row] = *asFlag(cellAt(row));
    column = Column(std::move(name), std::move(cells));
  } else if (anyValue && everyInstant) {
    std::vector<Instant> cells(count);
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) cells[row] = *decodeInstant(cellAt(row));
    column = Column(std::move(name), std::move(cells));
  } else if (anyValue && everyNumber) {
    std::vector<double> cells(count, std::nan(""));
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) cells[row] = *asNumber(cellAt(row));
    column = Column(std::move(name), std::move(cells));
  } else {
    std::vector<std::string> cells(count);
    for (size_t row = 0; row < count; ++row)
      if (!absent[row]) cells[row] = std::string(cellAt(row));
    column = Column(std::move(name), std::move(cells));
  }
  for (size_t row = 0; row < count; ++row)
    if (absent[row]) column.markMissing(row);
  return column;
}

bool endsWith(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         text.substr(text.size() - suffix.size()) == suffix;
}

}  // namespace

std::optional<Instant> decodeInstant(std::string_view text) {
  const std::string_view stamp = trimmed(text);
  // YYYY-MM-DD is the shortest thing that names a day.
  if (stamp.size() < 10) return std::nullopt;
  if (!digits(stamp, 0, 4) || stamp[4] != '-' || !digits(stamp, 5, 2) ||
      stamp[7] != '-' || !digits(stamp, 8, 2))
    return std::nullopt;
  const long year = static_cast<long>(number(stamp, 0, 4));
  const unsigned month = number(stamp, 5, 2);
  const unsigned day = number(stamp, 8, 2);
  if (month < 1 || month > 12 || day < 1 || day > 31) return std::nullopt;
  double seconds =
      static_cast<double>(daysFromCivil(year, month, day)) * 86400.0;
  if (stamp.size() == 10) return Instant{seconds};

  if (stamp[10] != 'T' && stamp[10] != ' ') return std::nullopt;
  if (!digits(stamp, 11, 2) || stamp.size() < 16 || stamp[13] != ':' ||
      !digits(stamp, 14, 2))
    return std::nullopt;
  const unsigned hour = number(stamp, 11, 2);
  const unsigned minute = number(stamp, 14, 2);
  if (hour > 23 || minute > 59) return std::nullopt;
  seconds += hour * 3600.0 + minute * 60.0;
  std::string_view rest = stamp.substr(16);
  if (!rest.empty() && rest.front() == ':') {
    if (!digits(rest, 1, 2)) return std::nullopt;
    seconds += number(rest, 1, 2);
    rest.remove_prefix(3);
    if (!rest.empty() && rest.front() == '.') {
      size_t at = 1;
      double scale = 0.1;
      while (at < rest.size() &&
             std::isdigit(static_cast<unsigned char>(rest[at]))) {
        seconds += (rest[at] - '0') * scale;
        scale *= 0.1;
        ++at;
      }
      if (at == 1) return std::nullopt;
      rest.remove_prefix(at);
    }
  }
  if (rest == "Z") rest = {};
  if (!rest.empty()) return std::nullopt;
  return Instant{seconds};
}

std::optional<Table> decodeCsv(std::string_view text, const CsvOptions& options,
                               std::string_view name) {
  // A byte order mark is a claim about the encoding, not a field.
  if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);
  if (trimmed(text).empty()) return std::nullopt;

  char delimiter = options.delimiter;
  if (delimiter == '\0') {
    if (endsWith(name, ".tsv") || endsWith(name, ".tab"))
      delimiter = '\t';
    else if (endsWith(name, ".csv"))
      delimiter = ',';
    else
      delimiter = inferredDelimiter(text.substr(0, text.find('\n')));
  }

  std::vector<std::vector<std::string>> rows =
      rowsOf(text, delimiter, options.comment);
  if (rows.empty()) return std::nullopt;

  std::vector<std::string> names;
  if (options.header) {
    names = std::move(rows.front());
    rows.erase(rows.begin());
    for (size_t i = 0; i < names.size(); ++i) {
      names[i] = std::string(trimmed(names[i]));
      if (names[i].empty()) names[i] = std::to_string(i + 1);
    }
  } else {
    size_t widest = 0;
    for (const std::vector<std::string>& row : rows)
      widest = std::max(widest, row.size());
    for (size_t i = 0; i < widest; ++i) names.push_back(std::to_string(i + 1));
  }
  if (names.empty()) return std::nullopt;

  Table table;
  for (size_t i = 0; i < names.size(); ++i)
    table.add(columnOf(rows, i, std::move(names[i])));
  return table;
}

}  // namespace sigil::data
