quest arena_2v2_manager begin
    state start begin

        when 70058.use begin
            local player_name = pc.get_name()
            if queue == nil then
                return   
            else
                remove_from_queue(player_name)
            end
        end

        when logout begin
            local player_name = pc.get_name()
            remove_from_queue(player_name)
        end

        when 20011.chat."Join 2v2 Match" begin
            -- Initialize queue if not already done
            if queue == nil then
                queue = {}
            end

            function remove_from_queue(player_name)
                for index, name in ipairs(queue) do
                    if name == player_name then
                        table.remove(queue, index)
                        return
                    end
                end
            end

            -- Add player to queue if not already in it
            local player_name = pc.get_name()
            local already_in_queue = false

            -- Check if player is already in the queue
            for _, name in ipairs(queue) do
                if name == player_name then
                    already_in_queue = true
                    break
                end
            end

            -- Handle duplicate registration
            if already_in_queue then
                syschat("You are already in the 2v2 queue.")
                say("You are already in the 2v2 queue.")
            else
                -- Add player to queue
                table.insert(queue, player_name)
                syschat("Player " .. player_name .. " added to the 2v2 queue.")
                say("Player " .. player_name .. " added to the 2v2 queue.")
            end

            -- Show the current queue size using table.getn()
            local queue_size = table.getn(queue)
            syschat("Current 2v2 queue size: " .. queue_size)
            say("Current 2v2 queue size: " .. queue_size)

            -- If queue has 4 players, notify and clear the queue
            if queue_size == 4 then
                npc.lock()
                syschat("Queue is full! Creating parties...")
                say("Queue is full! Creating parties...")

                -- Call the function to create parties
                party1LeaderVID, party1MemberVID, party2LeaderVID, party2MemberVID = arena_2v2.create_parties(queue[1], queue[2], queue[3], queue[4])
                -- Warp The first player
                pc.select(party1LeaderVID)
                local ownerName = pc.get_name()
                d.new_jump( 450 , 1434080 , 1434380 )
                -- Fetch the dungeon new map index
                local newDungeonIndex = d.get_map_index()
                -- Push newDungeonIndex into the config indexes and set kills and deaths to 0
                notice_all("New dungeon index: " .. newDungeonIndex)
                pc.select(party1LeaderVID)
                local party1LeaderName = pc.get_name()
                pc.select(party1MemberVID)
                local party1MemberName = pc.get_name()
                pc.select(party2LeaderVID)
                local party2LeaderName = pc.get_name()
                pc.select(party2MemberVID)
                local party2MemberName = pc.get_name()
                arena_2v2.register_dungeon(newDungeonIndex, party1LeaderVID, party1MemberVID, party2LeaderVID, party2MemberVID, party1LeaderName, party1MemberName, party2LeaderName, party2MemberName)
                
                -- Select and warp other players
                pc.select(party2MemberVID)
                pc.warp(1435580, 1435880, newDungeonIndex)
                pc.select(party2LeaderVID)
                pc.warp(1435580, 1435880, newDungeonIndex)
                pc.select(party1MemberVID)
                pc.warp(1434080, 1434380, newDungeonIndex)
                
                -- Clear the queue after the match is set up
                queue = {}
                npc.unlock()
            end
        end

        when kill with pc.in_dungeon() begin
            killerName = pc.get_name()
            mapIndex = pc.get_map_index()
        
            -- Send the kill event to C++ backend
            arena_2v2.on_kill(killerName, mapIndex)
        end        

    end
end
