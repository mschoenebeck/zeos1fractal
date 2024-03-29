#include "zeos1fractal.hpp"
#include <cmath>

zeos1fractal::zeos1fractal(
    name self,
    name code,
    datastream<const char *> ds
) : contract(self, code, ds),
    _globalv2(_self, _self.value)
{}


void zeos1fractal::init(const uint64_t &first_event_block_height)
{
    require_auth(_self);
    check(first_event_block_height == 0 || first_event_block_height > static_cast<uint64_t>(current_block_number()), "first event must take place in the future");

    _globalv2.set({
        STATE_IDLE,
        0,
        first_event_block_height == 0 ? current_block_number() + 500 : first_event_block_height,
        360,    //  3 min
        1200,   // 10 min
        3,      // fib offset
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
        {"delegate"_n, 111, 9.25},
        {"approver"_n, 56, 4.66}
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

}
void zeos1fractal::initstats()
{
    asset initial_supply = asset(0, symbol("REZPECT", 4)); // 0.0000 REZPECT
    asset max_supply = asset(10000000000, symbol("REZPECT", 4)); // 10 000 000.0000 REZPECT
    name issuer = "zeos1fractal"_n;

    stats statstable(_self, initial_supply.symbol.code().raw());

    // Check if it already exists
    auto stat_itr = statstable.find(initial_supply.symbol.code().raw());
    check(stat_itr == statstable.end(), "Token with symbol already initialized in stats table");

    // Create new row
    statstable.emplace(_self, [&](auto &row) {
        row.supply = initial_supply;
        row.max_supply = max_supply;
        row.issuer = issuer;
    });
}
void zeos1fractal::changestate()
{
    // anyone can execute this action
    check(_globalv2.exists(), "contract not initialized");
    auto g = _globalv2.get();
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

            create_groups();
        }
        break;

        case STATE_ROOMS:
        {
            check(cbn >= (g.next_event_block_height + g.rooms_duration), "too early to move into IDLE state");
            g.state = STATE_IDLE;
            g.next_event_block_height = g.next_event_block_height + 1209600; // add one week of blocks
            g.event_count++;
            
            //if (g.global_meeting_counter == 12) 
            //{
            //  g.global_meeting_counter = 0;
            //} 
            //else 
            //{
            //  g.global_meeting_counter++;
            //}
            
            vector<vector<name>> consensus_rankings = check_consensus();
            distribute_rewards(consensus_rankings);
            determine_council();
            
            // TODO:
            // reset event tables
        }
        break;
    }

    _globalv2.set(g, _self);
}

void zeos1fractal::setevent(const uint64_t& block_height)
{
    require_auth(_self);
    check(_globalv2.exists(), "contract not initialized");
    check(block_height > current_block_number(), "must be in the future");
    auto g = _globalv2.get();
    check(g.state == STATE_IDLE, "only in IDLE state");
    g.next_event_block_height = block_height;
    _globalv2.set(g, _self);
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
    check(_globalv2.exists(), "contract not initialized");
    membersv2_t members(_self, _self.value);
    auto usr = members.find(user.value);
    check(usr == members.end(), "user already signed up");
    //UI will automatically read the user's account name so should not be a problem, but I'd still add it.
    //check(is_account(user), "account " + acc.to_string() + " does not exist");

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
        //row.avg_respect = 0;
    });
}

void zeos1fractal::approve(
    const name &user,
    const name &user_to_approve
)
{
    require_auth(user);
    check(_globalv2.exists(), "contract not initialized");
    membersv2_t members(_self, _self.value);
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
        // any authorized member can approve new members
        auto usr = members.find(user.value);
        check(usr != members.end(), "user doesn't exist");
        check(usr->is_approved, "user is not approved");

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
    check(_globalv2.exists(), "contract not initialized");
    auto g = _globalv2.get();
    check(g.state == STATE_PARTICIPATE, "can only join during PARTICIPATE phase");

    membersv2_t members(_self, _self.value);
    auto usr = members.find(user.value);
    check(usr != members.end(), "user is not a member");
    check(usr->is_approved, "user is not approved");

    participants_t participants(_self, _self.value);
    auto p = participants.find(user.value);
    check(p == participants.end(), "user is already participating");

    participants.emplace(_self, [&](auto &row) { row.user = user; });
}

