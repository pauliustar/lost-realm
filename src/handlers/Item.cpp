/* handlers/Item.cpp
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../party.hpp"
#include "../quest.hpp"
#include "../world.hpp"
#include "../console.hpp"

#include "../util.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

namespace Handlers
{

static std::list<int> ExceptUnserialize(std::string serialized)
{
    std::list<int> list;

    std::size_t p = 0;
    std::size_t lastp = std::numeric_limits<std::size_t>::max();

    if (!serialized.empty() && *(serialized.end()-1) != ',')
    {
        serialized.push_back(',');
    }

    while ((p = serialized.find_first_of(',', p+1)) != std::string::npos)
    {
        list.push_back(util::to_int(serialized.substr(lastp + 1, p-lastp - 1)));
        lastp = p;
    }

    return list;
}	

// Player using an item
void Item_Use(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	int id = reader.GetShort();

	if (character->HasItem(id))
	{
		const EIF_Data& item = character->world->eif->Get(id);
		PacketBuilder reply(PACKET_ITEM, PACKET_REPLY, 3);
		reply.AddChar(item.type);
		reply.AddShort(id);

		auto QuestUsedItems = [](Character* character, int id)
		{
			UTIL_FOREACH(character->quests, q)
			{
				if (!q.second || q.second->GetQuest()->Disabled())
					continue;

				q.second->UsedItem(id);
			}
		};

        for (int i = 0 ; i < int(character->world->buffitems_config["Amount"]); i++)
        {
            if (id == int(character->world->buffitems_config[util::to_string(i+1) + ".BuffItem"]))
            {
                int effect = character->world->buffitems_config[util::to_string(i+1) + ".BuffEffect"];
                int str = character->world->buffitems_config[util::to_string(i+1) + ".BuffSTR"];
                int lnt = character->world->buffitems_config[util::to_string(i+1) + ".BuffINT"];
                int wis = character->world->buffitems_config[util::to_string(i+1) + ".BuffWIS"];
                int agi = character->world->buffitems_config[util::to_string(i+1) + ".BuffAGI"];
                int con = character->world->buffitems_config[util::to_string(i+1) + ".BuffCON"];
                int cha = character->world->buffitems_config[util::to_string(i+1) + ".BuffCHA"];
                int time = character->world->buffitems_config[util::to_string(i+1) + ".BuffTime"];
                double exp = character->world->buffitems_config[util::to_string(i+1) + ".EXPRate"];
                double drop = character->world->buffitems_config[util::to_string(i+1) + ".DropRate"];

                if (character->boosttimer < Timer::GetTime())
                {
                    character->boosttimer = Timer::GetTime() + int(time);

                    if (effect > 0) character->boosteffect = effect;
                    if (str > 0) character->booststr += str;
                    if (lnt > 0) character->boostint += lnt;
                    if (wis > 0) character->boostwis += wis;
                    if (agi > 0) character->boostagi += agi;
                    if (con > 0) character->boostcon += con;
                    if (cha > 0) character->boostcha += cha;
                    if (exp > 0) character->boostexp = exp;
                    if (drop > 0) character->boostdrop = drop;

					character->DelItem(id, 1);

					reply.ReserveMore(14);
					reply.AddInt(character->HasItem(id));
					reply.AddChar(character->weight);
					reply.AddChar(character->maxweight);

					character->Send(reply);

                    character->CalculateStats();
                    character->PacketRecover();
                    character->UpdateStats();

                    character->ServerMsg("You have been buffed for " + util::to_string(time) + " seconds.");
                    character->Effect(14);
                }
                else
                {
                    character->ServerMsg("You are already buffed!");
                }
            }
        }

        if (id == int(character->world->buffitems_config["UndoBuffItem"]))
            character->UndoBuff();
     
        for (int i = 0 ; i < static_cast<int>(character->world->gearbox_config["Amount"]); i++) // Gearbox
        {
            int GearID = static_cast<int>(character->world->gearbox_config[util::to_string(i+1) + ".GearID"]);
            int GearLevel = static_cast<int>(character->world->gearbox_config[util::to_string(i+1) + ".GearLevel"]);

            if (id == GearID)
            {
            	if (character->level >= GearLevel)
            	{
	                for (int ii = 1; std::string(character->world->gearbox_config[util::to_string(i+1) + ".RewardID" + util::to_string(ii)]) != "0"; ++ii)
	                {
	                	short item_id = std::stoi(std::string(character->world->gearbox_config[util::to_string(i+1) + ".RewardID" + util::to_string(ii)]));
	                	EIF_Data& item_info = character->world->eif->Get(item_id);
	                	if (item_info.type != EIF::Armor)
	                	{
	                		character->GiveItem(item_id, 1);
	                	}
	                	else if (item_info.type == EIF::Armor && item_info.gender == character->gender)
	                	{
	                    	character->GiveItem(item_id, 1);
	                	}
	                }

	                character->DelItem(id, 1);

					PacketBuilder builder(PACKET_ITEM, PACKET_JUNK, 11);
					builder.AddShort(id);
					builder.AddThree(1); // Overflows, does it matter?
					builder.AddInt(character->HasItem(id));
					builder.AddChar(character->weight);
					builder.AddChar(character->maxweight);
					character->Send(builder);
            	}
            	else
            	{
            		character->ServerMsg("This Gearbox requires a higher level.");
            	}
            }
        }     	

		for (int i = 0; i < static_cast<int>(character->world->giftbox_config["Amount"]); i++) // Giftbox / Lootbag
		{
		    int GiftID = static_cast<int>(character->world->giftbox_config[util::to_string(i + 1) + ".GiftID"]);
		    int Random = util::rand(1, 100);

		    if (id == GiftID)
		    {
		        // Create vectors to store the item IDs and their corresponding chances
		        std::vector<short> item_ids;
		        std::vector<int> item_chances;
		        std::vector<int> item_amounts;

		        for (int ii = 1;; ++ii)
		        {
		            std::string reward_id_str = character->world->giftbox_config[util::to_string(i + 1) + ".RewardID" + util::to_string(ii)];
		            if (reward_id_str == "0")
		            {
		                break; // End of rewards for this GiftID
		            }

		            short item_id = std::stoi(reward_id_str);
		            int item_chance = std::stoi(character->world->giftbox_config[util::to_string(i + 1) + ".ChanceID" + util::to_string(ii)]);
					int item_amount = std::stoi(character->world->giftbox_config[util::to_string(i + 1) + ".AmountID" + util::to_string(ii)]);

		            item_ids.push_back(item_id);
		            item_chances.push_back(item_chance);
		            item_amounts.push_back(item_amount);
		        }

		        // Select a winning item based on the chances
		        short winning_item_id = 0;
		        int winning_item_amount = 0;
		        int total_chances = 0;
		        for (int chance : item_chances)
		        {
		            total_chances += chance;
		        }

		        int roll = util::rand(1, total_chances);
		        int accumulated_chances = 0;

		        for (size_t idx = 0; idx < item_ids.size(); ++idx)
		        {
		            accumulated_chances += item_chances[idx];
		            if (roll <= accumulated_chances)
		            {
		                winning_item_id = item_ids[idx];
		                winning_item_amount = item_amounts[idx];
		                break;
		            }
		        }

		        if (winning_item_id != 0)
		        {
		        	const EIF_Data& item = character->world->eif->Get(winning_item_id);
		        	character->ServerMsg("You have found x" + util::to_string(winning_item_amount) + " " + item.name);
		            character->GiveItem(winning_item_id, winning_item_amount);
		        }

		        character->DelItem(id, 1);

	        	PacketBuilder builder(PACKET_ITEM, PACKET_JUNK, 11);
				builder.AddShort(id);
				builder.AddThree(1); // Overflows, does it matter?
				builder.AddInt(character->HasItem(id));
				builder.AddChar(character->weight);
				builder.AddChar(character->maxweight);
				character->Send(builder);
		    }
		}

		switch (item.type)
		{
			case EIF::Title:
			{

				std::string newTitleString;

				PacketBuilder builder(PACKET_ITEM, PACKET_REPORT, 2 + 1 + 12);

				builder.AddShort(id);
				builder.AddByte(255);
				builder.AddBreakString(newTitleString);

				character->Send(builder);

			}
			break;

			case EIF::Spell:
			{

				character->DelItem(id, 1);

				character->race = Skin(item.dollgraphic);

				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				character->Send(reply);

				PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 5);

				builder.AddShort(character->PlayerID());
				builder.AddChar(4); // mutation type
				builder.AddChar(1); // sound
				builder.AddChar(item.dollgraphic); // skin ID

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				character->Send(builder);
 
			}
			break;

			case EIF::Teleport:
			{
				if (!character->map->scroll)
					break;

				if (item.scrollmap == 0)
				{
					if (character->mapid == character->SpawnMap() && character->x == character->SpawnX() && character->y == character->SpawnY())
						break;
				}
				else
				{
					if (character->mapid == item.scrollmap && character->x == item.scrollx && character->y == item.scrolly)
						break;
				}

				// Dungeons

				std::list<int> allowed_list = ExceptUnserialize(character->world->dungeons_config["DungeonScrolls"]);
		    	if (std::find(allowed_list.begin(), allowed_list.end(), item.scrollmap) != allowed_list.end())
		        {				
	            	if(character->world->DungeonEnabled == false && character->world->SecretDungeon == false)
	            	{
	            		int level = static_cast<int>(character->world->dungeons_config[util::to_string(item.scrollmap) + ".Level"]);
	            		if (character->level >= static_cast<int>(character->world->dungeons_config[util::to_string(item.scrollmap) + ".Level"]))
	            		{
	            			character->world->currentdungeon = item.scrollmap;
		            		character->world->DungeonTimer();
		            		character->indungeon = true;
			            	std::string name = character->world->dungeons_config[util::to_string(item.scrollmap) + ".Name"];
						    std::string message = "You have started a dungeon. Please wait in lobby while the game starts.";

							PacketBuilder builder(PACKET_TALK, PACKET_TELL, 2 + name.length() + message.length());
					        builder.AddBreakString(name);
					        builder.AddBreakString(message);
					        character->Send(builder);
	            		}
	            		else
	            		{
			            	std::string name = "[Dungeon]";
			   				std::string message = "You do not meet the required level to start this dungeon! Needed level: " + util::to_string(static_cast<int>(character->world->dungeons_config[util::to_string(item.scrollmap) + ".Level"]));

							PacketBuilder builder(PACKET_TALK, PACKET_TELL, 2 + name.length() + message.length());
		        			builder.AddBreakString(name);
		        			builder.AddBreakString(message);
		        			character->Send(builder);

		        			break;	            			
	            		}
	            	}
	            	else if((character->world->SecretDungeon == true && character->world->DungeonEnabled == false) || (character->world->SecretDungeon == false && character->world->DungeonEnabled == true))
	            	{
		            	std::string name = "[Dungeon]";
		   				std::string message = "Another dungeon is already in progress.. Please wait before trying again.";

						PacketBuilder builder(PACKET_TALK, PACKET_TELL, 2 + name.length() + message.length());
	        			builder.AddBreakString(name);
	        			builder.AddBreakString(message);
	        			character->Send(builder);

	        			break;
	            	}
	        	}

				character->DelItem(id, 1);

				reply.ReserveMore(6);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				if (item.scrollmap == 0)
				{
					character->Warp(character->SpawnMap(), character->SpawnX(), character->SpawnY(), WARP_ANIMATION_SCROLL);
				}
				else
				{
					character->Warp(item.scrollmap, item.scrollx, item.scrolly, WARP_ANIMATION_SCROLL);
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::Heal:
			{
				int hpgain = item.hp;
				int tpgain = item.tp;

				if (character->world->config["LimitDamage"])
				{
					hpgain = std::min(hpgain, character->maxhp - character->hp);
					tpgain = std::min(tpgain, character->maxtp - character->tp);
				}

				hpgain = std::max(hpgain, 0);
				tpgain = std::max(tpgain, 0);

				if (hpgain == 0 && tpgain == 0)
					break;

				character->hp += hpgain;
				character->tp += tpgain;

				if (!character->world->config["LimitDamage"])
				{
					character->hp = std::min(character->hp, character->maxhp);
					character->tp = std::min(character->tp, character->maxtp);
				}

				character->DelItem(id, 1);

				reply.ReserveMore(14);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddInt(hpgain);
				reply.AddShort(character->hp);
				reply.AddShort(character->tp);

				PacketBuilder builder(PACKET_RECOVER, PACKET_AGREE, 7);
				builder.AddShort(character->PlayerID());
				builder.AddInt(hpgain);
				builder.AddChar(util::clamp<int>(double(character->hp) / double(character->maxhp) * 100.0, 0, 100));

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				if (character->party)
				{
					character->party->UpdateHP(character);
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::HairDye:
			{
				if (character->haircolor == item.haircolor)
					break;

				character->haircolor = item.haircolor;

				character->DelItem(id, 1);

				reply.ReserveMore(7);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddChar(item.haircolor);

				PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 5);
				builder.AddShort(character->PlayerID());
				builder.AddChar(SLOT_HAIRCOLOR);
				builder.AddChar(0); // subloc
				builder.AddChar(item.haircolor);

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::Beer:
			{

				character->DelItem(id, 1);

				reply.ReserveMore(6);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				character->Send(reply);

				QuestUsedItems(character, id);
				
			}
			break;

			case EIF::EffectPotion:
			{
				if (id == 36)
	            {
	            	character->weaponbankopen = true;
	            	character->shieldbankopen = false;
	            	character->cosmeticsopen = false;
					PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->weaponbank.size() * 5);
					reply.AddChar(0);
					reply.AddChar(0);
					UTIL_FOREACH(character->weaponbank, item)
					{
						reply.AddShort(item.id);
						reply.AddThree(item.amount);
					}
					character->Send(reply);
				}
	            else if (id == 37)
	            {
	            	character->shieldbankopen = true;
	            	character->weaponbankopen = false;
	            	character->cosmeticsopen = false;
					PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->shieldbank.size() * 5);
					reply.AddChar(0);
					reply.AddChar(0);
					UTIL_FOREACH(character->shieldbank, item)
					{
						reply.AddShort(item.id);
						reply.AddThree(item.amount);
					}
					character->Send(reply);
				}
	            else if (id == 53)
	            {
	            	character->shieldbankopen = false;
	            	character->weaponbankopen = false;
	            	character->cosmeticsopen = true;
					PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->cosmeticsbank.size() * 5);
					reply.AddChar(0);
					reply.AddChar(0);
					UTIL_FOREACH(character->cosmeticsbank, item)
					{
						reply.AddShort(item.id);
						reply.AddThree(item.amount);
					}
					character->Send(reply);
				}
				else if (id == 79)
				{
					std::list<int> maps_list = ExceptUnserialize(character->world->dungeons_config["DungeonMaps"]);
			    	if (std::find(maps_list.begin(), maps_list.end(), character->mapid) != maps_list.end()) 
			    	{
			    		character->Warp(2, 18, 7, WARP_ANIMATION_ADMIN);

						character->DelItem(id, 1);

						reply.ReserveMore(8);
						reply.AddInt(character->HasItem(id));
						reply.AddChar(character->weight);
						reply.AddChar(character->maxweight);
						reply.AddShort(item.effect);

						character->Effect(item.effect, false);

						character->Send(reply);			    		
			    	}
		    		else
		    		{
		    			break;
		    		}
				}
				else
				{
					character->DelItem(id, 1);

					reply.ReserveMore(8);
					reply.AddInt(character->HasItem(id));
					reply.AddChar(character->weight);
					reply.AddChar(character->maxweight);
					reply.AddShort(item.effect);

					character->Effect(item.effect, false);

					character->Send(reply);

					QuestUsedItems(character, id);
				}
			}
			break;

			case EIF::CureCurse:
			{
				bool found = false;

				for (std::size_t i = 0; i < character->paperdoll.size(); ++i)
				{
					if (character->world->eif->Get(character->paperdoll[i]).special == EIF::Cursed)
					{
						character->paperdoll[i] = 0;
						found = true;
					}
				}

				if (!found)
					break;

				character->CalculateStats();

				character->DelItem(id, 1);

				reply.ReserveMore(32);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddShort(character->maxhp);
				reply.AddShort(character->maxtp);

				reply.AddShort(character->display_str);
				reply.AddShort(character->display_intl);
				reply.AddShort(character->display_wis);
				reply.AddShort(character->display_agi);
				reply.AddShort(character->display_con);
				reply.AddShort(character->display_cha);

				reply.AddShort(character->mindam);
				reply.AddShort(character->maxdam);
				reply.AddShort(character->accuracy);
				reply.AddShort(character->evade);
				reply.AddShort(character->armor);

				PacketBuilder builder(PACKET_AVATAR, PACKET_AGREE, 14);
				builder.AddShort(character->PlayerID());
				builder.AddChar(SLOT_CLOTHES);
				builder.AddChar(0); // sound
				character->AddPaperdollData(builder, "BAHWS");

				UTIL_FOREACH(character->map->characters, updatecharacter)
				{
					if (updatecharacter != character && character->InRange(updatecharacter))
					{
						updatecharacter->Send(builder);
					}
				}

				character->Send(reply);

				QuestUsedItems(character, id);
			}
			break;

			case EIF::EXPReward:
			{
				bool level_up = false;

				character->exp += item.expreward;

				character->exp = std::min(character->exp, static_cast<int>(character->map->world->config["MaxExp"]));

				while (character->level < static_cast<int>(character->map->world->config["MaxLevel"])
				 && character->exp >= character->map->world->exp_table[character->level+1])
				{
					level_up = true;
					++character->level;
					if (character->level > 5)
					{
						character->statpoints += static_cast<int>(character->map->world->config["StatPerLevel"]);
					}
					if (character->level % 2 == 0)
					{
						character->skillpoints += static_cast<int>(character->map->world->config["SkillPerLevel"]);
					}
				
					character->CalculateStats();
				}

				character->DelItem(id, 1);

				reply.ReserveMore(21);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				reply.AddInt(character->exp);

				reply.AddChar(level_up ? character->level : 0);
				reply.AddShort(character->statpoints);
				reply.AddShort(character->skillpoints);
				reply.AddShort(character->maxhp);
				reply.AddShort(character->maxtp);
				reply.AddShort(character->maxsp);

				if (level_up)
				{
					PacketBuilder builder(PACKET_RECOVER, PACKET_REPLY, 9);
					builder.AddInt(character->exp);
					builder.AddShort(character->karma);
					builder.AddChar(character->level);
					builder.AddShort(character->statpoints);
					builder.AddShort(character->skillpoints);

					UTIL_FOREACH(character->map->characters, check)
					{
						if (character != check && character->InRange(check))
						{
							PacketBuilder builder(PACKET_ITEM, PACKET_ACCEPT, 2);
							builder.AddShort(character->PlayerID());
							character->Send(builder);
						}
					}
				}

				character->Send(reply);

				if (level_up)
				{
					character->hp = character->maxhp;
					character->tp = character->maxtp;	
		            character->CalculateStats();
		            character->PacketRecover();
		            character->UpdateStats();
				}

				QuestUsedItems(character, id);
			}
			break;

			default:
				return;
		}
	}
}

// Drop an item on the ground
void Item_Drop(Character *character, PacketReader &reader)
{
	if (character->trading) return;
	if (!character->CanInteractItems()) return;

	int id = reader.GetShort();
	int amount;

	if (character->world->eif->Get(id).special == EIF::Lore)
	{
		return;
	}

	if (reader.Remaining() == 5)
	{
		amount = reader.GetThree();
	}
	else
	{
		amount = reader.GetInt();
	}
	unsigned char x = reader.GetByte(); // ?
	unsigned char y = reader.GetByte(); // ?

	amount = std::min<int>(amount, character->world->config["MaxDrop"]);

	if (amount <= 0)
	{
		return;
	}

	if (x == 255 && y == 255)
	{
		x = character->x;
		y = character->y;
	}
	else
	{
		x = PacketProcessor::Number(x);
		y = PacketProcessor::Number(y);
	}

	int distance = util::path_length(x, y, character->x, character->y);

	if (distance > static_cast<int>(character->world->config["DropDistance"]))
	{
		return;
	}

	if (!character->map->Walkable(x, y))
	{
		return;
	}

	if (character->HasItem(id) >= amount && character->mapid != static_cast<int>(character->world->config["JailMap"]))
	{
		std::shared_ptr<Map_Item> item = character->map->AddItem(id, amount, x, y, character);

		if (item)
		{
			item->owner = character->PlayerID();
			item->unprotecttime = Timer::GetTime() + static_cast<double>(character->world->config["ProtectPlayerDrop"]);
			character->DelItem(id, amount);

			PacketBuilder reply(PACKET_ITEM, PACKET_DROP, 15);
			reply.AddShort(id);
			reply.AddThree(amount);
			reply.AddInt(character->HasItem(id));
			reply.AddShort(item->uid);
			reply.AddChar(x);
			reply.AddChar(y);
			reply.AddChar(character->weight);
			reply.AddChar(character->maxweight);
			character->Send(reply);
		}
	}
}

// Destroying an item
void Item_Junk(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	int id = reader.GetShort();
	int amount = reader.GetInt();

	if (id == 36 || id == 37 || id == 53 || id == 109) return;

	if (amount <= 0)
		return;

	if (character->HasItem(id) >= amount)
	{
		character->DelItem(id, amount);

		PacketBuilder reply(PACKET_ITEM, PACKET_JUNK, 11);
		reply.AddShort(id);
		reply.AddThree(amount); // Overflows, does it matter?
		reply.AddInt(character->HasItem(id));
		reply.AddChar(character->weight);
		reply.AddChar(character->maxweight);
		character->Send(reply);
	}
}

// Retrieve an item from the ground
void Item_Get(Character *character, PacketReader &reader)
{
	int uid = reader.GetShort();

	std::shared_ptr<Map_Item> item = character->map->GetItem(uid);

	if (item)
	{
		int distance = util::path_length(item->x, item->y, character->x, character->y);

		if (distance > static_cast<int>(character->world->config["DropDistance"]))
		{
			return;
		}

		if (item->owner != character->PlayerID() && item->unprotecttime > Timer::GetTime())
		{
			return;
		}

		int taken = character->CanHoldItem(item->id, item->amount);

		if (taken > 0)
		{
			character->AddItem(item->id, taken);

			PacketBuilder reply(PACKET_ITEM, PACKET_GET, 9);
			reply.AddShort(uid);
			reply.AddShort(item->id);
			reply.AddThree(taken);
			reply.AddChar(character->weight);
			reply.AddChar(character->maxweight);
			character->Send(reply);

			character->map->DelSomeItem(item->uid, taken, character);
		}
	}
}


// Title Certificate logic
void Item_Report(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	int id = reader.GetShort();
	int amount = 1;

	reader.GetByte();

	std::string title = reader.GetEndString();

	if (amount <= 0)
		return;

	if (character->HasItem(id) >= 1)
	{
		character->DelItem(id, 1);

		character->title = title;

		PacketBuilder reply(PACKET_ITEM, PACKET_JUNK, 11);
		reply.AddShort(id);
		reply.AddThree(1); // Overflows, does it matter?
		reply.AddInt(character->HasItem(id));
		reply.AddChar(character->weight);
		reply.AddChar(character->maxweight);
		character->Send(reply);
	}
}

PACKET_HANDLER_REGISTER(PACKET_ITEM)
	Register(PACKET_USE, Item_Use, Playing);
	Register(PACKET_DROP, Item_Drop, Playing);
	Register(PACKET_JUNK, Item_Junk, Playing);
	Register(PACKET_GET, Item_Get, Playing);
	Register(PACKET_REPORT, Item_Report, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_ITEM)

}
