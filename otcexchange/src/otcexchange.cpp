#include "otcexchange.hpp"
#include <eosio/print.hpp>
#include <algorithm>
#include <cmath>

//#include "nlohmann/json.hpp"
//using namespace nlohmann;

ACTION otcexchange::hi(name nm)
{
   print_f("Name : %\n", nm);
}

ACTION otcexchange::newmarket(const symbol &stock,
                              const symbol &money,
                              asset taker_ask_fee_rate,
                              asset taker_bid_fee_rate,
                              asset maker_ask_fee_rate,
                              asset maker_bid_fee_rate,
                              asset amount_min,
                              asset amount_max,
                              asset price_min,
                              asset price_max,
                              uint32_t pay_timeout,
                              uint32_t self_playcoin_time,
                              uint32_t def_cancel_timeout,
                              uint32_t def_playcoin_timeout,
                              uint8_t cancel_ad_num)
{

   require_auth(get_self()); //必须要求是合约账号的权限

   //入参校验

   check(stock.is_valid(), "invalid symbol name");
   check(money.is_valid(), "invalid symbol name");
   check(taker_ask_fee_rate.symbol.code() == stock.code(), "费率的symbol跟coin的不一致");
   check(taker_bid_fee_rate.symbol.code() == stock.code(), "费率的symbol跟coin的不一致");

   check(maker_ask_fee_rate.symbol.code() == stock.code(), "费率的symbol跟coin的不一致");
   check(maker_bid_fee_rate.symbol.code() == stock.code(), "费率的symbol跟coin的不一致");

   check(taker_ask_fee_rate.symbol == taker_bid_fee_rate.symbol, "精度也要一致");
   check(maker_ask_fee_rate.symbol == maker_bid_fee_rate.symbol, "精度也要一致");
   check(taker_ask_fee_rate.symbol == maker_ask_fee_rate.symbol, "精度也要一致");

   check(amount_max.symbol == stock, "amount的symbol跟coin的不一致");
   check(amount_max.symbol == amount_min.symbol, "2个amount的symbol不一致");

   check(price_max.symbol == money, "price的symbol跟fiat的不一致");
   check(price_max.symbol == price_min.symbol, "2个price的symbol不一致");

   auto zero_stock = ZERO_ASSET(amount_max.symbol);
   auto zero_money = ZERO_ASSET(price_max.symbol);
   auto zero_rate = ZERO_ASSET(taker_ask_fee_rate.symbol);

   check(taker_ask_fee_rate >= zero_rate, ERR_MSG_PARAM_LT_ZERO(50100, taker_ask_fee_rate, ERR_MSG_PARAM_PAIR_TAKER_FEE_RATE_MUST_GE_ZERO));
   check(taker_bid_fee_rate >= zero_rate, ERR_MSG_PARAM_LT_ZERO(50100, taker_bid_fee_rate, ERR_MSG_PARAM_PAIR_TAKER_FEE_RATE_MUST_GE_ZERO));

   check(maker_ask_fee_rate >= zero_rate, ERR_MSG_PARAM_LT_ZERO(50101, maker_ask_fee_rate, ERR_MSG_PARAM_PAIR_MAKER_FEE_RATE_MUST_GE_ZERO));
   check(maker_bid_fee_rate >= zero_rate, ERR_MSG_PARAM_LT_ZERO(50101, maker_bid_fee_rate, ERR_MSG_PARAM_PAIR_MAKER_FEE_RATE_MUST_GE_ZERO));

   check(amount_min > zero_stock, ERR_MSG_PARAM_LE_ZERO(50102, amount_min, ERR_MSG_PARAM_PAIR_AMOUNT_MIN_MUST_GT_ZERO));
   check(amount_max > zero_stock, ERR_MSG_PARAM_LE_ZERO(50103, amount_max, ERR_MSG_PARAM_PAIR_AMOUNT_MAX_MUST_GT_ZERO));

   check(amount_min <= amount_max, ERR_MSG_PARAM_LT(50104, amount_max, amount_min, ERR_MSG_PARAM_PAIR_AMOUNT_MAX_MUST_GE_MIN));

   check(price_min > zero_money, ERR_MSG_PARAM_LE_ZERO(50105, price_min, ERR_MSG_PARAM_PAIR_PRICE_MIN_MUST_GT_ZERO));
   check(price_max > zero_money, ERR_MSG_PARAM_LE_ZERO(50106, price_max, ERR_MSG_PARAM_PAIR_PRICE_MAX_MUST_GT_ZERO));
   check(price_min <= price_max, ERR_MSG_PARAM_LT(50107, amount_max, amount_min, ERR_MSG_PARAM_PAIR_PRICE_MAX_MUST_GE_MIN));

   check(pay_timeout > 0, ERR_MSG_PARAM_LT_ZERO(50108, pay_timeout, ERR_MSG_PARAM_PAIR_PAY_TIMEOUT_MUST_GT_ZERO));
   check(cancel_ad_num > 0, ERR_MSG_PARAM_LT_ZERO(50109, cancel_ad_num, ERR_MSG_PARAM_PAIR_AD_CANCEL_NUM_MUST_GT_ZERO));

   std::string str_pair = std::move(stock.code().to_string() + money.code().to_string());

   symbol_code code_pair = symbol_code(str_pair);

   auto it = markets_.find(code_pair.raw());

   check(it == markets_.end(), ERR_MSG_CHECK_FAILED(50110, str_pair + ERR_MSG_PAIR_HAS_EXISTED)); //exchange pair has exist

   auto state = overview_.get();
   state.fiats.emplace(money.code().to_string());
   state.stocks.emplace(stock.code().to_string());
   state.pairs.emplace(str_pair);
   overview_.set(state, _self);

   markets_.emplace(_self, [&](market &m) {
      m.pair = code_pair;
      m.stock = stock;
      m.money = money;
      m.taker_ask_fee_rate = taker_ask_fee_rate;
      m.taker_bid_fee_rate = taker_bid_fee_rate;
      m.maker_ask_fee_rate = maker_ask_fee_rate;
      m.maker_bid_fee_rate = maker_bid_fee_rate;
      m.amount_min = amount_min;
      m.amount_max = amount_max;
      m.price_min = price_min;
      m.price_max = price_max;
      m.zero_stock = zero_stock;
      m.zero_money = zero_money;
      m.zero_rate = zero_rate;
      m.cancel_ad_num = cancel_ad_num;
      m.pay_timeout = pay_timeout;
      m.self_playcoin_time = self_playcoin_time;
      m.def_playcoin_time = def_playcoin_timeout;
      m.def_cancel_time = def_cancel_timeout;
      m.status = MARKET_STATUS_ON;
      m.str_status = MARKET_STATUS_ON_STR;
      m.nickname = str_pair;
      m.stockname = stock.code().to_string();
      m.moneyname = money.code().to_string();
      m.ctime = time_point_sec(current_time_point().sec_since_epoch());
      m.utime = time_point_sec(current_time_point().sec_since_epoch());
   });
}

