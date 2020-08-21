//
// Created by Arne Wouters on 21/08/2020.
//

#ifndef BYTRA_EMA_H
#define BYTRA_EMA_H

#include "Strategy.h"

class Ema : public Strategy {
public:
    Ema();

    bool checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) override;

    bool checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                   std::shared_ptr<Position> position) override;

    std::pair<double, double> calculateEMA(std::vector<std::shared_ptr<Candle>> &candles, const int &timePeriod);

};


#endif //BYTRA_EMA_H
