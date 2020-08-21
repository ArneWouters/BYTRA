#include <doctest/doctest.h>

#include <string>

#include "../bytra/source/strategies/Rsi.cpp"

TEST_CASE("RSI") {
    auto s = std::make_shared<Rsi>();

    CHECK(s->getSymbol() == "BTCUSD");
}
