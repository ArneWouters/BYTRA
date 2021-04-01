//
// Created by awouters on 22/01/2021.
//

#ifndef BYTRA_PAYLOAD_H
#define BYTRA_PAYLOAD_H

#include <string>

namespace RESTClient {
    struct Pair {
        Pair(const std::string& key, const std::string& value)
            : key{key}, value{value} {}
        Pair(std::string&& key, std::string&& value)
            : key{std::move(key)}, value{std::move(value)} {}

        std::string key;
        std::string value;
    };

    class Payload {
      public:
        Payload() = default;

        Payload(const std::initializer_list<Pair>& pairs);

        void addPair(const Pair &p);

        std::string content;
        std::string json{"{}"};
    };
}

#endif  // BYTRA_PAYLOAD_H
