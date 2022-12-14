#include <amax.stoken/amax.stoken.hpp>
#include "utils.hpp"

namespace amax {

void stoken::addslotkey( const name& title, const name& perm_type, const set<name>& admins ) {
   require_auth( _gstate.admin );

   auto slotkey = slot_key_t( title );
   CHECKC( !_db.get( slotkey ), err::RECORD_FOUND, "slot key already defined" )

   slotkey.perm_type = perm_type;
   slotkey.admins    = admins;

   _db.set( slotkey );

}

void stoken::addslot( const name& owner, const string& meta_uri, const map<name, string>& props ) {
   require_auth( _gstate.admin );

   auto slot = slot_t( ++ _gstate.last_slot_id );
   CHECKC( !_db.get( slot ), err::RECORD_FOUND, "slot already defined" )
   CHECKC( meta_uri.size() <= max_uri_size, err::OVERSIZED, "meta uri length > 1024" )
   CHECKC( props.size() <= max_prop_size, err::OVERSIZED, "props size > 64" )

   slot.owner        = owner;
   slot.properties   = props;
   slot.meta_uri     = meta_uri;
   slot.created_at   = current_time_point();

   auto slots        = slot_t::idx_t(_self, _self.value);
   auto idx          = slots.get_index<"slothash"_n>();
   auto itr          = idx.find( slot.hash() );
   CHECKC( itr == idx.end(), err::RECORD_FOUND, "slot with the same hash found" )
   _db.set( slot );

   auto slothash     = slot_hash_t( slot.id );
   slothash.hash     = slot.hash();
   _db.set( slothash );

}

void stoken::setslotprop( const name& signer, const uint64_t& slot_id, const name& prop_key, const string& prop_value ) {
   require_auth( signer );

   auto slot = slot_t( slot_id );
   CHECKC( _db.get( slot ), err::RECORD_NOT_FOUND, "slot not defined" )
   CHECKC( slot.properties.count( prop_key ), err::RECORD_NOT_FOUND, "prop key not found: " + prop_key.to_string() )

   auto slotkey = slot_key_t( prop_key );
   CHECKC( _db.get( slotkey ), err::RECORD_NOT_FOUND, "slot key not defined" )

   switch( slotkey.perm_type.value ) {
      case slot_perm::ADMIN.value: {
         CHECKC( slotkey.admins.count( signer ), err::NO_AUTH, "signer is not slotkey admin" )
         break;
      }
      case slot_perm::OWNER.value: {
         CHECKC( slot.owner == signer, err::NO_AUTH, "signer is not slot owner" )
         break;
      }
      default: CHECKC(false, err::NO_AUTH, "perm type invalid" )
   }

   slot.properties[ prop_key ] = prop_value;
   _db.set( slot );

}

void stoken::create( const name& signer, const uint64_t& asset_id, const uint64_t& slot_id, const int64_t& maximum_supply )
{
   require_auth( signer );

   check( is_account(signer), "signer account does not exist" );
   check( maximum_supply > 0, "max-supply must be positive" );

   auto slot            = slot_t( slot_id );
   CHECKC( _db.get( slot ), err::RECORD_NOT_FOUND, "slot not found: " + to_string(slot_id) )

   auto slothashes      = slot_hash_t::idx_t(_self, _self.value );
   auto slothashidx     = slothashes.get_index<"slothash"_n>();
   auto slothashitr     = slothashidx.find( slot.hash() );
   CHECKC( slothashitr != slothashidx.end(), err::RECORD_NOT_FOUND, "slot hash not found" )

   auto id              = asset_id;
   auto slotids         = slot_s( slot_id, slothashitr->id );
   auto stats           = sft_stats_t::idx_t( _self, _self.value );
   auto statitr         = stats.find( asset_id );

   if (id > 0)
      CHECKC( statitr == stats.end(), err::RECORD_FOUND, "sasset with the same ID already exists: " + to_string(asset_id) )
   else
      id                = ++ _gstate.last_sft_id/*stats.available_primary_key() */;

   int64_t zero_supply  = 0;
   stats.emplace( signer, [&]( auto& s ) {
      s.supply          = sasset( id, slotids, zero_supply );
      s.creator         = signer;
      s.created_at      = current_time_point();
   });
}

void stoken::issue( const name& to, const sasset& quantity, const string& memo )
{
   require_auth( to );
   check( memo.size()   <= 256, "memo has more than 256 bytes" );

   auto asset_id        = quantity.id;
   auto stats           = sft_stats_t::idx_t( _self, _self.value );
   auto itr             = stats.find( asset_id );
   check( itr != stats.end(), "asset does not exist, create sft asset before issue" );
   const auto& st = *itr;
   check( to == st.creator, "SFT assets can only be issued to creator account" );
   check( quantity.amount > 0, "must issue positive quantity" );
   check( quantity.id == st.supply.id, "asset ID mismatch" );

   stats.modify( st, same_payer, [&]( auto& s ) {
      s.supply += quantity;
   });

   add_balance( st.creator, quantity, st.creator );
}

void stoken::retire( const sasset& quantity, const string& memo )
{
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto stats = sft_stats_t::idx_t( _self, _self.value );
   auto itr = stats.find( quantity.id );
   check( itr != stats.end(), "SFT asset with asset ID does not exist" );
   const auto& st = *itr;
   require_auth( st.creator );
   check( quantity.amount > 0, "must retire positive quantity" );
   check( quantity.id == st.supply.id, "SFT asset ID mismatch" );

   stats.modify( st, same_payer, [&]( auto& s ) {
      s.supply -= quantity;
   });

   sub_balance( st.creator, quantity );
}

/**
 * @brief - all cases:
 *       1. slot has NO owner
 *          1.1 to has no common slot
 *             1.1.1 full transfer - move nft ID 
 *             1.1.2 partial transfer - generate a new NFT ID
 *          1.2 to has a common slot
 *             1.2.1 full transfer - 
 *             1.2.2 partial transfer
 * 
 *       2. slot has an owner
 *          - create a new slot & SFT asset
 *          - move the new slot asset into receivers account
 * 
 *  @memo - format: merge:$sft_id
 */
void stoken::transfer( const name& from, const name& to, const sasset& quantity, const string& memo )
{
   require_auth( from );

   CHECKC( from != to, err::TRANSFER_SELF, "cannot transfer to self" )
   CHECKC( is_account( to ), err::INVALID_ACCOUNT, "to account does not exist" )
   CHECKC( memo.size()  <= max_memo_size, err::OVERSIZED, "memo has more than 256 bytes" )
   auto payer           = has_auth( to ) ? to : from;

   require_recipient( from );
   require_recipient( to );

   uint64_t to_merge_sft_id = 0;
   if (memo != "") {
      vector<string_view> memo_params = split(memo, ":");
      if (memo_params.size() == 2 && memo_params[0] == "merge") {
         to_merge_sft_id = to_uint64( memo_params[1], "to merge SFT ID" );
      }
   }

   auto now             = current_time_point();
   auto stats           = sft_stats_t::idx_t( _self, _self.value );
   const auto& st       = stats.get( quantity.id );
   CHECKC( quantity.amount > 0, err::NOT_POSITIVE, "must transfer positive quantity" )

   auto from_acnt       = account_t( quantity.id );
   CHECKC( _db.get( from.value, from_acnt ), err::RECORD_NOT_FOUND, "from sasset not found: " + to_string( quantity.id ) )
   
   auto slot            = slot_t( quantity.slot.id );
   CHECKC( _db.get( slot ), err::RECORD_NOT_FOUND, "slot not found" )

   
   sasset new_sft       = quantity;
   if (slot.owner != name(0)) { //must create a new slot & new SFT
      auto new_slot     = slot;
      create_new_slot( to, new_slot );
      create_new_sft( from, new_slot, new_sft );
      
   } else { //no slot owner, hence no need to create a new slot
      if (from_acnt.balance > quantity) { //partial transfer
         create_new_sft( from, slot, new_sft );
      }
   }

   sub_balance( from, quantity );

   /// must check if SFT can be merged
   add_balance( to, new_sft, same_payer );

}

inline void stoken::create_new_slot(const name& new_owner, slot_t& new_slot) {
   new_slot.id          = ++_gstate.last_slot_id;
   new_slot.owner       = new_owner;
   new_slot.created_at  = current_time_point();

   _db.set( new_slot );
}

inline void stoken::create_new_sft( const name& creator, const slot_t& new_slot, sasset& new_sft ) {
   auto slothashes      = slot_hash_t::idx_t( _self, _self.value );
   auto slothashidx     = slothashes.get_index<"slothash"_n>();
   if (slothashidx.find( new_slot.hash() ) == slothashidx.end()) {
      slothashes.emplace( creator, [&]( auto& row ){
         row.id         = ++_gstate.last_slot_hid;
         row.hash       = new_slot.hash();
      });
   }

   new_sft.id           = ++_gstate.last_sft_id; //hid remains the same
   new_sft.slot         = slot_s( new_slot.id, _gstate.last_slot_hid );
   auto stat            = sft_stats_t( new_sft );
   stat.creator         = creator;
   stat.created_at      = current_time_point();

   _db.set( stat );
}

void stoken::sub_balance( const name& owner, const sasset& value ) {
   auto from_acnts      = account_t::idx_t( get_self(), owner.value );
   const auto& from     = from_acnts.get( value.id, "no balance object found" );
   CHECKC( from.balance >= value, err::OVERDRAWN, "overdrawn balance" )

   from_acnts.modify( from, owner, [&]( auto& a ) {
      a.balance         -= value;
   });
}

void stoken::add_balance( const name& owner, const sasset& value, const name& ram_payer )
{
   auto to_acnts        = account_t::idx_t( get_self(), owner.value );
   auto to              = to_acnts.find( value.id );

   if( to != to_acnts.end() ) {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance       += value;
      });

      return;
   }

   auto slothids        = to_acnts.get_index<"slothid"_n>();
   auto acct_itr        = slothids.find( value.slot.hid );
   if( acct_itr == slothids.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
         a.balance      = value;
      });

   } else { //found a common slot, hence merging with it
      to_acnts.modify( *acct_itr, same_payer, [&]( auto& a ) {
        a.balance       += value;
      });
   }
}

} //namespace amax