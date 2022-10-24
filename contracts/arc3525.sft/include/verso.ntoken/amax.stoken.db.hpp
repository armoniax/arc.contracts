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

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())
#define TBL struct [[eosio::table, eosio::contract("amax.stoken")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.stoken")]]

NTBL("global") global_t {
    uint64_t                    last_slot_key_id    = 0;
    uint64_t                    last_slot_id        = 0;
    uint64_t                    allowance_max_size  = 100;

    typedef eosio::singleton< "global"_n, global_t > global_singleton;

    EOSLIB_SERIALIZE( global_t, (last_slot_key_id)(last_slot_id)(allowance_max_size) )
};

//slot_id must be created prior to creation of this ssymbol
struct ssymbol {
    uint32_t slot_id;         //LEFT: token slot ID. one token ID can only belong to one slot ID
    uint32_t token_id;        //RIGHT: token ID

    ssymbol() {}
    ssymbol(const uint64_t& id): slot_id(id >> 32), token_id( (id << 32) >> 32 ) {}
    ssymbol(const uint32_t& sid, const uint32_t& tid): slot_id(sid), token_id(tid) {}

    friend bool operator==(const ssymbol&, const ssymbol&);
    uint64_t raw()const { return( (uint64_t) slot_id << 32 | token_id ); } 

    EOSLIB_SERIALIZE( ssymbol, (slot_id)(token_id) )
};

bool operator==(const ssymbol& symb1, const ssymbol& symb2) { 
    return( symb1.slot_id == symb2.slot_id && symb1.token_id == symb2.token_id ); 
}

namespace slot_perm {
    static constexpr eosio::name ADMIN         = "admin"_n; // only specified admin accounts can update
    static constexpr eosio::name OWNER         = "owner"_n; // only owner of the token can update
    static constexpr eosio::name ALL           = "all"_n;   // everyone can udpate
};


// Scope: application (name type)
TBL slot_key_t {
    uint64_t                    id;             //slot key ID
    name                        title;          //key title
    name                        author_type;    //who can author its value
    set<name>                   admin_authors;  //a set of admin accounts for author_type that is admin

    slot_key_t() {}
    slot_key_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const  { return id; }

    typedef eosio::multi_index
    < "slotkeys"_n,  slot_key_t,
    > idx_t;

    EOSLIB_SERIALIZE( slot_key_t, (id)(title)(author_type)(admin_authors) )
}

/**
 * @brief   a slot shall be created prior to generation of a new token
 *          and a new token ID must be created and assigned while the previous one must be emptied
 *          upon change/update of slot key-value and its generated hash value for one existing token
 * 
 */
// Scope: application (name type)
TBL slot_t {
    uint64_t                    id;             // 1:1 associated with slot hash
    map<uint64_t, string>       properties;     // slot key ID -> value (all original formats will be converted into string)
                                                // slot key ID must be from slot_key table only and its value can only be updated by
                                                // authorized user(s) based on the key slot perm
    string                      meta_uri;       // globally unique uri for token metadata { image, desc,..etc }
    time_point_sec              created_at;

    slot_t() {}
    slot_t(const uint64_t& i): id(i) {}

    checksum256 hash() {
        string content = "";
        for (auto const& prop : properties) {
            if (content != "") content += ",";
            content += prop.first.to_string() + ":" + prop.second;
        }
        return HASH256( content );
    }

    uint64_t primary_key()const  { return id; }

    checksum256 by_slot_hash()const { return slot.hash(); }

    typedef eosio::multi_index
    < "slots"_n,  slot_t,
        indexed_by<"slothash"_n,       const_mem_fun<slot_t, checksum256, &slot_t::by_slot_hash> >,
    > idx_t;

    EOSLIB_SERIALIZE( slot_t, (id)(properties)(meta_uri)(created_at) )
}
struct sft_asset {
    int64_t         amount;
    ssymbol         symbol;

    sft_asset() {}
    sft_asset(const uint64_t& id): symbol(id) {}
    sft_asset(const uint32_t& tid, const uint32_t& sid): symbol(tid, sid), amount(0) {}
    sft_asset(const uint32_t& tid, const uint32_t& sid, const int64_t& am): symbol(tid, sid), amount(am) {}
    sft_asset(const int64_t& amt, const ssymbol& s): amount(amt), symbol(s) {}

    sft_asset& operator+=(const sft_asset& quantity) { 
        check( quantity.symbol.slot_id == this->symbol.slot_id, "ssymbol slot mismatch");
        this->amount += quantity.amount; return *this;
    } 
    sft_asset& operator-=(const sft_asset& quantity) { 
        check( quantity.symbol.slot_id == this->symbol.slot_id, "ssymbol slot mismatch");
        this->amount -= quantity.amount; return *this; 
    }

    EOSLIB_SERIALIZE( sft_asset, (amount)(symbol) )
};

TBL sft_stats_t {
    sft_asset       supply;         // PK: symbol.raw()
    sft_asset       max_supply;     // 1 means NFT-721 type
    name            creator;         // who created/uploaded/issued this NFT
    time_point_sec  created_at;
    bool            paused;

    sft_stats_t() {};
    sft_stats_t(const uint64_t& id): supply(id) {};
    
    uint64_t primary_key()const     { return supply.symbol.raw(); }
    uint64_t by_slot_id()const      { return supply.symbol.slot_id; }

    typedef eosio::multi_index
    < "tokenstats"_n,  sft_stats_t,
        indexed_by<"slotidx"_n,         const_mem_fun<sft_stats_t, uint64_t, &sft_stats_t::by_slot_id> >
    > idx_t;

    EOSLIB_SERIALIZE(sft_stats_t,  (supply)(max_supply)(creator)(created_at)(paused) )
};

///Scope: owner's account
TBL account_t {
    sft_asset                   balance;            //diff tokens w unique IDs could share the same asset slot 
    bool                        paused = false;     //when true it can no longer be transferred

    account_t() {}
    account_t(const sft_asset& asset): balance(asset) {}

    uint64_t primary_key()const { return balance.symbol.raw(); }
    uint64_t by_slot_id()const  { return balance.slot_id; }

    EOSLIB_SERIALIZE(account_t, (balance)(paused) )

    typedef eosio::multi_index
    < "accounts"_n, account_t,
        indexed_by<"slotidx"_n      const_mem_fun<account_t, uint64_t, &account_t::by_slot_id> 
    > idx_t;
};

///Scope: owner's account
TBL allowance_t{
    name                   spender;                 // PK
    map<name, uint64_t>    allowances;              // KV : token_type -> amount

    allowance_t() {}
    uint64_t primary_key()const { return spender.value; }

    EOSLIB_SERIALIZE(allowance_t, (spender)(allowances) )

    typedef eosio::multi_index< "allowances"_n, allowance_t > idx_t;
};


} //namespace amax