//
// Created by Arne Wouters on 21/08/2020.
//

#include "Ema.h"

#include <spdlog/spdlog.h>
#include <ta-lib/ta_libc.h>

Ema::Ema() {
    /** EMA strategy
     * An exponential moving average (EMA) is a type of moving average (MA)
     * that places a greater weight and significance on the most recent data points.
     * We use the 20-EMA and 50-EMA and consider the moment when the 20-EMA crosses above the 50-EMA
     * a buy signal. The sell signal is when the 20-EMA crosses below the 50-EMA.
     * */
    name = "EMA";
    timeframes = {{"1", 1000}};
    symbol = "BTCUSD";
    qty = 100;
    orderType = "Market";
    slippage = 10.0;
    stopLossPercentage = 0.03;
}

bool Ema::checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) {
    auto tf = TimeFrame(timeframes[0].first, timeframes[0].second);

    // calculate 20-EMA values
    auto val = calculateEMA(candles[tf], 20);
    double currEmaValue = val.first;
    double prevEmaValue = val.second;

    // calculate 50-EMA values
    val = calculateEMA(candles[tf], 50);
    double currEmaValue2 = val.first;
    double prevEmaValue2 = val.second;

    // check if 20-EMA crossed above 50-EMA
    if (prevEmaValue < prevEmaValue2 && currEmaValue > currEmaValue2) {
        spdlog::debug("Entry Long {}", name);
        return true;
    }

    return false;
}

bool Ema::checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) {
    auto tf = TimeFrame(timeframes[0].first, timeframes[0].second);

    // calculate 20-EMA values
    auto val = calculateEMA(candles[tf], 20);
    double currEmaValue = val.first;
    double prevEmaValue = val.second;

    // calculate 50-EMA values
    val = calculateEMA(candles[tf], 50);
    double currEmaValue2 = val.first;
    double prevEmaValue2 = val.second;

    // check if 20-EMA crossed below 50-EMA
    if (prevEmaValue > prevEmaValue2 && currEmaValue < currEmaValue2) {
        spdlog::debug("Entry Short {}", name);
        return true;
    }

    return false;
}

bool Ema::checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                    std::shared_ptr<Position> position) {
    if ((position->isLong() && checkShortEntry(candles)) || (position->isShort() && checkLongEntry(candles))) {
        spdlog::debug("Exit {}", name);
        return true;
    }

    return false;
}

std::pair<double, double> Ema::calculateEMA(std::vector<std::shared_ptr<Candle>> &candles, const int &timePeriod) {
    std::vector<double> close;
    close.reserve(candles.size());

    for (auto &candle : candles) {
        close.push_back(candle->close);
    }

    int endIdx = (int)close.size() - 1;
    int startIdx = endIdx - timeframes[0].second + 1;
    int outBegIdx;
    int outNbElement;
    std::vector<double> ema(close.size() - timePeriod + 1);

    TA_RetCode retCode = TA_EMA(startIdx, endIdx, close.data(), timePeriod, &outBegIdx, &outNbElement, ema.data());

    double currEmaValue = ema[outNbElement - 1];
    double prevEmaValue = ema[outNbElement - 2];

    return {currEmaValue, prevEmaValue};
}
