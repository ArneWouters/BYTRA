//
// Created by Arne Wouters on 02/08/2020.
//

#ifndef MEXTRA_RSI_H
#define MEXTRA_RSI_H

#include <map>
#include <string>
#include <vector>

#include "Strategy.h"

class Rsi : public Strategy {
  public:
    Rsi();

    bool checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles, std::shared_ptr<Position> position) override;

    double calculateRSI(std::vector<std::shared_ptr<Candle>> &candles);
};

#endif  // MEXTRA_RSI_H
