#include "third_party/doctest.h"

#include "primec/SymbolInterner.h"

TEST_SUITE_BEGIN("primestruct.semantics.symbol_interner");

TEST_CASE("symbol interner assigns deterministic first-seen ids") {
  primec::SymbolInterner interner;
  const primec::SymbolId alpha1 = interner.intern("/std/math/Vec2");
  const primec::SymbolId beta = interner.intern("/std/math/Mat2");
  const primec::SymbolId alpha2 = interner.intern("/std/math/Vec2");

  CHECK(alpha1 == 1);
  CHECK(beta == 2);
  CHECK(alpha2 == alpha1);
  CHECK(interner.size() == 2);
  CHECK(interner.resolve(alpha1) == "/std/math/Vec2");
  CHECK(interner.resolve(beta) == "/std/math/Mat2");
}

TEST_CASE("symbol interner lookup and resolve handle missing values") {
  primec::SymbolInterner interner;
  CHECK(interner.empty());
  CHECK(interner.lookup("/missing").has_value() == false);
  CHECK(interner.resolve(primec::InvalidSymbolId).empty());
  CHECK(interner.resolve(123).empty());

  const primec::SymbolId id = interner.intern("/present");
  REQUIRE(id != primec::InvalidSymbolId);
  CHECK(interner.contains("/present"));
  CHECK(interner.lookup("/present").has_value());
  CHECK(interner.lookup("/present").value() == id);
  CHECK(interner.resolve(id) == "/present");
}

TEST_CASE("symbol interner clears ids and restarts numbering") {
  primec::SymbolInterner interner;
  const primec::SymbolId first = interner.intern("alpha");
  const primec::SymbolId second = interner.intern("beta");
  REQUIRE(first == 1);
  REQUIRE(second == 2);

  interner.clear();
  CHECK(interner.empty());
  CHECK(interner.lookup("alpha").has_value() == false);
  CHECK(interner.resolve(first).empty());

  const primec::SymbolId reset = interner.intern("alpha");
  CHECK(reset == 1);
  CHECK(interner.resolve(reset) == "alpha");
}

TEST_SUITE_END();
