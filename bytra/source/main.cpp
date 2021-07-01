//
// Created by Arne Wouters on 29/07/2020.
//

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <chrono>
#include <cli/CLI11.hpp>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <toml++/toml.hpp>
#include <simdjson.h>

#include "Bybit.h"
#include "TerminalColors.h"
#include "strategies/Ema.h"
#include "strategies/Rsi.h"
#include "strategies/Strategy.h"

namespace beast = boost::beast;

void checkEnvVar(std::string &var) {
    if (var.size() > 3 && var.substr(0, 2) == "${" && var[var.size()-1] == '}') {
        if (auto env_var = std::getenv(var.substr(2, var.size()-3).c_str()); env_var) {
            var = env_var;
            return;
        }

        std::cerr << "Undefined environment variable in configuration: " << var << std::endl;
        spdlog::error("Undefined environment variable in configuration: {}", var);
        exit(1);
    }
}

void signal_callback_handler(int signum) {
    std::cout << std::endl;
    std::cout << "...Terminating" << std::endl;
    spdlog::debug("Terminating program.");
    std::cout << RED << "✗ " << RESET << "Program terminated." << std::endl;
    exit(signum);
}

void run(std::shared_ptr<WebSocket> &ws, std::shared_ptr<Bybit> &bybit) {
    bybit->setPosition(bybit->getPosition());
    ws->connect(bybit->getTopics());
    long websocketHeartbeatTimer = std::time(nullptr);
    long websocketOrderBookSyncTimer = std::time(nullptr);
    long positionUpdateTimer = std::time(nullptr);

    // Program Loop
    for (;;) {
        while (ws->isConnected()) {
            std::string message{};

            try {
                message = ws->readBuffer();
            } catch (const boost::system::system_error &err) {
                if (err.code() == boost::asio::error::eof) {
                    spdlog::error("boost::system::system_error: {}", err.what());
                    break;
                }

                throw boost::system::system_error(err);
            }

            if (!message.empty()) {
                bybit->parseWebsocketMessage(message);
            }

            bybit->doAutomatedTrading();
            bybit->removeUnusedCandles();

            long currentTime = std::time(nullptr);

            // Send heartbeat packet every 45 seconds to maintain websocket connection
            if (currentTime - websocketHeartbeatTimer > 45) {
                websocketHeartbeatTimer = currentTime;
                ws->sendHeartbeat();
            }

            // Sync order book after an hour
            if (currentTime - websocketOrderBookSyncTimer > 3600) {
                websocketOrderBookSyncTimer = currentTime;
                bybit->syncOrderBook();
            }

            // Update position
            if (currentTime - positionUpdateTimer > 1) {
                bybit->printPosition();
                positionUpdateTimer = currentTime;
            }
        }

        sleep(3);
        std::cout << "Attempting to reconnect..." << std::endl;
        ws->disconnect();
        ws->connect(bybit->getTopics());
    }
}

void checkOrders(std::shared_ptr<Bybit> &bybit) {
    // Check for active orders, ask to cancel them or abort
    auto orders = bybit->getActiveOrders();
    if (orders.begin() == orders.end()) return;

    std::string word;
    std::cout << "Active orders found! Do you want Bytra to cancel them? [yes/no] " << std::endl;

    while (std::cin >> word) {
        if (word == "y") {
            bybit->cancelAllActiveOrders();
            spdlog::info("Cancelled all active orders.");
            break;
        } else if (word == "n") {
            std::cout << "Please cancel the active orders before using this program." << std::endl;
            signal_callback_handler(0);
        } else {
            std::cout << "Please respond with y(es) or n(o)." << std::endl;
        }
    }
}

int main(int argc, char **argv) {
    std::ios_base::sync_with_stdio(false);
    std::cout << "    ____ __  __ ______ ____   ___ " << std::endl;
    std::cout << R"(   / __ )\ \/ //_  __// __ \ /   |)" << std::endl;
    std::cout << R"(  / __  | \  /  / /  / /_/ // /| |)" << std::endl;
    std::cout << " / /_/ /  / /  / /  / _, _// ___ |" << std::endl;
    std::cout << "/_____/  /_/  /_/  /_/ |_|/_/  |_|   Made by Arne." << std::endl << std::endl;

    // CLI setup
    CLI::App app("BYTRA");
    std::string strategy;
    app.add_option("-s,--strategy,strategy", strategy, "Name of the strategy")->required();
    int d{0};
    app.add_flag("-d,--debug", d, "Debug flag that enables debug logging");
    int t{0};
    app.add_flag("-t,--testnet", t, "Testnet flag that makes it use the testnet configuration");

    CLI11_PARSE(app, argc, argv)

    // Register signal and signal handler
    signal(SIGINT, signal_callback_handler);

    std::cout << "Setting up BYTRA" << std::endl;
    std::cout << " - Logging setup" << std::flush;

    // spdlog setup
    try {
        std::string fname = "data/log-" + std::to_string((int) std::time(nullptr)) + ".txt";
        auto logger = spdlog::basic_logger_mt("basic_logger", fname);
        spdlog::set_default_logger(logger);
        spdlog::flush_every(std::chrono::seconds(1));
        spdlog::flush_on(spdlog::level::err);
        spdlog::set_pattern("[%Y-%m-%d %T.%f] [%L] %v");
        spdlog::info("Welcome to spdlog!");

        if (d) {
            spdlog::set_level(spdlog::level::debug);
            spdlog::debug("Debug ON");
        }

        spdlog::info("Using Strategy: {}", strategy);
    } catch (const spdlog::spdlog_ex &ex) {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    std::cout << GREEN << " ✔" << RESET << std::endl;
    std::cout << " - Parsing configuration file: " << std::flush;

    // Parsing configuration file
    toml::table tbl;
    try {
        tbl = toml::parse_file("data/configuration.toml");
    } catch (const toml::parse_error &err) {
        std::cerr << "Parsing failed:\n" << err << std::endl;
        spdlog::error("Parsing configuration failed");
        return 1;
    }

    std::map<std::string, std::shared_ptr<Strategy>> validStrategies
        = {{"rsi", std::make_shared<Rsi>()},
           {"ema", std::make_shared<Ema>()}};

    if (!validStrategies[strategy].get()) {
        spdlog::error("Invalid strategy: " + strategy);
        throw std::invalid_argument("Invalid strategy: " + strategy);
    }

    std::cout << strategy << " strategy found! " << GREEN << "✔" << RESET << std::endl;
    std::cout << " - Setting up strategy" << std::flush;

    std::string configEntry = t ? "bybit-testnet" : "bybit";
    std::string baseUrl = *tbl[configEntry]["baseUrl"].value<std::string>();
    std::string websocketHost = *tbl[configEntry]["websocketHost"].value<std::string>();
    std::string websocketTarget = *tbl[configEntry]["websocketTarget"].value<std::string>();
    std::string apiKey = *tbl[configEntry]["apiKey"].value<std::string>();
    std::string apiSecret = *tbl[configEntry]["apiSecret"].value<std::string>();

    // Check if apiKey and apiSecret are env variables
    checkEnvVar(apiKey);
    checkEnvVar(apiSecret);

    auto ws = std::make_shared<WebSocket>(websocketHost, websocketTarget, apiKey, apiSecret);
    auto bybit = std::make_shared<Bybit>(baseUrl, apiKey, apiSecret, ws, validStrategies[strategy]);

    std::cout << GREEN << " ✔" << RESET << std::endl;

    // Check for active orders
    checkOrders(bybit);

    std::cout << "Connecting..." << std::endl;
    run(ws, bybit);

    return 0;
}
