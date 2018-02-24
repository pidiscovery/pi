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
#include <fc/uint128.hpp>
#include <fc/real128.hpp>
#include <graphene/chain/hardfork.hpp>

using namespace fc;

namespace graphene { namespace chain {
    void_result construction_capital_create_evaluator::do_evaluate( const construction_capital_create_operation& op ) {
        try {
            try {
                const auto& gpo = db().get_global_properties();
                FC_ASSERT(op.fee.amount >= 0);
                FC_ASSERT(op.amount >= gpo.parameters.min_construction_capital_amount);
                FC_ASSERT(
                    op.period >= gpo.parameters.min_construction_capital_period
                        && op.period <= gpo.parameters.max_construction_capital_period
                );
                FC_ASSERT(
                    op.total_periods >= gpo.parameters.min_construction_capital_period_len
                        && gpo.parameters.max_construction_capital_period_len
                );
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
                obj.left_vote_point = 0;
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
            //can not vote to itself
            FC_ASSERT(
                op.cc_from != op.cc_to,
                "from:${from} = to:${to}",
                ("from", op.cc_from)
                ("to", op.cc_to)                
            );
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
            const auto& gpo = db().get_global_properties();
            FC_ASSERT(
                index_from.count(op.cc_from) <= gpo.parameters.max_construction_capital_vote,
                "No more vote share left for ${cc}",
                ("cc", op.cc_from)
            );
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    void_result construction_capital_vote_evaluator::do_apply( const construction_capital_vote_operation& op ) {
        try {
            // wlog("cc vote: ${ccv_op}", ("ccv_op", op));
            //modify destination construction capital object
            const auto& index = db().get_index_type<construction_capital_index>().indices().get<by_id>();
            const auto& cc_to = index.find(op.cc_to);
            //a share of accelerate period amount
            uint128 accelerate_period_amount = uint128(cc_to->amount.value) * uint128(cc_to->period) * uint128(cc_to->total_periods);
            //vote points get by now
            const auto& cc_from = index.find(op.cc_from);
            uint128 total_point = fc::uint128(cc_from->amount.value)
                * fc::uint128(cc_from->period) 
                * fc::uint128(cc_from->total_periods)
                + cc_to->left_vote_point;
            //count accelerate
            db().modify(*cc_to, [&](construction_capital_object &obj) {
                while (total_point >= accelerate_period_amount && obj.pending + obj.achieved < obj.total_periods) {
                    obj.pending += 1;
                    total_point -= accelerate_period_amount;
                }
                obj.left_vote_point = total_point;
            });
            // record this vote
            db().create<construction_capital_vote_object>( [&](construction_capital_vote_object& obj) {
                obj.cc_from = op.cc_from;
                obj.cc_to = op.cc_to;
            } );            
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    void_result construction_capital_rate_vote_evaluator::do_evaluate( const construction_capital_rate_vote_operation& op ) {
        try {
            FC_ASSERT(
                op.vote_option == 0 || op.vote_option == 1 | op.vote_option == 2,
                "Unknow vote option: ${vote_option}, ${account}",
                ("account", op.account_id(db()).name)
                ("vote_option", op.vote_option)
            );
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

    void_result construction_capital_rate_vote_evaluator::do_apply( const construction_capital_rate_vote_operation& op ) {
        try {
            const auto& index = db().get_index_type<construction_capital_rate_vote_index>().indices().get<by_account>();
            const auto& ccrv = index.find(op.account_id);
            if (ccrv == index.end()) {
                db().create<construction_capital_rate_vote_object>([&](construction_capital_rate_vote_object &obj){
                    obj.account = op.account_id;
                    obj.vote_option = op.vote_option;
                    obj.timestamp = db().head_block_time();
                });
            } else {
                db().modify(*ccrv, [&](construction_capital_rate_vote_object &obj){
                    obj.vote_option = op.vote_option;
                    obj.timestamp = db().head_block_time();
                });
            }
            return void_result();
    } FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
