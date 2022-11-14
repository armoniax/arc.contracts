#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;


#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm{"active"_n};

static constexpr uint64_t percent_boost     = 10000;
static constexpr uint64_t max_uri_size      = 1024;
static constexpr uint64_t max_prop_size     = 64;
static constexpr uint64_t max_memo_size     = 256;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())
#define TBL struct [[eosio::table, eosio::contract("amax.stoken")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.stoken")]]


enum class err: uint8_t {
    NONE                = 0,
    RECORD_NOT_FOUND    = 1,
    RECORD_FOUND        = 2,
    INVALID_ACCOUNT     = 3,
    SYMBOL_MISMATCH     = 4,
    PARAM_INCORRECT     = 5,
    PAUSED              = 6,
    NO_AUTH             = 7,
    NOT_POSITIVE        = 8,
    NOT_STARTED         = 9,
    OVERSIZED           = 10,
    OVERDRAWN           = 11,
    TRANSFER_SELF       = 12,

};


NTBL("global") global_t {
    name                        admin               = "armoniaadmin"_n;
    uint64_t                    last_slot_id        = 0;
    uint64_t                    last_slot_hid       = 0;
    uint64_t                    last_sft_id         = 0;
   

    typedef eosio::singleton< "global"_n, global_t > singleton;

    EOSLIB_SERIALIZE( global_t, (admin)(last_slot_id)(last_slot_hid)(last_sft_id) )
};

namespace slot_perm {
    static constexpr eosio::name ADMIN         = "admin"_n; // only specified admin accounts can update
    static constexpr eosio::name OWNER         = "owner"_n; // only owner of the token can update
    static constexpr eosio::name NONE          = "none"_n;  // no one can update after creation
};


// Scope: default
TBL slot_key_t {
    name                        title;          //key title
    name                        perm_type       = slot_perm::NONE;      //who can author its value
    set<name>                   admins;         //a set of admin accounts for perm_type that is admin

    slot_key_t() {}
    slot_key_t(const name& t): title(t) {}

    uint64_t primary_key()const  { return title.value; }

    typedef eosio::multi_index< "slotkeys"_n, slot_key_t> idx_t;

    EOSLIB_SERIALIZE( slot_key_t, (title)(perm_type)(admins) )
};

// Scope: default
TBL slot_t {
    uint64_t                    id;             // PK 1:1 associated with slot hash

    name                        owner = name(0);   // owner account, if null it can be shared by multiple accounts
    map<name, string>           properties;     // slot key title -> value (all original formats will be converted into string)
                                                // slot key must be from slot_key table only and its value can only be updated by
                                                // authorized user(s) based on the key slot perm
    string                      meta_uri;       // globally unique uri for token metadata { image, desc,..etc }
    time_point_sec              created_at;

    slot_t() {}
    slot_t(const uint64_t& i): id(i) {}

    checksum256 hash()const {
        string content = "";    //owner shall not be included here
        for (auto const& prop : properties) {
            content += prop.first.to_string() + ":" + prop.second;
        }
        content += meta_uri;
        return HASH256( content );
    }

    uint64_t primary_key()const  { return id; }

    uint64_t by_slot_owner()const { return owner.value; }
    checksum256 by_slot_hash()const { return hash(); } //unique as ID

    typedef eosio::multi_index
    < "slots"_n,  slot_t,
        indexed_by<"slotowner"_n, const_mem_fun<slot_t, uint64_t, &slot_t::by_slot_owner> >,
        indexed_by<"slothash"_n, const_mem_fun<slot_t, checksum256, &slot_t::by_slot_hash> >
    > idx_t;

    EOSLIB_SERIALIZE( slot_t, (id)(owner)(properties)(meta_uri)(created_at) )
};

