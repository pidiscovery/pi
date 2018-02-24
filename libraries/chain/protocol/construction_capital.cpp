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
#include <graphene/chain/protocol/construction_capital.hpp>
#include <graphene/chain/hardfork.hpp>


namespace graphene { namespace chain {

    void construction_capital_create_operation::validate() const {
        // FC_ASSERT( fee.amount >= 0 );
        // FC_ASSERT( amount >= GRAPHENE_DEFAULT_MIN_CONSTRUCTION_CAPITAL_AMOUNT );
        // FC_ASSERT( period >= GRAPHENE_DEFAULT_MIN_CONSTRUCTION_CAPITAL_PERIOD 
        //     &&  period <= GRAPHENE_DEFAULT_MAX_CONSTRUCTION_CAPITAL_PERIOD);
        // FC_ASSERT( total_periods >= GRAPHENE_DEFAULT_MIN_CONSTRUCTION_CAPITAL_PERIOD_LEN 
        //     &&  total_periods <= GRAPHENE_DEFAULT_MAX_CONSTRUCTION_CAPITAL_PERIOD_LEN );
    }

    void construction_capital_vote_operation::validate() const {
    }
    
    void construction_capital_rate_vote_operation::validate() const {
    }
    
}} // graphene::chain
