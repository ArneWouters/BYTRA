//
// Created by awouters on 22/01/2021.
//

#include <simdjson.h>
#include <spdlog/spdlog.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <chrono>

#include "WebSocket.h"
#include "../Encryption.h"

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace ssl = boost::asio::ssl;        // from <boost/asio/ssl.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>
using namespace std::chrono;
using namespace simdjson;

WebSocket::WebSocket(std::string &host, std::string &target, std::string &apiKey, std::string &apiSecret) {
    this->host = host;
    this->target = target;
    this->apiKey = apiKey;
    this->apiSecret = apiSecret;
}

void WebSocket::connect(const std::vector<std::string> &topics) {
    std::string expires
        = std::to_string(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() + 5000);

    std::string auth_msg = R"({"op":"auth","args":[")" + apiKey + R"(",")" + expires + R"(",")"
                           + HmacEncode("GET/realtime" + expires, apiSecret) + R"("]})";
    std::string topics_msg
        = R"({"op": "subscribe", "args": [)";

    for (auto &topic: topics) {
        topics_msg.append("\"");
        topics_msg.append(topic);
        topics_msg.append("\",");
    }

    if (!topics.empty()) {
        topics_msg.pop_back();
    }

    topics_msg.append("]}");

    const std::string port = "443";

    // The io_context is required for all I/O
    net::io_context ioc;

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv12_client};

    // These objects perform our I/O
    tcp::resolver resolver{ioc};

    ws.reset(new websocket::stream<ssl::stream<tcp::socket>>{ioc, ctx});

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(ws->next_layer().native_handle(), host.c_str())) {
        boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
        throw boost::system::system_error{ec};
    }

    // Look up the domain name
    auto const results = resolver.resolve(host, port);

    // Make the connection on the IP address we get from a lookup
    net::connect(ws->next_layer().next_layer(), results);

    // Perform the SSL handshake
    ws->next_layer().handshake(ssl::stream_base::client);

    // Set a decorator to change the User-Agent of the handshake
    ws->set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
      req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
    }));

    // Perform the websocket handshake
    ws->handshake(host, target);

    // Send the message
    ws->write(net::buffer(auth_msg));
    ws->write(net::buffer(topics_msg));
}

[[maybe_unused]] void WebSocket::disconnect() {
    if (!ws) { return; }
    ws->close(websocket::close_code::normal);
    ws.reset();
}

bool WebSocket::isConnected() {
    if (!ws) { return false; }
    return ws->is_open();
}

std::string WebSocket::readBuffer() {
    ws->read(messageBuffer);
    std::string message{};

    // Check for a message in our buffer
    if (messageBuffer.size() != 0) {
        message = beast::buffers_to_string(messageBuffer.data());
        messageBuffer.clear();
    }

    return message;
}

void WebSocket::sendHeartbeat() {
    if (isConnected()) {
        ws->write(net::buffer(R"({"op":"ping"})"));
    }
}

void WebSocket::writeMessage(const std::string &msg) {
    ws->write(net::buffer(msg));
}
