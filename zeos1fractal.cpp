#include "zeos1fractal.hpp"
#include <cmath>

zeos1fractal::zeos1fractal(
    name self,
    name code,
    datastream<const char *> ds
) : contract(self, code, ds),
    _global(_self, _self.value)
{}

void zeos1fractal::init(const uint64_t &first_event_block_height)
{
    require_auth(_self);
    check(first_event_block_height == 0 || first_event_block_height > static_cast<uint64_t>(current_block_number()), "first event must take place in the future");

    _global.set({
        STATE_IDLE,
        0,
        first_event_block_height == 0 ? current_block_number() + 500 : first_event_block_height,
        360,    //  3 min
        1200,   // 10 min
        3,      // fib offset, I suggest to set it to 3, which produces min RESPECT of 2 and max RESPECT of 21 (that's how it is in fractally whitepaper too)
    }, _self);

    abilities_t abilities(_self, _self.value);
    auto abilities_itr = abilities.begin();
    while(abilities_itr != abilities.end())
    {
        abilities_itr = abilities.erase(abilities_itr);
    }

    // Define a struct inside the action for better readability
    struct ability_info {
        name ability_name;
        uint64_t total_respect;
        double avg_respect;
    };

    // Define initial abilities
    vector<ability_info> initial_abilities = {
        {"delegate"_n, 0, 0},
        {"approver"_n, 0, 0}
    };

    // Add initial abilities to the table
    for (const auto& ability : initial_abilities)
    {
        abilities.emplace(_self, [&](auto &row) {
            row.name = ability.ability_name;
            row.total_respect = ability.total_respect;
            row.average_respect = ability.avg_respect;
        });
    }

    asset initial_supply = asset(0, symbol("REZPECT", 0));
    asset max_supply = asset(asset::max_amount, symbol("REZPECT", 0));

    stats_t statstable(_self, initial_supply.symbol.code().raw());

    auto stat_itr = statstable.find(initial_supply.symbol.code().raw());
    check(stat_itr == statstable.end(), "Token with symbol already initialized in stats table");

    statstable.emplace(_self, [&](auto &row) {
        row.supply = initial_supply;
        row.max_supply = max_supply;
        row.issuer = _self;
    });
}

void zeos1fractal::init2()
{
    require_auth(_self);

    _global.set({
        STATE_IDLE,
        0,
        current_block_number() + 500,
        240,    //  2 min
        600,    //  5 min
        3,      // fib offset, I suggest to set it to 3, which produces min RESPECT of 2 and max RESPECT of 21 (that's how it is in fractally whitepaper too)
    }, _self);

    abilities_t abilities(_self, _self.value);

    // Define a struct inside the action for better readability
    struct ability_info {
        name ability_name;
        uint64_t total_respect;
        double avg_respect;
    };

    // Define initial abilities
    vector<ability_info> initial_abilities = {
        {"delegate"_n, 9, 1.58},
        {"approver"_n, 9, 1.58}
    };

    // Add initial abilities to the table
    for (const auto& ability : initial_abilities)
    {
        auto it = abilities.find(ability.ability_name.value);
        abilities.modify(it, _self, [&](auto &row) {
            row.total_respect = ability.total_respect;
            row.average_respect = ability.avg_respect;
        });
    }

    members_t members(_self, _self.value);
    auto it = members.find("mschoenebeck"_n.value);
    members.modify(it, _self, [&](auto &row) {
        row.total_respect = 0;
        row.recent_respect = deque<uint64_t>();
        for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
    });
    it = members.find("geztomzxguge"_n.value);
    members.modify(it, _self, [&](auto &row) {
        row.total_respect = 0;
        row.recent_respect = deque<uint64_t>();
        for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
    });
    it = members.find("vladislav.x"_n.value);
    members.modify(it, _self, [&](auto &row) {
        row.total_respect = 0;
        row.recent_respect = deque<uint64_t>();
        for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
    });

    cleartables();
}

