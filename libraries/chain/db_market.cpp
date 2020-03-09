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

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>
#include <fc/real128.hpp>
#include <graphene/chain/deflation_object.hpp>

using namespace fc;

namespace graphene { namespace chain {

/**
 * All margin positions are force closed at the swan price
 * Collateral received goes into a force-settlement fund
 * No new margin positions can be created for this asset
 * No more price feed updates
 * Force settlement happens without delay at the swan price, deducting from force-settlement fund
 * No more asset updates may be issued.
*/
void database::globally_settle_asset( const asset_object& mia, const price& settlement_price )
{ try {
   /*
   elog( "BLACK SWAN!" );
   debug_dump();
   edump( (mia.symbol)(settlement_price) );
   */

   const asset_bitasset_data_object& bitasset = mia.bitasset_data(*this);
   FC_ASSERT( !bitasset.has_settlement(), "black swan already occurred, it should not happen again" );

   const asset_object& backing_asset = bitasset.options.short_backing_asset(*this);
   asset collateral_gathered = backing_asset.amount(0);

   const asset_dynamic_data_object& mia_dyn = mia.dynamic_asset_data_id(*this);
   auto original_mia_supply = mia_dyn.current_supply;

   const call_order_index& call_index = get_index_type<call_order_index>();
   const auto& call_price_index = call_index.indices().get<by_price>();

   // cancel all call orders and accumulate it into collateral_gathered
   auto call_itr = call_price_index.lower_bound( price::min( bitasset.options.short_backing_asset, mia.id ) );
   auto call_end = call_price_index.upper_bound( price::max( bitasset.options.short_backing_asset, mia.id ) );
   while( call_itr != call_end )
   {
      auto pays = call_itr->get_debt() * settlement_price;

      if( pays > call_itr->get_collateral() )
         pays = call_itr->get_collateral();

      collateral_gathered += pays;
      const auto&  order = *call_itr;
      ++call_itr;
      FC_ASSERT( fill_order( order, pays, order.get_debt() ) );
   }

   modify( bitasset, [&]( asset_bitasset_data_object& obj ){
           assert( collateral_gathered.asset_id == settlement_price.quote.asset_id );
           obj.settlement_price = mia.amount(original_mia_supply) / collateral_gathered; //settlement_price;
           obj.settlement_fund  = collateral_gathered.amount;
           });

   /// After all margin positions are closed, the current supply will be reported as 0, but
   /// that is a lie, the supply didn't change.   We need to capture the current supply before
   /// filling all call orders and then restore it afterward.   Then in the force settlement
   /// evaluator reduce the supply
   modify( mia_dyn, [&]( asset_dynamic_data_object& obj ){
           obj.current_supply = original_mia_supply;
         });

} FC_CAPTURE_AND_RETHROW( (mia)(settlement_price) ) }

void database::cancel_order(const force_settlement_object& order, bool create_virtual_op)
{
   adjust_balance(order.owner, order.balance);

   if( create_virtual_op )
   {
      asset_settle_cancel_operation vop;
      vop.settlement = order.id;
      vop.account = order.owner;
      vop.amount = order.balance;
      push_applied_operation( vop );
   }
   remove(order);
}

void database::cancel_order( const limit_order_object& order, bool create_virtual_op  )
{
   // check deflation
   asset deflation(0, order.sell_price.base.asset_id);
   if (order.sell_price.base.asset_id == asset_id_type()) {
      const auto &dflt_idx = get_index_type<deflation_index>().indices().get<by_id>();
      auto dlft_it = dflt_idx.rbegin();
      if (dlft_it != dflt_idx.rend() 
            && !dlft_it->order_cleared 
            && (dlft_it->last_order > limit_order_id_type(order.id) || dlft_it->last_order == limit_order_id_type(order.id))
            && (dlft_it->order_cursor < limit_order_id_type(order.id) || dlft_it->order_cursor == limit_order_id_type(order.id))) {
         const auto &order_dflt_idx = get_index_type<order_deflation_index>().indices().get<by_order>();
         auto order_dflt_it = order_dflt_idx.find(order.id);
         if (order_dflt_it == order_dflt_idx.end() ||  !order_dflt_it->cleared) {
            uint128_t amount = uint128_t(order.for_sale.value) * dlft_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
            deflation.amount = int64_t(amount.to_uint64());

            modify(*dlft_it, [&](deflation_object &obj){
               obj.total_amount += deflation.amount;
            });

            // create a virtual order_deflation_operation
            order_deflation_operation vop;
            vop.deflation_id = dlft_it->id;
            vop.order = order.id;
            vop.owner = order.seller;
            vop.amount = deflation.amount;
            push_applied_operation( vop );

            // clear order_deflation_object when cancel
            if (order_dflt_it != order_dflt_idx.end()) {
               remove(*order_dflt_it);
            }

            // if (order_dflt_it == order_dflt_idx.end()) {
            //    create<order_deflation_object>([&](order_deflation_object &obj){
            //       obj.order = order.id;
            //       obj.last_deflation_id = deflation_id_type(0);
            //       obj.frozen = deflation.amount;
            //       obj.cleared = true;
            //    }); 
            // } else {
            //    modify(*order_dflt_it, [&](order_deflation_object &obj){
            //       obj.frozen = deflation.amount;
            //       obj.cleared = true;
            //    });
            // }
         }
      }
   }

   auto refunded = order.amount_for_sale() - deflation;

   modify( order.seller(*this).statistics(*this),[&]( account_statistics_object& obj ){
      if( refunded.asset_id == asset_id_type() )
      {
         obj.total_core_in_orders -= order.amount_for_sale().amount;
      }
   });
   adjust_balance(order.seller, refunded);
   adjust_balance(order.seller, order.deferred_fee);

   if( create_virtual_op )
   {
      limit_order_cancel_operation vop;
      vop.order = order.id;
      vop.fee_paying_account = order.seller;
      push_applied_operation( vop );
   }

   remove(order);
}

bool maybe_cull_small_order( database& db, const limit_order_object& order )
{
   /**
    *  There are times when the AMOUNT_FOR_SALE * SALE_PRICE == 0 which means that we
    *  have hit the limit where the seller is asking for nothing in return.  When this
    *  happens we must refund any balance back to the seller, it is too small to be
    *  sold at the sale price.
    *
    *  If the order is a taker order (as opposed to a maker order), so the price is
    *  set by the counterparty, this check is deferred until the order becomes unmatched
    *  (see #555) -- however, detecting this condition is the responsibility of the caller.
    */
   if( order.amount_to_receive().amount == 0 )
   {
      //ilog( "applied epsilon logic" );
      db.cancel_order(order);
      return true;
   }
   return false;
}

bool database::apply_order(const limit_order_object& new_order_object, bool allow_black_swan)
{
   auto order_id = new_order_object.id;
   const asset_object& sell_asset = get(new_order_object.amount_for_sale().asset_id);
   const asset_object& receive_asset = get(new_order_object.amount_to_receive().asset_id);

   // Possible optimization: We only need to check calls if both are true:
   //  - The new order is at the front of the book
   //  - The new order is below the call limit price
   bool called_some = check_call_orders(sell_asset, allow_black_swan);
   called_some |= check_call_orders(receive_asset, allow_black_swan);
   if( called_some && !find_object(order_id) ) // then we were filled by call order
      return true;

   const auto& limit_price_idx = get_index_type<limit_order_index>().indices().get<by_price>();

   // TODO: it should be possible to simply check the NEXT/PREV iterator after new_order_object to
   // determine whether or not this order has "changed the book" in a way that requires us to
   // check orders. For now I just lookup the lower bound and check for equality... this is log(n) vs
   // constant time check. Potential optimization.

   auto max_price = ~new_order_object.sell_price;
   auto limit_itr = limit_price_idx.lower_bound(max_price.max());
   auto limit_end = limit_price_idx.upper_bound(max_price);

   bool finished = false;
   while( !finished && limit_itr != limit_end )
   {
      auto old_limit_itr = limit_itr;
      // order deflation check here
      if (old_limit_itr->sell_price.base.asset_id == asset_id_type(0)) {
         auto &dflt_idx = get_index_type<deflation_index>().indices().get<by_id>();
         const auto &dflt_it = dflt_idx.rbegin();
         // a deflation is running & order deflation not finished
         if (dflt_it != dflt_idx.rend() && !dflt_it->order_cleared) {
            const auto &order_dflt_idx = get_index_type<order_deflation_index>().indices().get<by_order>();
            auto order_dflt_it = order_dflt_idx.find(old_limit_itr->id);
            // order_deflation_object2 not found or it has done deflation this round
            if (order_dflt_it == order_dflt_idx.end() || (order_dflt_it->last_deflation_id < deflation_id_type(dflt_it->id) && !order_dflt_it->cleared)) {
               uint128_t amount = uint128_t(old_limit_itr->for_sale.value) * dflt_it->rate / GRAPHENE_DEFLATION_RATE_SCALE;
               share_type deflation_amount = int64_t(amount.to_uint64());               
               if (deflation_amount > 0) {
                  if (order_dflt_it == order_dflt_idx.end()) {
                     create<order_deflation_object>([&](order_deflation_object &obj){
                        obj.order = old_limit_itr->id;
                        obj.frozen = deflation_amount;
                        obj.cleared = true;                     
                     });
                  } else {
                     modify(*order_dflt_it, [&](order_deflation_object &obj){
                        obj.frozen = deflation_amount;
                        obj.cleared = true;                     
                     });
                  }
                  pay_order(old_limit_itr->seller(*this), asset(0), asset(deflation_amount));
               }
            }
         }
      }
      ++limit_itr;
      // match returns 2 when only the old order was fully filled. In this case, we keep matching; otherwise, we stop.
      finished = (match(new_order_object, *old_limit_itr, old_limit_itr->sell_price) != 2);
   }

   //Possible optimization: only check calls if the new order completely filled some old order
   //Do I need to check both assets?
   check_call_orders(sell_asset, allow_black_swan);
   check_call_orders(receive_asset, allow_black_swan);

   const limit_order_object* updated_order_object = find< limit_order_object >( order_id );
   if( updated_order_object == nullptr )
      return true;
   if( head_block_time() <= HARDFORK_555_TIME )
      return false;
   // before #555 we would have done maybe_cull_small_order() logic as a result of fill_order() being called by match() above
   // however after #555 we need to get rid of small orders -- #555 hardfork defers logic that was done too eagerly before, and
   // this is the point it's deferred to.
   return maybe_cull_small_order( *this, *updated_order_object );
}

/**
 *  Matches the two orders,
 *
 *  @return a bit field indicating which orders were filled (and thus removed)
 *
 *  0 - no orders were matched
 *  1 - bid was filled
 *  2 - ask was filled
 *  3 - both were filled
 */
template<typename OrderType>
int database::match( const limit_order_object& usd, const OrderType& core, const price& match_price )
{
   assert( usd.sell_price.quote.asset_id == core.sell_price.base.asset_id );
   assert( usd.sell_price.base.asset_id  == core.sell_price.quote.asset_id );
   assert( usd.for_sale > 0 && core.for_sale > 0 );

   auto usd_for_sale = usd.amount_for_sale();
   auto core_for_sale = core.amount_for_sale();

   asset usd_pays, usd_receives, core_pays, core_receives;

   if( usd_for_sale <= core_for_sale * match_price )
   {
      core_receives = usd_for_sale;
      usd_receives  = usd_for_sale * match_price;
   }
   else
   {
      //This line once read: assert( core_for_sale < usd_for_sale * match_price );
      //This assert is not always true -- see trade_amount_equals_zero in operation_tests.cpp
      //Although usd_for_sale is greater than core_for_sale * match_price, core_for_sale == usd_for_sale * match_price
      //Removing the assert seems to be safe -- apparently no asset is created or destroyed.
      usd_receives = core_for_sale;
      core_receives = core_for_sale * match_price;
   }

   core_pays = usd_receives;
   usd_pays  = core_receives;

   assert( usd_pays == usd.amount_for_sale() ||
           core_pays == core.amount_for_sale() );

   int result = 0;
   result |= fill_order( usd, usd_pays, usd_receives, false );
   result |= fill_order( core, core_pays, core_receives, true ) << 1;
   assert( result != 0 );
   return result;
}

int database::match( const limit_order_object& bid, const limit_order_object& ask, const price& match_price )
{
   return match<limit_order_object>( bid, ask, match_price );
}


asset database::match( const call_order_object& call, 
                       const force_settlement_object& settle, 
                       const price& match_price,
                       asset max_settlement )
{ try {
   FC_ASSERT(call.get_debt().asset_id == settle.balance.asset_id );
   FC_ASSERT(call.debt > 0 && call.collateral > 0 && settle.balance.amount > 0);

   auto settle_for_sale = std::min(settle.balance, max_settlement);
   auto call_debt = call.get_debt();

   asset call_receives   = std::min(settle_for_sale, call_debt);
   asset call_pays       = call_receives * match_price;
   asset settle_pays     = call_receives;
   asset settle_receives = call_pays;

   /**
    *  If the least collateralized call position lacks sufficient
    *  collateral to cover at the match price then this indicates a black 
    *  swan event according to the price feed, but only the market 
    *  can trigger a black swan.  So now we must cancel the forced settlement
    *  object.
    */
   GRAPHENE_ASSERT( call_pays < call.get_collateral(), black_swan_exception, "" );

   assert( settle_pays == settle_for_sale || call_receives == call.get_debt() );

   fill_order(call, call_pays, call_receives);
   fill_order(settle, settle_pays, settle_receives);

   return call_receives;
} FC_CAPTURE_AND_RETHROW( (call)(settle)(match_price)(max_settlement) ) }

bool database::fill_order( const limit_order_object& order, const asset& pays, const asset& receives, bool cull_if_small )
{ try {
   cull_if_small |= (head_block_time() < HARDFORK_555_TIME);

   FC_ASSERT( order.amount_for_sale().asset_id == pays.asset_id );
   FC_ASSERT( pays.asset_id != receives.asset_id );

   const account_object& seller = order.seller(*this);
   const asset_object& recv_asset = receives.asset_id(*this);

   auto issuer_fees = pay_market_fees( recv_asset, receives );
   pay_order( seller, receives - issuer_fees, pays );

   // pay exchange fee
   uint32_t exchange_fee_rate = 0;
   account_id_type exchange_fee_receiver = GRAPHENE_NULL_ACCOUNT;
   if (order.exchange_fee_receiver) {
      exchange_fee_receiver = *order.exchange_fee_receiver;
      const auto& index = get_index_type<limit_order_fee_config_index>().indices().get<by_receiver>();
      const auto& fee_conf = index.find(exchange_fee_receiver);
      if (fee_conf != index.end()) {
        auto rate = fee_conf->get_fee_rate(receives.asset_id, pays.asset_id);
        if (rate.first > 0) {
          exchange_fee_rate = rate.first;
          asset total_receive = receives - issuer_fees;
          real128 amount = real128(total_receive.amount.value) * real128(exchange_fee_rate) / real128(GRAPHENE_EXCHANGE_RATE_SCALE);
          asset exchange_got(amount.to_uint64(), total_receive.asset_id);
          adjust_balance(seller.get_id(), -exchange_got);
          adjust_balance(exchange_fee_receiver, exchange_got);
        }
      }
   }
  
   assert( pays.asset_id != receives.asset_id );
   push_applied_operation( fill_order_operation( order.id, order.seller, pays, receives, issuer_fees, exchange_fee_rate, exchange_fee_receiver ) );

   // conditional because cheap integer comparison may allow us to avoid two expensive modify() and object lookups
   if( order.deferred_fee > 0 )
   {
      modify( seller.statistics(*this), [&]( account_statistics_object& statistics )
      {
         statistics.pay_fee( order.deferred_fee, get_global_properties().parameters.cashback_vesting_threshold );
      } );
   }

   if( pays == order.amount_for_sale() )
   {
      remove( order );
      return true;
   }
   else
   {
      modify( order, [&]( limit_order_object& b ) {
                             b.for_sale -= pays.amount;
                             b.deferred_fee = 0;
                          });
      if( cull_if_small )
         return maybe_cull_small_order( *this, order );
      return false;
   }
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }


bool database::fill_order( const call_order_object& order, const asset& pays, const asset& receives )
{ try {
   //idump((pays)(receives)(order));
   FC_ASSERT( order.get_debt().asset_id == receives.asset_id );
   FC_ASSERT( order.get_collateral().asset_id == pays.asset_id );
   FC_ASSERT( order.get_collateral() >= pays );

   optional<asset> collateral_freed;
   modify( order, [&]( call_order_object& o ){
            o.debt       -= receives.amount;
            o.collateral -= pays.amount;
            if( o.debt == 0 )
            {
              collateral_freed = o.get_collateral();
              o.collateral = 0;
            }
       });
   const asset_object& mia = receives.asset_id(*this);
   assert( mia.is_market_issued() );

   const asset_dynamic_data_object& mia_ddo = mia.dynamic_asset_data_id(*this);

   modify( mia_ddo, [&]( asset_dynamic_data_object& ao ){
       //idump((receives));
        ao.current_supply -= receives.amount;
      });

   const account_object& borrower = order.borrower(*this);
   if( collateral_freed || pays.asset_id == asset_id_type() )
   {
      const account_statistics_object& borrower_statistics = borrower.statistics(*this);
      if( collateral_freed )
         adjust_balance(borrower.get_id(), *collateral_freed);

      modify( borrower_statistics, [&]( account_statistics_object& b ){
              if( collateral_freed && collateral_freed->amount > 0 )
                b.total_core_in_orders -= collateral_freed->amount;
              if( pays.asset_id == asset_id_type() )
                b.total_core_in_orders -= pays.amount;

              assert( b.total_core_in_orders >= 0 );
           });
   }

   assert( pays.asset_id != receives.asset_id );
   push_applied_operation( fill_order_operation{ order.id, order.borrower, pays, receives, asset(0, pays.asset_id) } );

   if( collateral_freed )
      remove( order );

   return collateral_freed.valid();
} FC_CAPTURE_AND_RETHROW( (order)(pays)(receives) ) }

bool database::fill_order(const force_settlement_object& settle, const asset& pays, const asset& receives)
{ try {
   bool filled = false;

   auto issuer_fees = pay_market_fees(get(receives.asset_id), receives);

   if( pays < settle.balance )
   {
      modify(settle, [&pays](force_settlement_object& s) {
         s.balance -= pays;
      });
      filled = false;
   } else {
      filled = true;
   }
   adjust_balance(settle.owner, receives - issuer_fees);

   assert( pays.asset_id != receives.asset_id );
   push_applied_operation( fill_order_operation{ settle.id, settle.owner, pays, receives, issuer_fees } );

   if (filled)
      remove(settle);

   return filled;
} FC_CAPTURE_AND_RETHROW( (settle)(pays)(receives) ) }

/**
 *  Starting with the least collateralized orders, fill them if their
 *  call price is above the max(lowest bid,call_limit).
 *
 *  This method will return true if it filled a short or limit
 *
 *  @param mia - the market issued asset that should be called.
 *  @param enable_black_swan - when adjusting collateral, triggering a black swan is invalid and will throw
 *                             if enable_black_swan is not set to true.
 *
 *  @return true if a margin call was executed.
 */
bool database::check_call_orders(const asset_object& mia, bool enable_black_swan)
{ try {
    if( !mia.is_market_issued() ) return false;

    if( check_for_blackswan( mia, enable_black_swan ) ) 
       return false;

    const asset_bitasset_data_object& bitasset = mia.bitasset_data(*this);
    if( bitasset.is_prediction_market ) return false;
    if( bitasset.current_feed.settlement_price.is_null() ) return false;

    const call_order_index& call_index = get_index_type<call_order_index>();
    const auto& call_price_index = call_index.indices().get<by_price>();

    const limit_order_index& limit_index = get_index_type<limit_order_index>();
    const auto& limit_price_index = limit_index.indices().get<by_price>();

    // looking for limit orders selling the most USD for the least CORE
    auto max_price = price::max( mia.id, bitasset.options.short_backing_asset );
    // stop when limit orders are selling too little USD for too much CORE
    auto min_price = bitasset.current_feed.max_short_squeeze_price();

    assert( max_price.base.asset_id == min_price.base.asset_id );
    // NOTE limit_price_index is sorted from greatest to least
    auto limit_itr = limit_price_index.lower_bound( max_price );
    auto limit_end = limit_price_index.upper_bound( min_price );

    if( limit_itr == limit_end )
       return false;

    auto call_min = price::min( bitasset.options.short_backing_asset, mia.id );
    auto call_max = price::max( bitasset.options.short_backing_asset, mia.id );
    auto call_itr = call_price_index.lower_bound( call_min );
    auto call_end = call_price_index.upper_bound( call_max );

    bool filled_limit = false;
    bool margin_called = false;

    while( !check_for_blackswan( mia, enable_black_swan ) && call_itr != call_end )
    {
       bool  filled_call      = false;
       price match_price;
       asset usd_for_sale;
       if( limit_itr != limit_end )
       {
          assert( limit_itr != limit_price_index.end() );
          match_price      = limit_itr->sell_price;
          usd_for_sale     = limit_itr->amount_for_sale();
       }
       else return margin_called;

       match_price.validate();

       // would be margin called, but there is no matching order #436
       bool feed_protected = ( bitasset.current_feed.settlement_price > ~call_itr->call_price );
       if( feed_protected && (head_block_time() > HARDFORK_436_TIME) )
          return margin_called;

       // would be margin called, but there is no matching order
       if( match_price > ~call_itr->call_price )
          return margin_called;

       if( feed_protected )
       {
          ilog( "Feed protected margin call executing (HARDFORK_436_TIME not here yet)" );
          idump( (*call_itr) );
          idump( (*limit_itr) );
       }

     //  idump((*call_itr));
     //  idump((*limit_itr));

     //  ilog( "match_price <= ~call_itr->call_price  performing a margin call" );

       margin_called = true;

       auto usd_to_buy   = call_itr->get_debt();

       if( usd_to_buy * match_price > call_itr->get_collateral() )
       {
          elog( "black swan detected" ); 
          edump((enable_black_swan));
          FC_ASSERT( enable_black_swan );
          globally_settle_asset(mia, bitasset.current_feed.settlement_price );
          return true;
       }

       asset call_pays, call_receives, order_pays, order_receives;
       if( usd_to_buy >= usd_for_sale )
       {  // fill order
          call_receives   = usd_for_sale;
          order_receives  = usd_for_sale * match_price;
          call_pays       = order_receives;
          order_pays      = usd_for_sale;

          filled_limit = true;
          filled_call           = (usd_to_buy == usd_for_sale);
       } else { // fill call
          call_receives  = usd_to_buy;
          order_receives = usd_to_buy * match_price;
          call_pays      = order_receives;
          order_pays     = usd_to_buy;

          filled_call    = true;
       }

       FC_ASSERT( filled_call || filled_limit );

       auto old_call_itr = call_itr;
       if( filled_call ) ++call_itr;
       fill_order(*old_call_itr, call_pays, call_receives);

       auto old_limit_itr = filled_limit ? limit_itr++ : limit_itr;
       fill_order(*old_limit_itr, order_pays, order_receives, true);

    } // whlie call_itr != call_end

    return margin_called;
} FC_CAPTURE_AND_RETHROW() }

void database::pay_order( const account_object& receiver, const asset& receives, const asset& pays )
{
   const auto& balances = receiver.statistics(*this);
   modify( balances, [&]( account_statistics_object& b ){
         if( pays.asset_id == asset_id_type() )
         {
            b.total_core_in_orders -= pays.amount;
         }
   });
   adjust_balance(receiver.get_id(), receives);
}

asset database::calculate_market_fee( const asset_object& trade_asset, const asset& trade_amount )
{
   assert( trade_asset.id == trade_amount.asset_id );

   if( !trade_asset.charges_market_fees() )
      return trade_asset.amount(0);
   if( trade_asset.options.market_fee_percent == 0 )
      return trade_asset.amount(0);

   fc::uint128 a(trade_amount.amount.value);
   a *= trade_asset.options.market_fee_percent;
   a /= GRAPHENE_100_PERCENT;
   asset percent_fee = trade_asset.amount(a.to_uint64());

   if( percent_fee.amount > trade_asset.options.max_market_fee )
      percent_fee.amount = trade_asset.options.max_market_fee;

   return percent_fee;
}

asset database::pay_market_fees( const asset_object& recv_asset, const asset& receives )
{
   auto issuer_fees = calculate_market_fee( recv_asset, receives );
   assert(issuer_fees <= receives );

   //Don't dirty undo state if not actually collecting any fees
   if( issuer_fees.amount > 0 )
   {
      const auto& recv_dyn_data = recv_asset.dynamic_asset_data_id(*this);
      modify( recv_dyn_data, [&]( asset_dynamic_data_object& obj ){
                   //idump((issuer_fees));
         obj.accumulated_fees += issuer_fees.amount;
      });
   }

   return issuer_fees;
}

} }