ACTION otcexchange::closemarket(const symbol_code &pair)
{
   require_auth(_self); //必须要求是合约账号的权限
   auto it = markets_.find(pair.raw());
   check(it != markets_.end(), ERR_MSG_CHECK_FAILED(50110, pair.to_string().append(ERR_MSG_PAIR_NOT_EXIST)).c_str());
   if (it->status != MARKET_STATUS_OFF)
   {
      markets_.modify(it, _self, [](market &m) {
         m.status = MARKET_STATUS_OFF;
         m.str_status = MARKET_STATUS_OFF_STR;
         m.utime = time_point_sec(current_time_point().sec_since_epoch());
      });
   }
}

ACTION otcexchange::openmarket(const symbol_code &pair)
{
   require_auth(_self); //必须要求是合约账号的权限
   auto it = markets_.find(pair.raw());
   check(it != markets_.end(), ERR_MSG_CHECK_FAILED(50110, pair.to_string().append(ERR_MSG_PAIR_NOT_EXIST)).c_str());
   if (it->status != MARKET_STATUS_ON)
   {
      markets_.modify(it, _self, [](market &m) {
         m.status = MARKET_STATUS_ON;
         m.str_status = MARKET_STATUS_ON_STR;
         m.utime = time_point_sec(current_time_point().sec_since_epoch());
      });
   }
}

ACTION otcexchange::rmmarket(const symbol_code &stock, const symbol_code &money)
{
   require_auth(_self); //必须要求是合约账号的权限+table是合约范围内的
   auto s1 = stock.to_string();
   auto m1 = money.to_string();
   symbol_code pair(s1 + m1);
   auto it = get_market(pair);
   markets_.erase(it);

   auto state = overview_.get();
   state.fiats.erase(m1);
   state.stocks.erase(s1);
   state.pairs.erase(s1 + m1);
   overview_.set(state, _self);

   print("delete one market finish\n");
}

ACTION otcexchange::rmmarkets()
{
   require_auth(_self); //必须要求是合约账号的权限
   erase_markets();     //table是合约范围内的
   overview_.remove();

   print("delete all market finish\n");
}
ACTION otcexchange::rmad(const symbol_code &pair, const std::string &side, uint64_t ad_id)
{
   require_auth(get_self());
   auto adtable_ = get_adtable(pair, side);
   auto it = adtable_.require_find(ad_id, ERR_MSG_AD_NOT_EXIST);
   adtable_.erase(it);
}

ACTION otcexchange::rmads(const symbol_code &pair, const std::string &side)
{
   require_auth(get_self());
   erase_adorders(pair, side);
}

ACTION otcexchange::rmdeal(const symbol_code &pair, uint64_t id)
{
   require_auth(get_self());
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(id, "吃单找不到");
   dealtable_.erase(it);
}

ACTION otcexchange::rmdeals(const symbol_code &pair)
{
   require_auth(get_self());
   erase_deals(pair);
}

ACTION otcexchange::putadorder(const symbol_code &pair,
                               const std::string &side,
                               name user,
                               asset price,
                               asset amount,
                               asset amount_min,
                               asset amount_max,
                               const std::string &source)
{
   require_auth(user);

   auto itr_pair = get_open_market(pair);

   //判断交易价格是否在交易对的配置区间
   check(price > itr_pair->zero_money, ERR_MSG_PARAM_LT_ZERO(50210, price, ERR_MSG_PARAM_AD_PRICE_MUST_GT_ZERO));
   check(price >= itr_pair->price_min, "广告交易价格小于合约配置最低价"); //提示用户卖出最低价不能小于XX
   check(price <= itr_pair->price_max, "广告交易价格大于合约配置最高价"); //提示用户买入最高价不能大于XX

   //判断交易数量是否在交易对配置区间
   check(amount > itr_pair->zero_stock, ERR_MSG_PARAM_LT_ZERO(50211, amount, ERR_MSG_PARAM_AD_AMOUNT_MUST_GT_ZERO));
   check(amount >= itr_pair->amount_min, "广告交易数量小于配置最小交易数量"); //提示用户最小交易数量不能小于XX
   check(amount <= itr_pair->amount_max, "广告交易数量大于配置最大交易数量"); //提示用户最大交易数量不能大于多少

   //判断吃单的交易限额是否合法
   check(amount_min > itr_pair->zero_stock, ERR_MSG_PARAM_LE_ZERO(50212, amount_min, ERR_MSG_PARAM_AD_AMOUNT_MIN_MUST_GT_ZERO));
   check(amount_min <= itr_pair->amount_min, "允许吃单的最小数量不能大于广告的最小委托数量");
   check(amount_max > itr_pair->zero_stock, ERR_MSG_PARAM_LE_ZERO(50213, amount_max, ERR_MSG_PARAM_AD_AMOUNT_MAX_MUST_GT_ZERO));
   check(amount_min <= amount_max, ERR_MSG_PARAM_LE(50215, amount_min, amount_max, ERR_MSG_PARAM_AD_AMOUNT_MAX_MUST_GE_MIN));
   check(amount_max <= amount, ERR_MSG_PARAM_LE(50216, amount_max, amount, ERR_MSG_PARAM_AD_AMOUNT_MUST_GE_AMOUNT_MAX));

   check((side == MARKET_ORDER_SIDE_ASK_STR || side == MARKET_ORDER_SIDE_BID_STR), ERR_MSG_SIDE);

   auto side_int = side_to_uint(side);
   //是卖，判断余额够不够
   if (side_int == MARKET_ORDER_SIDE_ASK)
   {
      freeze_stock(user, name{TOKEN_TEMP_ACCOUNT}, amount, USER_TYPE_OTC, std::string("广告卖币,需要冻结,把钱转到合约帐号:").append(amount.to_string()));

      print("是卖，可用余额足"); //这里要发送一笔转账,也就是先要冻结
   }

   //要注意name类型的长度限制
   std::string scope = pair.to_string() + MARKET_ROLE_MAKER_STR + side;
   transform(scope.begin(), scope.end(), scope.begin(), ::tolower); //转成小写

   adorder_index_t adtable_(_self, name{scope}.value);

   adtable_.emplace(user, [&](adorder &order) {
      order.id = adtable_.available_primary_key();

      order.user = user;                                       //订单所属用户，注意是name类型
      order.pair = pair;                                       //
      order.ctime = current_time_point();                      //订单创建时间，精确到微秒
      order.utime = order.ctime;                               //订单更新时间，精确到微秒
      order.status = AD_STATUS_ONTHESHELF;                     //订单状态，上架中
      order.side = side_int;                                   //买卖类型，1卖 2买
      order.type = MARKET_ORDER_TYPE_LIMIT;                    //订单类型，1限价 2市价
      order.role = MARKET_ROLE_MAKER;                          //订单类型，1挂单 2吃单
      order.price = price;                                     //订单交易价格
      order.amount = amount;                                   //订单交易数量
      order.amount_min = amount_min;                           //订单最小成交数量,默认是2
      order.amount_max = amount_max;                           //订单最小成交数量,默认是2
      order.maker_ask_fee_rate = itr_pair->maker_ask_fee_rate; //吃单的手续费率
      order.maker_bid_fee_rate = itr_pair->maker_bid_fee_rate; //挂单的手续费率
      order.left = amount;                                     //剩余多少数量未成交
      order.freeze = itr_pair->zero_stock;                     //冻结的stock或者money
      order.deal_fee = itr_pair->zero_stock;                   //累计的交易手续费
      order.deal_stock = itr_pair->zero_stock;                 //累计的交易sotck数量
      order.deal_money = itr_pair->zero_money;                 //累计的交易money
      order.source = source;                                   //备注信息，订单来源
   });

   print("广告上架成功");
}

