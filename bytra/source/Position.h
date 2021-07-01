//
// Created by Arne Wouters on 09/08/2020.
//

#ifndef MEXTRA_POSITION_H
#define MEXTRA_POSITION_H

#include <string>

#include "Candle.h"
#include "Order.h"

class Position {
  private:
    long size;
    double entryPrice;
    double liquidationPrice;
    double unrealisedPnl;
    double positionMargin;

  public:
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

    [[nodiscard]] bool isOpen() const { return size != 0; }

    [[nodiscard]] long getSize() const { return size; }

    [[nodiscard]] double getEntryPrice() const { return entryPrice; }

    [[nodiscard]] double getLiquidationPrice() const { return liquidationPrice; }

    [[nodiscard]] double getUnrealisedPnl() const { return unrealisedPnl; }

    [[nodiscard]] double getPositionMargin() const { return positionMargin; }

};

#endif  // MEXTRA_POSITION_H
