//
// Created by Arne Wouters on 19/08/2020.
//

#include "Bybit.h"
#include <mutex>
#include <simdjson.h>
#include <spdlog/spdlog.h>

#include "Encryption.h"
#include "http/RESTClient.hpp"
#include "ws/WebSocket.h"

using namespace simdjson;
using namespace std::chrono;

std::once_flag flag1;

Bybit::Bybit(std::string &baseUrl, std::string &apiKey, std::string &apiSecret, std::shared_ptr<WebSocket> &ws,
             const std::shared_ptr<Strategy> &strategy) {
    this->baseUrl = baseUrl;
    this->apiKey = apiKey;
    this->apiSecret = apiSecret;
    this->strategy = strategy;

    for (const auto &tf : strategy->getTimeframes()) {
        auto it = std::find(allowedTimeframes.begin(), allowedTimeframes.end(), tf.first);

        if (it == allowedTimeframes.end()) {
            spdlog::error("Bybit::Bybit(..) - invalid timeframe");
            throw std::invalid_argument("Invalid timeframe: " + tf.first + " in strategy " + strategy->getName());
        }
        candles[TimeFrame(tf.first, tf.second)] = {};
    }

    loadCandles();
    orderBook = std::make_shared<OrderBook>();
    websocket = ws;
}

void Bybit::loadCandles() {
    for (auto &[tf, vec] : candles) {
        long currentTime = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        long from = currentTime - (tf.ticks * (tf.amount + 1) * 60);
        std::vector<std::shared_ptr<Candle>> candles_tf;

        int batch_size = 200;
        int batches = std::ceil(float(tf.amount + 1) / float(batch_size));

        std::string endpoint = "/v2/public/kline/list";

        for (int i = 0; i < batches; i++) {
            auto parameters = RESTClient::Parameters{
                {"symbol", strategy->getSymbol()}, {"interval", tf.symbol},
                {"from", std::to_string(from)}};
            auto response_json = RESTClient::Get(baseUrl, endpoint, parameters);
            dom::parser parser;
            dom::element response = parser.parse(response_json);

            for (dom::object item : response["result"]) {
                double open = std::stod((std::string)item["open"]);
                double high = std::stod((std::string)item["high"]);
                double low = std::stod((std::string)item["low"]);
                double close = std::stod((std::string)item["close"]);
                double volume = std::stod((std::string)item["volume"]);
                long timestamp = (long)item["open_time"];
                auto candle = std::make_shared<Candle>(Candle{open, high, low, close, volume, timestamp});

                candles_tf.push_back(candle);
            }
            from = candles_tf[candles_tf.size() - 1]->timestamp + 1;
        }
        candles_tf.pop_back();  // last candle is not complete
        vec = candles_tf;
    }
}

