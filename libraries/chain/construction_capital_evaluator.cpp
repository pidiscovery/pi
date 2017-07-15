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
#include <graphene/chain/construction_capital_evaluator.hpp>
#include <graphene/chain/construction_capital_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/exceptions.hpp>
#include <fc/real128.hpp>

using namespace fc;

namespace graphene { namespace chain {
    void_result construction_capital_create_evaluator::do_evaluate( const construction_capital_create_operation& op ) {
        try {
            try {
                // get core asset balance
                asset balance = db().get_balance(op.account_id, asset_id_type(0)); 
                FC_ASSERT(
                    balance.amount >= op.amount,
                    "Insufficient Balance: ${balance}, ${account} unable to create construction capital of ${amount}",
                    ("balance", db().to_pretty_string(balance))
                    ("account", op.account_id(db()).name)
                    ("amount", op.amount)
                );
                return void_result();
            } FC_RETHROW_EXCEPTIONS( error, "Unable to create construction capital for ${account} of ${balance}", ("account", op.account_id(db()).name)("balance", op.amount) );
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    object_id_type construction_capital_create_evaluator::do_apply( const construction_capital_create_operation& op ) {
        try {
//            wlog("cc create: ${cc_op}", ("cc_op", op));
            db().adjust_balance(op.account_id, -asset(op.amount, asset_id_type(0)));
            const auto& new_cc_object = db().create<construction_capital_object>( [&]( construction_capital_object& obj ){
                obj.owner = op.account_id;
                obj.amount = op.amount;
                obj.period = op.period;
                obj.total_periods = op.total_periods;
                obj.achieved = 0;
                obj.pending = 0;
                obj.next_slot = fc::time_point_sec(db().head_block_time() + op.period);
                obj.timestamp = db().head_block_time();
            });
            // instant payback
            account_object &acc_obj = (account_object &)db().get_object(op.account_id);
            real128 amount = real128(op.amount.value) * real128(GRAPHENE_DEFAULT_INSTANT_PAYBACL_RATE) / real128(GRAPHENE_ISSUANCE_RATE_SCALE);
            if (acc_obj.is_instant_payback(db().head_block_time())) {    
                db().adjust_balance(op.account_id, asset(amount.to_uint64(), asset_id_type(0)));
                db().adjust_balance(GRAPHENE_MARKET_FOUND_ACCOUNT, asset(amount.to_uint64(), asset_id_type(0)));
            } else {
                db().adjust_balance(GRAPHENE_MARKET_FOUND_ACCOUNT, asset(amount.to_uint64(), asset_id_type(0)));
            }
            // all locked shares go to GRAPHENE_CONSTRUCTION_CAPITAL_ACCOUNT
            db().adjust_balance(GRAPHENE_CONSTRUCTION_CAPITAL_ACCOUNT, asset(op.amount, asset_id_type(0)));
            return new_cc_object.id;
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    void_result construction_capital_vote_evaluator::do_evaluate( const construction_capital_vote_operation& op ) {
        try {
            //vote pair can exist only once
            const auto& index_vote_pair = db().get_index_type<construction_capital_vote_index>().indices().get<by_vote_pair>();
            FC_ASSERT(
                index_vote_pair.find(boost::make_tuple(op.cc_from, op.cc_to)) == index_vote_pair.end(),
                "vote pair: ${from} - ${to} already exist",
                ("from", op.cc_from)
                ("to", op.cc_to)
            );
            const auto& index = db().get_index_type<construction_capital_index>().indices().get<by_id>();
            const auto& cc_from = index.find(op.cc_from);
            //source construction capital must exist
            FC_ASSERT(
                cc_from != index.end(),
                "source construction capital ${cc} not exist",
                ("cc", op.cc_from)
            );
            //source construction capital should not expire
            FC_ASSERT(
                cc_from->achieved < cc_from->total_periods,
                "source constuction capital ${cc} has expired",
                ("cc", op.cc_from)
            );
            //one can only vote with oneselve's constuctions capital only
            FC_ASSERT(
                cc_from->owner == op.account_id,
                "account - ${acc} is not owner of construction capital - ${cc}, should be ${acc_ok}",
                ("acc", op.account_id)
                ("cc", op.cc_from)
                ("acc_ok", cc_from->owner)
            );
            //destination construction capital must exist
            const auto& cc_to = index.find(op.cc_to);
            FC_ASSERT(
                cc_to != index.end(),
                "destination construction capital ${cc} not exist",
                ("cc", op.cc_to)
            );
            //destination construction capital should not expire
            FC_ASSERT(
                cc_to->achieved < cc_to->total_periods,
                "destination constuction capital ${cc} has expired",
                ("cc", op.cc_to)
            );            
            const auto& index_from = db().get_index_type<construction_capital_vote_index>().indices().get<by_vote_from>();
            //can vote at most GRAPHENE_DEFAULT_MAX_CONSTRUCTION_CAPITAL_VOTE votes
            FC_ASSERT(
                index_from.count(op.cc_from) < GRAPHENE_DEFAULT_MAX_CONSTRUCTION_CAPITAL_VOTE,
                "No more vote share left for ${cc}",
                ("cc", op.cc_from)
            );
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    void_result construction_capital_vote_evaluator::do_apply( const construction_capital_vote_operation& op ) {
        try {
            wlog("cc vote: ${ccv_op}", ("ccv_op", op));
            //modify destination construction capital object
            const auto& index = db().get_index_type<construction_capital_index>().indices().get<by_id>();
            const auto& cc_to = index.find(op.cc_to);
            //a share of accelerate period amount
            share_type accelerate_period_amount = cc_to->amount * cc_to->period * cc_to->total_periods;
            //calculate accelerate got already by other votes
            share_type accelerate_got = 0;
            const auto& index_vote_to = db().get_index_type<construction_capital_vote_index>().indices().get<by_vote_to>();
            for (auto it = index_vote_to.lower_bound(op.cc_to); it != index_vote_to.end() && it->cc_to == op.cc_to; ++it) {
                auto from_obj_it = index.find(it->cc_from);
                accelerate_got += from_obj_it->amount * from_obj_it->period * from_obj_it->total_periods;
            }
            real128 max_acclerate_real = real128(accelerate_period_amount.value) * real128(cc_to->total_periods * GRAPHENE_DEFAULT_MAX_INCENTIVE_ACCELERATE_RATE) / real128(100);
            share_type max_acclerate = max_acclerate_real.to_uint64(); 
            //accelerate has an upper limit, when reached, no accelerate effect take palce
            if (accelerate_got < max_acclerate) {
                //calculate incentive accelerate
                const auto& cc_from = index.find(op.cc_from);
                share_type accelerate_amount = cc_from->amount * cc_from->period * cc_from->total_periods;
                if (accelerate_amount + accelerate_got > max_acclerate) {
                    accelerate_amount = max_acclerate - accelerate_got;
                }
                //total accelerate time
                real128 total_accl_real = real128(accelerate_amount.value) / real128(accelerate_period_amount.value) * real128(cc_to->period);
                uint32_t total_accl = total_accl_real.to_uint64();
                db().modify(*cc_to, [&](construction_capital_object &obj) {
                    fc::microseconds accl_left(total_accl * 1000000);
                    //calculate periods of incentive release accelerated by this vote
                    while (accl_left >= obj.next_slot - db().head_block_time() && obj.achieved < obj.total_periods) {
                        accl_left -= (obj.next_slot - db().head_block_time());
                        obj.next_slot = db().head_block_time() + obj.period;
                        obj.pending += 1;
                    }
                    obj.next_slot -= accl_left;
                });
            }
            //record this vote
            db().create<construction_capital_vote_object>( [&](construction_capital_vote_object& obj) {
                obj.cc_from = op.cc_from;
                obj.cc_to = op.cc_to;
            } );            
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