ACTION otcexchange::offad(const symbol_code &pair,
                          const std::string &side,
                          uint64_t ad_id,
                          const std::string &reason)
{
   //如果是卖币的广告单，不能取消，因为买币方可能已经支付了法币

   auto adtable_ = get_adtable(pair, side);
   auto it = adtable_.require_find(ad_id, ORDER_NOT_EXIST_STR);
   auto user = it->user;
   require_auth(user); //鉴权
   check(it->status != AD_STATUS_MAN_OFFTHESHELF, "ad order 已经被手动下架");

   //哪种条件才能取下架合约
   //检查当前的广告下面的吃单最新状态，看是否都完成了或者都取消了
   //转换大小写
   deal_index_t dealtable_(_self, name{lower_str(pair.to_string())}.value);

   for (auto &id : it->vec_deal)
   {
      auto d = dealtable_.require_find(id, "吃单找不到");
      //ad下面挂的吃单都是这几种，才能撤销广告
      check((d->status == DEAL_STATUS_UNPAID_MAN_CANCEL ||
             d->status == DEAL_STATUS_UNPAID_TIMEOUT_CANCEL ||
             d->status == DEAL_STATUS_CANCEL_FINISHED ||
             d->status == DEAL_STATUS_SUCCESS_FINISHED),
            "有正在交易的taker deal,不能下架");
   }

   //是撤销广告单，是卖币方才有解冻
   if (it->side == MARKET_ORDER_SIDE_ASK)
   {
      std::string memo = std::string("卖币广告手动下架,需要解冻,把钱转给用户:");
      memo.append(it->left.to_string());
      unfreeze_stock(name{TOKEN_TEMP_ACCOUNT}, user, it->left, USER_TYPE_OTC, memo);
      print("卖币广告手动下架,需要解冻,把钱转给用户ok");
   }

   adtable_.modify(it, user, [&](adorder &ad) {
      ad.status = AD_STATUS_MAN_OFFTHESHELF; //手动下架
      ad.utime = current_time_point();
      ad.source.append("手动下架").append(reason);
   });

   //todo 用户每手动下架一个广告，记录一次

   print("广告下架成功");
}

ACTION otcexchange::puttkorder(const symbol_code &pair,
                               const std::string &side,
                               name user,
                               asset price,
                               asset amount,
                               uint64_t ad_id,
                               const std::string &source)

