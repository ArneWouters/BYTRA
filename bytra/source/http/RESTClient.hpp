//
// Created by awouters on 22/01/2021.
//

#ifndef BYTRA_RESTCLIENT_HPP
#define BYTRA_RESTCLIENT_HPP

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Parameters.h"
#include "Payload.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace RESTClient {
    std::string sendRequest(const std::string &host, http::request<http::string_body> &req) {
        auto const port = "443";

        // The io_context is required for all I/O
        net::io_context ioc;

        // The SSL context is required, and holds certificates
        ssl::context ctx(ssl::context::tlsv12_client);

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        // Set SNI Hostname (many hosts need this to handshake successfully)
        if(! SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            throw beast::system_error{ec};
        }

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(stream).connect(results);

        // Perform the SSL handshake
        stream.handshake(ssl::stream_base::client);

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        if(ec == net::error::eof)
        {
            // Rationale:
            // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
            ec = {};
        }
        if(ec)
            throw beast::system_error{ec};

        // If we get here then the connection is closed gracefully
        return res.body();
    }

    std::string Get(const std::string &host, const std::string &endpoint, const Parameters &params) {
        spdlog::debug("[HTTP GET] <host:" + host + "> <endpoint:"
                      + endpoint + "> <parameters:" + params.content + ">");
        auto const target = (std::string) endpoint + "?" + params.content;

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, target, 1.0};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        auto response_json = sendRequest(host, req);
        spdlog::debug("[RESPONSE] " + response_json);

        return response_json;
    }

    std::string Post(const std::string &host, const std::string &endpoint, const Payload &payload) {
        spdlog::debug("[HTTP POST] <host:" + host + "> <endpoint:"
                      + endpoint + "> <payload:" + payload.content + ">");

        // Set up an HTTP POST request message
        http::request<http::string_body> req{http::verb::post, endpoint, 1.0};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.body() = payload.json;
        req.set(http::field::content_type, "application/json");
        req.prepare_payload();
        auto response_json = sendRequest(host, req);
        spdlog::debug("[RESPONSE] " + response_json);

        return response_json;
    }
}

#endif  // BYTRA_RESTCLIENT_HPP