Position Bybit::getPosition() {
    std::string endpoint = "/v2/private/position/list";
    // pairs have to be in alphabetic order
    auto parameters = RESTClient::Parameters{
        {"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", getExpireTime()}};

    parameters.addParameter({"sign", HmacEncode(parameters.content, apiSecret)});
    auto response_json = RESTClient::Get(baseUrl, endpoint, parameters);
    dom::element response = parser->parse(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::getPosition - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    double entryPrice = std::stod((std::string)response["result"]["entry_price"]);
    std::string side = (std::string)response["result"]["side"];
    long size = response["result"]["size"].get_int64();
    if (side == "Sell") size = -size;
    double liqPrice = std::stod((std::string)response["result"]["liq_price"]);
    double leverage = std::stod((std::string)response["result"]["leverage"]);

    return Position(size, entryPrice, liqPrice, leverage);
}

simdjson::simdjson_result<ondemand::array> Bybit::getActiveOrders() {
    std::string endpoint = "/v2/private/order";
    auto parameters = RESTClient::Parameters{
        {"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", getExpireTime()}};
    parameters.addParameter({"sign", HmacEncode(parameters.content, apiSecret)});
    auto response_json = padded_string(RESTClient::Get(baseUrl, endpoint, parameters));

    ondemand::parser temp_parser;
    auto response = temp_parser.iterate(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        std::string_view returnMsg;
        response["ret_msg"].get(returnMsg);
        spdlog::error("Bybit::getActiveOrders - bad response - " + (std::string)returnMsg);
        throw std::runtime_error("Bad API response.");
    }

    return response.find_field("result").get_array();
}

void Bybit::cancelAllActiveOrders() {
    std::string endpoint = "/v2/private/order/cancelAll";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{
        {"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", getExpireTime()}};
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::element response = parser->parse(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::cancelAllActiveOrders - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::syncOrderBook() {
    if (websocket->isConnected()) {
        websocket->writeMessage(
            R"({"op": "unsubscribe", "args": ["orderBookL2_25.)" + strategy->getSymbol() + R"("]})");
        websocket->writeMessage(
            R"({"op": "subscribe", "args": ["orderBookL2_25.)" + strategy->getSymbol() + R"("]})");
    }
}

void Bybit::placeMarketOrder(const Order &ord) {
    std::string endpoint = "/v2/private/order/create";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{
        {"api_key", apiKey}, {"order_type", "Market"}, {"qty", std::to_string(std::abs(ord.qty))}};

    if (ord.reduce) {
        payload.addPair({"reduce_only", "true"});
    }

    payload.addPair({"side", ord.qty > 0 ? "Buy" : "Sell"});
    payload.addPair({"symbol", strategy->getSymbol()});
    payload.addPair({"time_in_force", "ImmediateOrCancel"});
    payload.addPair({"timestamp", getExpireTime()});
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});

    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::element response = parser->parse(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30063) {
        spdlog::error("Bybit::placeMarketOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::placeLimitOrder(const Order &ord) {
    std::string endpoint = "/v2/private/order/create";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{{"api_key", apiKey},
                                       {"order_type", "Limit"},
                                       {"price", std::to_string(ord.price)},
                                       {"qty", std::to_string(std::abs(ord.qty))}};

    if (ord.reduce) {
        payload.addPair({"reduce_only", "true"});
    }

    payload.addPair({"side", ord.qty > 0 ? "Buy" : "Sell"});
    payload.addPair({"symbol", strategy->getSymbol()});
    payload.addPair({"time_in_force", "PostOnly"});
    payload.addPair({"timestamp", getExpireTime()});
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});

    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::element response = parser->parse(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::placeLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::amendLimitOrder(const Order &ord) {
    std::string endpoint = "/open-api/order/replace";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{{"api_key", apiKey},
                                       {"order_id", ord.id},
                                       {"p_r_price", std::to_string(ord.price)},
                                       {"symbol", strategy->getSymbol()},
                                       {"timestamp", getExpireTime()}};

    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::element response = parser->parse(response_json);
    long retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30032 && retCode != 30037 && retCode != 20001) {
        spdlog::error("Bybit::amendLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::cancelActiveLimitOrder(const std::string &id) {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

    std::string endpoint = "/v2/private/order/cancel";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{{"api_key", apiKey},
                                       {"order_id", id},
                                       {"symbol", strategy->getSymbol()},
                                       {"timestamp", expires}};
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::element response = parser->parse(response_json);

    long retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30032) {
        spdlog::error("Bybit::cancelLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::doAutomatedTrading() {
    if (newCandleAdded) {
        newCandleAdded = false;

        if (position.isOpen()) {
            auto exit = strategy->checkExit(candles, position);

            if (exit) {
                placeMarketOrder(Order(-position.getSize(), true));
            }
        }

        if (!position.isOpen() && strategy->checkLongEntry(candles)) {
            spdlog::debug("Entry signal: Long");
            placeMarketOrder(Order(strategy->getQty()));

        } else if (!position.isOpen() && strategy->checkShortEntry(candles)) {
            spdlog::debug("Entry signal: Short");
            placeMarketOrder(Order(-strategy->getQty()));
        }
    }
}

void Bybit::removeUnusedCandles() {
    int factor = 4;

    for (auto const &[tf, vec] : candles) {
        if (vec.size() >= tf.amount * factor) {
            auto first = vec.end() - tf.amount;
            auto last = vec.end();
            std::vector<std::shared_ptr<Candle>> new_vec(first, last);
            candles[tf] = new_vec;
        }
    }
}

void Bybit::parseWebsocketMessage(const std::string &msg) {
    dom::element response = parser->parse(msg);
    dom::element elem;

    // authentication and subscribe messages
    if (auto error = response["request"].get(elem); !error) {
        bool success = response["success"].get_bool();

        if(!success) {
            spdlog::error("Websocket: {}", (std::string) response["ret_msg"]);

        } else {
            std::basic_string_view op = response["request"]["op"].get_string().value();

            if (op == "auth") {
                std::cout << "Connected and authenticated with Bybit websocket!" << std::endl;
                spdlog::info("[WebSocket] Connected to the Bybit Realtime API.");
            } else if (op == "subscribe") {
                for (dom::element item : response["request"]["args"]) {
                    spdlog::info("[WebSocket] Successfully subscribed to " + (std::string)item);
                }
            }
        }

        return;
    }

    if (auto error = response["topic"].get(elem); !error) {
        std::string topic = (std::string)response["topic"];

        if (topic.size() > 7 && topic.substr(0, 7) == "klineV2") {
            // parse message to add new candles
            parseCandleMessage(msg);

        } else if (topic.size() > 14 && topic.substr(0, 14) == "orderBookL2_25") {
            parseOBMessage(msg);

        } else if (topic == "position") {
            parsePositionMessage(msg);
        }
    } else {
        spdlog::debug("websocket msg: {}", msg);
    }
}

void Bybit::parseCandleMessage(const std::string &msg) {
    dom::element response = parser->parse(msg);
    std::string topic = (std::string)response["topic"];

    std::string::size_type n = topic.find('.');
    std::string::size_type n2 = topic.find('.', n + 1);

    std::string interval = topic.substr(n + 1, n2 - n - 1);
    std::string symbol = topic.substr(n2 + 1);

    for (dom::object item : response["data"]) {
        if (bool confirm = item["confirm"].get_bool(); !confirm) continue;

        // create candle
        double open = item["open"].get_double();
        double high = item["high"].get_double();
        double low = item["low"].get_double();
        double close = item["close"].get_double();
        double volume = item["volume"].get_double();
        long timestamp = item["start"].get_int64();
        auto candle = std::make_shared<Candle>(Candle{open, high, low, close, volume, timestamp});

        // add candle
        for (auto &[tf, vec] : candles) {
            if (tf.symbol == interval && vec.back()->timestamp != candle->timestamp) {
                vec.push_back(candle);
                newCandleAdded = true;
                spdlog::debug("Added Candle");
                break;
            }
        }
    }
}

void Bybit::parseOBMessage(const std::string &msg) {
    dom::element response = parser->parse(msg);
    std::string type = (std::string)response["type"];

    if (type == "snapshot") {
        // Create new order book
        orderBook = std::make_shared<OrderBook>();

        for (dom::object item : response["data"]) {
            long id = (long)item["id"];
            double price = std::stod((std::string)item["price"]);
            std::string side = (std::string)item["side"];
            long size = (long)item["size"];

            if (side == "Sell") {
                orderBook->addAskEntry(id, OrderBookEntry(price, size));
            } else if (side == "Buy") {
                orderBook->addBidEntry(id, OrderBookEntry(price, size));
            }
        }

    } else if (type == "delta") {
        for (dom::object item : response["data"]["delete"]) {
            long id = (long)item["id"];
            std::string side = (std::string)item["side"];

            if (side == "Sell") {
                orderBook->removeAskEntry(id);
            } else if (side == "Buy") {
                orderBook->removeBidEntry(id);
            }
        }

        for (dom::object item : response["data"]["update"]) {
            long id = (long)item["id"];
            std::string side = (std::string)item["side"];
            long size = (long)item["size"];

            if (side == "Sell") {
                orderBook->updateAskEntry(id, size);
            } else if (side == "Buy") {
                orderBook->updateBidEntry(id, size);
            }
        }

        for (dom::object item : response["data"]["insert"]) {
            long id = (long)item["id"];
            double price = std::stod((std::string)item["price"]);
            std::string side = (std::string)item["side"];
            long size = (long)item["size"];

            if (side == "Sell") {
                orderBook->addAskEntry(id, OrderBookEntry(price, size));
            } else if (side == "Buy") {
                orderBook->addBidEntry(id, OrderBookEntry(price, size));
            }
        }
    }
}

void Bybit::parsePositionMessage(const std::string &msg) {
    dom::element response = parser->parse(msg);

    for (dom::object item : response["data"]) {
        std::string symbol = (std::string)item["symbol"];

        if (symbol == strategy->getSymbol()) {
            double entryPrice = std::stod((std::string)item["entry_price"]);
            std::string side = (std::string)item["side"];
            long size = item["size"].get_int64();
            if (side == "Sell") size = -size;
            double liqPrice = std::stod((std::string)item["liq_price"]);
            double leverage = std::stod((std::string)item["leverage"]);

            return setPosition(Position(size, entryPrice, liqPrice, leverage));
        }
    }
}

std::vector<std::string> Bybit::getTopics() {
    std::vector<std::string> topics{
        "position",
//        "order",
        "orderBookL2_25." + strategy->getSymbol()
    };

    for (auto const &[tf, val] : candles) {
        std::string msg{};
        msg.append("klineV2." + tf.symbol + ".");
        msg.append(strategy->getSymbol());
        topics.push_back(msg);
    }

    return topics;
}

std::string Bybit::getExpireTime() {
    return std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

void Bybit::setPosition(const Position &pos) {
    position = pos;
}

void Bybit::printPosition() const {
    std::call_once(flag1, []() {
      std::cout << std::endl << "Qty | Entry Price | Liq. Price | Unrealized P&L" << std::endl;
    });

    std::printf("\r%li | %.4f | %.4f | %.8f(%.2f%s)", position.getSize(), position.getEntryPrice(),
                position.getLiquidationPrice(), position.getUnrealisedPnl(orderBook->getMidPrice()),
                position.getUnrealisedPnlPercentage(orderBook->getMidPrice()), "%");
    fflush(stdout);
}
