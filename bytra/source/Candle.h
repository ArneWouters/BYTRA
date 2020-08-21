//
// Created by Arne Wouters on 30/07/2020.
//

#ifndef MEXTRA_CANDLE_H
#define MEXTRA_CANDLE_H

#include <string>

struct TimeFrame {
    int ticks;  // 1 tick = 1 minute
    std::string symbol;
    int amount;

    TimeFrame(const std::string& symbol, const int& amount) {
        this->symbol = symbol;
        this->amount = amount;

        if (symbol == "D") {
            ticks = 1440;
        } else if (symbol == "W") {
            ticks = 10080;
        } else if (symbol == "M") {
            ticks = 43800;
        } else {
            ticks = std::stoi(symbol);
        }
    }
};

inline bool operator<(const TimeFrame& tf1, const TimeFrame& tf2) { return tf1.ticks < tf2.ticks; }

struct Candle {
    double open;
    double high;
    double low;
    double close;
    double volume;
    long timestamp;
};

#endif  // MEXTRA_CANDLE_H
