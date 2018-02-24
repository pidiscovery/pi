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

#include <iostream>
#include <fc/rpc/websocket_api.hpp>
#include <graphene/app/api.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/wallet/wallet.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/base36.hpp>
#include <fstream>
#include <string>
#include <stdio.h>

#define CHAIN_ID "ae471be89b3509bf7474710dda6bf35d893387bae70402b54b616d72b83bc5a4"
#define SERVER_ENDPOINT "ws://cold.pi-const.com:8010"

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace graphene::wallet;
using namespace std;

void set_expiration(wallet_api& api, transaction& tx)
{
   const chain_parameters& params = api.get_global_properties().parameters;
   auto dynamic_global_properties = api.get_dynamic_global_properties();
   tx.set_reference_block(dynamic_global_properties.head_block_id);
   tx.set_expiration(dynamic_global_properties.time + fc::seconds(params.block_interval * (params.maintenance_skip_slots + 1) * 3));
   return;
}

void set_tx_fees(wallet_api& api, signed_transaction &tx) {
    const fee_schedule& s = api.get_global_properties().parameters.current_fees;
    for (auto &op : tx.operations) {
        s.set_fee(op);
    }
}

void list_account_balances(wallet_api& api, const std::string &account_name) {
    auto balances = api.list_account_balances(account_name);    
    for (auto it : balances) {
        asset_object asset_obj = api.get_asset(std::string(object_id_type(it.asset_id)));
        printf("%s: %.5f\n", asset_obj.symbol.c_str(), it.amount.value / double(asset::scaled_precision(asset_obj.precision).value));
    }
}

bool is_public_key(const std::string &key_str) {
    try {
        auto pub_key = public_key_type(key_str);
        if (pub_key == public_key_type()) {
            return false;
        }
    } catch (...) {
        return false;
    }
    return true;
}

vector<account_id_type> get_registered_name_by_key(wallet_api& api, public_key_type key) {
    return api.get_key_references(key);
}

public_key_type get_public_key_by_name(const std::string &name) {
    if (name.size() <= 2) {
        return public_key_type();
    }
    if (name[0] != 'n') {
        return public_key_type();
    }
    try {
        string base36(name.begin() + 1, name.end());
        auto bdata = fc::from_base36(base36);
        fc::ecc::public_key_data bkey ;
        if (bdata.size() != bkey.size()) {
            return public_key_type();
        }
        memcpy(bkey.data, bdata.data(), sizeof(bkey));
        auto pub_key = fc::ecc::public_key(bkey);
        return public_key_type(pub_key);
    } catch (...) {
        return public_key_type();
    }
}


bool transfer(
        wallet_api& api, 
        const std::string &sign_key,
        const std::string &from, 
        const std::string &to, 
        const std::string &amount, 
        const std::string &symbol, 
        const std::string &memo) {

    fc::optional<fc::ecc::private_key> pri_key = wif_to_key(sign_key);
    if (!pri_key) {
        return false;
    }

    transfer_operation op;

    account_object from_account = api.get_account(from);
    account_object to_account = api.get_account(to);
    fc::optional<asset_object> asset_obj = api.get_asset(symbol);

    op.from = from_account.id;
    op.to = to_account.id;
    op.amount = asset_obj->amount_from_string(amount);

    if (memo.size()) {
        op.memo = memo_data();
        op.memo->from = from_account.options.memo_key;
        op.memo->to = to_account.options.memo_key;
        op.memo->set_message(*pri_key, to_account.options.memo_key, memo);
    }

    signed_transaction tx;
    tx.operations.emplace_back(op);
    set_expiration(api, tx);
    set_tx_fees(api, tx);
    tx.validate();
    fc::sha256 chain_id;
    from_variant(CHAIN_ID, chain_id);
    tx.sign(*pri_key, chain_id_type(CHAIN_ID));
    api.broadcast_transaction(tx);

    return true;
}

bool create_account_by_transfer(
        wallet_api& api, 
        const std::string &sign_key,
        const std::string &from, 
        public_key_type to, 
        const std::string &amount, 
        const std::string &symbol, 
        const std::string &memo) {

    fc::optional<fc::ecc::private_key> pri_key = wif_to_key(sign_key);
    if (!pri_key) {
        return false;
    }

    account_create_by_transfer_operation op;

    account_object from_account = api.get_account(from);
    // account_object to_account = api.get_account(to);
    fc::optional<asset_object> asset_obj = api.get_asset(symbol);

    op.from = from_account.id;
    op.account_key = to;
    op.amount = asset_obj->amount_from_string(amount);

    if (memo.size()) {
        op.memo = memo_data();
        op.memo->from = from_account.options.memo_key;
        op.memo->to = to;
        op.memo->set_message(*pri_key, to, memo);
    }

    signed_transaction tx;
    tx.operations.emplace_back(op);
    set_expiration(api, tx);
    set_tx_fees(api, tx);
    tx.validate();
    fc::sha256 chain_id;
    from_variant(CHAIN_ID, chain_id);
    tx.sign(*pri_key, chain_id_type(CHAIN_ID));
    api.broadcast_transaction(tx);

    return true;
}

