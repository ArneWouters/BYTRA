//
// Created by Arne Wouters on 09/08/2020.
//

#ifndef MEXTRA_POSITION_H
#define MEXTRA_POSITION_H

#include <string>

#include "Candle.h"

struct Position {
    std::string symbol;
    long timestamp = 0;
    int qty = 0;
    double entryPrice = 0.0;
    double currentAskPrice = 0.0;
    double currentBidPrice = 0.0;

    [[nodiscard]] bool isShort() const { return qty < 0; }

    [[nodiscard]] bool isLong() const { return qty > 0; }
};

#endif  // MEXTRA_POSITION_H
