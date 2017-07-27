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

#include <graphene/transaction_record/transaction_record_plugin.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

namespace graphene { namespace transaction_record {

namespace detail {


class transaction_record_plugin_impl {
    public:
        transaction_record_plugin_impl(transaction_record_plugin& _plugin) : _self( _plugin ) {
        }
        virtual ~transaction_record_plugin_impl();

        /** this method is called as a callback after a block is applied
        * and will process/index all operations that were applied in the block.
        */
        void update_transaction_records( const signed_block& b );

        graphene::chain::database& database() {
            return _self.database();
        }

        transaction_record_plugin& _self;
};

transaction_record_plugin_impl::~transaction_record_plugin_impl() {
    return;
}

void transaction_record_plugin_impl::update_transaction_records( const signed_block& b ) {
    graphene::chain::database& db = database();
    uint32_t block_num = b.block_num();
    uint32_t counter = 0;
    for (auto trx : b.transactions) {
        db.create<transaction_record_object>([&](transaction_record_object &obj) {
            obj.trx_id = trx.id();
            obj.block_num = block_num;
            obj.trx_in_block = counter++;
            // wlog("trx=${trx}", ("trx", trx.id()));
        });
    }
    if (b.transactions.size() > 0 || block_num % 10000 == 0) {
        wlog("update_transaction_records, block_num=${block_num}, trx_num=${trx_num}", ("block_num", block_num)("trx_num", counter));
    }
}

} // end namespace detail

transaction_record_plugin::transaction_record_plugin() :
    my( new detail::transaction_record_plugin_impl(*this) ) {
}

transaction_record_plugin::~transaction_record_plugin() {
}

std::string transaction_record_plugin::plugin_name()const {
    return "transaction_record";
}

void transaction_record_plugin::plugin_set_program_options(
    boost::program_options::options_description& cli,
    boost::program_options::options_description& cfg) {
}

void transaction_record_plugin::plugin_initialize(const boost::program_options::variables_map& options) {
    database().applied_block.connect( [&](const signed_block& b){ my->update_transaction_records(b); } );
    database().add_index< primary_index< transaction_record_index  > >();
}

void transaction_record_plugin::plugin_startup() {
}

} } //end of namespace incentive_history