//Scope: default
TBL slot_hash_t {
    uint64_t                    id;             // PK 1:1 associated with slot hash, hid
    checksum256                 hash;

    slot_hash_t() {}
    slot_hash_t( const uint64_t& i): id(i) {}

    uint64_t primary_key()const  { return id; }
    checksum256 by_slot_hash()const { return hash; } //unique index

    typedef eosio::multi_index
    < "slothash"_n,  slot_hash_t,
        indexed_by<"slothash"_n, const_mem_fun<slot_hash_t, checksum256, &slot_hash_t::by_slot_hash> >
    > idx_t;

    EOSLIB_SERIALIZE( slot_hash_t, (id)(hash) )
};

struct slot_s {
    uint64_t                    id;     //slot id
    uint64_t                    hid;    //slot hash id

    slot_s() {}
    slot_s(const uint64_t& i, const uint64_t& h): id(i),hid(h) {}
    
    string to_string()const { return std::to_string(id) + ":" + std::to_string(hid); }

    friend bool operator==(const slot_s& s1, const slot_s& s2) { 
        return( s1.id == s2.id || s1.hid == s2.hid ); 
    }

    EOSLIB_SERIALIZE( slot_s, (id)(hid) )
};
// bool operator==(const slot_s& s1, const slot_s& s2) { 
//     return( s1.id == s2.id || s1.hash == s2.hash ); 
// }

struct sasset {
    uint64_t                    id;   //SFT token ID
    int64_t                     amount;
    slot_s                      slot;

    sasset() {}
    sasset(const uint64_t& i): id(i) {}
    sasset(const uint64_t& i, const slot_s& s, const int64_t& amt): id(i), slot(s), amount(amt) {}

    string to_string()const { return std::to_string(id) + ":" + std::to_string(amount) + ", slot:" + slot.to_string(); }
    sasset& operator+=(const sasset& quantity) {
        check( quantity.slot == this->slot, "slotids mismatches");
        this->amount += quantity.amount; return *this;
    }
    sasset& operator-=(const sasset& quantity) { 
        check( quantity.slot == this->slot, "slotids mismatches");
        this->amount -= quantity.amount; return *this; 
    }

    bool operator>=(const sasset& sft)const { 
        return( this->amount >= sft.amount );
    }
    bool operator>(const sasset& sft)const { 
        return( this->amount > sft.amount );
    }

    EOSLIB_SERIALIZE( sasset, (id)(amount)(slot) )
};

TBL sft_stats_t {
    sasset                      supply;     //PK: supply.id
    name                        creator;
    time_point_sec              created_at;

    sft_stats_t() {}
    sft_stats_t(const sasset& s): supply(s) {}

    uint64_t primary_key()const { return supply.id; }
    uint64_t by_slot_hid()const { return supply.slot.hid; }

    typedef eosio::multi_index
    < "sftstats"_n, sft_stats_t,
        indexed_by<"slothid"_n, const_mem_fun<sft_stats_t, uint64_t, &sft_stats_t::by_slot_hid> >
    > idx_t;

    EOSLIB_SERIALIZE( sft_stats_t, (supply)(creator)(created_at) )
};

///Scope: owner's account
TBL account_t {
    sasset                      balance;            //diff tokens w unique IDs could share the same asset slot 

    account_t() {}
    account_t(const sasset& asset): balance(asset) {}

    uint64_t primary_key()const     { return balance.id; }
    uint64_t by_slot_hid()const     { return balance.slot.hid; }

    EOSLIB_SERIALIZE( account_t, (balance) )

    typedef eosio::multi_index
    < "accounts"_n, account_t,
        indexed_by<"slothid"_n, const_mem_fun<account_t, uint64_t, &account_t::by_slot_hid> > 
    > idx_t;
};

///Scope: owner's account
// TBL allowance_t{
//     name                        spender;                 // PK
//     map<name, uint64_t>         allowances;              // KV : token_type -> amount

//     allowance_t() {}
//     uint64_t primary_key()const { return spender.value; }

//     EOSLIB_SERIALIZE(allowance_t, (spender)(allowances) )

//     typedef eosio::multi_index< "allowances"_n, allowance_t > idx_t;
// };


} //namespace amax