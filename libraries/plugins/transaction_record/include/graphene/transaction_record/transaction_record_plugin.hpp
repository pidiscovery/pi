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
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace transaction_record {

using namespace chain;

#ifndef ACCOUNT_HISTORY_SPACE_ID
#define ACCOUNT_HISTORY_SPACE_ID 5
#endif

#define TRANSACTION_RECORD_TYPE_ID 3

struct transaction_record_object : public abstract_object<transaction_record_object> {
    static const uint8_t space_id = ACCOUNT_HISTORY_SPACE_ID;
    static const uint8_t type_id  = TRANSACTION_RECORD_TYPE_ID;
    
    transaction_id_type trx_id;
    uint32_t block_num;
    uint32_t trx_in_block;
};

struct by_id;
struct by_trx_id;

typedef multi_index_container<
    transaction_record_object,
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
            tag<by_trx_id>, 
            member< 
                transaction_record_object, 
                transaction_id_type, 
                &transaction_record_object::trx_id
            >
        >
    >
> transaction_record_multi_index_type;

typedef generic_index<transaction_record_object, transaction_record_multi_index_type> transaction_record_index;

namespace detail {
    class transaction_record_plugin_impl;
}

class transaction_record_plugin : public graphene::app::plugin {
    public:
        transaction_record_plugin();
        virtual ~transaction_record_plugin();

        std::string plugin_name()const override;
        virtual void plugin_set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& cfg) override;
        virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
        virtual void plugin_startup() override;

        friend class detail::transaction_record_plugin_impl;
        std::unique_ptr<detail::transaction_record_plugin_impl> my;
};

} } //graphene::transaction_record

FC_REFLECT_DERIVED( graphene::transaction_record::transaction_record_object, (graphene::db::object), 
                    (trx_id) (block_num) (trx_in_block) )