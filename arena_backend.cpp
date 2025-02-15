#include "stdafx.h"
#include "questmanager.h"
#include "char.h"
#include "char_manager.h"
#include "party.h"
#include "start_position.h"
#include <iostream>

namespace quest
{
    std::queue<std::string> arena2v2Queue;

    struct TeamStats
    {
        int team1Kills;
        int team2Deaths;
        DWORD party1LeaderVID;
        DWORD party1MemberVID;
        DWORD party2LeaderVID;
        DWORD party2MemberVID;

        std::string party1LeaderName; 
        std::string party1MemberName; 
        std::string party2LeaderName; 
        std::string party2MemberName; 

        TeamStats() : team1Kills(0), team2Deaths(0),
                    party1LeaderVID(0), party1MemberVID(0),
                    party2LeaderVID(0), party2MemberVID(0),
                    party1LeaderName(""), party1MemberName(""),
                    party2LeaderName(""), party2MemberName("") {}
    };

    std::unordered_map<DWORD, TeamStats> arenaStats;  

    ALUA(arena_2v2_add_to_queue)
    {
        const char* name = lua_tostring(L, 1);
        arena2v2Queue.push(name);
        return 0;
    }

    ALUA(arena_2v2_get_from_queue)
    {
        if (arena2v2Queue.empty())
        {
            lua_pushstring(L, "");
        }
        else
        {
            std::string name = arena2v2Queue.front();
            arena2v2Queue.pop();
            lua_pushstring(L, name.c_str());
        }
        return 1;
    }

    ALUA(arena_2v2_register_dungeon)
    {
        DWORD mapIndex = lua_tonumber(L, 1);  
        DWORD party1LeaderVID = lua_tonumber(L, 3);
        DWORD party1MemberVID = lua_tonumber(L, 2);
        DWORD party2LeaderVID = lua_tonumber(L, 5);
        DWORD party2MemberVID = lua_tonumber(L, 4);

        const char* party1LeaderName = lua_tostring(L, 6);  
        const char* party1MemberName = lua_tostring(L, 7);  
        const char* party2LeaderName = lua_tostring(L, 8);  
        const char* party2MemberName = lua_tostring(L, 9);  

        if (arenaStats.find(mapIndex) != arenaStats.end())
        {
            if (EventNotRunning)
                return 0;
        }

        TeamStats stats;
        stats.party1LeaderVID = party1LeaderVID;
        stats.party1MemberVID = party1MemberVID;
        stats.party2LeaderVID = party2LeaderVID;
        stats.party2MemberVID = party2MemberVID;
        

        stats.party1LeaderName = party1LeaderName; 
        stats.party1MemberName = party1MemberName; 
        stats.party2LeaderName = party2LeaderName; 
        stats.party2MemberName = party2MemberName; 

        //sys_log(0, "[DEBUG] REGISTERED THE NAMES SUCCESSFULLY ON MAP %d", mapIndex);

        arenaStats[mapIndex] = stats;

        return 0;
    }

    ALUA(arena_2v2_queue_size)
    {
        lua_pushnumber(L, arena2v2Queue.size());
        //sys_log(0, "Current 2v2 queue size: %d", arena2v2Queue.size());
        return 1;
    }

