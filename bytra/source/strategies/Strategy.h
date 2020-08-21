//
// Created by Arne Wouters on 30/07/2020.
//

#ifndef MEXTRA_STRATEGY_H
#define MEXTRA_STRATEGY_H

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
    std::vector<std::pair<std::string, int>> timeframes;  // keep amounts below 12k
    int maxQty;
    std::string symbol;
    std::string orderType;
    double slippage;

  public:
    virtual bool checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) = 0;

    virtual bool checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) = 0;

    virtual bool checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                           std::shared_ptr<Position> position) = 0;

    std::string getName() { return name; }

    std::vector<std::pair<std::string, int>> getTimeframes() { return timeframes; }

    std::string getSymbol() { return symbol; }

    std::string getOrderType() { return orderType; }

    [[nodiscard]] int getMaxQty() const { return maxQty; }

    [[nodiscard]] double getSlippage() const { return slippage; }
};

#endif  // MEXTRA_STRATEGY_H