void zeos1fractal::submitranks(
    const name &user,
    const uint64_t &group_id,
    const vector<name> &rankings
)
{
    require_auth(user);

    // Check whether user is a part of a group he is submitting consensus for.
    rooms_t rooms(_self, _self.value);

    const auto &room_iter = rooms.get(group_id, "No group with such ID.");

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

    size_t group_size = rankings.size();

    // Check whether size of ranking equals to the size of users of that room
    check(group_size == room_iter.users.size(), "Rankings size must match the number of users in the group.");
    // Indeed checks below seem to be unneccesary but let's still keep?
    check(group_size >= 3, "too small group");
    check(group_size <= 6, "too big group");

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
            row.room = group_id;
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

void zeos1fractal::error(
    name from,
    name to,
    asset quantity,
    string memo
)
{
    if (to == _self)
    {
    check(false,"no");
    }
}
/*
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
        if(memo == "treasury")
        {
            treasury_t treasury(_self, _self.value);
            auto asset = treasury.find(quantity.symbol.raw());
            if(asset == treasury.end())
            {
                treasury.emplace(_self, [&](auto &row) {
                    row.quantity = extended_asset(quantity, first_receiver);
                });
            }
            else
            {
                treasury.modify(asset, _self, [&](auto &row) {
                    row.quantity += extended_asset(quantity, first_receiver);
                });
            }
        }
        else // all transfers other than type 'treasury' go into 'rewards'
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
*/
void zeos1fractal::create_groups() 
{
    participants_t participants(_self, _self.value);
    vector<name> all_participants;
    
    // Iterate over each participant in the table
    for (const auto& participant : participants) 
    {
        // Add each participant's name to the all_participants vector
        all_participants.push_back(participant.user);
    }
    if(all_participants.size() < 3) // Minimum number of participants could be also put in _global
    {
        auto g = _globalv2.get();
        g.state = STATE_IDLE;
        g.next_event_block_height = g.next_event_block_height + 1209600; // add one week of blocks
    }
    else
    {  
        // Shuffle the all_participants vector
        rng_t rndnmbr("r4ndomnumb3r"_n, "r4ndomnumb3r"_n.value);
        checksum256 x = rndnmbr.get().value;
        uint32_t seed = *reinterpret_cast<uint32_t*>(&x);
        my_shuffle(all_participants.begin(), all_participants.end(), seed);

        rooms_t rooms(_self, _self.value);
        auto num_participants = all_participants.size();

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
        auto iter = all_participants.begin();

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
            for (uint8_t j = 0; j < size && iter != all_participants.end(); j++, ++iter)
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
        size_t maximum_submissions = room_iter.users.size();

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
            if (most_voted_count * 3 >= maximum_submissions * 2)
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
    membersv2_t members(get_self(), get_self().value);
    auto g = _globalv2.get();
    set<name> attendees;

    // 1. Distribute respect to each member based on their rank.
    //    Respect is determined by the Fibonacci series and is independent of available tokens.
    for (const auto &rank : ranks)
    {
        // Determine the initial rank index based on group size.
        size_t group_size = rank.size();
        auto rankIndex = 6 - group_size;

        for (const auto &acc : rank)
        {
            // Calculate respect based on the Fibonacci series.
            auto respect_amount = static_cast<int64_t>(fib(rankIndex + 3));

            auto mem_itr = members.find(acc.value);

            members.modify(mem_itr, _self, [&](auto &row) {
                row.total_respect += respect_amount;
                row.recent_respect.push_front(respect_amount);
                row.recent_respect.pop_back();
            });
            attendees.insert(mem_itr->user);


            struct asset respect_token = { int64_t(respect_amount * 10000), symbol("REZPECT", 4) };

            accounts to_acnts(_self, mem_itr->user.value);
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

            stats statstable(_self, sym.code().raw());
            auto existing = statstable.find(sym.code().raw());
            check(existing != statstable.end(), "token with symbol does not exist, create token before issue");
            const auto& st = *existing;

            check(respect_token.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

            statstable.modify(st, _self, [&](auto& s) { s.supply += respect_token; });
        
            ++rankIndex;  // Move to next rank.
        }
    }

    // 2. Distribute token rewards based on available tokens and user rank.
    for (const auto& reward_entry : rewards)
    {
        auto availableReward = reward_entry.quantity.quantity.amount;

        // Skip if no tokens are available for distribution.
        if (availableReward <= 0)
        {
            continue;
        }

        // Calculate multiplier using polynomial coefficients and total available reward.
        auto coeffSum = accumulate(begin(polyCoeffs), end(polyCoeffs), 0.0);
        auto multiplier = static_cast<double>(availableReward) / (ranks.size() * coeffSum);

        // Determine token rewards for each rank.
        vector<int64_t> tokenRewards;
        transform(begin(polyCoeffs), end(polyCoeffs), back_inserter(tokenRewards), [&](const auto &c) {
            return static_cast<int64_t>(multiplier * c);
        });

        // Skip if any rank gets 0 tokens.
        if (any_of(tokenRewards.begin(), tokenRewards.end(), [](int64_t val) { return val == 0; }))
        {
            continue;
        }

        // Update or add claimable tokens for each member based on their rank.
        for (const auto &rank : ranks)
        {
            size_t group_size = rank.size();
            auto rankIndex = 6 - group_size;
            for (const auto &acc : rank)
            {
                auto tokenAmount = tokenRewards[rankIndex];

                if (tokenAmount > 0)  // Only proceed if the member should get tokens.
                {
                    claimv2_t claimables(get_self(), acc.value);
                    auto claim_itr = claimables.find(reward_entry.quantity.quantity.symbol.code().raw());

                    if (claim_itr != claimables.end())  // If member has claimables, update them.
                    {
                        claimables.modify(claim_itr, _self, [&](auto &row) {
                            row.quantity.quantity.amount += tokenAmount;
                        });
                    }
                    else  // Otherwise, add new claimable.
                    {
                        claimables.emplace(_self, [&](auto &row) {
                            row.quantity.quantity = asset{tokenAmount, reward_entry.quantity.quantity.symbol};
                            row.quantity.contract = reward_entry.quantity.contract;

                        });
                    }
                }

                ++rankIndex;  // Move to next rank.
            }
        }

        // Deduct distributed amount from total available rewards.
        rewards.modify(reward_entry, _self, [&](auto &row) {
            row.quantity.quantity.amount -= availableReward;
        });
    }

    for (auto memb_itr = members.begin(); memb_itr != members.end(); ++memb_itr)
    {
        if(attendees.count(memb_itr->user) == 0)
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
    membersv2_t members(get_self(), get_self().value);
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
    while(delegates.size() < 5 && i < respected_members.size())
    {
        if(respected_members[i].total_respect   >= delegate_ability->total_respect &&
            respected_members[i].average_respect >= delegate_ability->average_respect)
        {
        delegates.push_back(respected_members[i].user);
        }
        i++;
    }
    if(delegates.size() == 5)
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

        vector<permission_level_weight> accounts;
        for (const auto& delegate : alphabetically_ordered_delegates)
        {
            accounts.push_back({.permission = permission_level{delegate, "active"_n}, .weight = (uint16_t)1});
        }

        authority contract_authority;
        contract_authority.threshold = 4;
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
void zeos1fractal::testshuffle()
{
    vector<uint64_t> v = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    rng_t rndnmbr("r4ndomnumb3r"_n, "r4ndomnumb3r"_n.value);
    checksum256 x = rndnmbr.get().value;
    uint32_t seed = *reinterpret_cast<uint32_t*>(&x);
    my_shuffle(v.begin(), v.end(), seed);
    string s = "";
    for(auto e : v)
    {
        s += to_string(e) + ", ";
    }
    check(0, s);
}

void zeos1fractal::insertrank(name user, uint64_t room, std::vector<name> rankings) {

    rankings_t rankings_table(_self, _self.value);
    auto itr = rankings_table.find(user.value);

    // If the user's ranking does not exist, create a new one
    if (itr == rankings_table.end()) 
    {
        rankings_table.emplace(_self, [&](auto &r) {
            r.user = user;
            r.room = room;
            r.rankings = rankings;
        });
    }
    // If the user's ranking already exists, update the existing record
    else 
    {
        rankings_table.modify(itr, _self, [&](auto &r) {
            r.room = room;         // Update the room
            r.rankings = rankings; // Update the rankings
        });
    }

    // Handle members table
    membersv2_t members(_self, _self.value);
    auto usr = members.find(user.value);

    // If the user is not in the members table, create a new one
    if (usr == members.end()) 
    {
        members.emplace(_self, [&](auto &row) {
            row.user = user;
            row.is_approved = false;
            row.is_banned = false;
            row.approvers = vector<name>();
            row.total_respect = 0;
            row.profile_why = "why";
            row.profile_about = "why";
            row.recent_respect = deque<uint64_t>();
            for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
        });
    }

    // Handle rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.find(room);

    if (room_itr == rooms.end()) 
    {
        rooms.emplace(_self, [&](auto &r) {
            r.id = room;
            r.users = {user}; // initialize with just this user
        });
    } 
    else 
    {
        // Ensure the user is not already in the room. 
        // If not, append the user to the list.
        bool user_exists = std::find(room_itr->users.begin(), room_itr->users.end(), user) != room_itr->users.end();
        if (!user_exists) 
        {
            rooms.modify(room_itr, _self, [&](auto &r) {
                r.users.push_back(user);
            });
        }
    }
}

void zeos1fractal::populateone() 
{

    // Clear the rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.begin();
    while (room_itr != rooms.end()) {
        room_itr = rooms.erase(room_itr);
    }

  // Room 1 (3-user group with consensus)
insertrank("vladislav.x"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "consortiumlv"_n});
insertrank("kyotokimura2"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "consortiumlv"_n});
insertrank("consortiumlv"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "consortiumlv"_n});

}

void zeos1fractal::poponesix() 
{

    // Clear the rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.begin();
    while (room_itr != rooms.end()) {
        room_itr = rooms.erase(room_itr);
    }

  // Room 1 (3-user group with consensus)
insertrank("vladislav.x"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
insertrank("kyotokimura2"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
insertrank("testzeostest"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
insertrank("ironscimitar"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
insertrank("quantumphyss"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
insertrank("olgasportfel"_n, 1, {"vladislav.x"_n, "kyotokimura2"_n, "testzeostest"_n, "ironscimitar"_n,"quantumphyss"_n,"olgasportfel"_n});
}



void zeos1fractal::populatetwo() 
{

    // Clear the rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.begin();
    while (room_itr != rooms.end()) {
        room_itr = rooms.erase(room_itr);
    }

  // Room 1 (3-user group with consensus)
insertrank("vladislav.x"_n, 1, {"mschoenebeck"_n, "vladislav.x"_n, "gnomegenomes"_n});
insertrank("mschoenebeck"_n, 1, {"gnomegenomes"_n, "vladislav.x"_n, "mschoenebeck"_n});
insertrank("gnomegenomes"_n, 1, {"gnomegenomes"_n, "vladislav.x"_n, "mschoenebeck"_n});

insertrank("ncoltd123451"_n, 2, {"ncoltd123451"_n, "lennyaccount"_n, "kyotokimura2"_n});
insertrank("lennyaccount"_n, 2, {"ncoltd123451"_n, "kyotokimura2"_n, "lennyaccount"_n});
insertrank("kyotokimura2"_n, 2, {"ncoltd123451"_n, "kyotokimura2"_n, "lennyaccount"_n});


}


void zeos1fractal::populate() 
{

    // Clear the rooms table
    rooms_t rooms(_self, _self.value);
    auto room_itr = rooms.begin();
    while (room_itr != rooms.end()) {
        room_itr = rooms.erase(room_itr);
    }

  // Room 1 (3-user group with consensus)
insertrank("vladislav.x"_n, 1, {"mschoenebeck"_n, "vladislav.x"_n, "gnomegenomes"_n});
insertrank("mschoenebeck"_n, 1, {"mschoenebeck"_n, "vladislav.x"_n, "gnomegenomes"_n});
insertrank("gnomegenomes"_n, 1, {"mschoenebeck"_n, "vladislav.x"_n, "gnomegenomes"_n});

insertrank("ncoltd123451"_n, 2, {"ncoltd123451"_n, "lennyaccount"_n, "kyotokimura2"_n});
insertrank("lennyaccount"_n, 2, {"ncoltd123451"_n, "lennyaccount"_n, "kyotokimura2"_n});
insertrank("kyotokimura2"_n, 2, {"ncoltd123451"_n, "lennyaccount"_n, "kyotokimura2"_n});

// Room 11 (6-user group without consensus)
insertrank("2542pk.ftw"_n, 11, {"2542pk.ftw"_n, "2luminaries1"_n, "2wkuti52.ftw"_n, "3mg.ftw"_n, "3poalica.ftw"_n, "3rica.ftw"_n});
insertrank("2luminaries1"_n, 11, {"2luminaries1"_n, "2wkuti52.ftw"_n, "3rica.ftw"_n, "2542pk.ftw"_n, "3mg.ftw"_n, "3poalica.ftw"_n});
insertrank("2wkuti52.ftw"_n, 11, {"2wkuti52.ftw"_n, "3rica.ftw"_n, "2542pk.ftw"_n, "2luminaries1"_n, "3mg.ftw"_n, "3poalica.ftw"_n});
insertrank("3mg.ftw"_n, 11, {"3mg.ftw"_n, "2wkuti52.ftw"_n, "3rica.ftw"_n, "2542pk.ftw"_n, "2luminaries1"_n, "3poalica.ftw"_n});
insertrank("3poalica.ftw"_n, 11, {"3poalica.ftw"_n, "3mg.ftw"_n, "2542pk.ftw"_n, "2luminaries1"_n, "2wkuti52.ftw"_n, "3rica.ftw"_n});
insertrank("3rica.ftw"_n, 11, {"3rica.ftw"_n, "3poalica.ftw"_n, "3mg.ftw"_n, "2wkuti52.ftw"_n, "2luminaries1"_n, "2542pk.ftw"_n});

// Room 12 (6-user group with half the vectors identical) no consensus
insertrank("3xfyanfwnqum"_n, 12, {"3xfyanfwnqum"_n, "41aekkfnnfkk"_n, "43233dh.ftw"_n, "443fox.ftw"_n, "452345234523"_n, "4crypto.ftw"_n});
insertrank("41aekkfnnfkk"_n, 12, {"3xfyanfwnqum"_n, "41aekkfnnfkk"_n, "43233dh.ftw"_n, "443fox.ftw"_n, "452345234523"_n, "4crypto.ftw"_n});
insertrank("43233dh.ftw"_n, 12, {"3xfyanfwnqum"_n, "41aekkfnnfkk"_n, "43233dh.ftw"_n, "443fox.ftw"_n, "452345234523"_n, "4crypto.ftw"_n});
insertrank("443fox.ftw"_n, 12, {"443fox.ftw"_n, "452345234523"_n, "4crypto.ftw"_n, "3xfyanfwnqum"_n, "41aekkfnnfkk"_n, "43233dh.ftw"_n});
insertrank("452345234523"_n, 12, {"452345234523"_n, "4crypto.ftw"_n, "443fox.ftw"_n, "43233dh.ftw"_n, "41aekkfnnfkk"_n, "3xfyanfwnqum"_n});
insertrank("4crypto.ftw"_n, 12, {"4crypto.ftw"_n, "3xfyanfwnqum"_n, "41aekkfnnfkk"_n, "43233dh.ftw"_n, "443fox.ftw"_n, "452345234523"_n});

// Room 10 (6-user group with consensus)
insertrank("5115jess.ftw"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});
insertrank("51lucifer515"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});
insertrank("5514.ftw"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});
insertrank("5ht4zibb2rxk"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});
insertrank("5theotheos54"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});
insertrank("5viuqsej.ftw"_n, 10, {"5115jess.ftw"_n, "51lucifer515"_n, "5514.ftw"_n, "5ht4zibb2rxk"_n, "5theotheos54"_n, "5viuqsej.ftw"_n});

// Room 21 (3-user group with consensus)
insertrank("kesaritooooo"_n, 21, {"kesaritooooo"_n, "kevdamguer55"_n, "khaledmk.ftw"_n});
insertrank("kevdamguer55"_n, 21, {"kesaritooooo"_n, "kevdamguer55"_n, "khaledmk.ftw"_n});
insertrank("khaledmk.ftw"_n, 21, {"kesaritooooo"_n, "kevdamguer55"_n, "khaledmk.ftw"_n});

// Room 22 (3-user group without consensus)
insertrank("kickkers.ftw"_n, 22, {"kickkers.ftw"_n, "kihei14.ftw"_n, "kihri13.ftw"_n});
insertrank("kihei14.ftw"_n, 22, {"kihei14.ftw"_n, "kickkers.ftw"_n, "kihri13.ftw"_n});
insertrank("kihri13.ftw"_n, 22, {"kihri13.ftw"_n, "kickkers.ftw"_n, "kihei14.ftw"_n});

// Room 23 (6-user group with consensus)
insertrank("kikifeli.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});
insertrank("killacc.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});
insertrank("killc.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});
insertrank("killyoaz.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});
insertrank("kinez.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});
insertrank("kingbong.ftw"_n, 23, {"kikifeli.ftw"_n, "killacc.ftw"_n, "killc.ftw"_n, "killyoaz.ftw"_n, "kinez.ftw"_n, "kingbong.ftw"_n});

// Room 24 (6-user group without consensus)
insertrank("kinginaa.ftw"_n, 24, {"kinginaa.ftw"_n, "kingjejj.ftw"_n, "kingkurr.ftw"_n, "kingwi.ftw"_n, "kingxxxxxhuy"_n, "kkorzyn.ftw"_n});
insertrank("kingjejj.ftw"_n, 24, {"kinginaa.ftw"_n, "kingkurr.ftw"_n, "kingwi.ftw"_n, "kingjejj.ftw"_n, "kingxxxxxhuy"_n, "kkorzyn.ftw"_n});
insertrank("kingkurr.ftw"_n, 24, {"kinginaa.ftw"_n, "kingjejj.ftw"_n, "kingwi.ftw"_n, "kingkurr.ftw"_n, "kingxxxxxhuy"_n, "kkorzyn.ftw"_n});
insertrank("kingwi.ftw"_n, 24, {"kinginaa.ftw"_n, "kingjejj.ftw"_n, "kingkurr.ftw"_n, "kingwi.ftw"_n, "kingxxxxxhuy"_n, "kkorzyn.ftw"_n});
insertrank("kingxxxxxhuy"_n, 24, {"kinginaa.ftw"_n, "kingjejj.ftw"_n, "kingkurr.ftw"_n, "kingwi.ftw"_n, "kingxxxxxhuy"_n, "kkorzyn.ftw"_n});
insertrank("kkorzyn.ftw"_n, 24, {"kinginaa.ftw"_n, "kingjejj.ftw"_n, "kingkurr.ftw"_n, "kingxxxxxhuy"_n, "kingwi.ftw"_n, "kkorzyn.ftw"_n});

}

void zeos1fractal::three() 
{
    vector<vector<name>> consensus_rankings = check_consensus();
    distribute_rewards(consensus_rankings);
    determine_council();

}


void zeos1fractal::clearmembers() 
{
    // Clear participants table
    membersv2_t participants(_self, _self.value);
    auto part_itr = participants.begin();
    while(part_itr != participants.end()) 
    {
        part_itr = participants.erase(part_itr);
    }
}


void zeos1fractal::claimrewards(const name& user)
{
    require_auth(user);

    claimv2_t claims(_self, user.value);

    for (auto itr = claims.begin(); itr != claims.end();) 
    {
        action(
            permission_level{_self, "owner"_n},
            itr->quantity.contract,   // This is the token contract
            "transfer"_n,
            make_tuple(_self, user, itr->quantity.quantity, string("Claimed tokens from ZEOS fractal."))
        ).send();

        itr = claims.erase(itr);
    }
}


 void zeos1fractal::deleteall() {
        require_auth(_self); // Only the contract owner can run this

        participants_t participants(_self, _self.value);
        auto participants_itr = participants.begin();
        while (participants_itr != participants.end()) {
            participants_itr = participants.erase(participants_itr);
        }

        rooms_t groups(_self, _self.value);
        auto groups_itr = groups.begin();
        while (groups_itr != groups.end()) {
            groups_itr = groups.erase(groups_itr);
        }
         rankings_t rankings_table(_self, _self.value);

              // For clearing the rankings table
              auto rank_itr = rankings_table.begin();
              while (rank_itr != rankings_table.end()) {
                rank_itr = rankings_table.erase(rank_itr);
              }
    }

void zeos1fractal::createx(const name& issuer, const asset& maximum_supply)
{
    require_auth(_self);

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "invalid symbol name");
    check(maximum_supply.is_valid(), "invalid supply");
    check(maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable(_self, sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "token with symbol already exists");

    statstable.emplace(_self, [&](auto& s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply = maximum_supply;
        s.issuer = issuer;
    });
}

void zeos1fractal::issuetoken ( name owner, asset touser, asset tosupply )
    
{
            require_auth (_self);
            require_recipient( owner );

            
        auto sym = tosupply.symbol;
            check( sym.is_valid(), "Invalid symbol name" );

            auto sym_code_raw = sym.code().raw();

            stats statstable( _self, sym_code_raw );
            auto existing = statstable.find( sym_code_raw );
            check( existing != statstable.end(), "Token with that symbol name does not exist - Please create the token before issuing" );

            const auto& st = * existing;
            
        check( tosupply.is_valid(), "Invalid quantity value" );
            check( st.supply.symbol == tosupply.symbol, "Symbol precision mismatch" );
            check( st.max_supply.amount - st.supply.amount >= tosupply.amount, "Quantity value cannot exceed the available supply" );

            statstable.modify( st, _self, [&]( auto& s ) {
            s.supply += tosupply;
         });
            
        
       addd_balance( owner, touser, _self);
        
}


void zeos1fractal::addd_balance(name owner, asset value, name ram_payer)
{
    accounts to_acnts(_self, owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if (to == to_acnts.end()) {
        to_acnts.emplace(ram_payer, [&](auto& a) { a.balance = value; });
    }
    else {
        to_acnts.modify(to, same_payer, [&](auto& a) { a.balance += value; });
    }
}

void zeos1fractal::transfer(name from, name to, asset quantity, std::string memo)
{

    ///require_recipient(from);
    //require_recipient(to);

if (from != name{})
{
    check(false, "ded");
}

}