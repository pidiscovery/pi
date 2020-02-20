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
                "deflation rate should in range (0, 100), ${rate}% is invalide",
                ("rate", 100.0 * op.rate / GRAPHENE_DEFLATION_RATE_SCALE)
            );
            // check existing deflation tx
            auto &index = db().get_index_type<deflation_index>().indices().get<by_id>();
            auto it = index.rbegin();
            if (it != index.rend()) {
                // there should not be running deflation
                FC_ASSERT(
                    it->cleared == true,
                    "cannot issue a defaltion when another is in progress - issuer:${issuer}, rate:${rate}",
                    ("issuer", op.issuer)
                    ("rate", op.rate)
                );
                // check deflation interval
                FC_ASSERT(
                    it->timestamp + GRAPHENE_MINIMUM_DEFLATION_INTERVAL < db().head_block_time(),
                    "issue defaltion to offen, deflation can be issued after ${t}",
                    ("t", it->timestamp + GRAPHENE_MINIMUM_DEFLATION_INTERVAL)
                );
            }

            auto &acc_idx = db().get_index_type<account_index>().indices().get<by_id>();
            const auto &acc_last_it = acc_idx.rbegin();
            FC_ASSERT(
                acc_last_it != acc_idx.rend()
                    && (account_id_type(acc_last_it->id) > GRAPHENE_DEFLATION_ACCOUNT_START_MARKER
                        || account_id_type(acc_last_it->id) == GRAPHENE_DEFLATION_ACCOUNT_START_MARKER),
                "issue defaltion fail, for there's no account to deflate"
            );

            return void_result();
    } FC_CAPTURE_AND_RETHROW((op)) }

    void_result deflation_evaluator::do_apply( const deflation_operation& op ) {
        auto &acc_idx = db().get_index_type<account_index>().indices().get<by_id>();
        const auto &acc_last_it = acc_idx.rbegin();

        // auto &acc_idx = db().get_index_type<account_index>().indices().get<by_id>();
        // account_id_type acc_id = acc_idx.rbegin()->id;
        db().create<deflation_object>( [&]( deflation_object &obj){
            obj.issuer = op.issuer;
            obj.rate = op.rate;
            obj.timestamp = db().head_block_time();

            obj.last_account = acc_last_it->id;
            obj.cursor = GRAPHENE_DEFLATION_ACCOUNT_START_MARKER;
            obj.cleared = false;
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
                dflt_it->cleared == false,
                "deflation is already cleared"
            );
            FC_ASSERT(
                account_id_type(op.owner) > dflt_it->cursor || account_id_type(op.owner) == dflt_it->cursor,
                "deflation for this account-${acc} has been done before",
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
                obj.cleared = true;
            });
        } else {
            db().create<account_deflation_object>( [&]( account_deflation_object &obj){
                obj.owner = op.owner;
                obj.last_deflation_id = op.deflation_id;
                obj.frozen = 0;
                obj.cleared = true;
            });
        }

        // update account balance
        share_type deflation_amount = 0;
        if (!cleared) {
            auto balance = db().get_balance(op.owner, asset_id_type(0));
            deflation_amount = balance.amount.value * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
        }
        if (deflation_amount > 0) {
            db().adjust_balance(op.owner, -asset(deflation_amount, asset_id_type(0)));
        }

        db().modify(*dflt_it, [&](deflation_object &obj){
            obj.cursor = op.owner + 1;
            obj.total_amount += (deflation_amount + frozen);
            if (op.owner == dflt_it->last_account) {
                obj.cleared = true;
            }
        });

        return void_result();
    }
}} // graphene::chain
