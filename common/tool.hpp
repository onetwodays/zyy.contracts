#pragma once
#include <string>
#include <eosio/fixed_bytes.hpp>

class tool{
    public:
        static std::string to_hex(const eosio::checksum256 &hashed) {
            // Construct variables
            std::string result;
            static const char *hex_chars = "0123456789abcdef";
            const auto bytes = hashed.extract_as_byte_array();
            // Iterate hash and build result
            for (uint32_t i = 0; i < bytes.size(); ++i) {
                (result += hex_chars[(bytes.at(i) >> 4)]) += hex_chars[(bytes.at(i) & 0x0f)];
            }
            // Return string
            return result;
        }

};

/*
#include <eosiolib/eosio.token.hpp>   // right path to eosio.token.hpp file EOS合约内查询代币余额
void getBalance(account_name owner){
    eosio::token t(N(eosio.token));
    const auto sym_name = eosio::symbol_type(S(4,EOS)).name();
    const auto my_balance = t.get_balance(N(owner), sym_name );
    eosio::print("My balance is ", my_balance);
}

// 数据库中只保留最新的100条记录，旧纪录删除,最多100条
besthistory_tables besthistory_table(_self, _self.value);
total_count = 0;
auto itr_besthistory = besthistory_table.rbegin();
while(itr_besthistory != besthistory_table.rend()){
    if(total_count >= 99){
        auto itr = besthistory_table.erase(--itr_besthistory.base());
        itr_besthistory = std::reverse_iterator(itr);
    } else {
        total_count++;
        itr_besthistory++;
    }
}
*/



/*

using map_market_t  =  std::map<std::string,market>;
using set_title_t   =  std::set<std::string>; //stock money pair

TABLE  markets{
    map_market_t map_markets;
    set_title_t  set_titles;
    EOSLIB_SERIALIZE( markets, ( map_markets)(set_titles) )
};
using singleton_markets_t  = singleton< "markets"_n,markets>;
singleton_markets_t sg_markets; //定义了单例类,整个

static const name sg_markets_scope;



//初始化单例
inline void init_otc(){
    if(!sg_markets.exists()){
    sg_markets.get_or_create(_self,markets());
    }
}
*/

