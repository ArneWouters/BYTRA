//
// Created by Arne Wouters on 30/07/2020.
//

#ifndef MEXTRA_CANDLE_H
#define MEXTRA_CANDLE_H

#include <spdlog/spdlog.h>

#include <map>
#include <string>
#include <vector>

struct TimeFrame {
    int tick;  // 1 tick = 1 minute

    static TimeFrame parse(const std::string& tf) {
        std::map<std::string, int> timeframe_symbol_map = {{"m", 1}, {"h", 60}, {"d", 1440}, {"W", 10080}};
        int tf_pre = std::stoi(tf.substr(0, tf.size() - 1));
        std::string tf_symbol = tf.substr(tf.size() - 1, 1);

        auto it = timeframe_symbol_map.find(tf_symbol);

        if (it == timeframe_symbol_map.end()) {
            auto logger = spdlog::get("basic_logger");
            logger->error("TimeFrame::parse - invalid timeframe");
            logger->flush();
            throw std::invalid_argument("Invalid timeframe: " + tf);
        }

        return TimeFrame{tf_pre * timeframe_symbol_map[tf_symbol]};
    }

    [[nodiscard]] TimeFrame getBaseTimeFrame() const {
        TimeFrame tf{0};
        if (1 < tick && tick < 5) { tf.tick = 1; }
        else if (5 < tick && tick < 60) { tf.tick = 5; }
        else if (60 < tick && tick < 1440) { tf.tick = 60; }
        else if (1440 < tick) { tf.tick = 1440; }
        return tf;
    }
};

inline bool operator<(const TimeFrame& tf1, const TimeFrame& tf2) { return tf1.tick < tf2.tick; }

struct Datetime {
    long value;  // example: 2020-11-27 16:30 is stored as 202011271630

    [[nodiscard]] long get_year() const { return value / 100000000; }

    [[nodiscard]] long get_month() const { return (value % 100000000) / 1000000; }

    [[nodiscard]] long get_day() const { return (value % 1000000) / 10000; }

    [[nodiscard]] long get_hour() const { return (value % 10000) / 100; }

    [[nodiscard]] long get_minute() const { return (value % 100); }

    static Datetime parse(const std::string& timestamp) {
        /**
         * @param timestamp with form like 2020-08-01T13:02:00.000Z
         */
        std::string val = timestamp.substr(0, 4) + timestamp.substr(5, 2) + timestamp.substr(8, 2)
                          + timestamp.substr(11, 2) + timestamp.substr(14, 2);

        return Datetime{std::stol(val)};
    }

    [[nodiscard]] std::string to_string() const {
        std::string year = std::to_string(get_year());
        std::string month = std::to_string(get_month());
        std::string day = std::to_string(get_day());
        std::string hour = std::to_string(get_hour());
        std::string minute = std::to_string(get_minute());

        if (minute.size() == 1) {
            minute = "0" + minute;
        }

        if (hour.size() == 1) {
            hour = "0" + hour;
        }

        return year + "-" + month + "-" + day + " " + hour + ":" + minute;
    }
};

struct Candle {
    double open;
    double high;
    double low;
    double close;
    double volume;
    Datetime timestamp;
    TimeFrame timeframe;

    [[nodiscard]] std::string to_string() const {
        std::string s = "{open: " + std::to_string(open) + ", high: " + std::to_string(high)
                        + ", low: " + std::to_string(low) + ", close: " + std::to_string(close)
                        + ", timestamp: " + std::to_string(timestamp.value) + ", volume: " + std::to_string(volume)
                        + ", timeframe: " + std::to_string(timeframe.tick) + "}";
        return s;
    }
};

#endif  // MEXTRA_CANDLE_H
