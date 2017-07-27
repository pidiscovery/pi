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
#include <boost/multi_index/composite_key.hpp>
#include <fc/uint128.hpp>


namespace graphene { namespace chain {
    // class database;
    /**
    * @brief This class represents a construction capital created by a specified account.
    * @ingroup object
    * @ingroup protocol
    *
    * construction capital is used when calculating incetive.
    */
    class construction_capital_object : public graphene::db::abstract_object<construction_capital_object> {
    public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = construction_capital_object_type;
        
        account_id_type owner;
        share_type amount;
        uint32_t period;
        uint16_t total_periods;
        fc::time_point_sec timestamp;

        uint16_t achieved;
        uint16_t pending;
        fc::uint128 left_vote_point;
        fc::time_point_sec next_slot;
    };

    struct by_account;
    struct by_next_slot;
    struct by_pending;
    typedef multi_index_container<
        construction_capital_object,
        indexed_by<
            ordered_unique< 
                tag<by_id>, 
                member< 
                    object, 
                    object_id_type, 
                    &object::id 
                > 
            >,
            ordered_non_unique< 
                tag<by_account>, 
                member< 
                    construction_capital_object, 
                    account_id_type, 
                    &construction_capital_object::owner 
                > 
            >,
            ordered_non_unique< 
                tag<by_next_slot>, 
                member< 
                    construction_capital_object, 
                    fc::time_point_sec, 
                    &construction_capital_object::next_slot 
                > 
            >,
            ordered_non_unique< 
                tag<by_pending>, 
                member< 
                    construction_capital_object, 
                    uint16_t, 
                    &construction_capital_object::pending 
                > 
            >
        >
    > construction_capital_index_type;
    typedef generic_index<construction_capital_object, construction_capital_index_type> construction_capital_index;

    // class database;
    /**
    * @brief This class represents a construction capital vote
    * @ingroup object
    * @ingroup protocol
    *
    * construction capital vote can accelerate other account's 
    * incentive speed
    */
    class construction_capital_vote_object : public graphene::db::abstract_object<construction_capital_vote_object> {
    public:
        static const uint8_t space_id = protocol_ids;
        static const uint8_t type_id  = construction_capital_vote_object_type;
        construction_capital_id_type cc_from;
        construction_capital_id_type cc_to;
    };

    struct by_vote_from;
    struct by_vote_to;
    struct by_vote_pair;
    typedef multi_index_container<
        construction_capital_vote_object,
        indexed_by<
            ordered_unique< 
                tag<by_id>, 
                member< 
                    object, 
                    object_id_type, 
                    &object::id 
                > 
            >,
            ordered_non_unique< 
                tag<by_vote_from>, 
                member< 
                    construction_capital_vote_object, 
                    construction_capital_id_type, 
                    &construction_capital_vote_object::cc_from 
                > 
            >,
            ordered_non_unique< 
                tag<by_vote_to>, 
                member< 
                    construction_capital_vote_object, 
                    construction_capital_id_type, 
                    &construction_capital_vote_object::cc_to 
                > 
            >,
            ordered_unique< 
                tag<by_vote_pair>, 
                composite_key< 
                    construction_capital_vote_object, 
                    member< 
                        construction_capital_vote_object, 
                        construction_capital_id_type, 
                        &construction_capital_vote_object::cc_from 
                    >, 
                    member< 
                        construction_capital_vote_object, 
                        construction_capital_id_type, 
                        &construction_capital_vote_object::cc_to 
                    > 
                > 
            >
        >
    > construction_capital_vote_index_type;
    typedef generic_index<construction_capital_vote_object, construction_capital_vote_index_type> construction_capital_vote_index;

}} // graphene::chain


FC_REFLECT_DERIVED( graphene::chain::construction_capital_object,
                    (graphene::db::object),
                    (owner)
                    (amount)
                    (period)
                    (total_periods)
                    (timestamp)
                    (achieved)
                    (pending)
                    (left_vote_point)
                    (next_slot)
                )                    

FC_REFLECT_DERIVED( graphene::chain::construction_capital_vote_object,
                    (graphene::db::object),
                    (cc_from)(cc_to) 
                )
