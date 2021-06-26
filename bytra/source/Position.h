//
// Created by Arne Wouters on 09/08/2020.
//

#ifndef MEXTRA_POSITION_H
#define MEXTRA_POSITION_H

#include <string>

#include "Candle.h"
#include "Order.h"

class Position {
  public:
    long size;
    double entryPrice;
    double liquidationPrice;
    double unrealisedPnl;
    double positionMargin;

    Position() {
        size = 0;
        entryPrice = 0.0;
        liquidationPrice = 0.0;
        unrealisedPnl = 0.0;
        positionMargin = 0.0;
    }

    Position(long &size, double &entryPrice, double &liquidationPrice, double &unrealisedPnl, double &positionMargin) {
        this->size = size;
        this->entryPrice = entryPrice;
        this->liquidationPrice = liquidationPrice;
        this->unrealisedPnl = unrealisedPnl;
        this->positionMargin = positionMargin;
    }

    [[nodiscard]] bool isShort() const { return size < 0; }

    [[nodiscard]] bool isLong() const { return size > 0; }

};

#endif  // MEXTRA_POSITION_H
