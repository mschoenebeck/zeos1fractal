
/*


Test for automvalid(); function.

Function checks whether there is consensus in a group.
Vector of vectors is created from all the groups that have consensus.

To test:
1. trigger populate action
2. trigger automvalid action
3. see the results in the results table (scope _self)



*/

#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>

using namespace eosio;

CONTRACT zeosfractest : public contract {
public:
  using contract::contract;

  ACTION automvalid();
  ACTION insertrank(name user, uint64_t room, std::vector<name> rankings);
  ACTION populate(); // For prepopulating the rankings table
  ACTION clearranks();

  TABLE countdata {
    uint64_t id;         // Unique ID
    uint64_t room;       // Room number
    uint64_t rank;       // Rank index
    eosio::name account; // Account name
    uint64_t
        occurrences; // Occurrences of this account at this rank in this room
    auto primary_key() const { return id; }
  };
  typedef eosio::multi_index<"countdata"_n, countdata> countdata_t;

  TABLE ranking {
    name user;
    uint64_t room;
    std::vector<name> rankings;

    uint64_t primary_key() const { return user.value; }
    uint64_t by_secondary() const { return room; }
  };
  typedef eosio::multi_index<
      "rankings"_n, ranking,
      indexed_by<"bysecondary"_n,
                 const_mem_fun<ranking, uint64_t, &ranking::by_secondary>>>
      rankings_t;

  TABLE result {
    uint64_t id;
    std::vector<name> consensus_rankings;

    uint64_t primary_key() const { return id; }
  };
  typedef multi_index<"results"_n, result> results_t;

  // ... (other tables and actions if needed)
};

void zeosfractest::automvalid() {
  // Access the global singleton table to retrieve the current event number
  /*
  global global_singleton(_self, _self.value);
  check(global_singleton.exists(), "Global table does not exist.");
  auto global_data = global_singleton.get();
*/
  // TESTTT
  countdata_t countdata_table(_self,
                              _self.value); // Declare the table to save counts

  // Use the `rankings` table with the current event number as the scope, so
  // instead of 1 should be event_count
  rankings_t rankings_table(_self, 1);

  // Determine the number of groups by iterating over the rankings table
  // and finding the highest room number.
  uint64_t group_count = 0;
  for (auto itr = rankings_table.begin(); itr != rankings_table.end(); ++itr) {
    if (itr->room > group_count) {
      group_count = itr->room;
    }
  }

  // This will hold the consensus rankings for all groups
  std::vector<std::vector<eosio::name>> allRankings;

  // Iterate over each group
  for (uint64_t group_num = 1; group_num <= group_count; ++group_num) {
    auto idx = rankings_table.get_index<"bysecondary"_n>();
    auto itr_start = idx.lower_bound(group_num);
    auto itr_end = idx.upper_bound(group_num);

    // Collect all rankings submitted by group members
    std::vector<std::vector<eosio::name>> all_submissions;
    for (auto itr = itr_start; itr != itr_end; ++itr) {
      all_submissions.push_back(itr->rankings);
      // check(false, itr->rankings[0].value);
    }
    // Check consensus for this group
    std::vector<eosio::name> consensus_ranking;
    bool has_consensus = true;
    size_t total_submissions = all_submissions.size();

    // Compare rankings across submissions
    for (size_t i = 0; i < all_submissions[0].size(); ++i) {
      std::map<eosio::name, uint64_t> counts;

      // Count occurrences of each account name in the current rank
      for (const auto &submission : all_submissions) {
        counts[submission[i]]++;
      }

      // Save counts to the countdata table TESSSTTTTT
      for (const auto &entry : counts) {
        eosio::name account_name = entry.first;
        uint64_t occurrence_count = entry.second;

        countdata_table.emplace(_self, [&](auto &c) {
          c.id = countdata_table.available_primary_key();
          c.room = group_num;
          c.rank = i;
          c.account = account_name;
          c.occurrences = occurrence_count;
        });
      }

      eosio::name most_common_name;
      uint64_t most_common_count = 0;

      // Find the most common account name in the current rank
      for (const auto &[name, count] : counts) {
        if (count > most_common_count) {
          most_common_name = name;
          most_common_count = count;
        }
      }

      // Check if the most common account name occurs in at least 2/3 of
      // submissions
      if (most_common_count * 3 >= total_submissions * 2) {
        consensus_ranking.push_back(most_common_name);
      } else {
        has_consensus = false;
        break;
      }
    }

    // If consensus was reached for all ranks in the group, add to the result
    if (has_consensus) {
      allRankings.push_back(consensus_ranking);
    }
  }
  // Save the result to the results table
  results_t results_table(_self, _self.value);
  for (const auto &consensus_group : allRankings) {
    results_table.emplace(_self, [&](auto &r) {
      r.id = results_table.available_primary_key();
      r.consensus_rankings = consensus_group;
    });
  }
}

