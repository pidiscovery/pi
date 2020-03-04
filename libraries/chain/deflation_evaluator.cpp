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

#include <graphene/chain/deflation_evaluator.hpp>
#include <graphene/chain/protocol/deflation.hpp>
#include <graphene/chain/deflation_object.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/market_object.hpp>

using namespace fc;

namespace graphene { namespace chain {
    
    void_result deflation_evaluator::do_evaluate( const deflation_operation& op ) {
        try {
            // check issuer
            FC_ASSERT(
                op.issuer == GRAPHENE_DEFLATION_ISSUE_ACCOUNT,
                "only specified account can issue deflation"
            );
            // check defaltion rate
            FC_ASSERT(
                op.rate > 0 && op.rate < 1 * GRAPHENE_DEFLATION_RATE_SCALE,
                "deflation rate should in range (0, 100)%, ${rate}% is invalide",
                ("rate", 100.0 * op.rate / GRAPHENE_DEFLATION_RATE_SCALE)
            );
            // check existing deflation tx
            auto &index = db().get_index_type<deflation_index>().indices().get<by_id>();
            auto it = index.rbegin();
            if (it != index.rend()) {
                // there should not be running deflation
                FC_ASSERT(
                    it->balance_cleared && it->order_cleared,
                    "cannot issue a defaltion when another(${id}) is in progress",
                    ("id", it->id)
                );
                // check deflation interval
                FC_ASSERT(
                    it->timestamp + GRAPHENE_MINIMUM_DEFLATION_INTERVAL < db().head_block_time(),
                    "issue defaltion to offen, deflation can be issued after ${t}",
                    ("t", it->timestamp + GRAPHENE_MINIMUM_DEFLATION_INTERVAL)
                );
            }
            // check account deflation
            auto &acc_idx = db().get_index_type<account_index>().indices().get<by_id>();
            const auto &acc_last_it = acc_idx.rbegin();
            FC_ASSERT(
                acc_last_it != acc_idx.rend()
                    && (account_id_type(acc_last_it->id) > GRAPHENE_DEFLATION_ACCOUNT_START_MARKER
                        || account_id_type(acc_last_it->id) == GRAPHENE_DEFLATION_ACCOUNT_START_MARKER),
                "issue defaltion fail, for there's no account to deflate"
            );
            // check order deflation
            // nothing to check
            return void_result();
    } FC_CAPTURE_AND_RETHROW((op)) }

    void_result deflation_evaluator::do_apply( const deflation_operation& op ) {
        auto &index = db().get_index_type<deflation_index>().indices().get<by_id>();
        auto it = index.rbegin();
        if (it == index.rend()) {
            // create an placeholder object to present none
            db().create<deflation_object>([&](deflation_object &obj){
                obj.timestamp = db().head_block_time();;
                obj.issuer = op.issuer;
                obj.rate = 0;
                obj.last_account = GRAPHENE_DEFLATION_ACCOUNT_START_MARKER;
                obj.account_cursor = GRAPHENE_DEFLATION_ACCOUNT_START_MARKER;
                obj.balance_cleared = true;
                obj.last_order = limit_order_id_type(0);
                obj.order_cursor = limit_order_id_type(0);
                obj.order_cleared = true;
                obj.total_amount = 0;
            });
        }
        // account deflation
        auto &acc_idx = db().get_index_type<account_index>().indices().get<by_id>();
        const auto &acc_last_it = acc_idx.rbegin();
        // order deflation
        auto &order_idx = db().get_index_type<limit_order_index>().indices().get<by_id>();
        const auto &order_last_it = order_idx.rbegin();
        const auto &order_cursor_it = order_idx.begin();

        db().create<deflation_object>( [&](deflation_object &obj){
            obj.timestamp = db().head_block_time();
            obj.issuer = op.issuer;
            obj.rate = op.rate;

            obj.last_account = acc_last_it->id;
            obj.account_cursor = GRAPHENE_DEFLATION_ACCOUNT_START_MARKER;
            obj.balance_cleared = false;

            // check for limit orders
            if (order_last_it != order_idx.rend() && order_cursor_it != order_idx.end()) {
                obj.last_order = order_last_it->id;
                obj.order_cursor = order_cursor_it->id;
                obj.order_cleared = false;
            } else {
                obj.last_order = limit_order_id_type(0);
                obj.order_cursor = limit_order_id_type(0);
                obj.order_cleared = true;
            }

            obj.total_amount = 0;
        });

        return void_result();
    }

    void_result account_deflation_evaluator::do_evaluate( const account_deflation_operation& op ) {
        try {
            auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
            const auto &dflt_it = dflt_idx.find(op.deflation_id);
            FC_ASSERT(
                dflt_it != dflt_idx.end(),
                "deflation object not found for this account deflation. defaltion_object_id:${dflt_id}",
                ("dflt_id", op.deflation_id)
            );
            FC_ASSERT(
                dflt_it->balance_cleared == false,
                "account deflation is already cleared"
            );
            // FC_ASSERT(
            //     account_id_type(op.owner) > dflt_it->cursor || account_id_type(op.owner) == dflt_it->cursor,
            //     "deflation for this account-${acc} has been done before",
            //     ("acc", op.owner)
            // );
            FC_ASSERT(
                account_id_type(op.owner) == dflt_it->account_cursor,
                "deflation for this account-${acc} is in wrong order",
                ("acc", op.owner)
            );            
            const auto &acc_dflt_idx = db().get_index_type<account_deflation_index>().indices().get<by_owner>();
            const auto &acc_dflt_it = acc_dflt_idx.find(op.owner);
            if (acc_dflt_it != acc_dflt_idx.end()) {
                FC_ASSERT(
                    acc_dflt_it->last_deflation_id < deflation_id_type(op.deflation_id),
                    "accout: ${acc} last_deflation_id: ${acc_dflt_id} is not smaller than deflation: ${dflt_id}",
                    ("acc", op.owner)
                    ("acc_dflt_id", acc_dflt_it->last_deflation_id)
                    ("dflt_id", op.deflation_id)
                );
            }
            return void_result();
    } FC_CAPTURE_AND_RETHROW((op)) }