int main( int argc, char** argv )
{
    if (argc != 3 && argc != 6 && argc != 7) {
        fprintf(stderr, "Usage: "
            "\tCheck Balance:\n"
            "\t\t%s wallet_file check\n"
            "\tTransfer Asset:\n"
            "\t\t%s wallet_file transfer to_account amount asset memo\n",
            argv[0],
            argv[0]
        );
        return 0;
    }

    ifstream ifs(argv[1]);
    if (!ifs) {
        fprintf(stderr, "wallet file: %s not found.\n", argv[1]);
        return 0;
    }

    string name;
    string pub_key;
    string key;
    getline(ifs, name);
    getline(ifs, pub_key);
    getline(ifs, key);

    try {
        wallet_data wdata;
        wdata.chain_id = chain_id_type(CHAIN_ID);
        wdata.ws_server = SERVER_ENDPOINT;

        fc::http::websocket_client client;
        auto con  = client.connect( wdata.ws_server );
        auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);

        auto remote_api = apic->get_remote_api< login_api >(1);
      
        FC_ASSERT( remote_api->login( wdata.ws_user, wdata.ws_password ) );
        auto wapi_ptr = std::make_shared<wallet_api>( wdata, remote_api );

        if (string(argv[2]) == "check") {
            try {
                auto account_obj = wapi_ptr->get_account(name);
            } catch (const fc::exception& e) {
                fprintf(stderr, "user: %s not registered.\n", name.c_str());
                return 0;
            }            
            list_account_balances(*wapi_ptr, name);
            return 0;
        } else if (string(argv[2]) == "transfer") {
            string memo = "";
            if (argc == 7) {
                memo = argv[6];
            }

            if (is_public_key(argv[3])) {
                public_key_type to_pub_key(argv[3]);
                auto to_accounts = get_registered_name_by_key(*wapi_ptr, to_pub_key);
                if (to_accounts.size() == 0) {
                    // not registered
                    create_account_by_transfer(*wapi_ptr, key, name, to_pub_key, argv[4], argv[5], memo);
                    fprintf(stderr, "+* transfer %s %s from %s to %s, memo %s\n",
                        argv[4],
                        argv[5],
                        name.c_str(),
                        argv[3],
                        memo.c_str()
                    );
                    return 0;
                } else {
                    //registered
                    if (to_accounts.size() > 1) {
                        // public key is not unique for just one account, cannot transfer by public_key
                        fprintf(
                            stderr,
                            "this public_key: %s is registered, "
                            "but multi accounts are using it, please transfer by name\n",
                            argv[3]);
                        return 0;
                    } else {
                        auto acc = std::string(object_id_type(to_accounts[0]));
                        transfer(*wapi_ptr, key, name, acc, argv[4], argv[5], memo);
                        fprintf(stderr, "-* transfer %s %s from %s to %s, memo %s\n",
                            argv[4],
                            argv[5],
                            name.c_str(),
                            acc.c_str(),
                            memo.c_str()
                        );
                        return 0;
                    }
                }
            } else {
                auto to_account_id = wapi_ptr->get_account_id2(argv[3]);
                if (to_account_id == account_id_type()) {
                    public_key_type to_pub_key = get_public_key_by_name(argv[3]);
                    if (to_pub_key == public_key_type()) {
                        fprintf(stderr, "name: %s not registered and can not be created by transfer\n", argv[3]);
                        return 0;
                    } else {
                        create_account_by_transfer(*wapi_ptr, key, name, to_pub_key, argv[4], argv[5], memo);
                        fprintf(stderr, "+ transfer %s %s from %s to %s, memo %s\n",
                            argv[4],
                            argv[5],
                            name.c_str(),
                            argv[3],
                            memo.c_str()
                        );
                        return 0;
                    }
                } else {
                    transfer(*wapi_ptr, key, name, argv[3], argv[4], argv[5], memo);
                    fprintf(stderr, "- transfer %s %s from %s to %s, memo %s\n", 
                        argv[4],
                        argv[5],
                        name.c_str(),
                        argv[3],
                        memo.c_str()
                    );
                    return 0;
                }
            }
        } else {
            fprintf(stderr, "operation: %s not support.\n", argv[2]);
            return 0;
        }
   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
      return -1;
   }
   return 0;
}
