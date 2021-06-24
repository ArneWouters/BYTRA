//
// Created by Arne Wouters on 19/08/2020.
//

#include "Bybit.h"

#include <simdjson.h>
#include <spdlog/spdlog.h>

#include "Encryption.h"
#include "http/RESTClient.hpp"
#include "ws/WebSocket.h"

using namespace simdjson;
using namespace std::chrono;

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

    position = std::make_shared<Position>();
    position->stopLossPercentage = strategy->getStopLossPercentage();
    orderBook = std::make_shared<OrderBook>();
    loadPosition();
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

void Bybit::loadPosition() {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);
    std::string endpoint = "/v2/private/position/list";
    // pairs have to be in alphabetic order
    auto parameters = RESTClient::Parameters{
        {"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", expires}};
    parameters.addParameter({"sign", HmacEncode(parameters.content, apiSecret)});
    auto response_json = RESTClient::Get(baseUrl, endpoint, parameters);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::getPositionApi - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    double entryPrice = std::stod((std::string)response["result"]["entry_price"]);
    std::string side = (std::string)response["result"]["side"];
    long qty = (long)response["result"]["size"];
    if (side == "Sell") {
        qty = -qty;
    }

    position->update(qty, entryPrice);
}

void Bybit::cancelAllActiveOrders() {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

    std::string endpoint = "/v2/private/order/cancelAll";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{
        {"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", expires}};
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

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
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

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
    payload.addPair({"timestamp", expires});
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});

    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30063) {
        spdlog::error("Bybit::placeMarketOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    if (ord.reduce) {
        position->activeOrder = nullptr;
    }
}

void Bybit::placeLimitOrder(const Order &ord) {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

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
    payload.addPair({"timestamp", expires});
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});

    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::placeLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    position->activeOrder = std::make_shared<Order>(ord);
    position->activeOrder->id = (std::string)response["result"]["order_id"];
    spdlog::debug("Set activeOrder with price={}, interval=({}, {}), reduce={}", position->activeOrder->price,
                  position->activeOrder->priceInterval.first, position->activeOrder->priceInterval.second,
                  position->activeOrder->reduce);
}

void Bybit::amendLimitOrder(const Order &ord) {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

    std::string endpoint = "/open-api/order/replace";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{{"api_key", apiKey},
                                       {"order_id", ord.id},
                                       {"p_r_price", std::to_string(ord.price)},
                                       {"symbol", strategy->getSymbol()},
                                       {"timestamp", expires}};

    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30032 && retCode != 30037 && retCode != 20001) {
        spdlog::error("Bybit::amendLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    if (retCode == 30032 || retCode == 30037 || retCode == 20001) {
        position->activeOrder = nullptr;
    }
}

