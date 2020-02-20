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

        const auto &dflt_idx = get_index_type<deflation_index>().indices().get<by_id>();
        auto dlft_it = dflt_idx.rbegin();
        if (dlft_it == dflt_idx.rend()) {
            // no deflation found
            return tx;
        }

        if (dlft_it->cleared) {
            // already cleared
            return tx;
        }

        int op_cnt = 0;
        const auto &acc_idx = get_index_type<account_index>().indices().get<by_id>();
        for ( auto acc_it = acc_idx.find(dlft_it->cursor); 
                op_cnt < GRAPHENE_DEFAULT_MAX_DEFLATION_OPERATIONS_PER_BLOCK 
                    && acc_it != acc_idx.end() 
                    && ( account_id_type(acc_it->id) < dlft_it->last_account 
                        || account_id_type(acc_it->id) == dlft_it->last_account
                ); 
                ++acc_it ) {
            op_cnt += 1;
            account_deflation_operation op;
            op.deflation_id = dlft_it->id;
            op.owner = acc_it->id;
            tx.operations.push_back(op);
        }

        ilog("deflation running: ops_in_tx:${op_cnt} ${id}, cursor:${cursor}, last_account:${last_account}, total:${total_amount}", 
            ("op_cnt", op_cnt)
            ("id", dlft_it->id)
            ("cursor", dlft_it->cursor)
            ("last_account", dlft_it->last_account)
            ("total_amount", dlft_it->total_amount)
        );

        //set tx params
        auto dyn_props = get_dynamic_global_properties();
        tx.set_reference_block(dyn_props.head_block_id);
        tx.set_expiration( dyn_props.time + fc::seconds(30) );
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
