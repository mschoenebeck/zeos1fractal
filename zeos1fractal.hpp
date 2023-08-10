#include <eosio/eosio.hpp>
#include <vector>
#include <string>
#include <map>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>

using namespace eosio;
using namespace std;

#define STATE_IDLE 0
#define STATE_REGISTRATION 1
#define STATE_BREAKOUT_ROOMS 2

CONTRACT zeos1fractal : public contract
{
    public:

    TABLE global
    {
        uint64_t state;
        uint64_t event_count;
        uint64_t next_event_block_height;
        uint64_t registration_duration;
        uint64_t breakout_rooms_duration;
    };
    singleton<"global"_n, global> _global;

    TABLE member
    {
        name user;                          // EOS account name
        bool is_approved;                   // is user approved member of fractal?
        bool is_banned;                     // is user banned from fractal?
        vector<name> approvers;             // list of member who approved this member
        uint64_t respect;                   // amount of REZPECT
        string profile_why;                 // Why do you want to be part of the ZEOS fractal?
        string profile_about;               // A few words about yourself
        map<name, string> profile_links;    // for instance: twitter.com => @mschoenebeck1 or SSH => ssh-rsa AAAAB3NzaC1yc2E

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"members"_n, member> members_t;

    TABLE ability
    {
        name name;                          // name of the ability
        uint64_t respect;                   // amount of REZPECT required to unlock
    };
    typedef multi_index<"abilities"_n, ability> abilities_t;

    // event table: to register users for the upcoming event during the REGISTRATION phase
    TABLE participant
    {
        name user;

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"participants"_n, participant> participants_t;

    // event table: to randomize participants for the BREAKOUT_ROOMS phase
    TABLE group
    {
        uint64_t id;
        vector<name> users;

        uint64_t primary_key() const { return id; }
    };
    typedef multi_index<"groups"_n, group> groups_t;

    // event table: to gather the rankings of all participants of all groups during the BREAKOUT_ROOMS phase
    TABLE ranking
    {
        name user;
        uint64_t group_id;
        vector<name> rankings;

        uint64_t primary_key() const { return user.value; }
        uint64_t by_secondary() const { return group_id; }
    };
    typedef multi_index<"rankings"_n, ranking> rankings_t;

    // all rewards that get distributed in the upcoming event
    TABLE reward
    {
        extended_asset quantity;

        uint64_t primary_key() const { return quantity.quantity.symbol.raw(); }
    };
    typedef multi_index<"rewards"_n, reward> rewards_t;

    // long term treasury which only the owner-msig has access to
    TABLE treasury
    {
        extended_asset quantity;

        uint64_t primary_key() const { return quantity.quantity.symbol.raw(); }
    };
    typedef multi_index<"treasury"_n, treasury> treasury_t;

    zeos1fractal(name self, name code, datastream<const char *> ds);

    ACTION init(const uint64_t& first_event_block_height);
    ACTION changestate();
    ACTION setevent(const uint64_t& block_height);
    ACTION apply(const name& user, const string& why, const string& about, const vector<tuple<name, string>>& links);
    ACTION approve(const name& user, const name& user_to_approve);
    ACTION participate(const name& user);
    ACTION submitranks(const name& user, const uint64_t& group_id, const vector<name> &rankings);

    [[eosio::on_notify("*::transfer")]]
    void assetin(name from, name to, asset quantity, string memo);
};
