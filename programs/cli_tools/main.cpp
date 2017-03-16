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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>

#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/server.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/http_api.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/wallet/wallet.hpp>

#include <fc/interprocess/signals.hpp>
#include <boost/program_options.hpp>

#include <fc/log/console_appender.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/logger_config.hpp>
#include <boost/algorithm/string.hpp>
#include <random>

#include <chrono>
#include <thread>

#ifdef WIN32
# include <signal.h>
#else
# include <csignal>
#endif

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace graphene::wallet;
using namespace std;
namespace bpo = boost::program_options;

bool auto_transfer(fc::api<wallet_api> &api, const string &account_list_file);
bool import_balance(fc::api<wallet_api> &api, const string &account_list_file);
bool create_account(fc::api<wallet_api> &api, const string& list_file, const string &super_user, const string &super_key);
bool init_transfer(fc::api<wallet_api> &api, const string& list_file, const string &super_user, const string &super_key, int amount);
bool create_witness(fc::api<wallet_api> &api, const string& list_file);

int main( int argc, char** argv )
{
   try {

      boost::program_options::options_description opts;
         opts.add_options()
         ("help,h", "Print this help message and exit.")
         ("server-rpc-endpoint,s", bpo::value<string>()->implicit_value("ws://127.0.0.1:8090"), "Server websocket RPC endpoint")
         ("server-rpc-user,u", bpo::value<string>(), "Server Username")
         ("server-rpc-password,p", bpo::value<string>(), "Server Password")
         ("wallet-file,w", bpo::value<string>()->implicit_value("wallet.json"), "wallet to load")
         ("chain-id", bpo::value<string>(), "chain ID to connect to")
         ("operation,o", bpo::value<string>(), "operation to do")
         ("account-list-file,l", bpo::value<string>()->implicit_value("acc_list.txt"), "account list file path");

      bpo::variables_map options;

      bpo::store( bpo::parse_command_line(argc, argv, opts), options );

      if( options.count("help") )
      {
         std::cout << opts << "\n";
         return 0;
      }

      fc::path data_dir;
      fc::logging_config cfg;
      fc::path log_dir = data_dir / "logs";

      fc::file_appender::config ac;
      ac.filename             = log_dir / "rpc" / "rpc.log";
      ac.flush                = true;
      ac.rotate               = true;
      ac.rotation_interval    = fc::hours( 1 );
      ac.rotation_limit       = fc::days( 1 );

      std::cout << "Logging RPC to file: " << (data_dir / ac.filename).preferred_string() << "\n";

      cfg.appenders.push_back(fc::appender_config( "default", "console", fc::variant(fc::console_appender::config())));
      cfg.appenders.push_back(fc::appender_config( "rpc", "file", fc::variant(ac)));

      cfg.loggers = { fc::logger_config("default"), fc::logger_config( "rpc") };
      cfg.loggers.front().level = fc::log_level::info;
      cfg.loggers.front().appenders = {"default"};
      cfg.loggers.back().level = fc::log_level::debug;
      cfg.loggers.back().appenders = {"rpc"};

      fc::ecc::private_key committee_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));

      idump( (key_to_wif( committee_private_key ) ) );

      fc::ecc::private_key nathan_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      public_key_type nathan_pub_key = nathan_private_key.get_public_key();
      idump( (nathan_pub_key) );
      idump( (key_to_wif( nathan_private_key ) ) );

      //
      // TODO:  We read wallet_data twice, once in main() to grab the
      //    socket info, again in wallet_api when we do
      //    load_wallet_file().  Seems like this could be better
      //    designed.
      //
      wallet_data wdata;

      fc::path wallet_file( options.count("wallet-file") ? options.at("wallet-file").as<string>() : "wallet.json");
      if( fc::exists( wallet_file ) )
      {
         wdata = fc::json::from_file( wallet_file ).as<wallet_data>();
         if( options.count("chain-id") )
         {
            // the --chain-id on the CLI must match the chain ID embedded in the wallet file
            if( chain_id_type(options.at("chain-id").as<std::string>()) != wdata.chain_id )
            {
               std::cout << "Chain ID in wallet file does not match specified chain ID\n";
               return 1;
            }
         }
      }
      else
      {
         if( options.count("chain-id") )
         {
            wdata.chain_id = chain_id_type(options.at("chain-id").as<std::string>());
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (from CLI)\n";
         }
         else
         {
            wdata.chain_id = graphene::egenesis::get_egenesis_chain_id();
            std::cout << "Starting a new wallet with chain ID " << wdata.chain_id.str() << " (from egenesis)\n";
         }
      }

      // but allow CLI to override
      if( options.count("server-rpc-endpoint") )
         wdata.ws_server = options.at("server-rpc-endpoint").as<std::string>();
      if( options.count("server-rpc-user") )
         wdata.ws_user = options.at("server-rpc-user").as<std::string>();
      if( options.count("server-rpc-password") )
         wdata.ws_password = options.at("server-rpc-password").as<std::string>();
       
       string operation = "";
       if (options.count("operation")) {
           operation = options.at("operation").as<std::string>();
           cout << "operation: " << operation << endl;
       } else {
           cout << "operation needed " << endl;
           return 1;
       }
       
       string account_list_file = "acc_list.txt";
       if (options.count("account-list-file")) {
           account_list_file = options.at("account-list-file").as<std::string>();
       }
       cout << "account-list-file: " << account_list_file << endl;

      fc::http::websocket_client client;
      idump((wdata.ws_server));
      auto con  = client.connect( wdata.ws_server );
      auto apic = std::make_shared<fc::rpc::websocket_api_connection>(*con);

      auto remote_api = apic->get_remote_api< login_api >(1);
      edump((wdata.ws_user)(wdata.ws_password) );
      
      FC_ASSERT( remote_api->login( wdata.ws_user, wdata.ws_password ) );

      auto wapiptr = std::make_shared<wallet_api>( wdata, remote_api );
      wapiptr->set_wallet_filename( wallet_file.generic_string() );
      wapiptr->load_wallet_file();

      fc::api<wallet_api> wapi(wapiptr);
      wapi->unlock("1");
       
       if (operation == "import_balance") {
           import_balance(wapi, account_list_file);
       } else if (operation == "auto_transfer") {
            auto_transfer(wapi, account_list_file);
       } else if (operation == "create_account") {
            create_account(wapi, account_list_file, "russell2x2", "5K3Sc7C8X9acJHbMCqvhK2eMjXiWWNzg7UXjwdTvu2oEMvek1m1");
       } else if (operation == "init_transfer") {
           init_transfer(wapi, account_list_file, "russell2x2", "5K3Sc7C8X9acJHbMCqvhK2eMjXiWWNzg7UXjwdTvu2oEMvek1m1", 100);
       } else if (operation == "create_witness") {
           create_witness(wapi, account_list_file);
       }else {
           cout << "unknow operation- " << operation << endl;
       }
   }
   catch ( const fc::exception& e )
   {
      std::cout << e.to_detail_string() << "\n";
      return -1;
   }
   return 0;
}

