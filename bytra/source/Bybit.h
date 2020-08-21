//
// Created by Arne Wouters on 19/08/2020.
//

#ifndef BYTRA_BYBIT_H
#define BYTRA_BYBIT_H

#include <simdjson.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "Candle.h"
#include "Position.h"
#include "strategies/Strategy.h"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace ssl = boost::asio::ssl;        // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>
using namespace simdjson;

class Bybit {
  private:
    std::string baseUrl;
    std::string websocketHost;
    std::string websocketTarget;
    beast::flat_buffer websocketBuffer;
    std::string apiKey;
    std::string apiSecret;
    std::map<TimeFrame, std::vector<std::shared_ptr<Candle>>> candles;
    std::vector<std::string> allowedTimeframes = {"1", "3", "5", "15", "30", "60", "120", "240", "360", "D", "W", "M"};
    std::shared_ptr<websocket::stream<ssl::stream<tcp::socket>>> websocket;
    std::shared_ptr<Position> position;
    std::shared_ptr<Strategy> strategy;
    bool newCandleAdded = true;

  public:
    Bybit(std::string &baseUrl, std::string &apiKey, std::string &apiSecret, std::string &websocketHost,
          std::string &websocketTarget, const std::shared_ptr<Strategy> &strategy);

    void getCandlesApi();

    void getPositionApi();

    void cancelAllActiveOrders();

    void connect();

    void disconnect();

    bool isConnected();

    void readWebsocket();

    void parseWebsocketMsg(const std::string &msg);

    void sendWebsocketHeartbeat();

    void placeMarketOrder(const Order &ord);

    void doAutomatedTrading();

    void removeUnusedCandles();
};

#endif  // BYTRA_BYBIT_H