void zeos1fractal::changestate()
{
    // anyone can execute this action
    check(_global.exists(), "contract not initialized");
    auto g = _global.get();
    uint64_t cbn = static_cast<uint64_t>(current_block_number());

    switch(g.state)
    {
        case STATE_IDLE:
        {
            check(cbn >= (g.next_event_block_height - g.participate_duration), "too early to move into REGISTRATION state");
            g.state = STATE_PARTICIPATE;
        }
        break;

        case STATE_PARTICIPATE:
        {
            check(cbn >= g.next_event_block_height, "too early to move into ROOMS state");
            g.state = STATE_ROOMS;

            participants_t participants(_self, _self.value);
            vector<name> event_participants;

            // Iterate over each participant in the table and add each participant's name to the event_participants vector
            for (const auto& participant : participants)
            {
                event_participants.push_back(participant.user);
            }
            if(event_participants.size() < 3)
            {
                g.state = STATE_IDLE;
                g.next_event_block_height = g.next_event_block_height + 500; // add one week of blocks
                cleartables();
            }
            else
            {
                create_groups(event_participants);
            }
        }
        break;

        case STATE_ROOMS:
        {
            check(cbn >= (g.next_event_block_height + g.rooms_duration), "too early to move into IDLE state");
            g.state = STATE_IDLE;
            //g.next_event_block_height = g.next_event_block_height + 1209600; // add one week of blocks
            g.next_event_block_height = current_block_number() + 500;
            g.event_count++;

            distribute_rewards(check_consensus());
            determine_council();
            cleartables();
        }
        break;
    }

    _global.set(g, _self);
}

void zeos1fractal::setevent(const uint64_t& block_height)
{
    require_auth(_self);
    check(_global.exists(), "contract not initialized");
    check(block_height > current_block_number(), "must be in the future");
    auto g = _global.get();
    check(g.state == STATE_IDLE, "only in IDLE state");
    g.next_event_block_height = block_height;
    _global.set(g, _self);
}

void zeos1fractal::setability(
    const name& ability_name, 
    const uint64_t& total_respect, 
    const double& average_respect
)
{
    require_auth(_self); 

    abilities_t abilities(_self, _self.value);

    auto existing_ability = abilities.find(ability_name.value);
    
    if (existing_ability != abilities.end()) 
    {
        abilities.modify(existing_ability, _self, [&](auto &row) {
            row.total_respect = total_respect;
            row.average_respect = average_respect;
        });
    }
    else
    {
        abilities.emplace(_self, [&](auto &row) {
            row.name = ability_name;
            row.total_respect = total_respect;
            row.average_respect = average_respect;
        });
    }
}

void zeos1fractal::signup(
    const name& user,
    const string& why,
    const string& about,
    const map<name, string>& links
)
{
    require_auth(user);
    check(_global.exists(), "contract not initialized");
    members_t members(_self, _self.value);
    auto usr = members.find(user.value);
    check(usr == members.end(), "user already signed up");

    members.emplace(user, [&](auto &row) {
        row.user = user;
        row.is_approved = false;
        row.is_banned = false;
        row.approvers = vector<name>();
        row.total_respect = 0;
        row.profile_why = why;
        row.profile_about = about;
        row.profile_links = links;
        row.recent_respect = deque<uint64_t>();
        for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
    });
}

