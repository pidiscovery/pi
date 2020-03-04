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
    * @brief Generate an deflation
    *
    * This operation is used to generate a deflation
    */    
    struct deflation_operation : public base_operation {
        struct fee_parameters_type {uint64_t fee = 0; };
        
        deflation_operation() {}

        asset fee;                          //this is virtual operation, no fee is charged
        account_id_type issuer;
        uint32_t rate;
        

        account_id_type fee_payer() const {
            return issuer;
        }

        void validate() const;

        /// This is a virtual operation; there is no fee
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

   /**
    * @ingroup operations
    * @brief Generate an account deflation
    *
    * This operation is used to generate a deflation for a specified account
    */    
    struct account_deflation_operation : public base_operation {
        struct fee_parameters_type {uint64_t fee = 0; };
        
        account_deflation_operation() {}

        asset fee;                          //this is virtual operation, no fee is charged
        deflation_id_type deflation_id;
        account_id_type owner;
        

        share_type amount;  // only for history detail

        account_id_type fee_payer() const {
            return GRAPHENE_TEMP_ACCOUNT;
        }

        void validate() const;

        /// This is a virtual operation; there is no fee
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

   /**
    * @ingroup operations
    * @brief Generate an order deflation
    *
    * This operation is used to generate a deflation for a specified account
    */    
    struct order_deflation_operation : public base_operation {
        struct fee_parameters_type {uint64_t fee = 0; };
        
        order_deflation_operation() {}

        asset fee;                          //this is virtual operation, no fee is charged
        deflation_id_type deflation_id;
        limit_order_id_type order;

        account_id_type owner;
        share_type amount;                  // only for history detail

        account_id_type fee_payer() const {
            return GRAPHENE_TEMP_ACCOUNT;
        }

        void validate() const;

        /// This is a virtual operation; there is no fee
        share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
    };

}} // graphene::chain

FC_REFLECT( graphene::chain::deflation_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::deflation_operation, (fee)(issuer)(rate) )

FC_REFLECT( graphene::chain::account_deflation_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::account_deflation_operation, (fee)(deflation_id)(owner)(amount) )

FC_REFLECT( graphene::chain::order_deflation_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::order_deflation_operation, (fee)(deflation_id)(order)(owner)(amount) )
