//
// Created by Arne Wouters on 22/08/2020.
//

#ifndef BYTRA_ORDERBOOK_H
#define BYTRA_ORDERBOOK_H

#include <map>

struct OrderBookEntry {
    double price;
    long size;

    OrderBookEntry() = default;

    OrderBookEntry(const double &price, const long &size) {
        this->price = price;
        this->size = size;
    }
};

class OrderBook {
private:
    std::map<long, OrderBookEntry> askSide;
    std::map<long, OrderBookEntry> bidSide;

public:
    OrderBook() = default;

    void addAskEntry(const long &id, const OrderBookEntry &entry) { askSide[id] = entry; }

    void addBidEntry(const long &id, const OrderBookEntry &entry) { bidSide[id] = entry; }

    void removeAskEntry(const long &id) { askSide.erase(id); }

    void removeBidEntry(const long &id) { bidSide.erase(id); }

    void updateAskEntry(const long &id, const long &newSize) { askSide[id].size = newSize; }

    void updateBidEntry(const long &id, const long &newSize) { bidSide[id].size = newSize; }
};

#endif //BYTRA_ORDERBOOK_H
