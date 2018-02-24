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
   /**
    * @ingroup operations
    * @brief Create a construction capital
    */  
    struct construction_capital_create_operation : public base_operation {
        struct fee_parameters_type { uint64_t fee = 0; };

        asset           fee;
        account_id_type account_id;
        share_type amount;          //lock amount
        uint32_t period;            //release period (in seconds)
        uint16_t total_periods;     //total locke periods

        account_id_type fee_payer() const { 
            return account_id; 
        }
        void validate() const;
        share_type calculate_fee(const fee_parameters_type& k ) const {
            return k.fee;
        }
    };

   /**
    * @ingroup operations
    * @brief Vote other's construction capital to incentive their incentive speed
    */  
    struct construction_capital_vote_operation : public base_operation {
        struct fee_parameters_type { uint64_t fee = 0; };
        asset           fee;
        account_id_type account_id;
        construction_capital_id_type cc_from;   //own construction capital id
        construction_capital_id_type cc_to;     //their's construction capital id
        account_id_type fee_payer() const { 
            return account_id;
        }
        void validate() const;
        share_type calculate_fee(const fee_parameters_type& k ) const {
            return k.fee;
        }
    };

   /**
    * @ingroup operations
    * @brief Vote other's construction capital to incentive their incentive speed
    */  
    struct construction_capital_rate_vote_operation : public base_operation {
        struct fee_parameters_type { uint64_t fee = 0; };
        asset           fee;
        account_id_type account_id;
        uint8_t vote_option;
        account_id_type fee_payer() const { 
            return account_id;
        }
        void validate() const;
        share_type calculate_fee(const fee_parameters_type& k ) const {
            return k.fee;
        }
    };
}} // graphene::chain

FC_REFLECT( graphene::chain::construction_capital_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::construction_capital_create_operation, (fee)(account_id)(amount)(period)(total_periods) )

FC_REFLECT( graphene::chain::construction_capital_vote_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::construction_capital_vote_operation, (fee)(account_id)(cc_from)(cc_to) )

FC_REFLECT( graphene::chain::construction_capital_rate_vote_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::construction_capital_rate_vote_operation, (fee)(account_id)(vote_option) )
