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

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/operation_history_object.hpp>

#include <fc/thread/future.hpp>

namespace graphene { namespace incentive_history {

using namespace chain;

#ifndef ACCOUNT_HISTORY_SPACE_ID
#define ACCOUNT_HISTORY_SPACE_ID 5
#endif

#define CONSTRUCTION_CAPITAL_HISTORY_TYPE_ID 2

struct incentive_record {
    fc::time_point_sec timestamp;
    share_type amount;
    uint8_t reason;
};

struct construction_capital_vote_record {
    construction_capital_id_type cc_from;
    construction_capital_id_type cc_to;
    uint32_t accelerate;
    fc::time_point_sec timestamp;
};

struct construction_capital_history_object : public abstract_object<construction_capital_history_object> {
    static const uint8_t space_id = ACCOUNT_HISTORY_SPACE_ID;
    static const uint8_t type_id  = CONSTRUCTION_CAPITAL_HISTORY_TYPE_ID;
    
    construction_capital_id_type ccid;
    account_id_type owner;
    share_type amount;
    uint32_t period;
    uint16_t total_periods;
    fc::time_point_sec timestamp;
    fc::time_point_sec next_slot;
    uint16_t achieved;

    vector<construction_capital_vote_record> vote_from;
    vector<construction_capital_vote_record> vote_to;
    vector<incentive_record> incentive;
};

struct by_obj_id;
struct by_cc_id;

typedef multi_index_container<
    construction_capital_history_object,
    indexed_by<
        ordered_unique< 
            tag<by_obj_id>, 
            member< 
                object, 
                object_id_type, 
                &object::id 
            > 
        >,
        ordered_unique<
            tag<by_cc_id>, 
            member< 
                construction_capital_history_object, 
                construction_capital_id_type, 
                &construction_capital_history_object::ccid
            >
        >
    >
> construction_capital_history_multi_index_type;

typedef generic_index<construction_capital_history_object, construction_capital_history_multi_index_type> construction_capital_history_index;

namespace detail {
    class incentive_history_plugin_impl;
}

class incentive_history_plugin : public graphene::app::plugin {
    public:
        incentive_history_plugin();
        virtual ~incentive_history_plugin();

        std::string plugin_name()const override;
        virtual void plugin_set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& cfg) override;
        virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
        virtual void plugin_startup() override;

        friend class detail::incentive_history_plugin_impl;
        std::unique_ptr<detail::incentive_history_plugin_impl> my;
};

} } //graphene::incentive_history

FC_REFLECT( graphene::incentive_history::incentive_record, (timestamp)(amount)(reason) )
FC_REFLECT( graphene::incentive_history::construction_capital_vote_record, (cc_from)(cc_to)(accelerate)(timestamp) )
FC_REFLECT_DERIVED( graphene::incentive_history::construction_capital_history_object, (graphene::db::object), 
                    (ccid) (owner) (amount) (period) (total_periods) (timestamp) (next_slot) (achieved) (vote_from) (vote_to) (incentive) )