void zeos1fractal::approve(
    const name &user,
    const name &user_to_approve
)
{
    require_auth(user);
    check(_global.exists(), "contract not initialized");
    members_t members(_self, _self.value);
    auto usr_to_appr = members.find(user_to_approve.value);
    check(usr_to_appr != members.end(), "user_to_approve doesn't exist");

    if(user == _self)
    {
        // the fractal can approve anybody instantly
        members.modify(usr_to_appr, _self, [&](auto &row) {
            row.approvers.push_back(_self);
            row.is_approved = true;
        });
    }
    else if(user == user_to_approve)
    {
        // after enough approvals have been received, each user eventually has to approve himself
        check(usr_to_appr->approvers.size() >= NUM_APPROVALS_REQUIRED, "not enough approvals");
        members.modify(usr_to_appr, _self, [&](auto &row) {
            row.is_approved = true;
        });
    }
    else
    {

        // check if user has already approved
        auto already_approved = std::find(usr_to_appr->approvers.begin(), usr_to_appr->approvers.end(), user);
        check(already_approved == usr_to_appr->approvers.end(), "You have already approved this user.");

        // any authorized member can approve new members
        auto usr = members.find(user.value);
        check(usr != members.end(), "user doesn't exist");
        check(usr->is_approved, "user is not approved");
        check(!usr->is_banned, "user is banned");

        // Lookup the ability requirements for approving from the abilities table
        abilities_t abilities(_self, _self.value);
        auto ability_itr = abilities.find("approver"_n.value);
        check(ability_itr != abilities.end(), "approver ability not found");

        // Check if user has enough total respect
        check(usr->total_respect >= ability_itr->total_respect, "not enough total respect");

        // Calculate the average recent respect of the user
        double user_avg_recent_respect = static_cast<double>(accumulate(usr->recent_respect.begin(), usr->recent_respect.end(), 0)) / 12.0;

        // Check if user has enough average recent respect
        check(user_avg_recent_respect >= ability_itr->average_respect, "not enough average respect");

        // If both conditions are met, add user to the approvers list
        members.modify(usr_to_appr, _self, [&](auto &row) {
            row.approvers.push_back(user);
        });
    }
}

void zeos1fractal::participate(const name &user)
{
    require_auth(user);
    check(_global.exists(), "contract not initialized");
    auto g = _global.get();
    check(g.state == STATE_PARTICIPATE, "can only join during PARTICIPATE phase");

    members_t members(_self, _self.value);
    auto usr = members.find(user.value);
    check(usr != members.end(), "user is not a member");
    check(usr->is_approved, "user is not approved");
    check(!usr->is_banned, "user is banned");

    participants_t participants(_self, _self.value);
    auto p = participants.find(user.value);
    check(p == participants.end(), "user is already participating");

    participants.emplace(_self, [&](auto &row) { row.user = user; });
}

void zeos1fractal::submitranks(
    const name &user,
    const uint64_t &room,
    const vector<name> &rankings
)
{
    require_auth(user);

    // Check whether user is a part of a group he is submitting consensus for.
    rooms_t rooms(_self, _self.value);
    const auto &room_iter = rooms.get(room, "No group with such ID.");
    bool exists = false;
    for (const name& room_user : room_iter.users) 
    {
        if (room_user == user)
        {
            exists = true;
            break;
        }
    }

    check(exists, "Your account name not found in group");
    check(rankings.size() == room_iter.users.size(), "Rankings size must match the number of users in the group.");
    check(rankings.size() >= 3, "too small group");
    check(rankings.size() <= 6, "too big group");

    // Check if all the accounts in the rankings are part of the room
    for (const name& ranked_user : rankings)
    {
        auto iter = find(room_iter.users.begin(), room_iter.users.end(), ranked_user);
        check(iter != room_iter.users.end(), ranked_user.to_string() + " not found in the group.");
    }

    // Check that no account is listed more than once in rankings
    set<name> unique_ranked_accounts(rankings.begin(), rankings.end());
    check(rankings.size() == unique_ranked_accounts.size(), "Duplicate accounts found in rankings.");

    for (const name& ranked_user : rankings)
    {
        string rankname = ranked_user.to_string();
        check(is_account(ranked_user), rankname + " account does not exist.");
    }

    rankings_t rankings_table(_self, _self.value);

    if (rankings_table.find(user.value) == rankings_table.end()) 
    {
        rankings_table.emplace(user, [&](auto &row) {
            row.user = user;
            row.room = room;
            row.rankings = rankings;
        });
    }
    else
    {
        check(false, "You can vote only once my friend.");
    }
}

