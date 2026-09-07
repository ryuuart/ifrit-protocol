// Catalogue.cpp — the three files Catalogue.h names, read as tables.
//
// One decoder answers all three: a data file is a resource like an image
// is, so the hub caches it, reloads it when it changes on disc, and this
// unit holds no table of its own.

#include "Catalogue.h"

#include <sigildata/table/Table.h>
#include <sigilsketch/core/Assets.h>

#include <charconv>
#include <memory>
#include <string>
#include <string_view>

namespace dunhuang {
namespace {

using sigil::data::Table;

std::shared_ptr<const Table> read(sigil::sketch::Assets& assets,
                                  std::string_view name) {
  return assets.table("data/dunhuang/" + std::string(name));
}

/** The vertex words of one asterism, appended in the order the file
 *  wrote them: whitespace-separated star indices, a kVSep word between
 *  one polyline and the next. */
uint16_t appendWords(std::string_view run, std::vector<uint16_t>& out) {
  uint16_t words = 0;
  for (size_t at = 0; at < run.size();) {
    if (run[at] == ' ') {
      ++at;
      continue;
    }
    unsigned value = 0;
    const char* first = run.data() + at;
    const auto read = std::from_chars(first, run.data() + run.size(), value);
    if (read.ec != std::errc{}) break;
    out.push_back((uint16_t)value);
    ++words;
    at = (size_t)(read.ptr - run.data());
  }
  return words;
}

}  // namespace

Catalogue catalogue(sigil::sketch::Assets& assets) {
  Catalogue c;

  if (const auto stars = read(assets, "stars.csv")) {
    const auto ra = stars->column<double>("ra");
    const auto dec = stars->column<double>("dec");
    const auto mag = stars->column<double>("mag");
    c.star.reserve(ra.size() * 3);
    for (size_t i = 0; i < ra.size(); ++i) {
      c.star.push_back((float)ra[i]);
      c.star.push_back((float)dec[i]);
      c.star.push_back((float)mag[i]);
    }
  }

  if (const auto ast = read(assets, "asterisms.csv")) {
    const auto id = ast->column<std::string>("id");
    const auto pinyin = ast->column<std::string>("pinyin");
    const auto native = ast->column<std::string>("native");
    const auto english = ast->column<std::string>("english");
    const auto stars = ast->column<double>("stars");
    const auto line = ast->column<std::string>("line");
    c.asterism.reserve(id.size());
    for (size_t i = 0; i < id.size(); ++i) {
      AstRec rec{id[i],
                 pinyin[i],
                 native[i],
                 english[i],
                 (uint16_t)c.verts.size(),
                 0,
                 (uint16_t)stars[i]};
      rec.words = appendWords(line[i], c.verts);
      c.asterism.push_back(std::move(rec));
    }
  }

  if (const auto xiu = read(assets, "mansions.csv")) {
    const auto native = xiu->column<std::string>("native");
    const auto pinyin = xiu->column<std::string>("pinyin");
    const auto star = xiu->column<double>("star");
    c.mansion.reserve(native.size());
    for (size_t i = 0; i < native.size(); ++i)
      c.mansion.push_back({native[i], pinyin[i], (int)star[i]});
  }

  return c;
}

}  // namespace dunhuang
