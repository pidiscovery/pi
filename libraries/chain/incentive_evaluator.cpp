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

namespace graphene { namespace chain {

    void_result incentive_evaluator::do_evaluate( const incentive_operation& op ) {
        return void_result();
    }

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
        //if release of this construction capital is done, 
        //release the locked shares and remove this object
        if (it->achieved >= it->total_periods) {
            db().adjust_balance(it->owner, asset(it->amount, asset_id_type(0)));
            db().remove(*it);
        }        

        wlog("incentive_evaluator::do_apply: ${obj}", ("obj", *it));
        return void_result();
    }
}} // graphene::chain