{

   require_auth(user);

   auto itr_pair = get_open_market(pair);

   check(price > itr_pair->zero_money, ERR_CHECK_PRICE_GREAT_ZERO);   //要求吃单价>0
   check(amount > itr_pair->zero_stock, ERR_CHECK_AMOUNT_GREAT_ZERO); //吃单数量>0

   //交易对的属性比对

   //校验side是否合法
   check((side == MARKET_ORDER_SIDE_ASK_STR || side == MARKET_ORDER_SIDE_BID_STR), ERR_MSG_SIDE);

   //1.找到对手方订单的买卖的方向
   std::string mk_side("");
   if (side == MARKET_ORDER_SIDE_ASK_STR)
   {
      mk_side = MARKET_ORDER_SIDE_BID_STR;
   }
   else
   {
      mk_side = MARKET_ORDER_SIDE_ASK_STR;
   }

   std::string scope_maker = pair.to_string() + MARKET_ROLE_MAKER_STR + mk_side;
   std::transform(scope_maker.begin(), scope_maker.end(), scope_maker.begin(), ::tolower); //scope要求是小写

   adorder_index_t adtable_(_self, name{scope_maker}.value);
   //要求挂单必须存在
   auto mk_it = adtable_.require_find(ad_id, MAKER_ORDER_NOR_EXIST_STR);
   check(mk_it->status == AD_STATUS_ONTHESHELF, "广告必须是上架中");

   //获取挂单的用户
   const auto &mk_user = mk_it->user;
   //必须不是自成交
   check(mk_user != user, ERR_FORBID_SELF_EXCHANGE); //不能是自成交

   //下单数量必须小于等于广告未交易数量
   check(amount <= mk_it->left, ERR_NOT_ENOUGH_TOKEN_TO_SELL); //提示广告剩余数量不足,请重新下单
   //判断数量是否在广告单的限额内提示用户下单数量应在[]之间
   check(amount >= mk_it->amount_min, "吃单数量小于广告单最小交易数量");
   check(amount <= mk_it->amount_max, "吃单交易数量大于广告单最大交易数量");

   auto tk_side = side_to_uint(side);

   if (tk_side == MARKET_ORDER_SIDE_BID)
   {
      check(price >= mk_it->price, "我出的买价必须>=卖价,才能撮合");
      print("我是买币，我出的买价>=卖家的卖价");
   }
   else
   {

      //我是卖币方,看我是不是还有币可卖,如果有,则直接冻结,这里发送一个内联调用action,如果执行成功，就继续，失败，就不往下走
      check(price <= mk_it->price, "我的卖价必须<=买价,才能撮合");
      print("我是卖币，我出的卖价<=买家的买价");

      //冻结我的币,提示用户您的余额不足
      freeze_stock(user, name{TOKEN_TEMP_ACCOUNT}, amount, USER_TYPE_OTC, std::string("taker 卖币,需要冻结,把钱转到合约帐号:").append(amount.to_string()));

      print("我要卖币给广告方，可用余额够", user.to_string()); //这里要发送一笔转账,也就是先要冻结
   }

   //上面判断都通过，生成一个成交记录，也就是吃单详情,属于合约，scope是交易对

   std::string scope = pair.to_string();
   transform(scope.begin(), scope.end(), scope.begin(), ::tolower);

   deal_index_t dealtable_(_self, name{scope}.value);

   uint64_t deal_id = dealtable_.available_primary_key();
   auto now = current_time_point();

   dealtable_.emplace(user, [&](deal &deal) {
      deal.id = deal_id;
      deal.user = user;    //订单所属用户，注意是name类型
      deal.side = tk_side; //买卖类型，1卖 2买

      deal.type = MARKET_ORDER_TYPE_LIMIT; //订单类型，1限价 2市价
      deal.role = MARKET_ROLE_TAKER;       //订单类型，1挂单 2吃单
      deal.ctime = now;                    //订单创建时间，精确到微秒
      deal.utime = deal.ctime;             //订单更新时间，精确到微秒

      deal.maker_user = mk_user;
      deal.maker_order_id = ad_id;

      deal.price = price;   //订单交易价格
      deal.amount = amount; //订单交易数量

      auto pr = std::pow(10, amount.symbol.precision());

      deal.quota = (price * amount.amount) / pr; //算一下交易额是多少,也就是买币方要支付的法币

      if (tk_side == MARKET_ORDER_SIDE_ASK)
      {                                                                      //假如我是卖币
         deal.ask_fee = amount.amount * (itr_pair->taker_ask_fee_rate) / pr; //2个不同精度的asset能不能互相需要验证一下
         deal.bid_fee = amount.amount * (itr_pair->maker_bid_fee_rate) / pr;
      }
      else
      {
         deal.ask_fee = amount.amount * (itr_pair->maker_ask_fee_rate) / pr;
         deal.bid_fee = amount.amount * (itr_pair->taker_bid_fee_rate) / pr;
      }
      deal.fiat_pay_method = FIAT_PAY_UNKOWN;
      deal.fiat_account = "";

      deal.status = DEAL_STATUS_UNPAID; //待支付状态
      deal.pay_timeout = itr_pair->pay_timeout;
      deal.pay_timeout_sender_id = now.sec_since_epoch();
      deal.self_playcoin_sender_id = 0;
      deal.arb_sender_id = 0;
      deal.pair = pair;
      deal.source = source;
   });

   //修改广告挂单的状态
   adtable_.modify(mk_it, _self, [&itr_pair, &deal_id, &amount, &tk_side](adorder &o) {
      o.vec_deal.emplace_back(deal_id);
      o.source.append("|").append("被吃单");
      o.utime = current_time_point();
      o.left = o.left - amount; //下单成功,减少广告单可交易数量
      //mk是卖币方
      o.freeze = o.freeze + amount; //o.freeze是在处于交易中的币正在等待对方法币支付
      //otcexchange::update_ad_status(o, itr_pair);
      o.update_ad_status(itr_pair);
   });

   //待支付状态,需要发送一个延时事物,法币支付超时自动取消,发送一个延迟事物;取消交易 deal

   transaction t{};

   name who = (tk_side == MARKET_ORDER_SIDE_ASK) ? mk_user : user; //找到谁是买方

   t.actions.emplace_back(
       permission_level(_self, name{"active"}),
       _self,
       name{"defcldeal"},
       std::make_tuple(pair, who, deal_id, DEAL_STATUS_UNPAID_TIMEOUT_CANCEL, "支付超时取消deal"));

   t.delay_sec = itr_pair->pay_timeout;

   t.send(now.sec_since_epoch(), _self);
   printf("Sent with a delay of %d ", t.delay_sec);
}

//法币支付发点击了已支付
ACTION otcexchange::paydeal(const symbol_code &pair, name who, uint64_t deal_id, uint8_t fiat_pay_method, const std::string &fiat_account) //支付
{
   require_auth(who);

   auto str_pair = pair.to_string();
   str_pair = lower_str(str_pair);
   deal_index_t dealtable_(_self, name{str_pair}.value);

   auto it = dealtable_.require_find(deal_id, "吃单找不到");

   //检查一下status是否正确
   check(it->status == DEAL_STATUS_UNPAID, "只有在未支付状态下才能点击法币支付");

   //校验who是买家
   bool who_taker_bid = (who == it->user) && (it->side == MARKET_ORDER_SIDE_BID);
   bool who_maker_bid = (who == it->maker_user) && (it->side == MARKET_ORDER_SIDE_ASK);
   check(who_taker_bid || who_maker_bid, "只有在买方未支付状态下才能手动取消交易");

   dealtable_.modify(it, _self, [&](deal &d) {
      d.status = DEAL_STATUS_PAID_WAIT_PLAYCOIN;
      d.utime = current_time_point();
      d.fiat_pay_method = fiat_pay_method;
      d.fiat_account = fiat_account;
      d.source.append("|点击已支付");
   });
   //取消异步事物
   auto res = cancel_deferred(it->pay_timeout_sender_id);
   print(res, "->1 if transaction was canceled, 0 if transaction was not found");
}

ACTION otcexchange::selfplaycoin(const symbol_code &pair, name who, uint64_t deal_id, const std::string &reason, bool right_now) //放币操作
{

   require_auth(who);

   auto str_pair = pair.to_string();
   str_pair = lower_str(str_pair);
   deal_index_t dealtable_(_self, name{str_pair}.value);

   auto it = dealtable_.require_find(deal_id, "吃单找不到");
   check(it->status == DEAL_STATUS_PAID_WAIT_PLAYCOIN, "只有买方法币支付了，才能放币");

   //校验who是不是卖币者
   bool who_taker_bid = (who == it->user) && (it->side == MARKET_ORDER_SIDE_ASK);
   bool who_maker_bid = (who == it->maker_user) && (it->side == MARKET_ORDER_SIDE_BID);
   check(who_taker_bid || who_maker_bid, "我是卖币者，只能在买方已经支付法币的前提下放币");

   dealtable_.modify(it, _self, [&](deal &d) {
      d.status = DEAL_STATUS_PAID_PLAYCOIN_ING; //把状态改为放币中
   });

   //是否立即放币

   if (right_now)
   {
      commit_deal(who, dealtable_, it, DEAL_STATUS_SUCCESS_FINISHED, reason);
   }
   else
   { //延迟事物放币
      auto now = current_time_point().sec_since_epoch();
      dealtable_.modify(it, _self, [&](deal &d) {
         d.self_playcoin_sender_id = now;
      });

      //放币XX后生效

      transaction t{};

      t.actions.emplace_back(
          permission_level(_self, name{"active"}),
          _self,
          name{"defcmdeal"},
          std::make_tuple(pair, who, deal_id, DEAL_STATUS_SUCCESS_FINISHED, "卖方主动放币，延迟放币完成"));

      auto itr_pair = get_market(pair);

      t.delay_sec = itr_pair->self_playcoin_time;

      t.send(now, _self);

      //print_f("Sent with a delay of %d ", t.delay_sec);
   }
}

