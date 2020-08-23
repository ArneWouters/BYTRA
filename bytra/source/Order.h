//
// Created by Arne Wouters on 18/08/2020.
//

#ifndef MEXTRA_ORDER_H
#define MEXTRA_ORDER_H

#include <string>
#include <chrono>

using namespace std::chrono;

class Order {
public:
    std::string id;
    long qty;
    double price;
    std::pair<double, double> priceInterval;
    double slippage;
    bool reduce = true;

    Order(const long &qty, const bool &reduce = false) {
        this->reduce = reduce;
        this->qty = qty;
    }

    Order(const double &price, const long &qty, const double &slippage, const bool &reduce = false) {
        this->reduce = reduce;
        this->qty = qty;
        this->slippage = slippage;
        this->price = price;
        this->priceInterval = {price - slippage, price + slippage};
    }

    [[nodiscard]] bool isSell() const { return qty < 0; }

    [[nodiscard]] bool isBuy() const { return qty > 0; }
};

#endif  // MEXTRA_ORDER_H
