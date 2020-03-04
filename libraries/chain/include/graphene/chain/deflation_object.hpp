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

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>
// #include <boost/multi_index/composite_key.hpp>
// #include <fc/uint128.hpp>


namespace graphene { namespace chain {
    class deflation_object : public graphene::db::abstract_object<deflation_object> {
    public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = deflation_object_type;
        
        fc::time_point_sec timestamp;
        account_id_type issuer;
        uint32_t rate; 
        
        account_id_type last_account;
        account_id_type account_cursor;
        bool balance_cleared;

        limit_order_id_type last_order;
        limit_order_id_type order_cursor;
        bool order_cleared;

        share_type total_amount;
    };

    struct by_cursor;
    typedef multi_index_container<
        deflation_object,
        indexed_by<
            ordered_unique< 
                tag<by_id>, 
                member< 
                    object, 
                    object_id_type, 
                    &object::id 
                > 
            >
        >
    > deflation_index_type;
    typedef generic_index<deflation_object, deflation_index_type> deflation_index;    

    // class account_deflation_object;
    /**
    * @brief This class represents deflation for a specified account.
    * @ingroup object
    * @ingroup protocol
    *
    */
    class account_deflation_object : public graphene::db::abstract_object<account_deflation_object> {
    public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = account_deflation_object_type;
        
        account_id_type owner;
        deflation_id_type last_deflation_id;
        share_type frozen;
        bool cleared;
    };

    struct by_owner;
    typedef multi_index_container<
        account_deflation_object,
        indexed_by<
            ordered_unique< 
                tag<by_id>, 
                member< 
                    object, 
                    object_id_type, 
                    &object::id 
                > 
            >,
            ordered_unique< 
                tag<by_owner>, 
                member< 
                    account_deflation_object, 
                    account_id_type, 
                    &account_deflation_object::owner 
                > 
            >
        >
    > account_deflation_index_type;
    typedef generic_index<account_deflation_object, account_deflation_index_type> account_deflation_index;


    // class order_deflation_object;
    /**
    * @brief This class represents deflation for a specified order.
    * @ingroup object
    * @ingroup protocol
    *
    */
    class order_deflation_object : public graphene::db::abstract_object<order_deflation_object> {
    public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = order_deflation_object_type;
        
        limit_order_id_type order;
        deflation_id_type last_deflation_id;

        share_type frozen;
        bool cleared;
    };

    struct by_order;
    typedef multi_index_container<
        order_deflation_object,
        indexed_by<
            ordered_unique< 
                tag<by_id>, 
                member< 
                    object, 
                    object_id_type, 
                    &object::id 
                > 
            >,
            ordered_unique< 
                tag<by_order>, 
                member< 
                    order_deflation_object, 
                    limit_order_id_type, 
                    &order_deflation_object::order
                > 
            >
        >
    > order_deflation_index_type;
    typedef generic_index<order_deflation_object, order_deflation_index_type> order_deflation_index;


}} // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::deflation_object,
                    (graphene::db::object),
                    (timestamp)(issuer)(rate)(last_account)(account_cursor)(balance_cleared)(last_order)(order_cursor)(order_cleared)(total_amount)
                )

FC_REFLECT_DERIVED( graphene::chain::account_deflation_object,
                    (graphene::db::object),
                    (owner)
                    (last_deflation_id)
                    (frozen)
                    (cleared)
                )

FC_REFLECT_DERIVED( graphene::chain::order_deflation_object,
                    (graphene::db::object),
                    (order)
                    (last_deflation_id)
                    (frozen)
                    (cleared)
                )

