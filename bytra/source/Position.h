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
    long qty = 0;
    double entryPrice = 0;
    double stopLossPercentage = 0.03;
    double stopLossPrice = 0;
    std::shared_ptr<Order> activeOrder;

    Position() = default;

    [[nodiscard]] bool isShort() const { return qty < 0; }

    [[nodiscard]] bool isLong() const { return qty > 0; }

    void update(const long &newQty, const double &newPrice) {
        qty = newQty;
        entryPrice = newPrice;

        if (qty == 0) { return; }
        else if (isLong()) { stopLossPrice = entryPrice - entryPrice*stopLossPercentage; }
        else {stopLossPrice = entryPrice + entryPrice*stopLossPercentage;}
    }
};

#endif  // MEXTRA_POSITION_H