void zeos1fractal::authenticate(
    const name &user,
    const uint64_t &event,
    const uint64_t &room)
{
    check(false, "action not executable");
}

// notification handler for FT deposits
void zeos1fractal::assetin(
    name from,
    name to,
    asset quantity,
    string memo
)
{
    // TODO: which token contracts are allowed? (make sure scam tokens aren't able
    // to block a particular token symbol) actually having a table for the symbol
    // -> contract mapping would begood to have. I would use the same table (a
    // "valid symbol" table) for the zeos protocol & DEX as well (include only
    // tokens the community whitelists)

    name first_receiver = get_first_receiver();
    if(first_receiver == "atomicassets"_n)
    {
        return;
    }
    if(to == _self)
    {
        if(memo == "rewards")
        {
            rewards_t rewards(_self, _self.value);
            auto asset = rewards.find(quantity.symbol.raw());
            if(asset == rewards.end())
            {
                rewards.emplace(_self, [&](auto &row) {
                    row.quantity = extended_asset(quantity, first_receiver);
                });
            }
            else
            {
                rewards.modify(asset, _self, [&](auto &row) {
                    row.quantity += extended_asset(quantity, first_receiver);
                });
            }
        }
    }
}

void zeos1fractal::create_groups(vector<name>& participants)
{
    // Shuffle the participants vector
    rng_t rndnmbr("r4ndomnumb3r"_n, "r4ndomnumb3r"_n.value);
    checksum256 x = rndnmbr.get().value;
    uint32_t seed = *reinterpret_cast<uint32_t*>(&x);
    my_shuffle(participants.begin(), participants.end(), seed);

    rooms_t rooms(_self, _self.value);
    auto num_participants = participants.size();

    vector<uint8_t> group_sizes;

    // First part: hardcoded groups up to 20
    if (num_participants <= 20)
    {
        if (num_participants == 3) group_sizes = {3};
        else if (num_participants == 4) group_sizes = {4};
        else if (num_participants == 5) group_sizes = {5};
        else if (num_participants == 6) group_sizes = {6};
        else if (num_participants == 7) group_sizes = {3, 4};
        else if (num_participants == 8) group_sizes = {4, 4};
        else if (num_participants == 9) group_sizes = {5, 4};
        else if (num_participants == 10) group_sizes = {5, 5};
        else if (num_participants == 11) group_sizes = {5, 6};
        else if (num_participants == 12) group_sizes = {6, 6};
        else if (num_participants == 13) group_sizes = {5, 4, 4};
        else if (num_participants == 14) group_sizes = {5, 5, 4};
        else if (num_participants == 15) group_sizes = {5, 5, 5};
        else if (num_participants == 16) group_sizes = {6, 5, 5};
        else if (num_participants == 17) group_sizes = {6, 6, 5};
        else if (num_participants == 18) group_sizes = {6, 6, 6};
        else if (num_participants == 19) group_sizes = {5, 5, 5, 4};
        else if (num_participants == 20) group_sizes = {5, 5, 5, 5};
    }

    // Second part: generic algorithm for 20+ participants
    else
    {
        // As per the constraint, we start with groups of 6
        uint8_t count_of_fives = 0;
        while (num_participants > 0)
        {
            if (num_participants % 6 != 0 && count_of_fives < 5)
            {
                group_sizes.push_back(5);
                num_participants -= 5;
                count_of_fives++;
            }
            else
            {
                group_sizes.push_back(6);
                num_participants -= 6;
                if (count_of_fives == 5) count_of_fives = 0;
            }
        }
    }

    // Create the actual groups using group_sizes
    auto iter = participants.begin();

    uint64_t room_id = rooms.available_primary_key();
    // Ensure we start from 1 if table is empty
    if (room_id == 0)
    {
        room_id = 1;
    }

    // Iterate over the group sizes to create and populate rooms
    for (uint8_t size : group_sizes)
    {
        // A temporary vector to store the users in the current room
        vector<name> users_in_room;

        // Iterate until the desired group size is reached or until all participants have been processed
        for (uint8_t j = 0; j < size && iter != participants.end(); j++, ++iter)
        {
            // Add the current participant to the users_in_room vector
            users_in_room.push_back(*iter);
        }

        // Insert a new room record into the rooms table
        rooms.emplace(_self, [&](auto& r) {
            // Assign a unique ID to the room and prepare for the next room's ID
            r.id = room_id++;
            // Assign the users_in_room vector to the current room's user list
            r.users = users_in_room;
        });
    }
}

