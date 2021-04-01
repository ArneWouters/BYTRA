//
// Created by awouters on 22/01/2021.
//

#include "Payload.h"

namespace RESTClient {
    Payload::Payload(const std::initializer_list<Pair> &pairs) {
        bool first = true;
        json.pop_back();

        for (const auto &p: pairs) {
            if (first) { first = !first; }
            else { json.append(","); content.append("&"); }
            json.append("\"" + p.key + "\":");
            content.append(p.key + "=" + p.value);

            json.append("\"" + p.value + "\"");
        }
        json.append("}");
    }

    void Payload::addPair(const Pair &p) {
        json.pop_back();
        if (json.size() > 1) { json.append(","); }
        json.append("\"" + p.key + "\":");

        if (!content.empty()) { content.append("&"); }
        content.append(p.key + "=" + p.value);
        json.append("\"" + p.value + "\"");
        json.append("}");
    }
}
