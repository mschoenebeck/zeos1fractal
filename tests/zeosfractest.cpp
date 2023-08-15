#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/asset.hpp>
#include <numeric>



using namespace eosio;
using namespace std;

CONTRACT r4ndomnumb3r : public contract
{
public:
    using contract::contract;

    ACTION generate();
    using generate_action = action_wrapper<"generate"_n, &r4ndomnumb3r::generate>;
};



CONTRACT zeosfractest : public contract {
public:
  using contract::contract;

  ACTION automvalid();
  ACTION insertrank(name user, uint64_t room, std::vector<name> rankings);
  ACTION populate(); // For prepopulating the rankings table
  ACTION clearranks();
  ACTION creategrps(uint32_t num);
  ACTION deleteall(); 
  //ACTION pede(); 
  ACTION testshuffle();


/*
scope: user acc name
*/
TABLE claim {
extended_asset quantity;
uint64_t primary_key() const { return quantity.quantity.symbol.code().raw(); }

};
typedef eosio::multi_index<"claims"_n, claim> claim_t;


    // all rewards that get distributed in the upcoming event
    TABLE reward
    {
        extended_asset quantity;

        uint64_t primary_key() const { return quantity.quantity.symbol.raw(); }
    };
    typedef multi_index<"rewards"_n, reward> rewards_t;

    

    TABLE rng
    {
        checksum256 value;
    };
    using rng_t = singleton<"rng"_n, rng>;

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
// Table for participants
TABLE participant {
    name user;

    uint64_t primary_key() const { return user.value; }
};
typedef eosio::multi_index<"participants"_n, participant> participants_t;



 TABLE room
    {
        uint64_t id;
        vector<name> users;

        uint64_t primary_key() const { return id; }
    };
    typedef multi_index<"rooms"_n, room> rooms_t;


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
            r4ndomnumb3r::generate_action r4ndomnumb3r_generate("r4ndomnumb3r"_n, {_self, "active"_n});
            r4ndomnumb3r_generate.send();
            rng_t rndnmbr("r4ndomnumb3r"_n, "r4ndomnumb3r"_n.value);
            checksum256 x = rndnmbr.get().value;
            uint64_t r = *reinterpret_cast<uint64_t*>(&x) % (i+1);
            my_swap(first[i], first[r]);
        }
    }


};
 