    ALUA(arena_2v2_on_kill)
    {
        const char* killerName = lua_tostring(L, 1);  
        DWORD mapIndex = lua_tonumber(L, 2);         

        // Verify the map index exists in arenaStats
        if (arenaStats.find(mapIndex) == arenaStats.end())
        {
            //sys_log(0, "[DEBUG] No stats found for map index %d", mapIndex);
            return 0;
        }

        TeamStats& stats = arenaStats[mapIndex]; // Retrieve the stats for this dungeon
        //sys_log(0, "[DEBUG] arena_2v2_on_kill called. MapIndex=%d, KillerName=%s", mapIndex, killerName);

        // Lambda to handle win logic
        auto handleWin = [](const char* leaderName, const char* memberName, const char* teamName, bool isWinner) {
            //sys_log(0, "%s wins! Awarding items and warping players to the city.", teamName);

            // Find leader by name
            if (LPCHARACTER leaderChar = CHARACTER_MANAGER::instance().FindPC(leaderName))  // Assuming a function to find by name
            {
                if (isWinner) {
                    leaderChar->AutoGiveItem(60003); // Give item only to winners
                }
                leaderChar->WarpSet(ARENA_RETURN_POINT_X(leaderChar->GetEmpire()), ARENA_RETURN_POINT_Y(leaderChar->GetEmpire())); // Teleport
                //sys_log(0, "[DEBUG] %s Leader (%s) rewarded and warped.", teamName, leaderName);
            }
            else
            {
                //sys_log(0, "[ERROR] Leader not found for %s. LeaderName=%s", teamName, leaderName);
            }

            // Process member by name
            if (LPCHARACTER memberChar = CHARACTER_MANAGER::instance().FindPC(memberName))  // Assuming a function to find by name
            {
                if (isWinner) {
                    memberChar->AutoGiveItem(60003); // Give item only to winners
                }
                memberChar->WarpSet(ARENA_RETURN_POINT_X(memberChar->GetEmpire()), ARENA_RETURN_POINT_Y(memberChar->GetEmpire())); // Teleport
                //sys_log(0, "[DEBUG] %s Member (%s) rewarded and warped.", teamName, memberName);
            }
            else
            {
                //sys_log(0, "[ERROR] Member not found for %s. MemberName=%s", teamName, memberName);
            }

            // Delete the party after processing the win
            if (LPCHARACTER leaderChar = CHARACTER_MANAGER::instance().FindPC(leaderName)) {
                if (leaderChar->GetParty()) {
                    CPartyManager::instance().DeleteParty(leaderChar->GetParty());
                }
            }
            if (LPCHARACTER memberChar = CHARACTER_MANAGER::instance().FindPC(memberName)) {
                if (memberChar->GetParty()) {
                    CPartyManager::instance().DeleteParty(memberChar->GetParty());
                }
            }
        };

        // Check if the killer belongs to team 1
        if (killerName == stats.party1LeaderName || killerName == stats.party1MemberName)
        {
            stats.team1Kills++;
            //sys_log(0, "[DEBUG] Team 1 kills updated: %d", stats.team1Kills);

            if (stats.team1Kills >= 5)
            {
                //sys_log(0, "[INFO] Team 1 reaches 5 kills. Triggering win condition.");
                handleWin(stats.party1LeaderName.c_str(), stats.party1MemberName.c_str(), "Team 1", true); // Winners
                handleWin(stats.party2LeaderName.c_str(), stats.party2MemberName.c_str(), "Team 2", false); // Losers
                arenaStats.erase(mapIndex); // Clear stats after win
            }
        }
        // Check if the killer belongs to team 2
        else if (killerName == stats.party2LeaderName || killerName == stats.party2MemberName)
        {
            stats.team2Deaths++;
            //sys_log(0, "[DEBUG] Team 2 deaths updated: %d", stats.team2Deaths);

            if (stats.team2Deaths >= 5)
            {
                //sys_log(0, "[INFO] Team 2 reaches 5 kills. Triggering win condition.");
                handleWin(stats.party2LeaderName.c_str(), stats.party2MemberName.c_str(), "Team 2", true); // Winners
                handleWin(stats.party1LeaderName.c_str(), stats.party1MemberName.c_str(), "Team 1", false); // Losers
                arenaStats.erase(mapIndex); // Clear stats after win
            }
        }
        else
        {
            //sys_log(0, "[DEBUG] KillerName=%s does not belong to either team.", killerName);
        }

        return 0;
    }





