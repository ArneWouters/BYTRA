//
// Created by Arne Wouters on 30/07/2020.
//

#ifndef BYTRA_STRATEGY_H
#define BYTRA_STRATEGY_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../Candle.h"
#include "../Order.h"
#include "../Position.h"

class Strategy {
  protected:
    std::string name;
    std::vector<std::pair<std::string, int>> timeframes;  // keep amounts below 10k
    long size;
    std::string symbol;

  public:
    virtual bool checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) = 0;

    virtual bool checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) = 0;

    virtual bool checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                          const Position &position)
        = 0;

    std::string getName() { return name; }

    std::vector<std::pair<std::string, int>> getTimeframes() { return timeframes; }

    std::string getSymbol() { return symbol; }

    [[nodiscard]] long getQty() const { return size; }
};

#endif  // BYTRA_STRATEGY_H
