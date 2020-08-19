//
// Created by Arne Wouters on 19/08/2020.
//

#include "Bybit.h"

Bybit::Bybit(std::string &baseUrl, std::string &apiKey, std::string &apiSecret, std::string &websocketHost,
             std::string &websocketTarget, std::shared_ptr<Strategy> strategy) {
    this->baseUrl = baseUrl;
    this->apiKey = apiKey;
    this->apiSecret = apiSecret;
    this->websocketHost = websocketHost;
    this->websocketTarget = websocketTarget;
    this->strategy = strategy;

    for (const auto &tf: strategy->getTimeframes()) {
        auto it = std::find(allowedTimeframes.begin(), allowedTimeframes.end(), tf.first);

        if (it == allowedTimeframes.end()) {
            spdlog::error("Bitmex::Bitmex(..) - invalid timeframe");
            throw std::invalid_argument("Invalid timeframe: " + tf.first + " in strategy " + strategy->getName());
        }

        candles[tf.first] = {};

        if (tf.second > minCandleAmount) {
            minCandleAmount = tf.second;
        }
    }

}
