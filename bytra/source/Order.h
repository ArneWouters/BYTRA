//
// Created by Arne Wouters on 18/08/2020.
//

#ifndef MEXTRA_ORDER_H
#define MEXTRA_ORDER_H

#include <string>

struct Order {
    std::string id;
    int qty = 0;
    int cumQty = 0;
    double price = 0.0;
    double allowedSlippage = 0.0;
    bool reduce = true;

    Order() = default;

    Order(const int &qty, const bool &reduce = false) {
        this->reduce = reduce;
        this->qty = qty;
    }

    Order(const double &price, const int &qty, const double &slippage, const bool &reduce = false) {
        this->reduce = reduce;
        this->qty = qty;
        this->allowedSlippage = slippage;
    }
};

#endif  // MEXTRA_ORDER_H
