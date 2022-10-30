#include <amax.stoken/amax.stoken.hpp>
#include "utils.hpp"

namespace amax {

void stoken::addslotkey( const name& app, const name& title, const name& perm_type, const set<name>& admins ) {
   require_auth( _gstate.admin );

   auto slotkey = slot_key_t( title );
   CHECKC( !_db.get( app.value, slotkey ), err::RECORD_FOUND, "slot key already defined" )

   slotkey.perm_type = perm_type;
   slotkey.admins    = admins;

   _db.set( app.value, slotkey );

}

void stoken::addslot( const name& app, const name& owner, const string& meta_uri, const map<name, string>& props ) {
   require_auth( _gstate.admin );

   auto slot = slot_t( ++ _gstate.last_slot_id );
   CHECKC( !_db.get( app.value, slot ), err::RECORD_FOUND, "slot already defined" )
   CHECKC( meta_uri.size() <= max_uri_size, err::OVERSIZED, "meta uri length > 1024" )
   CHECKC( props.size() <= max_prop_size, err::OVERSIZED, "props size > 64" )

   slot.owner        = owner;
   slot.properties   = props;
   slot.meta_uri     = meta_uri;
   slot.created_at   = current_time_point();

   auto slots = slot_t::idx_t(_self, app.value);
   auto idx = slots.get_index<"slothash"_n>();
   auto itr = idx.find( slot.hash() );
   CHECKC( itr == idx.end(), err::RECORD_FOUND, "slot with the same hash found" )

   _db.set( app.value, slot );

}

void stoken::setslotprop( const name& signer, const name& app, const uint64_t& slot_id, const name& prop_key, const string& prop_value ) {
   require_auth( signer );

   auto slot = slot_t( slot_id );
   CHECKC( _db.get( app.value, slot ), err::RECORD_NOT_FOUND, "slot not defined" )
   CHECKC( slot.properties.count( prop_key ), err::RECORD_NOT_FOUND, "prop key not found: " + prop_key.to_string() )

   auto slotkey = slot_key_t( prop_key );
   CHECKC( _db.get( app.value, slotkey ), err::RECORD_NOT_FOUND, "slot key not defined" )

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
   _db.set( app.value, slot );

}

void stoken::create( const name& signer, const name& app_name, const uint64_t& asset_id, const uint64_t& slot_id, const int64_t& maximum_supply )
{
   require_auth( signer );

   check( is_account(signer), "signer account does not exist" );
   check( maximum_supply > 0, "max-supply must be positive" );

   auto slotidx         = slot_t::idx_t( _self, app_name.value );
   auto slotitr         = slotidx.find( slot_id );
   check( slotitr != slotidx.end(), "slot not found: " + to_string(slot_id) );
   auto slot            = slot_s( slot_id, slotitr->hash() );
   auto stats           = sft_stats_t::idx_t( _self, _self.value );
   auto id              = asset_id;

   if (id > 0)
      check( stats.find( asset_id ) == stats.end(), "sasset with the same ID already exists: " + to_string(asset_id) );
   else
      id                = ++ _gstate.last_asset_id/*stats.available_primary_key() */;

   int64_t zero_supply  = 0;
   stats.emplace( signer, [&]( auto& s ) {
      s.supply          = sasset( id, slot, zero_supply );
      s.max_supply      = sasset( id, slot, maximum_supply );
      s.creator         = signer;
      s.created_at      = current_time_point();
   });
}

void stoken::issue( const name& to, const sasset& quantity, const string& memo )
{
   require_auth( to );
   check( memo.size() <= 256, "memo has more than 256 bytes" );

   auto asset_id        = quantity.id;
   auto stats           = sft_stats_t::idx_t( _self, _self.value );
   auto itr             = stats.find( asset_id );
   check( itr != stats.end(), "asset does not exist, create sft asset before issue" );
   const auto& st = *itr;
   check( to == st.creator, "SFT assets can only be issued to creator account" );
   check( quantity.amount > 0, "must issue positive quantity" );
   check( quantity.id == st.supply.id, "asset ID mismatch" );
   check( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

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

void stoken::transfer( const name& from, const name& to, const sasset& quantity, const string& memo  )
{
   check( from != to, "cannot transfer to self" );
   require_auth( from );
   check( is_account( to ), "to account does not exist");
   check( memo.size() <= max_memo_size, "memo has more than 256 bytes" );
   auto payer = has_auth( to ) ? to : from;

   require_recipient( from );
   require_recipient( to );

   auto stats = sft_stats_t::idx_t( _self, _self.value );
   const auto& st = stats.get( quantity.id );
   check( quantity.amount > 0, "must transfer positive quantity" );

   sub_balance( from, quantity );
   add_balance( to, quantity, payer );
}

void stoken::sub_balance( const name& owner, const sasset& value ) {
   auto from_acnts = account_t::idx_t( get_self(), owner.value );

   const auto& from = from_acnts.get( value.id, "no balance object found" );
   check( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
      a.balance -= value;
   });
}

void stoken::add_balance( const name& owner, const sasset& value, const name& ram_payer )
{
   auto to_acnts = account_t::idx_t( get_self(), owner.value );
   auto to = to_acnts.find( value.id );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });

   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} //namespace amax