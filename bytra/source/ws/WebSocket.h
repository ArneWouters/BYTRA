//
// Created by awouters on 22/01/2021.
//

#ifndef BYTRA_WEBSOCKET_H
#define BYTRA_WEBSOCKET_H


#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <string>

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace ssl = boost::asio::ssl;        // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

class WebSocket {
  public:
    WebSocket(std::string &host, std::string &target, std::string &apiKey, std::string &apiSecret);

    void connect(const std::vector<std::string> &topics);

    [[maybe_unused]] void disconnect();

    bool isConnected();

    std::string readBuffer();

    void sendHeartbeat();

    void writeMessage(const std::string &msg);

  private:
    std::string host;
    std::string target;
    std::string apiKey;
    std::string apiSecret;
    beast::flat_buffer messageBuffer;
    std::shared_ptr<websocket::stream<ssl::stream<tcp::socket>>> ws;
};

#endif  // BYTRA_WEBSOCKET_H
