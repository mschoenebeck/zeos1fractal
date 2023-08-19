#include <eosio/eosio.hpp>
#include <vector>
#include <string>
#include <map>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/transaction.hpp>
#include <eosio/crypto.hpp>
#include <numeric>


using namespace eosio;
using namespace std;

namespace 
{
//6 is maxgroupsize
constexpr std::array<double, 6> polyCoeffs{
    1, 1.618, 2.617924, 4.235801032, 6.85352607, 11.08900518};

// Other helpers
auto fib(uint8_t index) -> decltype(index) { //
  return (index <= 1) ? index : fib(index - 1) + fib(index - 2);
};
}



CONTRACT r4ndomnumb3r : public contract
{
public:
    using contract::contract;

    ACTION generate();
    using generate_action = action_wrapper<"generate"_n, &r4ndomnumb3r::generate>;
};

#define STATE_IDLE 0
#define STATE_PARTICIPATE 1
#define STATE_ROOMS 2

#define NUM_APPROVALS_REQUIRED 5

CONTRACT zeos1fractal : public contract
{
public:

    TABLE rng
    {
        checksum256 value;
    };
    using rng_t = singleton<"rng"_n, rng>;

/*

    TABLE global
    {
        uint64_t state;
        uint64_t event_count;
        uint64_t next_event_block_height;
        uint64_t participate_duration;
        uint64_t rooms_duration;
        //Needed for the REZPECT distribution, if we store it here amount of REZPECT that is being distributed could be increased by simply triggering modify action
        //eg. if fib_offset = 6, then lowest level (Rank 1), gets 8 REZPECT (8 sixth number in the Fibonacci sequence)
        //uint8_t fib_offset; 
        //uint8_t global_meeting_counter;

    };
    singleton<"global"_n, global> _global;
*/   

/*
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
        //vector<uint64_t> recent_respect;  // each element contains weekly REZPECT earned
        //uint8_t meeting_counter;          // shows which element in the vector to adjust
        //uint64_t avg_of_recent_respect    // this determines who gets to the msig


        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"members"_n, member> members_t;
*/

     TABLE globalv2
    {
        uint64_t state;
        uint64_t event_count;
        uint64_t next_event_block_height;
        uint64_t participate_duration;
        uint64_t rooms_duration;
        uint8_t fib_offset; 
        //uint8_t global_meeting_counter;
    };
    singleton<"globalv2"_n, globalv2> _global;

    TABLE memberv2
    {
        name user;                          // EOS account name
        bool is_approved;                   // is user approved member of fractal?
        bool is_banned;                     // is user banned from fractal?
        vector<name> approvers;             // list of member who approved this member
        uint64_t respect;                   // amount of REZPECT
        string profile_why;                 // Why do you want to be part of the ZEOS fractal?
        string profile_about;               // A few words about yourself
        map<name, string> profile_links;    // for instance: twitter.com => @mschoenebeck1 or SSH => ssh-rsa AAAAB3NzaC1yc2E
        //vector<uint64_t> recent_respect;    // each element contains weekly REZPECT earned
        deque<uint64_t> recent_respect;    // each element contains weekly REZPECT earned
        //uint8_t meeting_counter;            // shows which element in the vector to adjust
        uint64_t avg_respect;                // this determines who gets to the msig


        uint64_t primary_key() const { return user.value; }
        uint64_t by_secondary() const { return avg_respect; }
    };
    typedef eosio::multi_index<"memberv2"_n, memberv2,
    eosio::indexed_by<"memberv2"_n, eosio::const_mem_fun<memberv2, uint64_t,&memberv2::by_secondary>>> members_t;    

    TABLE ability
    {
        name name;                          // name of the ability
        uint64_t respect;                   // amount of REZPECT required to unlock
    };
    typedef multi_index<"abilities"_n, ability> abilities_t;


    // event table: to register users for the upcoming event during the PARTICIPATE phase
    TABLE participant
    {
        name user;

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"participants"_n, participant> participants_t;

    // event table: to randomize participants for the ROOMS phase
    TABLE room
    {
        uint64_t id;
        vector<name> users;

        uint64_t primary_key() const { return id; }
    };
    typedef multi_index<"rooms"_n, room> rooms_t;

    // event table: to gather the rankings of all participants of all groups during the ROOMS phase
    TABLE ranking
    {
        name user;
        uint64_t room;
        vector<name> rankings;

        uint64_t primary_key() const { return user.value; }
        uint64_t by_secondary() const { return room; }
    };
    typedef eosio::multi_index<"rankings"_n, ranking, 
    indexed_by<"bysecondary"_n, const_mem_fun<ranking, uint64_t, &ranking::by_secondary>>> rankings_t;

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

    TABLE council
    {
        name delegate;

        int64_t primary_key() const { return delegate.value; }
    };
    typedef eosio::multi_index<"delegates"_n, council> council_t;

    struct permission_level_weight
    {
        permission_level permission;
        uint16_t weight;
    };

    struct wait_weight
    {
        uint32_t wait_sec;
        uint16_t weight;
    };

    struct key_weight
    {
        public_key key;
        uint16_t weight;
    };

    struct authority
    {
        uint32_t threshold;
        std::vector<key_weight> keys;
        std::vector<permission_level_weight> accounts;
        std::vector<wait_weight> waits;
    };

    // Tables related to automatic msig end
    TABLE claim
    {
        extended_asset quantity;
        uint64_t primary_key() const { return quantity.quantity.symbol.code().raw(); }
    };
    typedef eosio::multi_index<"claims"_n, claim> claim_t;


    zeos1fractal(name self, name code, datastream<const char *> ds);

    ACTION init(const uint64_t& first_event_block_height);
    ACTION changestate();
    ACTION setevent(const uint64_t& block_height);
    ACTION signup(const name& user, const string& why, const string& about, const map<name, string>& links);
    ACTION approve(const name& user, const name& user_to_approve);
    ACTION participate(const name& user);
    ACTION submitranks(const name& user, const uint64_t& group_id, const vector<name>& rankings);
    ACTION authenticate(const name& user, const uint64_t& event, const uint64_t& room);
    ACTION testshuffle();

    [[eosio::on_notify("*::transfer")]]
    void assetin(name from, name to, asset quantity, string memo);

private:

    // simple generic swap
    template<typename T>
    void my_swap(T& t1, T& t2)
    {
        T tmp = move(t1);
        t1 = move(t2);
        t2 = move(tmp);
    }

    // an adaption of: https://cplusplus.com/reference/algorithm/shuffle/
    template<class RandomAccessIterator>
    void my_shuffle(RandomAccessIterator first, RandomAccessIterator last)
    {
        for(auto i = (last - first) - 1; i > 0; --i)
        {
            rng_t rndnmbr("r4ndomnumb3r"_n, "r4ndomnumb3r"_n.value);
            checksum256 x = rndnmbr.get().value;
            uint64_t r = *reinterpret_cast<uint64_t*>(&x) % (i+1);
            my_swap(first[i], first[r]);
        }
    }

    void create_groups();
    void check_consensus();
    void distribute_rewards(const vector<vector<name>> &ranks);
    void update_msig();

};
