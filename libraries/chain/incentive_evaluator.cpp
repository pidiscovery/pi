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

#include <graphene/chain/incentive_evaluator.hpp>
#include <graphene/chain/protocol/construction_capital.hpp>
#include <graphene/chain/construction_capital_object.hpp>
#include <fc/real128.hpp>
#include <graphene/chain/protocol/types.hpp>

using namespace fc;

namespace graphene { namespace chain {

    void_result incentive_evaluator::do_evaluate( const incentive_operation& op ) {
        try {
            auto& index = db().get_index_type<construction_capital_index>().indices().get<by_id>();
            auto it = index.find(op.ccid);
            //construction capital must exist
            FC_ASSERT(
                it != index.end(),
                "construction capital ${cc} not found",
                ("cc", op.ccid)
            );
            const auto& gpo = db().get_global_properties();
            real128 amount0 = real128(it->amount.value) / real128(it->total_periods);
            real128 amount1 = real128(it->amount.value)
                * real128(it->period) / real128(GRAPHENE_SECONDS_PER_YEAR) 
                * real128(gpo.parameters.issuance_rate) / real128(GRAPHENE_ISSUANCE_RATE_SCALE);
            real128 amount = amount0 + amount1;
            //check if incentive amount is valid
            FC_ASSERT(
                amount.to_uint64() == op.amount,
                "incentive amount invalid, should be ${should}, got ${got}",
                ("should", amount.to_uint64())
                ("got", op.amount)
            );            
            //check if has unreleased incentive period(s)
            FC_ASSERT(
                it->achieved < it->total_periods,
                "all periods are released already, total_periods- ${total}, achived- ${achived}",
                ("total", it->total_periods)
                ("achived", it->achieved)
            );
            if (op.reason == 0) {
                //incentive by period
                //should reach it's time slot
                FC_ASSERT(
                    it->next_slot <= db().head_block_time(),
                    "incentive by period should reach the time slot, should be smaller than ${should}, got ${got}",
                    ("should", it->next_slot)
                    ("got", db().head_block_time())
                );
            } else if (op.reason == 1) {
                //incentive by vote
                //should have unreleased pending incentive by vote
                FC_ASSERT(
                    it->pending >= 1,
                    "only construction capital has pending can be incentive by vote, pending - ${pending}",
                    ("pending", it->pending)
                );                
            }
            return void_result();
    } FC_CAPTURE_AND_RETHROW((op)) }

    void_result incentive_evaluator::do_apply( const incentive_operation& op ) {
        //modify construction capital
        auto& index = db().get_index_type<construction_capital_index>().indices().get<by_id>();
        auto it = index.find(op.ccid);
        //update next slot time and achieved count
        db().modify(*it, [&](construction_capital_object& obj) {
            if (op.reason == 0) {
                obj.next_slot += obj.period;
            } else {
                obj.pending -= 1;
            }
            obj.achieved += 1;
            //adjust balance
            db().adjust_balance(obj.owner, asset(op.amount, asset_id_type(0)));
        });
        // update current supply
        db().modify(db().get(asset_id_type()).dynamic_data(db()), [op](asset_dynamic_data_object& d) {
             d.current_supply += op.amount;
        });
        // update construction capital summary
        db().modify(db().get(construction_capital_summary_id_type()), [it, op](construction_capital_summary_object& o) {
            share_type deposit = it->amount / it->total_periods;
            share_type profit = op.amount - it->amount;
            o.deposit_in_life -= deposit;
            o.profit_all_time += profit;
            if (it->achieved >= it->total_periods) {
                o.count_in_life -= 1;
            }    
        });
        // wlog("incentive run, cc: ${cc}", ("cc", *it));
        //if release of this construction capital is done, 
        if (it->achieved >= it->total_periods) {
            wlog("incentive done, cc: ${cc}", ("cc", *it));
            db().remove(*it);
        }
        return void_result();
    }
}} // graphene::chain
