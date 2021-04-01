//
// Created by awouters on 22/01/2021.
//

#ifndef BYTRA_PARAMETERS_H
#define BYTRA_PARAMETERS_H

#include <string>

namespace RESTClient {
    struct Parameter {
        Parameter(const std::string& key, const std::string& value)
            : key{key}, value{value} {}
        Parameter(std::string&& key, std::string&& value)
            : key{std::move(key)}, value{std::move(value)} {}

        std::string key;
        std::string value;
    };

    class Parameters {
      public:
        Parameters() = default;

        Parameters(const std::initializer_list<Parameter>& parameters);

        void addParameter(const Parameter &p);

        std::string content{};
    };
}


#endif  // BYTRA_PARAMETERS_H