    void_result account_deflation_evaluator::do_apply( const account_deflation_operation& op ) {
        auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
        const auto &dflt_it = dflt_idx.find(op.deflation_id);

        const auto &acc_dflt_idx = db().get_index_type<account_deflation_index>().indices().get<by_owner>();
        const auto &acc_dflt_it = acc_dflt_idx.find(op.owner);

        bool cleared = false;
        share_type frozen = 0;

        // update account deflation object
        if (acc_dflt_it != acc_dflt_idx.end()) {
            cleared = acc_dflt_it->cleared;
            frozen = acc_dflt_it->frozen;
            db().modify(*acc_dflt_it, [&](account_deflation_object &obj) {
                obj.last_deflation_id = op.deflation_id;
                obj.frozen = 0;
                obj.cleared = false;
            });
        } else {
            db().create<account_deflation_object>( [&]( account_deflation_object &obj){
                obj.owner = op.owner;
                obj.last_deflation_id = op.deflation_id;
                obj.frozen = 0;
                obj.cleared = false;
            });
        }

        // update account balance
        share_type deflation_amount = 0;
        if (!cleared) {
            auto balance = db().get_balance(op.owner, asset_id_type(0));
            uint128_t amount = uint128_t(balance.amount.value)* dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
            deflation_amount = amount.to_uint64();
        }
        if (deflation_amount > 0) {
            db().adjust_balance(op.owner, -asset(deflation_amount, asset_id_type(0)));
        }

        db().modify(*dflt_it, [&](deflation_object &obj){
            obj.account_cursor = op.owner + 1;
            obj.total_amount += (deflation_amount + frozen);
            if (op.owner == dflt_it->last_account) {
                obj.balance_cleared = true;
            }
        });

        return void_result();
    }

    void_result order_deflation_evaluator::do_evaluate( const order_deflation_operation& op ) {
        try {
            auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
            const auto &dflt_it = dflt_idx.find(op.deflation_id);
            FC_ASSERT(
                dflt_it != dflt_idx.end(),
                "deflation object not found for this order deflation. defaltion_object_id:${dflt_id}",
                ("dflt_id", op.deflation_id)
            );
            FC_ASSERT(
                dflt_it->order_cleared == false,
                "order deflation is already cleared"
            );
            // order may not be continuous
            // FC_ASSERT(
            //     limit_order_id_type(op.order) == dflt_it->order_cursor,
            //     "deflation for this order-${order} is in wrong order",
            //     ("order", op.order)
            // );
            const auto &order_dflt_idx = db().get_index_type<order_deflation_index>().indices().get<by_order>();
            const auto &order_dflt_it = order_dflt_idx.find(op.order);
            if (order_dflt_it != order_dflt_idx.end()) {
                FC_ASSERT(
                    order_dflt_it->last_deflation_id < deflation_id_type(op.deflation_id),
                    "order: ${order} last_deflation_id: ${order_dflt_id} is not smaller than deflation: ${dflt_id}",
                    ("order", op.order)
                    ("order_dflt_id", order_dflt_it->last_deflation_id)
                    ("dflt_id", op.deflation_id)
                );
            }
            return void_result();
    } FC_CAPTURE_AND_RETHROW((op)) }

    void_result order_deflation_evaluator::do_apply( const order_deflation_operation& op ) {
        auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
        const auto &dflt_it = dflt_idx.find(op.deflation_id);

        const auto &order_dflt_idx = db().get_index_type<order_deflation_index>().indices().get<by_order>();
        const auto &order_dflt_it = order_dflt_idx.find(op.order);

        bool cleared = false;
        share_type frozen = 0;

        // update order deflation object
        if (order_dflt_it != order_dflt_idx.end()) {
            cleared = order_dflt_it->cleared;
            frozen = order_dflt_it->frozen;
            db().modify(*order_dflt_it, [&](order_deflation_object &obj) {
                obj.last_deflation_id = op.deflation_id;
                obj.frozen = 0;
                obj.cleared = false;
            });
        } else {
            db().create<order_deflation_object>([&](order_deflation_object &obj){
                obj.order = op.order;
                obj.last_deflation_id = op.deflation_id;
                obj.frozen = 0;
                obj.cleared = false;
            });
        }

        // update order balance
        share_type deflation_amount = 0;
        if (!cleared) {
            auto &order_idx = db().get_index_type<limit_order_index>().indices().get<by_id>();
            const auto &order_it = order_idx.find(op.owner);
            // order sale asset is PIC, now do deflation
            if (order_it->sell_price.base.asset_id == asset_id_type(0)) {
                uint128_t amount = uint128_t(order_it->for_sale.value) * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
                deflation_amount = int64_t(amount.to_uint64());
                if (deflation_amount > 0) {
                    db().modify(*order_it, [&](limit_order_object &obj){
                        obj.for_sale -= deflation_amount;
                    });
                    // adjust total_core_in_orders of account
                    db().pay_order(order_it->seller(db()), asset(0), asset(deflation_amount));
                }
            }
        }
        db().modify(*dflt_it, [&](deflation_object &obj){
            obj.order_cursor = op.order + 1;
            obj.total_amount += (deflation_amount + frozen);
            if (op.order == dflt_it->last_order) {
                obj.order_cleared = true;
            }
        });
        return void_result();
    }    
}} // graphene::chain
