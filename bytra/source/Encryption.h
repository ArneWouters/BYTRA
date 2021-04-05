//
// Created by Arne Wouters on 20/08/2020.
//

#ifndef BYTRA_ENCRYPTION_H
#define BYTRA_ENCRYPTION_H

#include <openssl/hmac.h>

#include <iomanip>
#include <string>
#include <map>

typedef std::map<std::string, std::string> Params;

std::string HmacEncode(const std::string &param_str, const std::string &apiSecret);

[[maybe_unused]] std::string GetSignature(const Params &param, const std::string &secret);

#endif  // BYTRA_ENCRYPTION_H
