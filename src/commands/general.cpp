#include "player.hpp"

#include "../util.hpp"
#include "../map.hpp"
#include "../npc.hpp"
#include "../eoplus.hpp"
#include "../packet.hpp"
#include "../player.hpp"
#include "../quest.hpp"
#include "../world.hpp"
#include "../eoclient.hpp"


namespace PlayerCommands
{
    void Fame(const std::vector<std::string>& arguments, Character* from)
    {
        from->ServerMsg("Fame: " + util::to_string(from->fame) + " Best Fame: " + util::to_string(from->bestfame));
    }

    void Join(const std::vector<std::string>& arguments, Character* from)
    {

        int death = 1;
        int trial3 = 4;
        int trial8 = 14;
        int trial16 = 17;
        std::list<int> list;

        UTIL_FOREACH(from->world->characters, character)
        {        
            if(character->mapid == static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Joinm"]))
            {
                list.push_back(character->player->client->GetRemoteAddr());
            }       
        }

        if(std::find(list.begin(), list.end(), from->player->client->GetRemoteAddr()) != list.end())
        {
            from->ServerMsg("You already have one of your characters in this dungeon.");
            return;
        }

        if (from->level < static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Level"]))
        {
        
        from->ServerMsg("You need to be Level " + util::to_string(static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Level"])) + " to join " + std::string(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Name"]));

        return;
        }

        if (from->mapid == death)
        {
        
        from->ServerMsg("You can't join " + std::string(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Name"]) + " from the death realm!");

        return;
        }
        if (from->mapid == trial3 || from->mapid == trial8 || from->mapid == trial16)
        {
        
        from->ServerMsg("You can't join " + std::string(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Name"]) + " on Trial!");

        return;
        }

        if (from->world->DungeonEnabled == true)
        {
            from->indungeon = true;
            from->Warp(static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Joinm"]), static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Joinx"]), static_cast<int>(from->world->dungeons_config[util::to_string(from->world->currentdungeon) + ".Joiny"]), from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);
        }
        else
        {
        from->ServerMsg("Unable to join. No active event!");
        }
    }

    void Leave(const std::vector<std::string>& arguments, Character* from)
    {

        if (from->indungeon == false)
        {
        from->ServerMsg("You need to be in a Dungeon to use this command");
        }
        else if (from->indungeon == true && from->world->SecretDungeon == true && from->world->SwitchingFloor == true)
        {
            from->indungeon = false;
            from->Warp(2, 9, 11, from->world->config["WarpBubbles"] ? WARP_ANIMATION_ADMIN : WARP_ANIMATION_NONE);        
        }
        else
        {
        from->ServerMsg("Unable to leave. Complete the floor first.");
        }
    }

    PLAYER_COMMAND_HANDLER_REGISTER()
        using namespace std::placeholders;
        RegisterCharacter({"fame"}, Fame);
        RegisterCharacter({"join"}, Join);
        RegisterCharacter({"leave"}, Leave);
    PLAYER_COMMAND_HANDLER_REGISTER_END()
}
