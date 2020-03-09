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
#include <graphene/chain/market_object.hpp>

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

        if (dlft_it->order_cleared && dlft_it->balance_cleared) {
            // already cleared
            return tx;
        }

        int op_cnt = 0;
        if (!dlft_it->order_cleared) {
            // do order deflation
            const auto &order_dflt_idx = get_index_type<order_deflation_index>().indices().get<by_order>();
            const auto &order_idx = get_index_type<limit_order_index>().indices().get<by_id>();
            auto order_it = order_idx.lower_bound(dlft_it->order_cursor);
            if (order_it == order_idx.end()) {
                // this check is necessary for last_order may be cleared by now
                modify(*dlft_it, [&](deflation_object &obj){
                    obj.order_cleared = true;
                });
            } else {
                for (; op_cnt < GRAPHENE_DEFAULT_MAX_DEFLATION_OPERATIONS_PER_BLOCK 
                            && order_it != order_idx.end()
                            && (limit_order_id_type(order_it->id) < dlft_it->last_order
                                || limit_order_id_type(order_it->id) == dlft_it->last_order); 
                        ++order_it) {
                    op_cnt += 1;
                    order_deflation_operation op;
                    op.deflation_id = dlft_it->id;
                    op.order = order_it->id;
                    // owner & amount only for history
                    op.owner = order_it->seller;
                    if (order_it->sell_price.base.asset_id == asset_id_type(0)) {
                        const auto &order_dflt_it = order_dflt_idx.find(order_it->id);
                        if (order_dflt_it != order_dflt_idx.end() && order_dflt_it->cleared) {
                            op.amount = order_dflt_it->frozen;
                        } else {
                            uint128_t amount = uint128_t(order_it->for_sale.value) * dlft_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
                            op.amount = int64_t(amount.to_uint64());
                        }
                    } else {
                        op.amount = 0;
                    }
                    tx.operations.push_back(op);
                }
            }
        }

        if (!dlft_it->balance_cleared && op_cnt < GRAPHENE_DEFAULT_MAX_DEFLATION_OPERATIONS_PER_BLOCK) {
            // do account balance deflation
            const auto &acc_dflt_idx = get_index_type<account_deflation_index>().indices().get<by_owner>();
            const auto &acc_idx = get_index_type<account_index>().indices().get<by_id>();
            for ( auto acc_it = acc_idx.find(dlft_it->account_cursor); 
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

                const auto &acc_dflt_it = acc_dflt_idx.find(acc_it->id);
                if (acc_dflt_it != acc_dflt_idx.end() && acc_dflt_it->cleared) {
                    op.amount = acc_dflt_it->frozen;
                } else {
                    uint128_t amount = uint128_t(get_balance(op.owner, asset_id_type(0)).amount.value)* dlft_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
                    op.amount = int64_t(amount.to_uint64());
                }
                tx.operations.push_back(op);
            }
        }

        ilog("deflation running: ops_in_tx:${op_cnt} ${id}, "
                "account_cursor:${account_cursor}, last_account:${last_account}, balance_cleared:${balance_cleared} "
                "order_cursor:${order_cursor}, last_order:${last_order}, order_cleared:${order_cleared}, "
                "total:${total_amount}", 
            ("op_cnt", op_cnt)
            ("id", dlft_it->id)
            ("account_cursor", dlft_it->account_cursor)
            ("last_account", dlft_it->last_account)
            ("balance_cleared", dlft_it->balance_cleared)
            ("order_cursor", dlft_it->order_cursor)
            ("last_order", dlft_it->last_order)
            ("order_cleared", dlft_it->order_cleared)
            ("total_amount", dlft_it->total_amount)
        );

        //set tx params
        auto dyn_props = get_dynamic_global_properties();
        tx.set_reference_block(dyn_props.head_block_id);
        tx.set_expiration( dyn_props.time + fc::seconds(30) );
        return tx;
    }

    processed_transaction database::apply_deflation(const processed_transaction &tx) {
        // wlog("deflation: apply, stage 1");
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