struct account_info {
    string privatekey;
    string publickey;
    string address;
    string username;
};

int load_accounts(const string& list_file, vector<account_info> &accounts, fc::api<wallet_api> &api) {
    fstream in(list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        if (strs.size() <= 5) {
            continue;
        }
        account_info ai {
            strs[0],
            strs[1],
            strs[2],
            strs[4]
        };
        accounts.push_back(ai);
    }
    cout << accounts.size() << " accounts loaded." << endl;
    
    return accounts.size();
}

bool import_balance(fc::api<wallet_api> &api, const string &account_list_file) {
    fstream in(account_list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        account_info ai {
            strs[0],
            strs[1],
            strs[2],
            strs[4]
        };
        try {
            api->import_key(ai.username, ai.privatekey);
            api->import_balance(ai.username, {ai.privatekey}, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } catch (const fc::exception& e) {
            std::cout << e.to_detail_string() << "\n";
            continue;
        }
    }
    return true;
}

bool init_transfer(fc::api<wallet_api> &api, const string& list_file, const string &super_user, const string &super_key, int amount) {
    api->import_key(super_user, super_key);
    fstream in(list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        api->transfer(super_user, strs[4], to_string(amount), "BTS", "init transfer", true);
    }
    return true;
}

bool create_account(fc::api<wallet_api> &api, const string& list_file, const string &super_user, const string &super_key) {
    api->import_key(super_user, super_key);
    fstream in(list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        auto key = fc::ecc::private_key::regenerate(fc::sha256::hash(strs[3]));
        auto pub_key = key.get_public_key();
        auto bts_pub_key = public_key_type(pub_key);
        api->register_account(strs[4], pub_key, pub_key, super_user, super_user, 50, true);
    }
    return true;
}

bool create_witness(fc::api<wallet_api> &api, const string& list_file) {
    fstream in(list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        try {
            api->import_key(strs[4], strs[0]);
            auto key = fc::ecc::private_key::regenerate(fc::sha256::hash(strs[3]));
            auto pub_key = key.get_public_key();
            auto bts_pub_key = public_key_type(pub_key);
            api->create_witness1(strs[4], bts_pub_key, strs[4], true);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            api->vote_for_witness(strs[4], strs[4], true, true);
            
//            witness_object wo = api->get_witness(strs[4]);
//            std::this_thread::sleep_for(std::chrono::milliseconds(100));
//            cout << std::string(wo.signing_key) << "\t" << strs[4] << "\t" << api->get_private_key(wo.signing_key) << "\t" << endl;
        } catch (const fc::exception& e) {
            std::cout << e.to_detail_string() << "\n";
            continue;
        }
    }
    return true;
}

bool auto_transfer(fc::api<wallet_api> &api, const string &account_list_file) {
    std::vector<std::pair<std::string, std::string> > accounts;
    fstream in(account_list_file);
    while (!in.eof()) {
        string line;
        getline(in, line);
        boost::trim_right_if(line, boost::is_any_of("\n"));
        vector<string> strs;
        boost::split(strs, line, boost::is_any_of("\t"));
        if (strs.size() < 5) {
            continue;
        }
        accounts.push_back(std::pair<std::string, std::string>(strs[4], strs[0]));
    }
    
    uint64_t acc_cnt = accounts.size();
    cout << acc_cnt << " accounts loaded." << endl;
    
    random_device rd;
    while (true) {
        uint32_t idx_from = rd() % acc_cnt;
        api->import_key(accounts[idx_from].first, accounts[idx_from].second);
        auto balances = api->list_account_balances(accounts[idx_from].first);
        if (balances.size() <= 0 || balances[0].amount.value <= 2000000) {
            cout << "[PASS] " << accounts[idx_from].first << " not enough money" << endl;
            continue;
        } else {
            uint32_t idx_to = rd() % acc_cnt;
            uint64_t money = (rd() % ((balances[0].amount.value - 2000000) / 2)) / 100000 + 1;
            if (money <= 0) {
                continue;
            }
            string memo = accounts[idx_from].first + " send " + accounts[idx_to].first + " " + to_string(money) + " BTS";
            try {
                api->transfer(accounts[idx_from].first, accounts[idx_to].first, to_string(money), "BTS", memo, true);
                cout << "[TRANS] " << memo << endl;
                int sleep_time = 1 + rd() % 10;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
            } catch (const fc::exception& e) {
                std::cout << e.to_detail_string() << "\n";
                continue;
            }

        }
        cout << balances[0].amount.value << endl;
    }
    return true;
}