void Bybit::cancelActiveLimitOrder() {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

    std::string endpoint = "/v2/private/order/cancel";
    // pairs have to be in alphabetic order
    auto payload = RESTClient::Payload{{"api_key", apiKey},
                                       {"order_id", position->activeOrder->id},
                                       {"symbol", strategy->getSymbol()},
                                       {"timestamp", expires}};
    payload.addPair({"sign", HmacEncode(payload.content, apiSecret)});
    auto response_json = RESTClient::Post(baseUrl, endpoint, payload);
    dom::parser parser;
    dom::element response = parser.parse(response_json);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0 && retCode != 30032) {
        spdlog::error("Bybit::cancelLimitOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    position->activeOrder = nullptr;
}

void Bybit::doAutomatedTrading() {
    if (newCandleAdded) {
        newCandleAdded = false;

        if (position->qty != 0) {
            auto exit = strategy->checkExit(candles, position);

            if (exit) {
                spdlog::debug("Exit signal");

                if (position->activeOrder && !position->activeOrder->reduce) {
                    cancelActiveLimitOrder();
                }

                if (strategy->getOrderType() == "Market") {
                    placeMarketOrder(Order(-position->qty, true));

                } else if (strategy->getOrderType() == "Limit" && !orderBook->isEmpty() && !position->activeOrder) {
                    double price = position->qty > 0 ? orderBook->askPrice() : orderBook->bidPrice();
                    Order ord(price, -position->qty, strategy->getSlippage(), true);
                    placeLimitOrder(ord);
                    return;
                }
            }
        }

        if (position->qty == 0 && !position->activeOrder && strategy->checkLongEntry(candles)) {
            spdlog::debug("Entry signal: Long");

            if (strategy->getOrderType() == "Market") {
                placeMarketOrder(Order(strategy->getQty()));

            } else if (strategy->getOrderType() == "Limit" && !orderBook->isEmpty()) {
                Order ord(orderBook->bidPrice(), strategy->getQty(), strategy->getSlippage());
                placeLimitOrder(ord);
            }

            return;
        } else if (position->qty == 0 && !position->activeOrder && strategy->checkShortEntry(candles)) {
            spdlog::debug("Entry signal: Short");

            if (strategy->getOrderType() == "Market") {
                placeMarketOrder(Order(-strategy->getQty()));

            } else if (strategy->getOrderType() == "Limit" && !orderBook->isEmpty()) {
                Order ord(orderBook->askPrice(), -strategy->getQty(), strategy->getSlippage());
                placeLimitOrder(ord);
            }
            return;
        }
    }

    if (position->activeOrder) {
        if (position->activeOrder->isBuy()) {
            double bidPrice = orderBook->bidPrice();

            if (bidPrice != position->activeOrder->price) {
                if (bidPrice >= position->activeOrder->priceInterval.first
                    && bidPrice <= position->activeOrder->priceInterval.second) {
                    position->activeOrder->price = bidPrice;
                    amendLimitOrder(*position->activeOrder);

                } else if (position->activeOrder->reduce) {
                    cancelActiveLimitOrder();
                    placeMarketOrder(Order(-position->qty, true));

                } else {
                    cancelActiveLimitOrder();
                }
            }
        } else {
            double askPrice = orderBook->askPrice();

            if (askPrice != position->activeOrder->price) {
                if (askPrice >= position->activeOrder->priceInterval.first
                    && askPrice <= position->activeOrder->priceInterval.second) {
                    position->activeOrder->price = askPrice;
                    amendLimitOrder(*position->activeOrder);

                } else if (position->activeOrder->reduce) {
                    cancelActiveLimitOrder();
                    placeMarketOrder(Order(-position->qty, true));

                } else {
                    cancelActiveLimitOrder();
                }
            }
        }
    }

    // Stop Loss
    if (position->qty != 0) {
        double midPrice = (orderBook->askPrice() + orderBook->bidPrice()) / 2;
        if ((position->isLong() && midPrice < position->stopLossPrice)
            || (position->isShort() && midPrice > position->stopLossPrice)) {
            if (position->activeOrder) {
                cancelActiveLimitOrder();
            }
            placeMarketOrder(Order(-position->qty, true));
            spdlog::info("Stop Loss triggered at {}", position->stopLossPrice);
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
    //std::cout << msg << std::endl;

    dom::parser parser;
    dom::element response = parser.parse(msg);
    dom::element elem;

    // authentication and subscribe messages
    if (auto error = response["success"].get(elem); !error) {
        bool success = (bool) response["success"];

        if(!success) {
            spdlog::error("Websocket: {}", (std::string) response["ret_msg"]);
            return;
        }

        std::string op = (std::string)response["request"]["op"];

        if (op == "auth") {
            std::cout << "Connected and authenticated with Bybit websocket!" << std::endl;
            spdlog::info("[WebSocket] Connected to the Bybit Realtime API.");

        } else if (op == "subscribe") {
            for (dom::element item : response["request"]["args"]) {
                spdlog::info("[WebSocket] Successfully subscribed to " + (std::string)item);
            }
        }
        return;
    }

    if (auto error = response["topic"].get(elem); !error) {
        std::string topic = (std::string)response["topic"];

        if (topic == "position") {
            for (dom::object item : response["data"]) {
                double entryPrice = std::stod((std::string)item["entry_price"]);
                std::string side = (std::string)item["side"];
                long qty = (long)item["size"];
                if (side == "Sell") {
                    qty = -qty;
                }

                position->update(qty, entryPrice);
            }

        } else if (topic == "order") {
            for (dom::object item : response["data"]) {
                std::string orderType = (std::string)item["order_type"];
                std::string orderStatus = (std::string)item["order_status"];
                double askPrice = orderBook->askPrice();
                double bidPrice = orderBook->bidPrice();

                if (orderStatus == "Cancelled" && position->activeOrder) {
                    if (position->activeOrder->isBuy()) {
                        if (bidPrice >= position->activeOrder->priceInterval.first
                            && bidPrice <= position->activeOrder->priceInterval.second) {
                            position->activeOrder->price = bidPrice;
                            placeLimitOrder(*position->activeOrder);
                        } else if (position->activeOrder->reduce) {
                            placeMarketOrder(*position->activeOrder);
                        } else {
                            position->activeOrder = nullptr;
                        }
                    } else {
                        if (askPrice >= position->activeOrder->priceInterval.first
                            && askPrice <= position->activeOrder->priceInterval.second) {
                            position->activeOrder->price = askPrice;
                            placeLimitOrder(*position->activeOrder);
                        } else if (position->activeOrder->reduce) {
                            placeMarketOrder(*position->activeOrder);
                        } else {
                            position->activeOrder = nullptr;
                        }
                    }
                } else if (orderStatus == "Filled") {
                    position->activeOrder = nullptr;
                }
            }
        } else if (topic.size() > 7 && topic.substr(0, 7) == "klineV2") {
            std::string::size_type n = topic.find('.');
            std::string::size_type n2 = topic.find('.', n + 1);

            std::string interval = topic.substr(n + 1, n2 - n - 1);
            std::string symbol = topic.substr(n2 + 1);

            for (dom::object item : response["data"]) {
                bool confirm = (bool)item["confirm"];
                if (!confirm) {
                    continue;
                }

                // create candle
                double open = (double)item["open"];
                double high = (double)item["high"];
                double low = (double)item["low"];
                double close = (double)item["close"];
                double volume = (double)item["volume"];
                long timestamp = (long)item["start"];
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
        } else if (topic.size() > 14 && topic.substr(0, 14) == "orderBookL2_25") {
            std::string type = (std::string)response["type"];

            if (type == "snapshot") {
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
    } else {
        spdlog::debug("websocket msg: {}", msg);
    }
}

std::vector<std::string> Bybit::getTopics() {
    std::vector<std::string> topics{
        "position",
        "order",
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
