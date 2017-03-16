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

#pragma once

#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {
    struct incentive_operation : public base_operation {
        struct fee_parameters_type {uint64_t fee = 0; };
        
        incentive_operation() {}
        incentive_operation(account_id_type acc_id, asset a) 
            : account_id(acc_id), amount(a) {
        }
        account_id_type account_id;
        asset amount;
        asset fee;

        account_id_type fee_payer() const {
            return account_id;
        }

        void validate() const;

        /// This is a virtual operation; there is no fee
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };
}} // graphene::chain

FC_REFLECT( graphene::chain::incentive_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::incentive_operation, (account_id)(amount) )
