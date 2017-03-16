/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * generate BTS accounts
 */

#include <graphene/chain/pts_address.hpp>

#include <graphene/chain/protocol/address.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/elliptic.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/address.hpp>
#include <graphene/utilities/key_conversion.hpp>

#include <iostream>
#include <string>
#include <random>

using namespace graphene::chain;
using namespace graphene::utilities;

const char ALPHA[] = "abcdefghijklmnopqrstuvwxyz";
const int ALPHA_LEN = strlen(ALPHA);
const char NUMBER[] = "0123456789";
const int NUMBER_LEN = 10;

uint32_t rand_num(uint32_t min, uint32_t max) {
    static std::random_device rd;
    uint32_t n = rd();
    return n % (max - min) + min;
}

char rand_char() {
    if (rand_num(0, 8) == 0) {
        return NUMBER[rand_num(0, NUMBER_LEN)];
    } else {
        return ALPHA[rand_num(0, ALPHA_LEN)];
    }
}

std::string rand_name() {
    std::string name = "";
    //name begin with char
    name += ALPHA[rand_num(0, ALPHA_LEN)];
    //at least 8 chars
    for (int i = 0; i < 8; i++) {
        name += rand_char();
    }
    //add 0~16 chars
    uint32_t var_len = rand_num(0, 16);
    for (int i = 0; i < var_len; i++) {
        name += rand_char();
    }
    return name;
}

std::string rand_seed() {
    std::string seed = "";
    for (int i = 0; i < 64; i++) {
        seed += rand_char();
    }
    return seed;
}

int main(int argc, char *argv[]) {
    
    int acc_cnt = 1;
    if (argc > 1) {
        acc_cnt = std::stoi(argv[1]);
    }
    
    for (int i = 0; i < acc_cnt; i++) {
        auto seed = rand_seed();
        auto name = rand_name();
        auto key = fc::ecc::private_key::regenerate(fc::sha256::hash(seed));
        auto pub_key = key.get_public_key();
        auto bts_pub_key = public_key_type(pub_key);
        
        std::cout << key_to_wif(key)
            << "\t" << std::string(bts_pub_key)
            << "\t" << std::string(address(pub_key))
            << "\t" << seed
            << "\t" << name
            << std::endl;
    }
    
    return 0;
}
