#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>

#include <string>

#include <amax.stoken/amax.stoken.db.hpp>
#include "wasm.db.hpp"

namespace amax {

using std::string;
using std::vector;

using namespace eosio;
using namespace wasm::db;
/**
 * The `amax.stoken` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.stoken` contract instead of developing their own.
 *
 * The `amax.stoken` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `amax.stoken` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.stoken")]] stoken : public contract {
   public:
      using contract::contract;

   stoken(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
        _global(get_self(), get_self().value), _db(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~stoken() { _global.set( _gstate, get_self() ); }

   ACTION addslotkey( const name& title, const name& auth_type, const set<name>& admins );
   ACTION addslot( const name& owner, const string& meta_uri, const map<name, string>& props );
   ACTION setslotprop( const name& signer, const uint64_t& slot_id, const name& prop_key, const string& prop_value );
   /**
    * @brief Allows `signer` account to create a SFT asset in supply of `maximum_supply`. If validation is successful a new entry in stats
    *
    * @param signer  - the account that creates the SFT asset
    * @param app_name - the app name under which the SFT assets to be created
    * @param asset_id - the asset ID. if 0 it will be using the default available primary ID
    * @param slot_id - the slot ID for the asset
    * @param maximum_supply - the maximum supply set for the token created
    * @return ACTION
    */
   ACTION create( const name& signer, const uint64_t& asset_id, const uint64_t& slot_id, const int64_t& maximum_supply );

   /**
    * @brief This action issues to `to` account a `quantity` of tokens.
    *
    * @param to - the account to issue tokens to, it must be the same as the issuer,
    * @param quntity - the amount of tokens to be issued,
    * @memo - the memo string that accompanies the token issue transaction.
    */
   ACTION issue( const name& to, const sasset& quantity, const string& memo );

   ACTION retire( const sasset& quantity, const string& memo );

   
	/**
	 * @brief Transfers one or more assets.
	 *
    * This action transfers one or more assets by changing scope.
    * Sender's RAM will be charged to transfer asset.
    * Transfer will fail if asset is offered for claim or is delegated.
    *
    * @param from is account who sends the asset.
    * @param to is account of receiver.
    * @param quantity is SFT asset
    * @param memo is transfers comment.
    * @return no return value.
    */
   ACTION transfer( const name& from, const name& to, const sasset& quantity, const string& memo );
   using transfer_action = action_wrapper< "transfer"_n, &stoken::transfer >;

   private:
      void add_balance( const name& owner, const sasset& value, const name& ram_payer );
      void sub_balance( const name& owner, const sasset& value );

      void create_new_slot(const name& new_owner, slot_t& new_slot);
      void create_new_sft( const name& creator, const slot_t& new_slot, sasset& new_sft );

   private:
      global_t::singleton        _global;
      global_t                   _gstate;
      dbc                        _db;
};
} //namespace amax
