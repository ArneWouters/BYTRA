//
// Created by Arne Wouters on 19/08/2020.
//

#include "Bybit.h"

#include <cpr/cpr.h>
#include <simdjson/simdjson.h>
#include <spdlog/spdlog.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include "Encryption.h"
#include "TerminalColors.h"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace ssl = boost::asio::ssl;        // from <boost/asio/ssl.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>
using namespace simdjson;
using namespace std::chrono;

Bybit::Bybit(std::string &baseUrl, std::string &apiKey, std::string &apiSecret, std::string &websocketHost,
             std::string &websocketTarget, const std::shared_ptr<Strategy> &strategy) {
    this->baseUrl = baseUrl;
    this->apiKey = apiKey;
    this->apiSecret = apiSecret;
    this->websocketHost = websocketHost;
    this->websocketTarget = websocketTarget;
    this->strategy = strategy;

    for (const auto &tf : strategy->getTimeframes()) {
        auto it = std::find(allowedTimeframes.begin(), allowedTimeframes.end(), tf.first);

        if (it == allowedTimeframes.end()) {
            spdlog::error("Bybit::Bybit(..) - invalid timeframe");
            throw std::invalid_argument("Invalid timeframe: " + tf.first + " in strategy " + strategy->getName());
        }
        candles[TimeFrame(tf.first, tf.second)] = {};
    }
    getCandlesApi();

    position = std::make_shared<Position>();
}

void Bybit::getCandlesApi() {
    for (auto &[tf, vec] : candles) {
        long currentTime
            = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
        long from = currentTime - (tf.ticks * (tf.amount + 1) * 60);
        std::vector<std::shared_ptr<Candle>> candles_tf;

        int batch_size = 200;
        int batches = std::ceil(float(tf.amount + 1) / float(batch_size));

        std::string endpoint = "/v2/public/kline/list";
        cpr::Session session;
        session.SetUrl(cpr::Url{baseUrl + endpoint});

        for (int i = 0; i < batches; i++) {
            auto parameters = cpr::Parameters{
                {"symbol", strategy->getSymbol()}, {"interval", tf.symbol}, {"from", std::to_string(from)}};
            session.SetParameters(parameters);

            spdlog::debug("[HTTP-GET] " + baseUrl + endpoint + " - " + parameters.content);
            cpr::Response r = session.Get();
            spdlog::debug("[RESP-" + std::to_string(r.status_code) + "]");

            if (r.status_code != 200) {
                spdlog::error("Bybit::getCandlesApi - bad response - " + r.text);
                throw std::runtime_error("Bad API response.");
            }

            dom::parser parser;
            dom::element response = parser.parse(r.text);

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

void Bybit::getPositionApi() {
    std::string expires = std::to_string(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()+ 1000);

    std::string endpoint = "/v2/private/position/list";
    // pairs have to be in alphabetic order
    auto parameters
            = cpr::Parameters{{"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", expires}};

    cpr::CurlHolder holder;
    parameters.AddParameter({"sign", HmacEncode(parameters.content, apiSecret)}, holder);

    spdlog::debug("[HTTP-GET] " + baseUrl + endpoint + parameters.content);
    cpr::Response r = cpr::Get(cpr::Url{baseUrl + endpoint}, parameters);
    spdlog::debug("[RESP-" + std::to_string(r.status_code) + "] " + r.text);

    if (r.status_code != 200) {
        spdlog::error("Bybit::getPositionApi - bad response - " + r.text);
        throw std::runtime_error("Bad API response.");
    }

    dom::parser parser;
    dom::element response = parser.parse(r.text);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::getPositionApi - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    std::string side = (std::string) response["result"]["side"];
    long qty = (long) response["result"]["size"];
    if (side == "Sell") { qty = -qty; }
    position->qty = qty;
}

void Bybit::cancelAllActiveOrders() {
    std::string expires = std::to_string(
            duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()+ 1000);

    std::string endpoint = "/v2/private/order/cancelAll";
    // pairs have to be in alphabetic order
    cpr::Payload payload
            = cpr::Payload{{"api_key", apiKey}, {"symbol", strategy->getSymbol()}, {"timestamp", expires}};

    cpr::CurlHolder holder;
    payload.AddPair({"sign", HmacEncode(payload.content, apiSecret)}, holder);

    spdlog::debug("[HTTP-POST] " + baseUrl + endpoint + payload.content);
    cpr::Response r = cpr::Post(cpr::Url{baseUrl + endpoint}, payload);
    spdlog::debug("[RESP-" + std::to_string(r.status_code) + "] " + r.text);

    if (r.status_code != 200) {
        spdlog::error("Bybit::cancelAllActiveOrders - bad response - " + r.text);
        throw std::runtime_error("Bad API response.");
    }

    dom::parser parser;
    dom::element response = parser.parse(r.text);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::cancelAllActiveOrders - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }
}

void Bybit::connect() {
    cancelAllActiveOrders();
    getPositionApi();

    const std::string host = websocketHost;
    const std::string port = "443";
    std::string expires = std::to_string(
        ::duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 1000);

    std::string auth_msg = R"({"op":"auth","args":[")" + apiKey + R"(",")" + expires + R"(",")"
                           + HmacEncode("GET/realtime" + expires, apiSecret) + R"("]})";
    std::string msg = R"({"op": "subscribe", "args": ["position","order",)";

    for (auto const &[tf, val] : candles) {
        msg.append("\"klineV2." + tf.symbol + ".");
        msg.append(strategy->getSymbol());
        msg.append("\",");
    }

    msg.pop_back();
    msg.append("]}");

    // The io_context is required for all I/O
    net::io_context ioc;

    // These objects perform our I/O
    tcp::resolver resolver{ioc};

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    websocket = std::make_shared<websocket::stream<ssl::stream<tcp::socket>>>(
        websocket::stream<ssl::stream<tcp::socket>>{ioc, ctx});

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(websocket->next_layer().native_handle(), host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    // Look up the domain name
    auto const results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    net::connect(websocket->next_layer().next_layer(), results);

    // Perform the SSL handshake
    websocket->next_layer().handshake(ssl::stream_base::client);

    // Set a decorator to change the User-Agent of the handshake
    websocket->set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
        req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

    // Perform the websocket handshake
    websocket->handshake(host, websocketTarget);

    // Send the message
    websocket->write(net::buffer(auth_msg));
    websocket->write(net::buffer(msg));
}

