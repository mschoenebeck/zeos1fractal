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

constexpr array<uint32_t, 33> CONSENSUS {
    0, 1, 2, 2, 3, 3, 4, 5, 5, 6,  7,  7,  8,  9,  9, 10, 11, 11, 12, 13, 13, 14, 15, 15, 16, 17, 17, 18, 19, 19, 20, 21, 21
//  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
};

auto fib(uint8_t index) -> decltype(index)
{
    return (index <= 1) ? index : fib(index - 1) + fib(index - 2);
};

CONTRACT r4ndomnumb3r : public contract
{
public:
    using contract::contract;

    ACTION generate(const uint64_t& salt);
    using generate_action = action_wrapper<"generate"_n, &r4ndomnumb3r::generate>;
};

#define STATE_IDLE 0
#define STATE_PARTICIPATE 1
#define STATE_ROOMS 2

CONTRACT zeos1fractal : public contract
{
public:

    TABLE rng
    {
        checksum256 value;
    };
    using rng_t = singleton<"rng"_n, rng>;

     TABLE global
    {
        uint64_t state;
        uint64_t event_count;
        uint64_t next_event_block_height;
        uint64_t event_interval;
        uint64_t participate_duration;
        uint64_t rooms_duration;
        uint8_t fib_offset;
        uint8_t council_size;
        uint8_t num_approvals_required;
        uint8_t min_num_participants;
    };
    singleton<"global"_n, global> _global;

    TABLE member
    {
        name user;                          // EOS account name
        bool is_approved;                   // is user approved member of fractal?
        bool is_banned;                     // is user banned from fractal?
        vector<name> approvers;             // list of member who approved this member
        deque<uint64_t> recent_respect;     // each element contains weekly REZPECT earned
        uint64_t total_respect;             // total amount of REZPECT
        string profile_why;                 // Why do you want to be part of the ZEOS fractal?
        string profile_about;               // A few words about yourself
        map<name, string> profile_links;    // for instance: twitter.com => @mschoenebeck1 or SSH => ssh-rsa AAAAB3NzaC1yc2E

        uint64_t primary_key() const { return user.value; }
    };
    typedef multi_index<"members"_n, member> members_t;

    TABLE ability
    {
        name name;                  // name of the ability
        uint64_t total_respect;     // amount of REZPECT required to unlock
        double average_respect;     // average respect required to unlock

        uint64_t primary_key() const { return name.value; }
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
    typedef multi_index<"rankings"_n, ranking, indexed_by<"bysecondary"_n, const_mem_fun<ranking, uint64_t, &ranking::by_secondary>>> rankings_t;

    // all rewards that get distributed in the upcoming event
    TABLE reward
    {
        extended_asset quantity;

        uint64_t primary_key() const { return quantity.quantity.symbol.code().raw(); }
    };
    typedef multi_index<"rewards"_n, reward> rewards_t;

    TABLE council
    {
        name delegate;

        int64_t primary_key() const { return delegate.value; }
    };
    typedef multi_index<"council"_n, council> council_t;

    TABLE claim
    {
        extended_asset quantity;

        uint64_t primary_key() const { return quantity.quantity.symbol.code().raw(); }
    };
    typedef multi_index<"claims"_n, claim> claims_t;

    // eosio.token tables
    TABLE account
    {
        asset balance;

        uint64_t primary_key() const { return balance.symbol.code().raw(); }
    };
    typedef multi_index<"accounts"_n, account> accounts_t;

    TABLE currency_stats
    {
        asset supply;
        asset max_supply;
        name issuer;

        uint64_t primary_key() const { return supply.symbol.code().raw(); }
    };
    typedef multi_index<"stat"_n, currency_stats> stats_t;

    TABLE respectrew 
    {
    asset quantity;
    };
    typedef singleton<"respectrew"_n, respectrew> _respectrew;

    zeos1fractal(name self, name code, datastream<const char *> ds);
    ACTION init(
        const uint64_t& first_event_block_height,
        const uint64_t& event_interval,
        const uint64_t& participate_duration,
        const uint64_t& rooms_duration,
        const uint8_t& fib_offset,
        const uint8_t& council_size,
        const uint8_t& num_approvals_required,
        const uint8_t& min_num_participants
    );
    ACTION reset();
    ACTION changestate();
    ACTION setevent(const uint64_t& block_height);
    ACTION seteventint(const uint64_t& event_interval);
    ACTION setability(const name& ability_name, const uint64_t& total_respect, const double& average_respect);
    ACTION setprtcptdur(const uint64_t& participate_duration);
    ACTION setroomsdur(const uint64_t& rooms_duration);
    ACTION setcouncilsz(const uint8_t& council_size);
    ACTION setnumappreq(const uint8_t& num_approvals_required);
    ACTION setminnumprt(const uint8_t& min_num_participants);
    ACTION signup(const name& user, const string& why, const string& about, const map<name, string>& links);
    ACTION approve(const name& user, const name& user_to_approve);
    ACTION deletemember(const name& user);
    ACTION participate(const name& user);
    ACTION submitranks(const name& user, const uint64_t& room, const vector<name>& rankings);
    ACTION authenticate(const name& user, const uint64_t& event, const uint64_t& room);
    ACTION updatereward(const asset& quantity, const name& contract);
    ACTION claimrewards(const name& user);
    ACTION banuser(const name& user);
    ACTION transfer(const name& from, const name& to, const asset& quantity, const string& memo);
    ACTION issue(const name& to, const asset& quantity, const string& memo);
    ACTION create(const name& issuer, const asset& maximum_supply);
    ACTION setrespect(const asset& quantity);

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
    void my_shuffle(RandomAccessIterator first, RandomAccessIterator last, uint32_t seed)
    {
        for(auto i = (last - first) - 1; i > 0; --i)
        {
            uint32_t r = seed % (i+1);
            my_swap(first[i], first[r]);
            // source: https://stackoverflow.com/a/11946674/2340535
            seed = (seed * 1103515245U + 12345U) & 0x7fffffffU;
        }
    }

    // a simple generic "shell sort" implementation
    // borrowed from: https://www.softwaretestinghelp.com/sorting-techniques-in-cpp/#Shell_Sort
    template <typename T, typename F>
    void my_sort(T arr[], int n, F predicate)
    {
        for(int gap = n/2; gap > 0; gap /= 2)
        {
            for(int i = gap; i < n; i += 1)
            {
                T tmp = arr[i];
                int j;
                for(j = i; j >= gap && predicate(tmp, arr[j - gap]); j -= gap)
                {
                    arr[j] = arr[j - gap];
                }
                arr[j] = tmp;
            }
        }
    }

    void create_groups(vector<name>& participants);
    vector<vector<name>> check_consensus();
    void distribute_rewards(const vector<vector<name>> &ranks);
    void determine_council();
    void cleartables();
    void delbalance(const name& user);
    void add_balance(const name& owner, const asset& value, const name& ram_payer);
    void sub_balance(const name& owner, const asset& value);
    void add_respect_rewards();
};