//手动取消：买币方待支付->待支付取消完成
//超时取消：买币方待支付->超时待支付取消
//仲裁取消：仲裁中->仲裁取消中->取消完成
//终审取消：仲裁放币/取消->终审取消中->取消完成

void otcexchange::rollback_deal(name who,
                                deal_index_t &dealtable_,
                                deal_iter_t itr_deal,
                                uint8_t status,
                                const std::string &reason)
{
   auto itr_pair = get_open_market(itr_deal->pair);
   auto user = itr_deal->user;          //deal的用户
   auto mk_user = itr_deal->maker_user; //广告主
   std::string mk_side("");
   if (itr_deal->side == MARKET_ORDER_SIDE_ASK)
   {
      mk_side = MARKET_ORDER_SIDE_BID_STR;
   }
   else
   {
      mk_side = MARKET_ORDER_SIDE_ASK_STR;
   }

   auto adtable_ = get_adtable(itr_deal->pair, mk_side);
   auto itr_ad = adtable_.require_find(itr_deal->maker_order_id, "对应的广告订单不存在,无法取消");

   //deal已经取消了,回滚广告单的处理
   adtable_.modify(itr_ad, _self, [&](adorder &ad) {
      ad.left = ad.left + itr_deal->amount;
      ad.freeze = ad.freeze - itr_deal->amount;
      ad.update_ad_status(itr_pair);
      //update_ad_status(ad, itr_pair);
   });

   //deal是买币的,就不用做什么操作了.
   //deal 表
   dealtable_.modify(itr_deal, _self, [&](deal &d) {
      d.status = DEAL_STATUS_CANCEL_FINISHED;
      d.utime = current_time_point();
      d.source.append(reason);

      //手动取消这些是不是重置为0
      //d.quota=itr_pair->zero_money;
      //d.ask_fee = itr_pair->zero_stock;
      //d.bid_fee = itr_pair->zero_stock;
   });
   //deal如果是taker卖币，还需要解冻,如果是广告主是卖币
   if (itr_deal->side == MARKET_ORDER_SIDE_ASK)
   {
      unfreeze_stock(name{TOKEN_TEMP_ACCOUNT}, itr_deal->user, itr_deal->amount, USER_TYPE_OTC, reason);
   }
}

void otcexchange::get_avail_arbiter(std::set<name> &res, int num, const time_point_sec &ctime)
{
   //在线时间优先
   auto atime = arbiters_.get_index<"bytime"_n>();
   auto it = atime.begin();
   auto end = atime.end();
   while (it != end && res.size() < num)
   {
      if (it->online_beg_time <= ctime && it->online_end_time >= ctime)
      {
         res.insert(it->account);
      }
      ++it;
   }
   if (res.size() == num)
   {
      return;
   }

   //有无正在进行的的工作
   auto astatus = arbiters_.get_index<"bystatus"_n>();
   auto it1_beg = astatus.lower_bound(ARBUSER_STATUS_WORKING);
   auto it2_end = astatus.upper_bound(ARBUSER_STATUS_NOTWORKING);
   auto it1 = it1_beg;
   while (it1 != it2_end && res.size() < num)
   {
      res.insert(it1->account);
      ++it1;
   }
   if (res.size() == num)
   {

      return;
   }

   auto awinnum = arbiters_.get_index<"bywinnum"_n>();
   auto it_winnum = awinnum.rbegin();
   while (it_winnum != awinnum.rend() && res.size() < num)
   {
      res.insert(it_winnum->account);
      ++it_winnum;
   }
   if (res.size() == num)
   {
      return;
   }

   auto awinrate = arbiters_.get_index<"bywinrate"_n>();
   auto it_winrate = awinrate.rbegin();
   while (it_winrate != awinrate.rend() && res.size() < num)
   {
      res.insert(it_winrate->account);
      ++it_winrate;
   }

   if (res.size() == num)
   {
      return;
   }
}

void otcexchange::commit_deal(name who,
                              deal_index_t &dealtable_,
                              deal_iter_t itr_deal,
                              uint8_t status,
                              const std::string &reason)
{
   auto user = itr_deal->user;
   name from;
   name to;

   if (itr_deal->side == MARKET_ORDER_SIDE_ASK) //是卖币是taker
   {
      /* code */
      from = itr_deal->user;
      to = itr_deal->maker_user;
   }
   else
   {
      to = itr_deal->user;
      from = itr_deal->maker_user;
   }
   check(who == from, "放币的人搞错了");

   std::string str_mk_side = (itr_deal->side == MARKET_ORDER_SIDE_ASK) ? MARKET_ORDER_SIDE_BID_STR : MARKET_ORDER_SIDE_ASK_STR;
   auto adtable = get_adtable(itr_deal->pair, str_mk_side);
   auto itr_ad = adtable.require_find(itr_deal->maker_order_id, "ad order not exist");

   //发一个转账action
   settle_stock(from, to, itr_deal->amount, itr_deal->bid_fee, "放币啦，卖币方把币打给买币的人，同时卖币方支付一笔手续费");

   print("放币了,把币转给用户ok");

   //修改deal的状态
   dealtable_.modify(itr_deal, user, [&](deal &d) {
      d.status = DEAL_STATUS_SUCCESS_FINISHED; //放币中
      d.utime = current_time_point();
      d.source.append(reason);
   });

   auto itr_pair = get_market(itr_deal->pair);

   //修改广告的状态
   adtable.modify(itr_ad, _self, [&](adorder &ad) {
      ad.utime = current_time_point();
      ad.source.append(reason);
      ad.freeze = ad.freeze - itr_deal->amount;
      ad.update_ad_status(itr_pair);
      //update_ad_status(ad, itr_pair);
   });
}