void Bybit::disconnect() {
    if (!websocket) {
        return;
    }
    websocket->close(websocket::close_code::normal);
}

bool Bybit::isConnected() {
    if (!websocket) {
        return false;
    }
    return websocket->is_open();
}

void Bybit::readWebsocket() {
    websocket->read(websocketBuffer);

    // Check for a message in our buffer
    if (websocketBuffer.size() != 0) {
        parseWebsocketMsg(beast::buffers_to_string(websocketBuffer.data()));
        websocketBuffer.clear();
    }
}

void Bybit::parseWebsocketMsg(const std::string &msg) {
//    std::cout << msg << std::endl;

    dom::parser parser;
    dom::element response = parser.parse(msg);
    dom::element elem;

    // authentication and subscribe messages
    auto error = response["success"].get(elem);
    if (!error) {
        std::string op = (std::string)response["request"]["op"];
        if (op == "auth") {
            std::cout << "Connected and authenticated with Bybit websocket " << GREEN << "✔" << RESET << std::endl;
            spdlog::info("[WebSocket] Connected to the Bybit Realtime API.");

        } else if (op == "subscribe") {
            for (dom::element item : response["request"]["args"]) {
                spdlog::info("[WebSocket] Successfully subscribed to " + (std::string)item);
            }
        }
        return;
    }

    error = response["topic"].get(elem);
    if (!error) {
        std::string topic = (std::string)response["topic"];

        if (topic == "position") {
            for (dom::object item : response["data"]) {
                std::string side = (std::string) item["side"];
                long qty = (long) item["size"];
                if (side == "Sell") { qty = -qty; }
                position->qty = qty;
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
                        break;
                    }
                }
            }
        }
    }
}

void Bybit::sendWebsocketHeartbeat() {
    if (isConnected()) { websocket->write(net::buffer(R"({"op":"ping"})")); }
}

void Bybit::placeMarketOrder(const Order &ord) {
    std::string expires = std::to_string(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()+ 1000);

    std::string endpoint = "/v2/private/order/create";
    // pairs have to be in alphabetic order
    cpr::Payload payload
        = cpr::Payload{{"api_key", apiKey}, {"order_type", "Market"}, {"qty", std::to_string(std::abs(ord.qty))}};

    cpr::CurlHolder holder;

    if (ord.reduce) {
        payload.AddPair({"reduce_only", "true"}, holder);
    }

    payload.AddPair({"side", ord.qty > 0 ? "Buy" : "Sell"}, holder);
    payload.AddPair({"symbol", strategy->getSymbol()}, holder);
    payload.AddPair({"time_in_force", "ImmediateOrCancel"}, holder);
    payload.AddPair({"timestamp", expires}, holder);
    payload.AddPair({"sign", HmacEncode(payload.content, apiSecret)}, holder);

    spdlog::debug("[HTTP-POST] " + baseUrl + endpoint + payload.content);
    cpr::Response r = cpr::Post(cpr::Url{baseUrl + endpoint}, payload);
    spdlog::debug("[RESP-" + std::to_string(r.status_code) + "] " + r.text);

    if (r.status_code != 200) {
        spdlog::error("Bybit::placeMarketOrder - bad response - " + r.text);
        throw std::runtime_error("Bad API response.");
    }

    dom::parser parser;
    dom::element response = parser.parse(r.text);

    int retCode = response["ret_code"].get_int64();

    if (retCode != 0) {
        spdlog::error("Bybit::placeMarketOrder - bad response - " + (std::string)response["ret_msg"]);
        throw std::runtime_error("Bad API response.");
    }

    position->activeOrder = true;
}

void Bybit::doAutomatedTrading() {
    if (newCandleAdded) {
        if (position->qty != 0) {
            auto exit = strategy->checkExit(candles, position);

            if (exit) {
                if (strategy->getOrderType() == "Market") {
                    placeMarketOrder(Order(-position->qty, true));
                }
            }
        }

        if (strategy->checkLongEntry(candles) && position->qty == 0 && !position->activeOrder) {
            if (strategy->getOrderType() == "Market") {
                placeMarketOrder(Order(strategy->getMaxQty()));
            }
        } else if (strategy->checkShortEntry(candles) && position->qty == 0 && !position->activeOrder) {
            if (strategy->getOrderType() == "Market") {
                placeMarketOrder(Order(-strategy->getMaxQty()));
            }
        }
        newCandleAdded = false;
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
