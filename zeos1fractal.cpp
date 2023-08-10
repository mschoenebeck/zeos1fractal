#include "zeos1fractal.hpp"

zeos1fractal::zeos1fractal(name self, name code, datastream<const char *> ds):
    contract(self, code, ds),
    _global(_self, _self.value)
{
}

void zeos1fractal::init(const uint64_t& first_event_block_height)
{
    require_auth(_self);
    check(first_event_block_height > static_cast<uint64_t>(current_block_number()), "first event must take place in the future");

    _global.set({
        STATE_IDLE,
        0,
        first_event_block_height,
        map<name, uint64_t>({
            {"approver"_n, 56},
            {"delegate"_n, 111},
        }),
        5400,   // 45 min
        1800,   // 15 min
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
            check(cbn >= (g.next_event_block_height - g.registration_duration), "too early to move into REGISTRATION state");
            g.state = STATE_REGISTRATION;
        }
        break;
            
        case STATE_REGISTRATION:
        {
            check(cbn >= g.next_event_block_height, "too early to move into BREAKOUT ROOMS state");
            g.state = STATE_BREAKOUT_ROOMS;

            // TODO:
            // randomize users in "participants" into groups for breakout rooms
        }
        break;
            
        case STATE_BREAKOUT_ROOMS:
        {
            check(cbn >= (g.next_event_block_height + g.breakout_rooms_duration), "too early to move into IDLE state");
            g.state = STATE_IDLE;
            g.next_event_block_height = g.next_event_block_height + 1209600; // add one week of blocks
            g.event_count++;

            // TODO: 
            // evaluate event and distribute REZPECT
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

    // TODO
}

void zeos1fractal::apply(const name& user, const string& why, const string& about, const vector<tuple<name, string>>& links)
{
    require_auth(user);

    // TODO
}

void zeos1fractal::approve(const name& user, const name& user_to_approve)
{
    require_auth(user);

    // TODO
}

void zeos1fractal::participate(const name& user)
{
    require_auth(user);
    check(_global.exists(), "contract not initialized");
    auto g = _global.get();
    check(g.state == STATE_REGISTRATION, "can only join during REGISTRATION phase");
    
    members_t members(_self, _self.value);
    auto usr = members.find(user.value);
    check(usr != members.end(), "user is not a member");
    check(usr->is_approved, "user is not approved");

    participants_t participants(_self, _self.value);
    auto p = participants.find(user.value);
    check(p == participants.end(), "user is already participating");

    participants.emplace(_self, [&](auto& row){
        row.user = user;
    });
}

void zeos1fractal::submitranks(const name& user, const uint64_t& group_id, const vector<name> &rankings)
{
    require_auth(user);

    // TODO
}