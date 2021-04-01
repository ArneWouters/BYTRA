//
// Created by awouters on 22/01/2021.
//

#include "Parameters.h"

namespace RESTClient {
    Parameters::Parameters(const std::initializer_list<Parameter>& parameters) {
        bool first = true;

        for (const auto &p: parameters) {
            if (first) { first = !first; }
            else { content.append("&"); }
            content.append(p.key + "=" + p.value);
        }
    }

    void Parameters::addParameter(const Parameter &p) {
        if (!content.empty()) { content.append("&"); }
        content.append(p.key + "=" + p.value);
    }
}
