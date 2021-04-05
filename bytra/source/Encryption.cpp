//
// Created by awouters on 05/04/2021.
//

#include "Encryption.h"

std::string HmacEncode(const std::string &param_str, const std::string &apiSecret) {
    std::stringstream ss;
    HMAC_CTX *h = HMAC_CTX_new();
    unsigned int len;
    unsigned char out[EVP_MAX_MD_SIZE];
    HMAC_Init_ex(h, apiSecret.c_str(), apiSecret.length(), EVP_sha256(), nullptr);
    HMAC_Update(h, (unsigned char *)param_str.c_str(), param_str.length());
    HMAC_Final(h, out, &len);
    HMAC_CTX_free(h);

    for (unsigned int i = 0; i < len; i++) {
        ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(out[i]);
    }

    return ss.str();
}

[[maybe_unused]] std::string GetSignature(const Params &param, const std::string &secret) {
    std::string input;
    for (auto &it : param) {
        input += it.first + "=" + it.second + "&";
    }

    return HmacEncode(secret, input.substr(0, input.length() - 1));
}