vector<vector<name>> zeos1fractal::check_consensus()
{
    // Access the rankings table (fuck it let's delete rankings after each election, we don't need it)
    rankings_t rankings_table(_self, _self.value);

    // Use a secondary index to sort by room
    auto idx = rankings_table.get_index<"bysecondary"_n>();

    // Store results of consensus rankings
    vector<vector<name>> all_rankings_with_consensus;

    // Iterate through rankings table by group, using the secondary index
    auto itr = idx.begin();
    while (itr != idx.end())
    {
        uint64_t current_group = itr->room;

        // Store all submissions for the current group
        vector<vector<name>> room_submissions;

        // Aggregate all submissions for the current group
        while (itr != idx.end() && itr->room == current_group)
        {
            room_submissions.push_back(itr->rankings);
            ++itr;
        }

        vector<name> consensus_ranking;
        bool has_consensus = true;

        // Get how many submission there should be per room
        rooms_t rooms(_self, _self.value);
        const auto &room_iter = rooms.get(current_group, "No group with such ID.");

        // Check consensus for this group
        for (size_t i = 0; i < room_submissions[0].size(); ++i)
        {
            map<name, uint64_t> counts;
            name most_voted_name;
            uint64_t most_voted_count = 0;

            // Count occurrences and identify the most common account at the same time
            for (const auto &submission : room_submissions)
            {
                counts[submission[i]]++;

                // Update most_voted_name if necessary
                if (counts[submission[i]] > most_voted_count)
                {
                    most_voted_name = submission[i];
                    most_voted_count = counts[submission[i]];
                }
            }

            // Check consensus for the current rank
            if (most_voted_count * 3 >= room_iter.users.size() * 2)
            {
                consensus_ranking.push_back(most_voted_name);
            }
            else
            {
                has_consensus = false;
                break;
            }
        }

        // Store consensus ranking for the group if consensus is achieved
        if (has_consensus)
        {
            all_rankings_with_consensus.push_back(consensus_ranking);
        }
    }

    // all_rankings_with_consensus is passed to the function to distribute RESPECT and make other tokens claimable
    return all_rankings_with_consensus;

}

