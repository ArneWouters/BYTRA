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
    size = 100;
}

bool Ema::checkLongEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) {
    // calculate 20-EMA values
    auto [currEmaValue, prevEmaValue] = calculateEMA(candles, 20);

    // calculate 50-EMA values
    auto [currEmaValue2, prevEmaValue2] = calculateEMA(candles, 50);

    // check if 20-EMA crossed above 50-EMA
    return (prevEmaValue < prevEmaValue2 && currEmaValue > currEmaValue2);
}

bool Ema::checkShortEntry(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles) {
    // calculate 20-EMA values
    auto [currEmaValue, prevEmaValue] = calculateEMA(candles, 20);

    // calculate 50-EMA values
    auto [currEmaValue2, prevEmaValue2] = calculateEMA(candles, 50);

    // check if 20-EMA crossed below 50-EMA
    return (prevEmaValue > prevEmaValue2 && currEmaValue < currEmaValue2);
}

bool Ema::checkExit(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                    const Position &position) {
    return ((position.isLong() && checkShortEntry(candles)) || (position.isShort() && checkLongEntry(candles)));
}

std::pair<double, double> Ema::calculateEMA(std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> &candles,
                                            const int &timePeriod) {
    auto tf = TimeFrame(timeframes[0].first, timeframes[0].second);

    std::vector<double> close;
    close.reserve(candles[tf].size());

    for (auto &candle : candles[tf]) {
        close.push_back(candle->close);
    }

    int endIdx = (int)close.size() - 1;
    int startIdx = endIdx - timeframes[0].second + 1;
    int outBegIdx;
    int outNbElement;
    std::vector<double> ema(close.size() - timePeriod + 1);

    TA_RetCode retCode = TA_EMA(startIdx, endIdx, close.data(), timePeriod, &outBegIdx, &outNbElement, ema.data());

    if (retCode != 0) {
        throw std::runtime_error("Bad TA_RetCode from TA_EMA.");
    }

    double currEmaValue = ema[outNbElement - 1];
    double prevEmaValue = ema[outNbElement - 2];

    spdlog::debug("Calculated ema({}): {}", timePeriod, currEmaValue);

    return {currEmaValue, prevEmaValue};
}
