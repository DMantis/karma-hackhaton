#include <eosiolib/eosio.hpp>
#include <eosiolib/multi_index.hpp>

using namespace eosio;


struct heir_rec {
  uint64_t pkey;
  // heir's name
  account_name name;
  // testator's name
  account_name testator;
  // heir's share in inheritance (percent, int from 0 to 100)
  uint8_t share;

  bool is_testator_dead;

  uint64_t primary_key()const { return pkey; }
  account_name get_testator()const { return testator; }
  uint8_t get_share()const {return share; }

  EOSLIB_SERIALIZE( heir_rec, (pkey)(name)(testator) )
};


struct authority_rec {
  // authoritie's name
  account_name name;
  // reputation of authority (grows with quantity of approved deaths)
  uint64_t reputation;
  uint64_t pledge;

  account_name primary_key()const { return name; }

  EOSLIB_SERIALIZE( authority_rec, (name)(reputation) )
};


struct testator_rec {
  // testator's name
  account_name name;
  // authority, which approved testator death, 0 if testator is alive
  account_name authority;
  account_name primary_key()const { return name; }

  EOSLIB_SERIALIZE( testator_rec, (name) )
};


typedef multi_index<N(heirs), heir_rec, indexed_by< N(bytestator), const_mem_fun<heir_rec, account_name, &heir_rec::get_testator>>> heir_table_type;

static const account_name code_account = N(inheritance_account);


class inheritance : public contract {
  public:
    // using contract::contract;
    inheritance( account_name self )
    :contract(self),authorities_table(self, self){};

    multi_index<N(authorities), authority_rec> authorities_table;
    multi_index<N(testators), testator_rec> testators_table;
    /*
      To check contract
    */
    // @abi action
    void debugfunc( account_name from, account_name to, uint64_t amount ) {
      auto header = "======== debugfunc ========";
      print( header, "\n" );
      print( eosio::name{_self}, "\n" );
      print( "from = ", from, "  to = ", to, "  amount = ", amount, "\n" );
      print( "from = ", eosio::name{from}, "  to = ", eosio::name{to}, "  amount = ", amount, "\n" );
    }

    // @abi_action
    void addheir(account_name testator, account_name heir) {
      auto header = "======== add_heir function ========";
      print(header, "\n" );

      heir_table_type heir_table( code_account, testator );

      eosio_assert(testator == _self, "you can't add heir to another person!");

      heir_table.emplace(testator, [&]( auto& h_rec ) {
        h_rec.pkey = heir_table.available_primary_key();
        h_rec.name = heir;
        h_rec.testator = testator;
        h_rec.share = 0;
        // h_rec.is_testator_dead = false;
      });
    }

    /*
        Claim that account_name is dead (run by one of the heirs)
    */
    // @abi_action
    void claimdead(account_name testator) {
      auto header = "======== claim_dead function ========";
      bool authorised = false;
      print(header, "\n" );

      heir_table_type heir_table( code_account, testator );

      // Nobody but heirs has right to claim that testator is dead!
      auto testator_index = heir_table.template get_index<N(bytestator)>();
      auto testator_itr = testator_index.find(testator);
      // print(testator_itr)
      eosio_assert(testator_itr == heir_table.end(), "is not registered as testator");

      while (testator_itr != heir_table.end() && testator_itr->testator == testator) {
        if (_self != testator_itr->name) {
          testator_itr++;
        }
        else {
          authorised = true;
        }
      }
      eosio_assert(authorised == true, "run by one of the heirs");
      
      heir_table.modify(testator_itr, 0, [&]( auto& h_rec ) {
        h_rec.is_testator_dead = true;
      });

      print(eosio::name{_self}, " claims that ", eosio::name{testator}, " is dead!");
    }
    
    /*
      Claim that account_name owner is alive (run by account_name owner)
    */
    // @abi_action
    void claimalive() { 
      auto header = "======== claim_alive function ========";
      print(header, "\n" );

      eosio_assert( is_dead(eosio::name{_self}) == false, "the guy is actually alive" );
      // todo add check that _self considered dead
      print(eosio::name{_self}, " claims that he is alive!");

      account_name testator_name = eosio::chain::string_to_name(_self);
      auto testator_itr = testators_table.find(testator_name);

      eosio_assert(testator_itr == testators_table.end(), "testator not found");
      auto authority_itr = authorities_table.find(testator_itr->authority);

      eosio_assert(authority_itr == authorities_table.end(), "authority not found");
      // punishment for authority to claim that alive person is dead.
      authorities_table.modify(authority_itr, 0, [&]( auto& a_rec ) {
        a_rec.reputation = 0;
      });

    }

    /*
        Prove that account_name owner is dead (run by witness/autority with rights to do that)  
        and send inheritance to testator's heirs.

        TODO User permissions on ActiveKey
    */
    // @abi_action
    void sendinheritance(account_name testator) {
      auto header = "======== send_inheritance function ========";
      print(header, "\n" );
      // todo permissions check
      // todo majority check

      // ask to permission 
      // action(
      //     permission_level{ from, N(active) },
      //     N(eosio.token), N(transfer),
      //     std::make_tuple(from, _self, testator.balance, std::string(""))
      //  ).send();

      // autothority reward
      authorities_table.modify(authority_itr, 0, [&]( auto& a_rec ) {
        a_rec.reputation += 1;
      });
    }

  private:

    bool is_dead(account_name testator) {

      heir_table_type heir_table( current_receiver(), testator );
      auto testator_index = heir_table.template get_index<N(bytestator)>();
      auto testator_itr = testator_index.find(testator);

      eosio_assert(testator_itr == heir_table.end(), "is not registered as testator");
    }

};

EOSIO_ABI( inheritance, (debugfunc)(claimdead) )


