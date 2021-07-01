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
    double leverage;

  public:
    Position() {
        size = 0;
        entryPrice = 0.0;
        liquidationPrice = 0.0;
        leverage = 0.0;
    }

    Position(long &size, double &entryPrice, double &liquidationPrice, double &leverage) {
        this->size = size;
        this->entryPrice = entryPrice;
        this->liquidationPrice = liquidationPrice;
        this->leverage = leverage;
    }

    [[nodiscard]] bool isShort() const { return size < 0; }

    [[nodiscard]] bool isLong() const { return size > 0; }

    [[nodiscard]] bool isOpen() const { return size != 0; }

    [[nodiscard]] long getSize() const { return size; }

    [[nodiscard]] double getEntryPrice() const { return entryPrice; }

    [[nodiscard]] double getLiquidationPrice() const { return liquidationPrice; }

    [[nodiscard]] double getLeverage() const { return leverage; }

    [[nodiscard]] double getUnrealisedPnl(const double &lastTradedPrice) const {
        return size * ((1/entryPrice) - (1/lastTradedPrice));
    }

    [[nodiscard]] double getUnrealisedPnlPercentage(const double &lastTradedPrice) const {
        return (getUnrealisedPnl(lastTradedPrice) / getPositionMargin()) * 100;
    }

    [[nodiscard]] double getPositionMargin() const {
        // 0.00075 = 0.075% Fee to close
        return std::abs((size / (entryPrice * leverage)) + ((size / liquidationPrice) * 0.00075));
    }
};

#endif  // MEXTRA_POSITION_H
