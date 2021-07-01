//
// Created by Arne Wouters on 02/08/2020.
//

#ifndef BYTRA_RSI_H
#define BYTRA_RSI_H

#include <map>
#include <string>
#include <vector>

#include "Strategy.h"

class Rsi : public Strategy {
  public:
    Rsi();

    bool checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                   const Position &position) override;

    double calculateRSI(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles);
};

#endif  // BYTRA_RSI_H