void zeosfractest::insertrank(name user, uint64_t room,
                              std::vector<name> rankings) {

  rankings_t rankings_table(_self, 1);
  auto itr = rankings_table.find(user.value);

  // If the user's ranking does not exist, create a new one
  if (itr == rankings_table.end()) {
    rankings_table.emplace(_self, [&](auto &r) {
      r.user = user;
      r.room = room;
      r.rankings = rankings;
    });
  }
  // If the user's ranking already exists, update the existing record
  else {
    rankings_table.modify(itr, _self, [&](auto &r) {
      r.room = room;         // Update the room
      r.rankings = rankings; // Update the rankings
    });
  }
}

void zeosfractest::populate() {

  // Example data for testing
  // Room 1 (3-user group with consensus)
  insertrank("john"_n, 1, {"john"_n, "carol"_n, "anton"_n});
  insertrank("anton"_n, 1, {"john"_n, "anton"_n, "carol"_n});
  insertrank("carol"_n, 1, {"john"_n, "anton"_n, "carol"_n});

  // Room 2 (3-user group without consensus)
  insertrank("dave"_n, 2, {"dave"_n, "eve"_n, "frank"_n});
  insertrank("eve"_n, 2, {"eve"_n, "dave"_n, "frank"_n});
  insertrank("frank"_n, 2, {"frank"_n, "eve"_n, "dave"_n});

  // Room 3 (3-user group without consensus)
  insertrank("george"_n, 3, {"george"_n, "hannah"_n, "ian"_n});
  insertrank("hannah"_n, 3, {"hannah"_n, "george"_n, "ian"_n});
  insertrank("ian"_n, 3, {"ian"_n, "hannah"_n, "george"_n});

  // Room 4 (4-user group with consensus)
  insertrank("jane"_n, 4, {"jane"_n, "kyle"_n, "lucia"_n, "matt"_n});
  insertrank("kyle"_n, 4, {"jane"_n, "kyle"_n, "lucia"_n, "matt"_n});
  insertrank("lucia"_n, 4, {"jane"_n, "kyle"_n, "lucia"_n, "matt"_n});
  insertrank("matt"_n, 4, {"matt"_n, "jane"_n, "kyle"_n, "lucia"_n});

  // Room 5 (4-user group without consensus)
  insertrank("nina"_n, 5, {"nina"_n, "oliver"_n, "patty"_n, "quentin"_n});
  insertrank("oliver"_n, 5, {"oliver"_n, "patty"_n, "nina"_n, "quentin"_n});
  insertrank("patty"_n, 5, {"patty"_n, "oliver"_n, "nina"_n, "quentin"_n});
  insertrank("quentin"_n, 5, {"quentin"_n, "nina"_n, "oliver"_n, "patty"_n});

  // Room 6 (5-user group with consensus)
  insertrank("rachel"_n, 6,
             {"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("steve"_n, 6,
             {"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("tina"_n, 6,
             {"rachel"_n, "steve"_n, "ulysses"_n, "tina"_n, "victor"_n});
  insertrank("ulysses"_n, 6,
             {"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("victor"_n, 6,
             {"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});

  // Room 7 (5-user group without consensus)
  insertrank("wanda"_n, 7,
             {"wanda"_n, "xander"_n, "yara"_n, "zane"_n, "ada"_n});
  insertrank("xander"_n, 7,
             {"xander"_n, "yara"_n, "ada"_n, "wanda"_n, "zane"_n});
  insertrank("yara"_n, 7, {"yara"_n, "ada"_n, "wanda"_n, "xander"_n, "zane"_n});
  insertrank("zane"_n, 7, {"zane"_n, "yara"_n, "ada"_n, "wanda"_n, "xander"_n});
  insertrank("ada"_n, 7, {"ada"_n, "zane"_n, "wanda"_n, "xander"_n, "yara"_n});

  // And so on for 6-user groups...
}

void zeosfractest::clearranks() {
  // Assuming you have the rankings table defined somewhere
  rankings_t rankings_table(_self, 1);

  // For clearing the rankings table
  auto rank_itr = rankings_table.begin();
  while (rank_itr != rankings_table.end()) {
    rank_itr = rankings_table.erase(rank_itr);
  }

  // For clearing the countdata table
  countdata_t countdata_table(_self, _self.value);
  auto count_itr = countdata_table.begin();
  while (count_itr != countdata_table.end()) {
    count_itr = countdata_table.erase(count_itr);
  }
}