void zeos1fractal::distribute_rewards(const vector<vector<name>> &ranks)
{
    rewards_t rewards(get_self(), get_self().value);
    members_t members(get_self(), get_self().value);
    auto g = _global.get();
    map<name, int64_t> respect_receivers;
    int64_t total_respect_distributed = 0;

    // 1. Distribute respect to each member based on their rank.
    //    Respect is determined by the Fibonacci series and is independent of available tokens.
    for (const auto &rank : ranks)
    {
        // Determine the initial rank index based on group size.
        auto rankIndex = 6 - rank.size();

        for (const auto &acc : rank)
        {
            // Calculate respect based on the Fibonacci series.
            int64_t respect_amount = static_cast<int64_t>(fib(rankIndex + g.fib_offset));
            total_respect_distributed += respect_amount;

            auto mem_itr = members.find(acc.value);

            members.modify(mem_itr, _self, [&](auto &row) {
                row.total_respect += respect_amount;
                row.recent_respect.push_front(respect_amount);
                row.recent_respect.pop_back();
            });
            respect_receivers.insert({mem_itr->user, respect_amount});

            struct asset respect_token = { int64_t(respect_amount), symbol("REZPECT", 0) };

            accounts_t to_acnts(_self, mem_itr->user.value);
            auto to = to_acnts.find(respect_token.symbol.code().raw());
            if (to == to_acnts.end()) 
            {
                to_acnts.emplace(_self, [&](auto& a) { a.balance = respect_token; });
            }
            else 
            {
                to_acnts.modify(to, _self, [&](auto& a) { a.balance += respect_token; });
            }

            auto sym = respect_token.symbol;

            stats_t statstable(_self, sym.code().raw());
            auto existing = statstable.find(sym.code().raw());
            check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
            const auto& st = *existing;

            check(respect_token.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

            statstable.modify(st, _self, [&](auto& s) { s.supply += respect_token; });
        
            ++rankIndex;  // Move to next rank.
        }
    }

    // 2. Distribute token rewards based on available tokens and user rank.
    for (const auto& reward : rewards)
    {
        // Skip if no tokens are available for distribution.
        if (reward.quantity.quantity.amount <= 0)
        {
            continue;
        }

        int64_t reward_amount_distributed = 0;

        for(auto it = respect_receivers.begin(); it != respect_receivers.end(); ++it)
        {
            // the amount of tokens each participant receives is proportional to the amount of respect received (in relation to the overall amount of respect distributed in this event)
            int64_t user_amount = static_cast<int64_t>(floor(static_cast<double>(it->second) / static_cast<double>(total_respect_distributed) * static_cast<double>(reward.quantity.quantity.amount)));

            if (user_amount > 0)  // Only proceed if the member should get tokens.
            {
                claim_t claimables(get_self(), it->first.value);
                auto claim_itr = claimables.find(reward.quantity.quantity.symbol.code().raw());
                if (claim_itr != claimables.end())  // If member has claimables, update them.
                {
                    claimables.modify(claim_itr, _self, [&](auto &row) {
                        row.quantity.quantity.amount += user_amount;
                    });
                }
                else  // Otherwise, add new claimable.
                {
                    claimables.emplace(_self, [&](auto &row) {
                        row.quantity.quantity = asset{user_amount, reward.quantity.quantity.symbol};
                        row.quantity.contract = reward.quantity.contract;
                    });
                }

                reward_amount_distributed += user_amount;
            }
        }

        // Deduct distributed amount from total available rewards.
        rewards.modify(reward, _self, [&](auto &row) {
            row.quantity.quantity.amount -= reward_amount_distributed;
            // this check should never fail
            check(row.quantity.quantity.amount >= 0, "cannot distribute more rewards than avaliable");
        });
    }

    for (auto memb_itr = members.begin(); memb_itr != members.end(); ++memb_itr)
    {
        // if this member didn't participate in this event it gets zero respect
        if(respect_receivers.count(memb_itr->user) == 0)
        {
            // Modify the member's row
            members.modify(memb_itr, get_self(), [&](auto &row) {
                row.recent_respect.push_front(0);
                row.recent_respect.pop_back();
            });
        }
    }
}

 void zeos1fractal::determine_council()
{
    members_t members(get_self(), get_self().value);
    council_t council(_self, _self.value);
    abilities_t abilities(_self, _self.value);

    // helper struct used for sorting only in order to determine delegates (aka council members)
    struct respected_member
    {
        name user;
        uint64_t total_respect;
        double average_respect;
    };
    // lambda functions used for sorting by total/average respect:
    // using "greater than" for descending order (index 0 => member with most average/total respect)
    auto ttl_cmp = [](const respected_member & a,const respected_member& b) { return a.total_respect > b.total_respect; };
    auto avg_cmp = [](const respected_member & a,const respected_member& b) { return a.average_respect > b.average_respect; };

    // walk through all members and calculate average respect
    vector<respected_member> respected_members;
    for(auto it = members.begin(); it != members.end(); ++it)
    {
        respected_members.push_back(respected_member{
            it->user,
            it->total_respect,
            static_cast<double>(accumulate(it->recent_respect.begin(), it->recent_respect.end(), 0)) / 12.0
        });
    }

    // sort members by total respect first, then by average respect as the latter is more important
    my_sort(respected_members.data(), respected_members.size(), ttl_cmp);
    my_sort(respected_members.data(), respected_members.size(), avg_cmp);

    // fetch the required respect level from abilities table
    auto delegate_ability = abilities.find("delegate"_n.value);
    check(delegate_ability != abilities.end(), "'delegate' ability not set");

    // determine delegates
    vector<name> delegates;
    int i = 0;
    while(delegates.size() < COUNCIL_SIZE && i < respected_members.size())
    {
        if(respected_members[i].total_respect   >= delegate_ability->total_respect &&
            respected_members[i].average_respect >= delegate_ability->average_respect)
        {
        delegates.push_back(respected_members[i].user);
        }
        i++;
    }
    if(delegates.size() == COUNCIL_SIZE)
    {
        // Clear the council table
        for (auto iterdel = council.begin(); iterdel != council.end();)
        {
            council.erase(iterdel++);
        }

        // Populate council table
        for (const auto& delegate : delegates)
        {
            council.emplace(_self, [&](auto &a) { a.delegate = delegate; });
        }

        vector<name> alphabetically_ordered_delegates;
        for (auto iter = council.begin(); iter != council.end(); iter++)
        {
            alphabetically_ordered_delegates.push_back(iter->delegate);
        }

        struct permission_level_weight
        {
            permission_level permission;
            uint16_t weight;
        };
        vector<permission_level_weight> accounts;
        for (const auto& delegate : alphabetically_ordered_delegates)
        {
            accounts.push_back({.permission = permission_level{delegate, "active"_n}, .weight = (uint16_t)1});
        }

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
            vector<key_weight> keys;
            vector<permission_level_weight> accounts;
            vector<wait_weight> waits;
        };
        authority contract_authority;
        contract_authority.threshold = CONSENSUS[COUNCIL_SIZE];
        contract_authority.keys = {};
        contract_authority.accounts = accounts;
        contract_authority.waits = {};

        action(
            permission_level{_self, name("owner")}, name("eosio"), name("updateauth"),
            make_tuple(_self, name("active"), name("owner"), contract_authority)
        ).send();
    }
}