ACTION otcexchange::mancldeal(const symbol_code &pair, name who, uint64_t deal_id, const std::string &reason)
{
   require_auth(who);
   auto str_pair = pair.to_string();
   str_pair = lower_str(str_pair);
   deal_index_t dealtable_(_self, name{str_pair}.value);
   auto it = dealtable_.require_find(deal_id, "deal id 不存在");

   //检查一下status是否正确
   check(it->status == DEAL_STATUS_UNPAID, "只有在未支付状态下才能手动取消交易");
   bool who_taker_bid = (who == it->user) && (it->side == MARKET_ORDER_SIDE_BID);
   bool who_maker_bid = (who == it->maker_user) && (it->side == MARKET_ORDER_SIDE_ASK);
   check(who_taker_bid || who_maker_bid, "只有在买方未支付状态下才能手动取消交易");
   rollback_deal(who, dealtable_, it, DEAL_STATUS_UNPAID_MAN_CANCEL, reason);
}

//定时任务取消deal
ACTION otcexchange::defcldeal(const symbol_code &pair, name who, uint64_t deal_id, uint8_t status, const std::string &reason)
{
   require_auth(_self);
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "deal id 不存在");

   //检查一下status是否正确
   check(it->status == DEAL_STATUS_UNPAID || it->status == DEAL_STATUS_PAID_ARBIARATE_CANCEL, "只有在未支付状态或者仲裁取消状态下才能取消交易");
   check(status == DEAL_STATUS_UNPAID_TIMEOUT_CANCEL || status == DEAL_STATUS_CANCEL_FINISHED, "status value invaid");
   if (it->status == DEAL_STATUS_UNPAID)
   {
      bool who_taker_bid = (who == it->user) && (it->side == MARKET_ORDER_SIDE_BID);
      bool who_maker_bid = (who == it->maker_user) && (it->side == MARKET_ORDER_SIDE_ASK);
      check(who_taker_bid || who_maker_bid, "只有在买方未支付状态下买方超时取消交易");
   }

   rollback_deal(who, dealtable_, it, status, reason);
}
//定时任务提交deal
ACTION otcexchange::defcmdeal(const symbol_code &pair, name who, uint64_t deal_id, uint8_t status, const std::string &reason)
{
   require_auth(who);
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "deal id 不存在");

   //检查一下status是否正确
   check(it->status == DEAL_STATUS_PAID_PLAYCOIN_ING, "放币中才能放币"); //注意修改
   commit_deal(who, dealtable_, it, status, reason);
}

int otcexchange::update_art(name arbiter, const symbol_code &pair, uint64_t deal_id, uint8_t choice, const std::string &reason)
{

   int ret = 0;
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "吃单找不到");
   //处于仲裁状态
   check(it->status == DEAL_STATUS_PAID_APPEAL_ASK ||
             DEAL_STATUS_PAID_APPEAL_BID ||
             DEAL_STATUS_PAID_APPEAL_ALL ||
             DEAL_STATUS_PAID_ARBIARATE_ING,
         "deal必须处于申诉状态和仲裁中才能发起仲裁");

   if (it->status != DEAL_STATUS_PAID_ARBIARATE_ING)
   {
      dealtable_.modify(it, _self, [&](deal &d) {
         d.status = DEAL_STATUS_PAID_ARBIARATE_ING; //立即更改状态使用户看的到，仲裁中
      });
   }

   arbtask_index_t arbtasks(_self, arbiter.value); //表名，按用户分表，不是按pair分表
   auto it_task = arbtasks.require_find(deal_id, "仲裁task找不到");
   arbtasks.modify(it_task, _self, [&](arbtask &t) {
      t.utime = time_point_sec(current_time_point().sec_since_epoch());
      t.self_arbit = choice; //我自己说NO
   });

   arbadorder_index_t arbs(_self, name{pair.to_string()}.value); //订单表
   auto it_order = arbs.require_find(it_task->arborder_id, "仲裁order找不到");
   auto now = time_point_sec(current_time_point().sec_since_epoch());

   arbs.modify(it_order, _self, [&](arborder &o) {
      o.utime = now;

      if (choice == ARBIT_YES)
      {
         o.yes_num++;
      }
      else if (choice == ARBIT_NO)
      {
         o.no_num++;
      }
      o.map_person_pick[arbiter] = choice;
   });

   it_order = arbs.require_find(it_task->arborder_id, "仲裁order找不到");

   auto yesorno = it_order->yes_num > it_order->no_num ? true : false;
   if (yesorno)
   {
      if (it_order->yes_num >= it_order->vec_arbiter.size() / 2 + 1)
      { //仲裁结果出来了，yes
         dealtable_.modify(it, _self, [&](deal &d) {
            d.status = DEAL_STATUS_PAID_ARBIARATE_PALYCOIN; //立即更改状态使用户看的到，仲裁放币
            d.arb_sender_id = now.sec_since_epoch();
         });

         it_task = arbtasks.require_find(deal_id, "仲裁task找不到");
         arbtasks.modify(it_task, _self, [&](arbtask &t) {
            t.final_arbit = ARBIT_YES; //我自己说NO
         });

         ret = 1;
      }
   }
   else
   {
      if (it_order->no_num >= it_order->vec_arbiter.size() / 2 + 1)
      { //仲裁结果出来了，yes
         dealtable_.modify(it, _self, [&](deal &d) {
            d.status = DEAL_STATUS_PAID_ARBIARATE_CANCEL; //立即更改状态使用户看的到，仲裁放币
            d.arb_sender_id = now.sec_since_epoch();
         });
         it_task = arbtasks.require_find(deal_id, "仲裁task找不到");
         arbtasks.modify(it_task, _self, [&](arbtask &t) {
            t.final_arbit = ARBIT_NO; //我自己说NO
         });
         ret = 2;
      }
   }
   return ret;
}

ACTION otcexchange::arbdeal(name arbiter, const symbol_code &pair, uint64_t deal_id, uint8_t choice, const std::string &reason)
{
   require_auth(arbiter);
   check(choice == ARBIT_YES || choice == ARBIT_NO, "choice value invalid");
   int ret = update_art(arbiter, pair, deal_id, choice, reason);
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "吃单找不到");
   name who = it->side == MARKET_ORDER_SIDE_ASK ? it->user : it->maker_user;

   if (ret == 1)
   {
      transaction t{};
      t.actions.emplace_back(
          permission_level(_self, name{"active"}),
          _self,
          name{"defcmdeal"},
          std::make_tuple(pair, who, deal_id, DEAL_STATUS_SUCCESS_FINISHED, "仲裁放币完成"));

      auto itr_pair = get_market(pair);
      t.delay_sec = itr_pair->def_playcoin_time;
      t.send(it->arb_sender_id, _self);
   }
   else if (ret == 2)
   {
      transaction t{};
      t.actions.emplace_back(
          permission_level(_self, name{"active"}),
          _self,
          name{"defcldeal"},
          std::make_tuple(pair, who, deal_id, DEAL_STATUS_CANCEL_FINISHED, "deal因仲裁取消但是用户又没有发起终审而取消"));

      auto itr_pair = get_market(pair);
      t.delay_sec = itr_pair->def_cancel_time;
      t.send(it->arb_sender_id, _self);
   }
}

//申请终审
ACTION otcexchange::putjudge(name who,
                             const symbol_code &pair,
                             uint64_t deal_id,
                             const std::string &reason)
{
   require_auth(who);
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "吃单找不到");
   check(who == it->user || who == it->maker_user, "复审申请者不是deal参与方");
   bool ing = it->status == DEAL_STATUS_PAID_ARBIARATE_CANCEL || it->status == DEAL_STATUS_PAID_ARBIARATE_PALYCOIN;
   bool ed = it->status == DEAL_STATUS_CANCEL_FINISHED || it->status == DEAL_STATUS_SUCCESS_FINISHED;
   check(ing || ed, "复审时deal的状态必须是仲裁结果执行中或者执行结束");
   //终审只能由失败者发起，检验who是不是失败者
   arbadorder_index_t arbs(_self, name{pair.to_string()}.value); //订单表
   auto it_order = arbs.require_find(deal_id, "仲裁order找不到");
   name ask = (it->side == MARKET_ORDER_SIDE_ASK) ? (it->user) : (it->maker_user);
   name bid = (it->side == MARKET_ORDER_SIDE_BID) ? (it->user) : (it->maker_user);

   name fail_name = (it_order->yes_num > it_order->no_num) ? ask : bid;
   check(who == fail_name, "终审只能由失败者发起");

   //这里改变deal的状态就算完事了。
   dealtable_.modify(it, _self, [&](deal &d) {
      d.status = DEAL_STATUS_PAID_JUDGING; //终审中
   });
}

ACTION otcexchange::judgedeal(name judger, const symbol_code &pair, uint64_t deal_id, uint8_t choice, const std::string &reason)
{
   require_auth(judger);
   check(choice == JUDGE_NO || choice == JUDGE_YES, "judge choice invalid");

   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto it = dealtable_.require_find(deal_id, "吃单找不到");
   check(it->status == DEAL_STATUS_PAID_JUDGING, "只有处于终审中的deal才能执行终审");
   uint8_t status = choice == JUDGE_NO ? DEAL_STATUS_PAID_ARBIARATE_CANCEL : DEAL_STATUS_PAID_ARBIARATE_PALYCOIN;
   dealtable_.modify(it, _self, [&](deal &d) {
      d.utime = time_point_sec(current_time_point().sec_since_epoch());
      d.status = DEAL_STATUS_PAID_JUDGE_CANCEL; //立即更改状态使用户看的到
      d.source.append(reason);                  //你的订单已经被仲裁取消，XX后生效，如有异议，请发起终审
   });
   //如果是
   arbadorder_index_t arbs(_self, name{pair.to_string()}.value); //订单表
   auto it_order = arbs.require_find(deal_id, "仲裁order找不到");

   name ask = (it->side == MARKET_ORDER_SIDE_ASK) ? (it->user) : (it->maker_user);
   name bid = (it->side == MARKET_ORDER_SIDE_BID) ? (it->user) : (it->maker_user);
   bool isyes = (it_order->yes_num > it_order->no_num);

   name fail_name = isyes ? ask : bid;

   if (isyes) //仲裁放币
   {
      if (choice == JUDGE_YES) //终审放币
      {
         print("终审和仲裁的结果是一致的:放币");
      }
      else if (choice == JUDGE_NO)
      {
         //这里需要根据放币是否完成来惩罚仲裁者;
         //仲裁放币，终审者取消，有可能已经把币打过去了
         int rescode = cancel_deferred(it->arb_sender_id);
         if (rescode == 1)
         {
            print("仲裁放币延迟事物取消ok，卖方没有放币，把仲裁者的10%给卖方");
            //币还没有从卖方打给买方：卖方实际上没有收到法币，这个时候要补偿卖方
         }
         else
         {
            print("仲裁放币延迟事物取消失败，卖方已经放币，把仲裁者的50%给卖方");
         }
      }
   }
   else
   {
      if (choice == JUDGE_NO) //终审取消放币
      {
         print("终审和仲裁的结果是一致的:取消放币");
      }
      else if (choice == JUDGE_YES) //
      {
         //仲裁取消，终审者放币，有可能deal已经取消
         int rescode = cancel_deferred(it->arb_sender_id);
         if (rescode == 1)
         {
            print("仲裁取消放币延迟事物取消ok，应该给买方放币，");
         }
         else
         {
            print("取消放币已经完成，没有给买方放币，把仲裁者的50%给买方，");
         }
      }
      //rollback_deal(who, dealtable_, it, DEAL_STATUS_SUCCESS_FINISHED, reason); //把订单状态改为取消完成
   }

   //判断仲裁的延迟事务是不是被执行了
}

ACTION otcexchange::putappeal(name who,
                              const std::string &side,
                              const symbol_code &pair,
                              uint64_t deal_id,
                              const std::string &content,
                              const std::vector<std::string> attachments,
                              const std::string &source)
{
   require_auth(who);

   check((side == MARKET_ORDER_SIDE_ASK_STR || side == MARKET_ORDER_SIDE_BID_STR), ERR_MSG_SIDE);
   check(!content.empty(), "伸诉内容不能为空");
   check(!attachments.empty(), "伸诉上传附件不能为空");

   //查看deal_id 是不是存在
   deal_index_t dealtable_(_self, name{pair.to_string()}.value);
   auto side_uint = side_to_uint(side);
   auto itr_deal = dealtable_.require_find(deal_id, "要申诉的deal不存在");
   check(itr_deal->status == DEAL_STATUS_PAID_WAIT_PLAYCOIN ||
             itr_deal->status == DEAL_STATUS_PAID_ARBIARATE_ING,
         "只有处在已支付待放币状态和仲裁中的deal 才允许买卖双方发起申诉");

   if (who == itr_deal->user) //taker上传材料
   {
      check(itr_deal->side == side_uint, "who 作为taker，side 出错");
   }
   else if (who == itr_deal->maker_user)
   {
      auto maker_side = (itr_deal->side == MARKET_ORDER_SIDE_ASK) ? MARKET_ORDER_SIDE_BID : MARKET_ORDER_SIDE_ASK;
      check(maker_side == side_uint, "who 作为maker，side 出错");
   }
   else
   {
      check(false, "申诉者必须是deal的参与者");
   }

   //先看是否存在这个申诉
   appeal_index_t appeals(_self, name{pair.to_string()}.value);
   auto itr_appeal = appeals.find(deal_id);
   auto now = current_time_point();

   if (itr_appeal == appeals.end())
   {
      //要创建一个deal
      appeals.emplace(_self, [&](appeal &a) {
         a.id = deal_id;
         a.pair = pair;
         a.initiator_side = side_uint;
         a.ctime = time_point_sec(now.sec_since_epoch());
         a.utime = a.ctime;

         if (side_uint == MARKET_ORDER_SIDE_ASK)
         {
            a.ask_user = who;
            a.ask_content = content;
            a.ask_attachments = attachments;
         }
         else
         {
            a.bid_user = who;
            a.bid_content = content;
            a.bid_attachments = attachments;
         }
         a.source.append(source);
      });
      dealtable_.modify(itr_deal, _self, [&side_uint](deal &d) {
         d.status = (side_uint == MARKET_ORDER_SIDE_ASK) ? DEAL_STATUS_PAID_APPEAL_ASK : DEAL_STATUS_PAID_APPEAL_BID;
      });

      //新增一个申诉，这个时候要生成一个仲裁订单，然后这个订单下面又有很多仲裁log，来记录仲裁员的仲裁记录

      std::set<name> arbiters; //分配仲裁员

      auto itr_pair = get_market(pair);
      get_avail_arbiter(arbiters, GROUP_ARBPEOPLE_NUM, time_point_sec(now.sec_since_epoch()));

      arbadorder_index_t arbs(_self, name{pair.to_string()}.value); //订单表

      arbs.emplace(_self, [&](arborder &a) {
         a.id = deal_id;
         a.ctime = time_point_sec(current_time_point().sec_since_epoch());
         a.utime = a.ctime;
         a.pair = pair;
         a.appeal_id = deal_id;
         a.deal_id = deal_id;
         a.initiator_side = side_uint;
         a.amount = itr_deal->amount;
         a.price = itr_deal->price;
         a.quota = itr_deal->quota;
         a.arbitrate_fee = itr_pair->zero_stock;
         a.vec_arbiter = arbiters;
         a.person_num = a.vec_arbiter.size();
         a.yes_num = 0;
         a.no_num = 0;
         a.status = ARBDEAL_STATUS_CREATED;
         a.source.append(source);
      });

      for (auto arbiter : arbiters)
      {
         arbtask_index_t arbtasks(_self, arbiter.value); //表名，按用户分表，不是按pair分表
         arbtasks.emplace(_self, [&](arbtask &log) {
            log.id = arbtasks.available_primary_key();
            log.ctime = time_point_sec(current_time_point().sec_since_epoch());
            log.utime = log.ctime;
            log.pair = pair;
            log.arborder_id = deal_id;
            log.deal_id = deal_id;
            log.appeal_id = deal_id;
            log.self_arbit = ARBIT_UNKOWN;
            log.final_arbit = ARBIT_UNKOWN;
            log.labor_fee = itr_pair->zero_stock;
            log.status = ARBDEAL_STATUS_CREATED;
            log.source = source;
         });
      }
   }
   else //对手房已经上传过申诉材料
   {
      check((itr_appeal->initiator_side == MARKET_ORDER_SIDE_ASK && side_uint == MARKET_ORDER_SIDE_BID) ||
                (itr_appeal->initiator_side == MARKET_ORDER_SIDE_BID && side_uint == MARKET_ORDER_SIDE_ASK),
            "已经发起过申诉");
      //说明之前已经有过申诉，这个应该是对手方的申诉
      appeals.modify(itr_appeal, _self, [&](appeal &a) {
         a.utime = time_point_sec(current_time_point().sec_since_epoch());
         if (itr_appeal->initiator_side == MARKET_ORDER_SIDE_ASK)
         {
            a.bid_user = who;
            a.bid_content = content;
            a.bid_attachments = attachments;
         }
         else
         {
            a.ask_user = who;
            a.ask_content = content;
            a.ask_attachments = attachments;
         }
         a.source.append("对手方上传申诉材料|").append(source);
      });

      if (itr_deal->status != DEAL_STATUS_PAID_ARBIARATE_ING)
      {
         dealtable_.modify(itr_deal, _self, [&side_uint](deal &d) {
            d.status = DEAL_STATUS_PAID_APPEAL_ALL;
         });
      }
   }
}

//自己放币：已付款待支付中->放币中->放币完成
//仲裁放币：仲裁中->仲裁放币中->放币完成
//终审放币：仲裁取消或放币->终审放币中->放币完成

/*
币币交易
资产解冻过程
balance=balance+amount
freeze=freeze-amount

资产冻结过程
freeze=freeze+amount
balance=balance-amount

订单第一次挂在盘口
买stock：money资产冻结freeze=order.price*order.left，更新订单的freeze字段
卖stock：stock资产冻结freeze=order.left，更新订单的freeze字段

订单撤销结束时
买stock：money资产解冻freeze
卖stock：stock资产解冻freeze


撮合过程中要计算（主动方式卖）
price 
amount
deal    = price*amount        // 代表交易额
ask_fee = deal*taker_fee     //  卖币方手续费
bid_fee = amount* maker_fee  //  买方手续费

//1.1-------------taker(假如是卖，主动发起）---------------
left=left-amount
deal_stock=deal_stock+amount //  已经交易的代币数
deal_money=deal_money+deal  //   已经交易的miney数目
deal_fee = deal_fee+ask_fee 

stock：type_availavle减amount
money：type_available加deal减ask_fee

如果taker要放在盘口,走冻结流程


//1.1------------maker(假如是买)----------------
left = left-amount;
freeze = freeze-deal; //这么多的钱不需要冻结，更新冻结字段
deal_stock=deal_stock+amount;
deal_money=deal_money+deal
deal_fee = deal_fee+bid_fee

money：type_freeze减去deal
stock：type_available加amount 2.减bid_fee






撮合过程中要计算（主动方式买）
price 
amount
deal    = price*amount        // 代表交易额
ask_fee = deal*(maker->maker_fee）    //  卖币方手续费
bid_fee = amount* （taker->taker_fee）  //  买方手续费

//1.1-------------taker(假如是买，主动发起）---------------
left=left-amount
deal_stock=deal_stock+amount //  已经交易的代币数
deal_money=deal_money+deal  //   已经交易的miney数目
deal_fee = deal_fee+bid_fee 

stock：type_availavle加amount减bid_fee
money：type_availavle减deal

如果taker要放在盘口,走冻结流程


//1.1------------maker(假如是卖币)----------------
left = left-amount;
freeze = freeze-amount; //这么多的代币不需要冻结，更新冻结字段
deal_stock=deal_stock+amount;
deal_money=deal_money+deal
deal_fee = deal_fee+ask_fee

money：type_available加deal减ask_fee
stock：type_freeze减amount




*/