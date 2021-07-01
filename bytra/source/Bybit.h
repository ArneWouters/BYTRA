//
// Created by Arne Wouters on 19/08/2020.
//

#ifndef BYTRA_BYBIT_H
#define BYTRA_BYBIT_H

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <simdjson.h>

#include "Candle.h"
#include "OrderBook.h"
#include "Position.h"
#include "strategies/Strategy.h"
#include "ws/WebSocket.h"


class Bybit {
  private:
    std::string baseUrl;
    std::string apiKey;
    std::string apiSecret;
    std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> candles;
    std::vector<std::string> allowedTimeframes = {"1", "3", "5", "15", "30", "60", "120", "240", "360", "D", "W", "M"};
    std::shared_ptr<WebSocket> websocket;
    Position position;
    std::shared_ptr<Strategy> strategy;
    std::shared_ptr<OrderBook> orderBook;
    std::shared_ptr<simdjson::dom::parser> parser = std::make_shared<simdjson::dom::parser>();
    bool newCandleAdded = true;

  public:
    Bybit(std::string &baseUrl, std::string &apiKey, std::string &apiSecret, std::shared_ptr<WebSocket> &ws,
          const std::shared_ptr<Strategy> &strategy);

    void loadCandles();

    Position getPosition();

    void setPosition(const Position &pos);

    void printPosition() const;

    simdjson::simdjson_result<simdjson::ondemand::array> getActiveOrders();

    void cancelAllActiveOrders();

    void syncOrderBook();

    void placeMarketOrder(const Order &ord);

    void placeLimitOrder(const Order &ord);

    void amendLimitOrder(const Order &ord);

    void cancelActiveLimitOrder(const std::string &id);

    void doAutomatedTrading();

    void removeUnusedCandles();

    void parseWebsocketMessage(const std::string &msg);

    std::vector<std::string> getTopics();

    static std::string getExpireTime();

    void parseCandleMessage(const std::string &msg);

    void parseOBMessage(const std::string &msg);

    void parsePositionMessage(const std::string &msg);
};

#endif  // BYTRA_BYBIT_H
