#include "zeos1fractal.hpp"

zeos1fractal::zeos1fractal(name self, name code, datastream<const char *> ds)
    : contract(self, code, ds), _global(_self, _self.value) {}

void zeos1fractal::init(const uint64_t &first_event_block_height) {
  require_auth(_self);
  check(first_event_block_height >
            static_cast<uint64_t>(current_block_number()),
        "first event must take place in the future");

  _global.set(
      {
          STATE_IDLE, 0, first_event_block_height,
          5400, // 45 min
          1800, // 15 min
      },
      _self);
}

void zeos1fractal::changestate() {
  // anyone can execute this action
  check(_global.exists(), "contract not initialized");
  auto g = _global.get();
  uint64_t cbn = static_cast<uint64_t>(current_block_number());

  switch (g.state) {
  case STATE_IDLE: {
    check(cbn >= (g.next_event_block_height - g.participate_duration),
          "too early to move into REGISTRATION state");
    g.state = STATE_PARTICIPATE;
  } break;

  case STATE_PARTICIPATE: {
    check(cbn >= g.next_event_block_height,
          "too early to move into ROOMS state");
    g.state = STATE_ROOMS;

    // TODO:
    // randomize users in "participants" into groups for breakout rooms
  } break;

  case STATE_ROOMS: {
    check(cbn >= (g.next_event_block_height + g.rooms_duration),
          "too early to move into IDLE state");
    g.state = STATE_IDLE;
    g.next_event_block_height =
        g.next_event_block_height + 1209600; // add one week of blocks
    g.event_count++;

    // TODO:
    // evaluate rankings table and distribute REZPECT
    // reset event tables
  } break;
  }

  _global.set(g, _self);
}

void zeos1fractal::setevent(const uint64_t &block_height) {
  require_auth(_self);
  check(_global.exists(), "contract not initialized");

  // TODO
}

void zeos1fractal::signup(const name &user, const string &why,
                          const string &about,
                          const vector<tuple<name, string>> &links) {
  require_auth(user);

  // TODO
}

void zeos1fractal::approve(const name &user, const name &user_to_approve) {
  require_auth(user);

  // TODO
}

void zeos1fractal::participate(const name &user) {
  require_auth(user);
  check(_global.exists(), "contract not initialized");
  auto g = _global.get();
  check(g.state == STATE_PARTICIPATE, "can only join during PARTICIPATE phase");

  members_t members(_self, _self.value);
  auto usr = members.find(user.value);
  check(usr != members.end(), "user is not a member");
  check(usr->is_approved, "user is not approved");

  participants_t participants(_self, _self.value);
  auto p = participants.find(user.value);
  check(p == participants.end(), "user is already participating");

  participants.emplace(_self, [&](auto &row) { row.user = user; });
}

void zeos1fractal::submitranks(const name &user, const uint64_t &group_id,
                               const vector<name> &rankings) {
  require_auth(user);

  // TODO
}

void zeos1fractal::authenticate(const name &user, const uint64_t &event,
                                const uint64_t &room) {
  check(false, "action not executable");
}

// notification handler for FT deposits
void zeos1fractal::assetin(name from, name to, asset quantity, string memo) {
  // TODO: which token contracts are allowed? (make sure scam tokens aren't able
  // to block a particular token symbol) actually having a table for the symbol
  // -> contract mapping would begood to have. I would use the same table (a
  // "valid symbol" table) for the zeos protocol & DEX as well (include only
  // tokens based on what the community whitelists)

  name first_receiver = get_first_receiver();
  if (first_receiver == "atomicassets"_n) {
    return;
  }
  if (to == _self) {
    if (memo == "treasury") {
      treasury_t treasury(_self, _self.value);
      auto asset = treasury.find(quantity.symbol.raw());
      if (asset == treasury.end()) {
        treasury.emplace(_self, [&](auto &row) {
          row.quantity = extended_asset(quantity, first_receiver);
        });
      } else {
        treasury.modify(asset, _self, [&](auto &row) {
          row.quantity += extended_asset(quantity, first_receiver);
        });
      }
    } else // all transfers other than type 'treasury' go into 'rewards'
    {
      rewards_t rewards(_self, _self.value);
      auto asset = rewards.find(quantity.symbol.raw());
      if (asset == rewards.end()) {
        rewards.emplace(_self, [&](auto &row) {
          row.quantity = extended_asset(quantity, first_receiver);
        });
      } else {
        rewards.modify(asset, _self, [&](auto &row) {
          row.quantity += extended_asset(quantity, first_receiver);
        });
      }
    }
  }
}

void zeos1fractal::testshuffle()
{
    vector<uint64_t> v = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    my_shuffle(v.begin(), v.end());
    string s = "";
    for(auto e : v)
    {
        s += to_string(e) + ", ";
    }
    check(0, s);
}