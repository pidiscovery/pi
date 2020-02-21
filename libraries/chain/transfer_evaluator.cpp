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
#include <graphene/chain/transfer_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/deflation_object.hpp>

namespace graphene { namespace chain {
void_result transfer_evaluator::do_evaluate( const transfer_operation& op )
{ try {
   
   const database& d = db();

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);
   const asset_object&   asset_type      = op.amount.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",op.from)
         ("asset",op.amount.asset_id)
         );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, to_account, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",op.to)
         ("asset",op.amount.asset_id)
         );

      if( asset_type.is_transfer_restricted() )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", op.amount.asset_id)
          );
      }

      // check if there is a deflation in progress
      share_type dlft_amt = 0;
      // deflation has effect on native PIC only 
      if (op.amount.asset_id == asset_id_type(0)) {
         auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
         const auto &dflt_it = dflt_idx.rbegin();
         // have deflation and it's not finished
         if (dflt_it != dflt_idx.rend() && !dflt_it->cleared) {
            const auto &acc_dflt_idx = db().get_index_type<account_deflation_index>().indices().get<by_owner>();
            const auto &acc_dflt_it = acc_dflt_idx.find(op.from);
            // this account is not finished
            if (acc_dflt_it == acc_dflt_idx.end() 
                  || (acc_dflt_it->last_deflation_id < deflation_id_type(dflt_it->id) 
                     && !acc_dflt_it->cleared)) {
               fc::uint128_t amt = fc::uint128_t(d.get_balance( from_account, asset_type ).amount.value) * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
               dlft_amt = amt.to_uint64();
            }
         }
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= op.amount.amount + dlft_amt;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'", 
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(op.amount))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(op.amount))("f",op.from(d).name)("t",op.to(d).name) );

}  FC_CAPTURE_AND_RETHROW( (op) ) }

void_result transfer_evaluator::do_apply( const transfer_operation& o )
{ try {
   // process deflation effect
   asset dlft_from(0, o.amount.asset_id);
   asset dlft_to(0, o.amount.asset_id);
   if (o.amount.asset_id == asset_id_type(0)) {
      auto &dflt_idx = db().get_index_type<deflation_index>().indices().get<by_id>();
      const auto &dflt_it = dflt_idx.rbegin();
      // have deflation and it's not finished
      if (dflt_it != dflt_idx.rend() && !dflt_it->cleared) {
         const auto &acc_dflt_idx = db().get_index_type<account_deflation_index>().indices().get<by_owner>();
         // FROM
         const auto &acc_dflt_it_from = acc_dflt_idx.find(o.from);
         if (acc_dflt_it_from == acc_dflt_idx.end() 
               || (acc_dflt_it_from->last_deflation_id < deflation_id_type(dflt_it->id)
                  && !acc_dflt_it_from->cleared)) {
            fc::uint128_t dlft_amt = fc::uint128_t(db().get_balance(o.from, o.amount.asset_id).amount.value) * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
            dlft_from.amount = dlft_amt.to_uint64();
            if (acc_dflt_it_from == acc_dflt_idx.end()) {
               db().create<account_deflation_object>([&](account_deflation_object &obj){
                  obj.owner = o.from;
                  obj.last_deflation_id = deflation_id_type(0);
                  obj.frozen = dlft_from.amount;
                  obj.cleared = true;
               });
            } else {
               db().modify(*acc_dflt_it_from, [&](account_deflation_object &obj){
                  obj.frozen = dlft_from.amount;
                  obj.cleared = true;
               });
            }
         }
         // TO
         const auto &acc_dflt_it_to = acc_dflt_idx.find(o.to);
         if (acc_dflt_it_to == acc_dflt_idx.end() 
               || (acc_dflt_it_to->last_deflation_id < deflation_id_type(dflt_it->id) 
                  && !acc_dflt_it_to->cleared)) {
            fc::uint128_t dlft_amt = fc::uint128_t(db().get_balance( o.to, o.amount.asset_id).amount.value) * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;;
            dlft_to.amount = dlft_amt.to_uint64();
            if (acc_dflt_it_to == acc_dflt_idx.end()) {
               db().create<account_deflation_object>([&](account_deflation_object &obj){
                  obj.owner = o.to;
                  obj.last_deflation_id = deflation_id_type(0);
                  obj.frozen = dlft_to.amount;
                  obj.cleared = true;
               });
            } else {
               db().modify(*acc_dflt_it_to, [&](account_deflation_object &obj){
                  obj.frozen = dlft_to.amount;
                  obj.cleared = true;
               });
            }
         }
      }
   }
   db().adjust_balance( o.from, -o.amount - dlft_from );
   db().adjust_balance( o.to, o.amount - dlft_to );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }



void_result override_transfer_evaluator::do_evaluate( const override_transfer_operation& op )
{ try {
   const database& d = db();

   const asset_object&   asset_type      = op.amount.asset_id(d);
   GRAPHENE_ASSERT(
      asset_type.can_override(),
      override_transfer_not_permitted,
      "override_transfer not permitted for asset ${asset}",
      ("asset", op.amount.asset_id)
      );
   FC_ASSERT( asset_type.issuer == op.issuer );

   const account_object& from_account    = op.from(d);
   const account_object& to_account      = op.to(d);

   FC_ASSERT( is_authorized_asset( d, to_account, asset_type ) );
   FC_ASSERT( is_authorized_asset( d, from_account, asset_type ) );

   if( d.head_block_time() <= HARDFORK_419_TIME )
   {
      FC_ASSERT( is_authorized_asset( d, from_account, asset_type ) );
   }
   // the above becomes no-op after hardfork because this check will then be performed in evaluator

   FC_ASSERT( d.get_balance( from_account, asset_type ).amount >= op.amount.amount,
              "", ("total_transfer",op.amount)("balance",d.get_balance(from_account, asset_type).amount) );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result override_transfer_evaluator::do_apply( const override_transfer_operation& o )
{ try {
   db().adjust_balance( o.from, -o.amount );
   db().adjust_balance( o.to, o.amount );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain
