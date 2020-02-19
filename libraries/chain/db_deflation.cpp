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

#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/deflation.hpp>
#include <graphene/chain/deflation_object.hpp>

using namespace fc;

namespace graphene { namespace chain {
    
    signed_transaction database::generate_deflation_transaction() {
        signed_transaction tx;

        wlog("deflation: gneration, stage 1");
        const auto &dflt_idx = get_index_type<deflation_index>().indices().get<by_id>();
        auto upper_it = dflt_idx.rbegin();
        if (upper_it == dflt_idx.rend()) {
            // no deflation found
            return tx;
        }
        wlog("deflation: gneration, stage 2");
        if (upper_it->cleared) {
            // already cleared
            return tx;
        }

        wlog("deflation: gneration, stage 3");
        int op_cnt = 0;
        const auto &acc_idx = get_index_type<account_index>().indices().get<by_id>();
        auto acc_it = acc_idx.upper_bound(upper_it->cursor);
        for (; op_cnt < GRAPHENE_DEFAULT_MAX_DEFLATION_OPERATIONS_PER_BLOCK && acc_it != acc_idx.end() && account_id_type(acc_it->id) > GRAPHENE_DEFLATION_ACCOUNT_END_MARKER; ++acc_it) {
            op_cnt += 1;
            account_deflation_operation op;
            op.deflation_id = upper_it->id;
            op.owner = acc_it->id;
            tx.operations.push_back(op);
            wlog("deflation: ${deflation} - ${account}", ("deflation", upper_it->id)("account", acc_it->id));
        }

        return tx;
    }

    processed_transaction database::apply_deflation(const processed_transaction &tx) {
        wlog("deflation: apply, stage 1");
        transaction_evaluation_state eval_state(this);
        //process the operations
        processed_transaction ptrx(tx);
        _current_op_in_trx = 0;
        for( const auto& op : ptrx.operations )
        {
            eval_state.operation_results.emplace_back(apply_operation(eval_state, op));
            ++_current_op_in_trx;
        }
        ptrx.operation_results = std::move(eval_state.operation_results);

        return ptrx;
    }
}}