void zeosfractest::automvalid() {
  // Access the rankings table
  rankings_t rankings_table(_self, 1);

  // Use a secondary index to sort by room
  auto idx = rankings_table.get_index<"bysecondary"_n>();

  // Store results of consensus rankings
  std::vector<std::vector<eosio::name>> allRankings;

  // Iterate through rankings table by group, using the secondary index
  auto itr = idx.begin();
  while (itr != idx.end()) {
    uint64_t current_group = itr->room;

    // Store all submissions for the current group
    std::vector<std::vector<eosio::name>> all_submissions;

    // Aggregate all submissions for the current group
    while (itr != idx.end() && itr->room == current_group) {
      all_submissions.push_back(itr->rankings);
      ++itr;
    }

    std::vector<eosio::name> consensus_ranking;
    bool has_consensus = true;
    size_t total_submissions = all_submissions.size();

    // Check consensus for this group
    for (size_t i = 0; i < all_submissions[0].size(); ++i) {
      std::map<eosio::name, uint64_t> counts;
      eosio::name most_common_name;
      uint64_t most_common_count = 0;

      // Count occurrences and identify the most common account at the same time
      for (const auto &submission : all_submissions) {
        counts[submission[i]]++;

        // Update most_common_name if necessary
        if (counts[submission[i]] > most_common_count) {
          most_common_name = submission[i];
          most_common_count = counts[submission[i]];
        }
      }

      // Check consensus for the current rank
      if (most_common_count * 3 >= total_submissions * 2) {
        consensus_ranking.push_back(most_common_name);
      } else {
        has_consensus = false;
        break;
      }
    }

    // Store consensus ranking for the group if consensus is achieved
    if (has_consensus) {
      allRankings.push_back(consensus_ranking);
    }
  }

  // Store results
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
  insertrank("dave"_n, 2, {"frank"_n, "eve"_n, "dave"_n});
  insertrank("eve"_n, 2, {"frank"_n, "eve"_n, "dave"_n});
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
  insertrank("oliver"_n, 5, {"nina"_n, "oliver"_n, "patty"_n, "quentin"_n});
  insertrank("patty"_n, 5, {"patty"_n, "oliver"_n, "nina"_n, "quentin"_n});
  insertrank("quentin"_n, 5, {"patty"_n, "oliver"_n, "nina"_n, "quentin"_n});

  // Room 6 (5-user group with consensus)
  insertrank("rachel"_n, 6,{"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("steve"_n, 6, {"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("tina"_n, 6,{"rachel"_n, "steve"_n, "ulysses"_n, "tina"_n, "victor"_n});
  insertrank("ulysses"_n, 6,{"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});
  insertrank("victor"_n, 6,{"rachel"_n, "steve"_n, "tina"_n, "ulysses"_n, "victor"_n});

  // Room 7 (5-user group without consensus)
  insertrank("wanda"_n, 7,{"wanda"_n, "xander"_n, "yara"_n, "zane"_n, "ada"_n});
  insertrank("xander"_n, 7,{"xander"_n, "yara"_n, "ada"_n, "wanda"_n, "zane"_n});
  insertrank("yara"_n, 7, {"yara"_n, "ada"_n, "wanda"_n, "xander"_n, "zane"_n});
  insertrank("zane"_n, 7, {"zane"_n, "yara"_n, "ada"_n, "wanda"_n, "xander"_n});
  insertrank("ada"_n, 7, {"ada"_n, "zane"_n, "wanda"_n, "xander"_n, "yara"_n});

 // Room 10 (6-user group with consensus)
insertrank("aleksandr"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("boris"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("dmitriy"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "galina"_n, "fedor"_n});
insertrank("elena"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "galina"_n, "fedor"_n});
insertrank("fedor"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("galina"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});

// Room 11 (6-user group without consensus)
insertrank("igor"_n, 11, {"igor"_n, "yulia"_n, "konstantin"_n, "ludmila"_n, "mikhail"_n, "natalya"_n});
insertrank("yulia"_n, 11, {"yulia"_n, "konstantin"_n, "natalya"_n, "igor"_n, "ludmila"_n, "mikhail"_n});
insertrank("konstantin"_n, 11, {"konstantin"_n, "natalya"_n, "igor"_n, "yulia"_n, "ludmila"_n, "mikhail"_n});
insertrank("ludmila"_n, 11, {"ludmila"_n, "konstantin"_n, "natalya"_n, "igor"_n, "yulia"_n, "mikhail"_n});
insertrank("mikhail"_n, 11, {"mikhail"_n, "ludmila"_n, "igor"_n, "yulia"_n, "konstantin"_n, "natalya"_n});
insertrank("natalya"_n, 11, {"natalya"_n, "mikhail"_n, "ludmila"_n, "konstantin"_n, "yulia"_n, "igor"_n});


// Room 12 (6-user group with half the vectors identical) no consensus
insertrank("usera"_n, 12, {"usera"_n, "userb"_n, "userc"_n, "userd"_n, "usere"_n, "userf"_n});
insertrank("userb"_n, 12, {"usera"_n, "userb"_n, "userc"_n, "userd"_n, "usere"_n, "userf"_n});
insertrank("userc"_n, 12, {"usera"_n, "userb"_n, "userc"_n, "userd"_n, "usere"_n, "userf"_n});
insertrank("userd"_n, 12, {"userd"_n, "usere"_n, "userf"_n, "usera"_n, "userb"_n, "userc"_n});
insertrank("usere"_n, 12, {"usere"_n, "userf"_n, "userd"_n, "userc"_n, "userb"_n, "usera"_n});
insertrank("userf"_n, 12, {"userf"_n, "usera"_n, "userb"_n, "userc"_n, "userd"_n, "usere"_n});

// Room 10 (6-user group with consensus)
insertrank("aleksandr"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("boris"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("dmitriy"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "galina"_n, "fedor"_n});
insertrank("elena"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "galina"_n, "fedor"_n});
insertrank("fedor"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});
insertrank("galina"_n, 10, {"aleksandr"_n, "boris"_n, "dmitriy"_n, "elena"_n, "fedor"_n, "galina"_n});

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

   void zeosfractest::deleteall() {
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
    }

    //void zeosfractest::pede(){}



    void zeosfractest::creategrps(uint32_t num) {

    vector<name> sample_accounts = {
    "usera1aa"_n, "usera2aa"_n, "usera3aa"_n, "usera4aa"_n, "usera5aa"_n,
    "userb1bb"_n, "userb2bb"_n, "userb3bb"_n, "userb4bb"_n, "userb5bb"_n,
    "userc1cc"_n, "userc2cc"_n, "userc3cc"_n, "userc4cc"_n, "userc5cc"_n,
    "userd1dd"_n, "userd2dd"_n, "userd3dd"_n, "userd4dd"_n, "userd5dd"_n,
    "usere1ee"_n, "usere2ee"_n, "usere3ee"_n, "usere4ee"_n, "usere5ee"_n,
    "userf1ff"_n, "userf2ff"_n, "userf3ff"_n, "userf4ff"_n, "userf5ff"_n,
    "userg1gg"_n, "userg2gg"_n, "userg3gg"_n, "userg4gg"_n, "userg5gg"_n,
    "userh1hh"_n, "userh2hh"_n, "userh3hh"_n, "userh4hh"_n, "userh5hh"_n,
    "useri1ii"_n, "useri2ii"_n, "useri3ii"_n, "useri4ii"_n, "useri5ii"_n,
    "userj1jj"_n, "userj2jj"_n, "userj3jj"_n, "userj4jj"_n, "userj5jj"_n,
    "k1k1"_n, "k2k2"_n, "k3k3"_n, "k4k4"_n, "k5k5"_n,
    "l1l1"_n, "l2l2"_n, "l3l3"_n, "l4l4"_n, "l5l5"_n,
    "m1m1"_n, "m2m2"_n, "m3m3"_n, "m4m4"_n, "m5m5"_n,
    "n1n1"_n, "n2n2"_n, "n3n3"_n, "n4n4"_n, "n5n5"_n,
    "o1o1"_n, "o2o2"_n, "o3o3"_n, "o4o4"_n, "o5o5"_n,
    "p1p1"_n, "p2p2"_n, "p3p3"_n, "p4p4"_n, "p5p5"_n,
    "q1q1"_n, "q2q2"_n, "q3q3"_n, "q4q4"_n, "q5q5"_n,
    "r1r1"_n, "r2r2"_n, "r3r3"_n, "r4r4"_n, "r5r5"_n,
    "s1s1"_n, "s2s2"_n, "s3s3"_n, "s4s4"_n, "s5s5"_n,
    "t1t1"_n, "t2t2"_n, "t3t3"_n, "t4t4"_n, "t5t5"_n
    

    };
    // Ensure num_participants is within bounds
    eosio::check(num <= sample_accounts.size(), "num_participants exceeds the size of sample accounts.");

    // Insert the selected number of participants into the participants table
    participants_t participants(_self, _self.value);
    for(uint32_t i = 0; i < num; ++i) {
        participants.emplace(_self, [&](auto& row) {
            row.user = sample_accounts[i];
        });
    }



    //participants_t participants(_self, _self.value);
    vector<name> all_participants;
    
    // Iterate over each participant in the table
    for (const auto& participant : participants) {
    // Add each participant's name to the all_participants vector
    all_participants.push_back(participant.user);
       }
    // Shuffle the all_participants vector
    my_shuffle(all_participants.begin(), all_participants.end());
/*
string s = "";
    for(const auto& e : all_participants)
    {
        s += e.to_string() + ", ";
    }

    // Log the shuffled names (using check for now, but print can be used for actual logging)
    check(0, s);
*/

    // Ensure we're working with valid participants count
    //eosio::check(all_participants.size() <= num, "Number of shuffled participants exceeds the size of the original participants list.");
    //eosio::check(all_participants.size() >= num, "Number of shuffled participants is smaller than the size of the original participants list.");

    rooms_t rooms(_self, _self.value);
    auto num_participants = all_participants.size();
    //check(false, num_participants);

    vector<uint8_t> group_sizes;

    // First part: hardcoded groups up to 20
        if (num_participants <= 20) {
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
        // Second part: generic algorithm for 21+ participants
        else {
            // As per the constraint, we start with groups of 6
            uint8_t count_of_fives = 0;
            while (num_participants > 0) {
                if (num_participants % 6 != 0 && count_of_fives < 5) {
                    group_sizes.push_back(5);
                    num_participants -= 5;
                    count_of_fives++;
                } else {
                    group_sizes.push_back(6);
                    num_participants -= 6;
                    if (count_of_fives == 5) count_of_fives = 0;
                }
            }
        }

        // Ensure the sum of group_sizes doesn't exceed all_participants.size().
//uint32_t total_group_size = std::accumulate(group_sizes.begin(), group_sizes.end(), 0);
//eosio::check(total_group_size <= all_participants.size(), "Sum of group sizes exceeds the total number of participants.");
    // Create the actual groups using group_sizes
    auto iter = all_participants.begin();

    uint64_t room_id = rooms.available_primary_key();
    // Ensure we start from 1 if table is empty
    if (room_id == 0) {
        room_id = 1;
    }

    // Iterate over the group sizes to create and populate rooms
    for (uint8_t size : group_sizes) {
    // A temporary vector to store the users in the current room
    vector<name> users_in_room;

    // Iterate until the desired group size is reached or until all participants have been processed
    for (uint8_t j = 0; j < size && iter != all_participants.end(); j++, ++iter) {
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

/*
void zeosfractest::testshuffle()
{

  vector<name> v = {
    "usera1aa"_n, "usera2aa"_n, "usera3aa"_n, "usera4aa"_n, "usera5aa"_n,
    "userb1bb"_n, "userb2bb"_n, "userb3bb"_n, "userb4bb"_n, "userb5bb"_n,};
    //vector<uint64_t> v = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    my_shuffle(v.begin(), v.end());
    string s = "";
    for(auto e : v)
    {
        s += to_string(e) + ", ";
    }
    check(0, s);
}
*/
void zeosfractest::testshuffle()
{
    // Create a sample vector of EOSIO names
    vector<name> v = {
        "usera1aa"_n, "userb1bb"_n, "userc1cc"_n, "userd1dd"_n,
        "usere1ee"_n, "userf1ff"_n, "userg1gg"_n, "userh1hh"_n
    };

    // Shuffle the names
    my_shuffle(v.begin(), v.end());

    // Convert shuffled names to a string for easier logging or checks
    string s = "";
    for(const auto& e : v)
    {
        s += e.to_string() + ", ";
    }

    // Log the shuffled names (using check for now, but print can be used for actual logging)
    //check(0, s);
}

/*
void awfractal::submitranks(const AllRankings &ranks) {

  auto coeffSum =
      std::accumulate(std::begin(polyCoeffs), std::end(polyCoeffs), 0.0);

//Loop should start here?
  // Calculation how much EOS per coefficient.
  auto multiplier = (double)newrew.eos_reward_amt / (numGroups * coeffSum);



  std::vector<int64_t> eosRewards;
  std::transform(std::begin(polyCoeffs), std::end(polyCoeffs),
                 std::back_inserter(eosRewards), [&](const auto &c) {
                   auto finalEosQuant = static_cast<int64_t>(multiplier * c);
                   check(finalEosQuant > 0,
                         "Total configured POLL distribution is too small to "
                         "distibute any reward to rank 1s");
                   return finalEosQuant;
                 });

  std::map<name, uint8_t> accounts;

  for (const auto &rank : ranks.allRankings) {

    size_t group_size = rank.ranking.size();
    auto rankIndex = max_group_size - group_size;
    for (const auto &acc : rank.ranking) {

      auto fibAmount = static_cast<int64_t>(fib(rankIndex + newrew.fib_offset));
      auto respect_amount = static_cast<int64_t>(fibAmount * std::pow(10, eden_symbol.precision()));
      auto respectnQuantity = asset{respect_amount, eden_symbol};

      //save the respectnQuantity in the members table for the user
      //save the respectnQuantity in the vector that stores the latest respect for the dynamic msig function

    
      auto eosQuantity = asset{eosRewards[rankIndex], poll_symbol};

      send(get_self(), acc, eosQuantity, "AW fractal $POLL rewards", _self);

      ++rankIndex;
    }
  }
}
*/


/*
void awfractal::submitranks(const AllRankings &ranks) {
  rewards_t rewards(get_self(), get_self().value);

  // Iterate over all tokens in the rewards table
  for (auto rew_itr = rewards.begin(); rew_itr != rewards.end(); ) {

    // Check if the current reward is non-zero
    if (rew_itr->quantity.amount <= 0) {
      ++rew_itr;
      continue;
    }

    auto availableReward = rew_itr->quantity.amount;
    auto current_symbol = rew_itr->quantity.symbol;
    auto coeffSum = std::accumulate(std::begin(polyCoeffs), std::end(polyCoeffs), 0.0);

    // Calculate the EOS per coefficient.
    auto multiplier = static_cast<double>(availableReward) / (numGroups * coeffSum);

    std::vector<int64_t> eosRewards;
    std::transform(std::begin(polyCoeffs), std::end(polyCoeffs),
                   std::back_inserter(eosRewards), [&](const auto &c) {
                       auto finalEosQuant = static_cast<int64_t>(multiplier * c);
                       return finalEosQuant;
                   });

    for (const auto &rank : ranks.allRankings) {
      size_t group_size = rank.ranking.size();
      auto rankIndex = max_group_size - group_size;
      for (const auto &acc : rank.ranking) {
        auto fibAmount = static_cast<int64_t>(fib(rankIndex + newrew.fib_offset));
        auto respect_amount = static_cast<int64_t>(fibAmount * std::pow(10, eden_symbol.precision()));
        
        members_t members(get_self(), get_self().value);
        auto mem_itr = members.find(acc.value);
        check(mem_itr != members.end(), "Account not found in members table");

        // Update the user's respect in members table
        members.modify(mem_itr, _self, [&](auto &row) {
          row.respect += respect_amount;
        });

        auto eosAmount = eosRewards[rankIndex];
        if (eosAmount > 0) {
          claimable_t claimables(get_self(), acc.value);
          auto claim_itr = claimables.find(current_symbol.code().raw());

          if (claim_itr != claimables.end()) {
            // Modify existing claimable balance
            claimables.modify(claim_itr, _self, [&](auto &row) {
              row.claimable.amount += eosAmount;
            });
          } else {
            // Create new claimable balance entry
            claimables.emplace(_self, [&](auto &row) {
              row.claimable = asset{eosAmount, current_symbol};
            });
          }
        }

        ++rankIndex;
      }
    }

    // Reduce the total reward in the rewards table by the distributed amount
    rew_itr = rewards.erase(rew_itr);  // erase and move to the next entry
  }
}
*/

/*void awfractal::submitranks(const AllRankings &ranks) {
  rewards_t rewards(get_self(), get_self().value);

  for (const auto& reward_entry : rewards) {
    auto availableReward = reward_entry.quantity.amount;
    if (availableReward <= 0) {
      continue;  // Skip the iteration if there are no rewards available for this token.
    }

    auto coeffSum = std::accumulate(std::begin(polyCoeffs), std::end(polyCoeffs), 0.0);

    // Calculate the EOS (or other token) per coefficient.
    auto multiplier = static_cast<double>(availableReward) / (numGroups * coeffSum);

    std::vector<int64_t> tokenRewards;
    std::transform(std::begin(polyCoeffs), std::end(polyCoeffs),
                   std::back_inserter(tokenRewards), [&](const auto &c) {
                       auto finalEosQuant = static_cast<int64_t>(multiplier * c);
                       return finalEosQuant;
                   });

    // If the reward for any coefficient is zero, skip distribution for this token.
    if (std::any_of(tokenRewards.begin(), tokenRewards.end(), [](int64_t val) { return val == 0; })) {
      continue;
    }

    for (const auto &rank : ranks.allRankings) {
      size_t group_size = rank.ranking.size();
      auto rankIndex = max_group_size - group_size;
      for (const auto &acc : rank.ranking) {
        auto fibAmount = static_cast<int64_t>(fib(rankIndex + newrew.fib_offset));
        auto respect_amount = static_cast<int64_t>(fibAmount * std::pow(10, eden_symbol.precision()));

        members_t members(get_self(), get_self().value);
        auto mem_itr = members.find(acc.value);
        check(mem_itr != members.end(), "Account not found in members table");

        // Update the user's respect in members table
        members.modify(mem_itr, _self, [&](auto &row) {
          row.respect += respect_amount;
        });

        auto tokenAmount = tokenRewards[rankIndex];
        if (tokenAmount > 0) {
          claimable_t claimables(get_self(), acc.value);
          auto claim_itr = claimables.find(reward_entry.quantity.symbol.code().raw());

          if (claim_itr != claimables.end()) {
            // Modify existing claimable balance
            claimables.modify(claim_itr, _self, [&](auto &row) {
              row.claimable.amount += tokenAmount;
            });
          } else {
            // Create new claimable balance entry
            claimables.emplace(_self, [&](auto &row) {
              row.claimable = asset{tokenAmount, reward_entry.quantity.symbol};
            });
          }
        }

        ++rankIndex;
      }
    }

    // Reduce the total reward in the rewards table by the distributed amount
    rewards.modify(reward_entry, _self, [&](auto &row) {
      row.quantity.amount -= availableReward;
    });
  }
}
*/