    ALUA(arena_2v2_create_parties)
    {
        const char* player1 = lua_tostring(L, 1);
        const char* player2 = lua_tostring(L, 2);
        const char* player3 = lua_tostring(L, 3);
        const char* player4 = lua_tostring(L, 4);

        LPCHARACTER ch1 = CHARACTER_MANAGER::instance().FindPC(player1);
        LPCHARACTER ch2 = CHARACTER_MANAGER::instance().FindPC(player2);
        LPCHARACTER ch3 = CHARACTER_MANAGER::instance().FindPC(player3);
        LPCHARACTER ch4 = CHARACTER_MANAGER::instance().FindPC(player4);

        if (ch1 && ch1->GetParty()) CPartyManager::instance().DeleteParty(ch1->GetParty());
        if (ch2 && ch2->GetParty()) CPartyManager::instance().DeleteParty(ch2->GetParty());
        if (ch3 && ch3->GetParty()) CPartyManager::instance().DeleteParty(ch3->GetParty());
        if (ch4 && ch4->GetParty()) CPartyManager::instance().DeleteParty(ch4->GetParty());

        // Ensure all players are found before proceeding
        if (ch1 && ch2 && ch3 && ch4)
        {
            // Create the first party with player1 as the leader
            LPPARTY party1 = CPartyManager::instance().CreateParty(ch1);
            if (party1)
            {
                //sys_log(0, "Party 1 created with %s as the leader.", player1);
                // Add player2 to Party 1 using P2PJoinParty
                CPartyManager::instance().P2PJoinParty(ch1->GetPlayerID(), ch2->GetPlayerID(), PARTY_ROLE_NORMAL);
                //sys_log(0, "%s added to Party 1.", player2);
            }
            else
            {
                //sys_log(0, "Failed to create Party 1.");
                lua_pushnumber(L, 0);  // Return 0 for failure
                return 1;
            }

            // Create the second party with player3 as the leader
            LPPARTY party2 = CPartyManager::instance().CreateParty(ch3);
            if (party2)
            {
                //sys_log(0, "Party 2 created with %s as the leader.", player3);
                // Add player4 to Party 2 using P2PJoinParty
                CPartyManager::instance().P2PJoinParty(ch3->GetPlayerID(), ch4->GetPlayerID(), PARTY_ROLE_NORMAL);
                //sys_log(0, "%s added to Party 2.", player4);
            }
            else
            {
                //sys_log(0, "Failed to create Party 2.");
                lua_pushnumber(L, 0);  // Return 0 for failure
                return 1;
            }

            // Retrieve the PIDs
            DWORD party1LeaderVID = ch1->GetVID();
            DWORD party1MemberVID = ch2->GetVID();
            DWORD party2LeaderVID = ch3->GetVID();
            DWORD party2MemberVID = ch4->GetVID();

            // Push the four PIDs to the Lua stack: party 1 leader, party 1 member, party 2 leader, party 2 member
            lua_pushnumber(L, party1LeaderVID);  // Party 1's leader PID
            lua_pushnumber(L, party1MemberVID);  // Party 1's member PID (player 2)
            lua_pushnumber(L, party2LeaderVID);  // Party 2's leader PID
            lua_pushnumber(L, party2MemberVID);  // Party 2's member PID (player 4)

            return 4;  // Return four values
        }
        else
        {
            lua_pushnumber(L, 0);  // Return 0 for failure
            return 1;
        }
    }


    void RegisterArena2v2FunctionTable()
    {
        //sys_log(0, "Registering arena_2v2 functions...");

        luaL_reg arena_2v2_functions[] =
        {
            {"add_to_queue", arena_2v2_add_to_queue},
            {"get_from_queue", arena_2v2_get_from_queue},
            {"queue_size", arena_2v2_queue_size},
            {"create_parties", arena_2v2_create_parties},
            {"on_kill", arena_2v2_on_kill},  // Add the kill handling function
            {"register_dungeon", arena_2v2_register_dungeon},  // Add the dungeon registration function
            {NULL, NULL}
        };

        CQuestManager::instance().AddLuaFunctionTable("arena_2v2", arena_2v2_functions);

        //sys_log(0, "arena_2v2 functions registered.");
    }
}