void zeos1fractal::cleartables() 
{
    // Clear participants table
    participants_t participants(_self, _self.value);
    auto part_itr = participants.begin();
    while(part_itr != participants.end()) 
    {
        part_itr = participants.erase(part_itr);
    }

    // Clear rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.begin();
    while(room_itr != rooms.end()) 
    {
        room_itr = rooms.erase(room_itr);
    }

    // Clear rankings table
    rankings_t rankings(_self, _self.value);
    auto rank_itr = rankings.begin();
    while(rank_itr != rankings.end()) 
    {
        rank_itr = rankings.erase(rank_itr);
    }
}

void zeos1fractal::claimrewards(const name& user)
{
    require_auth(user);

    claim_t claims(_self, user.value);
    check(claims.begin() != claims.end(), "nothing to claim");

    for (auto itr = claims.begin(); itr != claims.end();) 
    {
        action(
            permission_level{_self, "owner"_n},
            itr->quantity.contract,
            "transfer"_n,
            make_tuple(_self, user, itr->quantity.quantity, string("Claimed tokens from ZEOS fractal."))
        ).send();

        itr = claims.erase(itr);
    }
}

void zeos1fractal::banuser(const name& user) 
{
    require_auth(_self);

    members_t members(get_self(), get_self().value);
    auto itr = members.find(user.value);
    check(itr != members.end(), "User not found");

    members.modify(itr, get_self(), [&](auto& row) {
        row.is_banned = true;
    });
}

/*
void zeos1fractal::delbalance(const name& user) 
{
    require_auth(user);

    accounts_t accounts(get_self(), user.value);

    auto itr = accounts.begin();
    while (itr != accounts.end()) 
    {
        itr = accounts.erase(itr);
    }
}
*/
