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
        int64_t state;
        uint64_t event_count;
        uint64_t next_event_block_height;

        // Settings/Configuration
        map<name, uint64_t> respect_level;
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
        uint64_t rezpect_balance;           // amount of REZPECT

        // profile information
        string profile_why;                 // Why do you want to be part of the ZEOS fractal?
        string profile_about;               // A few words about yourself
        map<name, string> profile_links;    // for instance: twitter.com => @mschoenebeck1 or SSH => ssh-rsa AAAAB3NzaC1yc2E

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"members"_n, member> members_t;

    TABLE participant
    {
        name user;

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"participants"_n, participant> participants_t;

    TABLE group
    {
        uint64_t id;
        vector<name> users;

        uint64_t primary_key() const { return id; }
    };
    typedef multi_index<"groups"_n, group> groups_t;

    TABLE ranking
    {
        name user;
        uint64_t group_id;
        vector<name> rankings;

        uint64_t primary_key() const { return user.value; }
        uint64_t by_secondary() const { return group_id; }
    };
    typedef multi_index<"rankings"_n, ranking> rankings_t;

    zeos1fractal(name self, name code, datastream<const char *> ds);

    ACTION init(const uint64_t& first_event_block_height);
    ACTION changestate();
    ACTION setevent(const uint64_t& block_height);
    ACTION apply(const name& user, const string& why, const string& about, const vector<tuple<name, string>>& links);
    ACTION approve(const name& user, const name& user_to_approve);
    ACTION participate(const name& user);
    ACTION submitranks(const name& user, const uint64_t& group_id, const vector<name> &rankings);
};
