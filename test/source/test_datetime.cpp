#include <doctest/doctest.h>

#include <string>

#include "../bytra/source/Candle.h"

TEST_CASE("Datetime") {
    Datetime dt = Datetime::parse("2020-08-01T13:02:00.000Z");

    CHECK(dt.value == 202008011302);
}
