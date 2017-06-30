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

#include <graphene/incentive_history/incentive_history_plugin.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <graphene/chain/construction_capital_object.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>
#include <fc/real128.hpp>

using namespace fc;

namespace graphene { namespace incentive_history {

namespace detail {


class incentive_history_plugin_impl {
    public:
        incentive_history_plugin_impl(incentive_history_plugin& _plugin) : _self( _plugin ) {
        }
        virtual ~incentive_history_plugin_impl();

        /** this method is called as a callback after a block is applied
        * and will process/index all operations that were applied in the block.
        */
        void update_incentive_histories( const signed_block& b );

        graphene::chain::database& database() {
            return _self.database();
        }

        incentive_history_plugin& _self;

    private:
        void init_new_construction_capital_history_object(const construction_capital_id_type &ccid, 
                construction_capital_history_object &ccho) {
            // initialize construction_capital_history_object by record of construction_capital_object
            const auto& idx = database().get_index_type<construction_capital_index>().indices().get<by_id>();
            auto it = idx.find(ccid);
            FC_ASSERT(it != idx.end(),
                "construction capital ${cc} not found",
                ("cc", ccid)
            );
            auto &obj = *it;
            ccho.ccid = ccid;
            ccho.owner = obj.owner;
            ccho.amount = obj.amount;
            ccho.period = obj.period;
            ccho.total_periods = obj.total_periods;
            ccho.timestamp = obj.timestamp;
            ccho.next_slot = obj.next_slot;
            ccho.achieved = obj.achieved;
        }

        uint32_t calculate_accelerate(const construction_capital_id_type &cc_from_id, const construction_capital_id_type &cc_to_id) {
            // calculate accelerate time in second by this vote
            const auto& index = database().get_index_type<construction_capital_index>().indices().get<by_id>();
            const auto& cc_to = index.find(cc_to_id);
            const auto& cc_from = index.find(cc_from_id);
            share_type accelerate_period_amount = cc_to->amount * cc_to->period * cc_to->total_periods;
            share_type accelerate_got = 0;
            const auto& index_vote_to = database().get_index_type<construction_capital_vote_index>().indices().get<by_vote_to>();
            for (auto it = index_vote_to.lower_bound(cc_to_id); it->cc_to == cc_to_id; ++it) {
                if (it->cc_from == cc_from_id) {
                    continue;
                }
                auto from_obj_it = index.find(it->cc_from);
                accelerate_got += from_obj_it->amount * from_obj_it->period * from_obj_it->total_periods;
            }
            real128 max_acclerate_real = real128(accelerate_period_amount.value) * real128(cc_to->total_periods * GRAPHENE_DEFAULT_MAX_INCENTIVE_ACCELERATE_RATE) / real128(100);
            share_type max_acclerate = max_acclerate_real.to_uint64(); 
            //accelerate has an upper limit, when reached, no accelerate effect take palce
            if (accelerate_got < max_acclerate) {
                //calculate incentive accelerate
                share_type accelerate_amount = cc_from->amount * cc_from->period * cc_from->total_periods;
                if (accelerate_amount + accelerate_got > max_acclerate) {
                    accelerate_amount = max_acclerate - accelerate_got;
                }
                //total accelerate time
                real128 total_accl_real = real128(accelerate_amount.value) / real128(accelerate_period_amount.value) * real128(cc_to->period);
                return total_accl_real.to_uint64();
            }
            return 0;
        }

};

incentive_history_plugin_impl::~incentive_history_plugin_impl() {
    return;
}

void incentive_history_plugin_impl::update_incentive_histories( const signed_block& b ) {
    graphene::chain::database& db = database();
    const vector<optional< operation_history_object > >& hist = db.get_applied_operations();
    for( const optional< operation_history_object >& o_op : hist ) {
        if ( o_op->op.which() == operation::tag< incentive_operation >::value ) {
            // record incentive
            wlog("incentive_operation: recording history");
            auto op = o_op->op.get<incentive_operation>();
            incentive_record ir;
            ir.timestamp = b.timestamp;
            ir.amount = op.amount;
            ir.reason = op.reason;
            const auto& ih_idx = db.get_index_type<construction_capital_history_index>();
            const auto& by_id_idx = ih_idx.indices().get<by_cc_id>();
            auto itr = by_id_idx.find(op.ccid);
            if (itr == by_id_idx.end()) {
                db.create<construction_capital_history_object>([&](construction_capital_history_object &obj) {
                    init_new_construction_capital_history_object(op.ccid, obj);
                    obj.incentive.push_back(ir);
                });
            } else {
                db.modify(*itr, [&](construction_capital_history_object &obj) {
                    obj.incentive.push_back(ir);                    
                });
            }
        } else if ( o_op->op.which() == operation::tag< construction_capital_vote_operation >::value ) {
            // record vote
            wlog("construction_capital_vote_operation: recording history");
            auto op = o_op->op.get<construction_capital_vote_operation>();
            construction_capital_vote_record ccvr;
            ccvr.cc_from = op.cc_from;
            ccvr.cc_to = op.cc_to;
            ccvr.accelerate = calculate_accelerate(op.cc_from, op.cc_to);
            ccvr.timestamp = b.timestamp;
            const auto& cch_idx = db.get_index_type<construction_capital_history_index>();
            const auto& by_id_idx = cch_idx.indices().get<by_cc_id>();
            {
                auto itr = by_id_idx.find(op.cc_from);
                if (itr == by_id_idx.end()) {
                    db.create<construction_capital_history_object>([&](construction_capital_history_object &obj) {
                        init_new_construction_capital_history_object(op.cc_from, obj);
                        obj.vote_from.push_back(ccvr);
                    });
                } else {
                    db.modify(*itr, [&](construction_capital_history_object &obj) {
                        obj.vote_from.push_back(ccvr);
                    });
                }
            }
            {
                auto itr = by_id_idx.find(op.cc_to);
                if (itr == by_id_idx.end()) {
                    db.create<construction_capital_history_object>([&](construction_capital_history_object &obj) {
                        init_new_construction_capital_history_object(op.cc_to, obj);
                        obj.vote_to.push_back(ccvr);
                    });
                } else {
                    db.modify(*itr, [&](construction_capital_history_object &obj) {
                        obj.vote_to.push_back(ccvr);
                    });
                }
            }
            
        }
    }
}

} // end namespace detail

incentive_history_plugin::incentive_history_plugin() :
    my( new detail::incentive_history_plugin_impl(*this) ) {
}

incentive_history_plugin::~incentive_history_plugin() {
}

std::string incentive_history_plugin::plugin_name()const {
    return "incentive_history";
}

void incentive_history_plugin::plugin_set_program_options(
    boost::program_options::options_description& cli,
    boost::program_options::options_description& cfg) {
}

void incentive_history_plugin::plugin_initialize(const boost::program_options::variables_map& options) {
    database().applied_block.connect( [&](const signed_block& b){ my->update_incentive_histories(b); } );
    database().add_index< primary_index< construction_capital_history_index  > >();
}

void incentive_history_plugin::plugin_startup() {
}

} } //end of namespace incentive_history
