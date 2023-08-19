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
    }, _self);
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
            
            check_consensus(); //rename this to reflect all the actions that happen in sequence 
              
            // TODO:
            // reset event tables
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
    //UI will automatically read the user's account name so should not be a problem, but I'd still add it.
    //check(is_account(user), "account " + acc.to_string() + " does not exist");

    members.emplace(user, [&](auto &row) {
        row.user = user;
        row.is_approved = false;
        row.is_banned = false;
        row.approvers = vector<name>();
        row.respect = 0;
        row.profile_why = why;
        row.profile_about = about;
        row.profile_links = links;
        row.recent_respect = deque<uint64_t>();
        for(int i = 0; i < 12; i++) row.recent_respect.push_back(0);
        row.avg_respect = 0;
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
        // any authorized member can approve new members
        auto usr = members.find(user.value);
        check(usr != members.end(), "user doesn't exist");
        check(usr->is_approved, "user is not approved");
        // TODO: check if user has 'ability' to approve
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

    // Shuffle the all_participants vector
    my_shuffle(all_participants.begin(), all_participants.end());

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

void zeos1fractal::check_consensus()
{
    // Access the rankings table (fuck it let's delete rankings after each election, we don't need it)
    rankings_t rankings_table(_self, _self.value);

    // Use a secondary index to sort by room
    auto idx = rankings_table.get_index<"bysecondary"_n>();

    // Store results of consensus rankings
    vector<vector<name>> allRankings;

    // Iterate through rankings table by group, using the secondary index
    auto itr = idx.begin();
    while (itr != idx.end())
    {
        uint64_t current_group = itr->room;

        // Store all submissions for the current group
        vector<vector<name>> all_submissions;

        // Aggregate all submissions for the current group
        while (itr != idx.end() && itr->room == current_group)
        {
            all_submissions.push_back(itr->rankings);
            ++itr;
        }

        vector<name> consensus_ranking;
        bool has_consensus = true;
        size_t total_submissions = all_submissions.size();

        // Check consensus for this group
        for (size_t i = 0; i < all_submissions[0].size(); ++i)
        {
            map<name, uint64_t> counts;
            name most_common_name;
            uint64_t most_common_count = 0;

            // Count occurrences and identify the most common account at the same time
            for (const auto &submission : all_submissions)
            {
                counts[submission[i]]++;

                // Update most_common_name if necessary
                if (counts[submission[i]] > most_common_count)
                {
                    most_common_name = submission[i];
                    most_common_count = counts[submission[i]];
                }
            }

            // Check consensus for the current rank
            if (most_common_count * 3 >= total_submissions * 2)
            {
                consensus_ranking.push_back(most_common_name);
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
            allRankings.push_back(consensus_ranking);
        }
    }
    // allRankings is passed to the function to distribute RESPECT and make other tokens claimable
    distribute_rewards(allRankings);
}

void zeos1fractal::distribute_rewards(const vector<vector<name>> &ranks)
{
    rewards_t rewards(get_self(), get_self().value);
    members_t members(get_self(), get_self().value);
    map<name, bool> attendees;

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
            auto fibAmount = static_cast<int64_t>(fib(rankIndex + 5));
            auto respect_amount = static_cast<int64_t>(fibAmount * pow(10, 4));

            auto mem_itr = members.find(acc.value);

            //uint8_t meeting_counter_new;
            //if (mem_itr->meeting_counter == 12)
            //{
            //    meeting_counter_new = 0;
            //}
            //else
            //{
            //    meeting_counter_new = mem_itr->meeting_counter + 1;
            //}

            members.modify(mem_itr, _self, [&](auto &row) {
                row.respect += respect_amount;
                //row.recent_respect[mem_itr->meeting_counter] = respect_amount;
                row.recent_respect.push_front(respect_amount);
                row.recent_respect.pop_back();
                //row.meeting_counter = meeting_counter_new;
            });
            attendees[mem_itr->user] = true;
        
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
                    claim_t claimables(get_self(), acc.value);
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

    // Loop through the members who did not participate and update their recent_respect to 0 and meeting_counter
    auto g = _global.get();
    for (auto memb_itr = members.begin(); memb_itr != members.end(); ++memb_itr)
    {
        // Check if member's meeting_counter is different from global_meeting_counter
        //if (memb_itr->meeting_counter != g.global_meeting_counter)
        // Check if this member attended this meeting
        if(attendees.count(memb_itr->user) == 0)
        {
            // Modify the member's row
            members.modify(memb_itr, get_self(), [&](auto &row) {
                //row.recent_respect[memb_itr->meeting_counter] = 0;
                row.recent_respect.push_front(0);
                row.recent_respect.pop_back();
                //row.meeting_counter = g.global_meeting_counter;
            });
        }
    }

    update_msig();

}

 void zeos1fractal::update_msig()
{
    members_t members(get_self(), get_self().value);
    council_t council(_self, _self.value);

    // Calculate avg_respect of each member and save it to members_t
    for (auto iter = members.begin(); iter != members.end(); ++iter)
    {
        uint64_t sum_of_respect = accumulate(iter->recent_respect.begin(),iter->recent_respect.end(), 0);
        uint64_t nr_of_weeks = 12;
        uint64_t avg_respect = sum_of_respect / nr_of_weeks;

        auto to = members.find(iter->user.value);
        if (to == members.end())
        {
            members.emplace(_self, [&](auto &a) {
                a.user = iter->user;
                a.avg_respect = avg_respect;
            });
        }
        else
        {
            members.modify(to, _self, [&](auto &a) { a.avg_respect = avg_respect; });
        }
    }

    auto members_idx = members.get_index<name("memberv2")>();

    vector<name> delegates;

    auto iter = members_idx.rbegin();
    for(int i = 0; i < 5 && iter != members_idx.rend(); ++i, ++iter)
    {
        delegates.push_back(iter->user);
    }

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

    authority contract_authority;

    vector<permission_level_weight> accounts;
    for (const auto& delegate : alphabetically_ordered_delegates)
    {
        accounts.push_back({.permission = permission_level{delegate, "active"_n}, .weight = (uint16_t)1});
    }

    contract_authority.threshold = 4;
    contract_authority.keys = {};
    contract_authority.accounts = accounts;
    contract_authority.waits = {};

    action(
        permission_level{_self, name("owner")}, name("eosio"), name("updateauth"),
        make_tuple(_self, name("active"), name("owner"), contract_authority)
    ).send();
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