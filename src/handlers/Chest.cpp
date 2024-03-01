/* handlers/Chest.cpp
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../eodata.hpp"
#include "../map.hpp"
#include "../packet.hpp"
#include "../world.hpp"

#include "../util.hpp"

#include <algorithm>

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

void Chest_Add(Character *character, PacketReader &reader)
{
	if (character->trading) return;
	if (!character->CanInteractItems()) return;

	int x = reader.GetChar();
	int y = reader.GetChar();
	int id = reader.GetShort();
	int amount = reader.GetThree();

	if (character->world->eif->Get(id).special == EIF::Lore)
	{
		return;
	}

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::Chest)
		{
			UTIL_FOREACH(character->map->chests, chest)
			{
				if (chest->x == x && chest->y == y)
				{
					amount = std::min(amount, int(character->world->config["MaxChest"]) - chest->HasItem(id));

					if (character->HasItem(id) >= amount && chest->AddItem(id, amount))
					{
						character->DelItem(id, amount);
						chest->Update(character->map, character);

						PacketBuilder reply(PACKET_CHEST, PACKET_REPLY, 8 + chest->items.size() * 5);
						reply.AddShort(id);
						reply.AddInt(character->HasItem(id));
						reply.AddChar(character->weight);
						reply.AddChar(character->maxweight);

						UTIL_CIFOREACH(chest->items, item)
						{
							if (item->id != 0)
							{
								reply.AddShort(item->id);
								reply.AddThree(item->amount);
							}
						}

						character->Send(reply);
					}

					break;
				}
			}
		}
	}
	if (x == 12 && y == 20)
	{

		Map_Chest chest;
		chest.maxchest = 100;
		chest.chestslots = 1;
		chest.x = 12;
		chest.y = 20;
		chest.slots = 0;

		amount = std::min(amount, 100 - chest.HasItem(id));

		std::list<int> allowed_list = ExceptUnserialize(character->world->recycle_config["RecycleIDs"]);
    	if (std::find(allowed_list.begin(), allowed_list.end(), id) != allowed_list.end())
        {
			if (character->HasItem(id) >= amount && chest.AddItem(id, amount))
			{
				character->DelItem(id, amount);

				PacketBuilder reply(PACKET_CHEST, PACKET_REPLY, 8 + chest.items.size() * 5);
				reply.AddShort(id);
				reply.AddInt(character->HasItem(id));
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				character->Send(reply);

			    std::map<std::string, int> materialToItem = {
				    {"metal", 54},
				    {"wood", 57},
				    {"cloth", 56}
				};
				
				int successfulRecycles = 0;
				int failedRecycles = 0;
			    std::map<int, int> total_winning_items; // Map item ID to its total amount
			    std::vector<std::string> materials = {"metal", "wood", "cloth"};

				for (int recycleCount = 0; recycleCount < amount; ++recycleCount)
				{
					int recycleSuccess = 0;

				    for (const std::string& material : materials)
				    {
				        int chance = std::stoi(character->world->recycle_config[util::to_string(id) + "." + material + "chance"]);
				        int min_amount = std::stoi(character->world->recycle_config[util::to_string(id) + "." + material + "minamount"]);
				        int max_amount = std::stoi(character->world->recycle_config[util::to_string(id) + "." + material + "maxamount"]);

				        int roll = util::rand(1, 100);

					    if (roll <= chance && chance >= 0)
					    {
					        int winning_item_amount = util::rand(min_amount, max_amount);
					        int item_id = materialToItem[material]; // Get the corresponding item ID from the mapping
					        total_winning_items[item_id] += winning_item_amount;
					        recycleSuccess++;
					    }
				    }

				    if (recycleSuccess >= 1)
				    {
				    	successfulRecycles++;
				    }
				    else
				    {
				    	failedRecycles++;
				    }
				}

				PacketBuilder builder(PACKET_QUEST, PACKET_DIALOG, 250);
				builder.AddChar(1); // quest count
				builder.AddShort(445); // ?
				builder.AddShort(445);
				builder.AddInt(0);
				builder.AddByte(255);
				builder.AddShort(445); // Quest ID

				builder.AddBreakString("Recycler");

				builder.AddShort(1);

				std::string items_message = "Item(s) have been recycled.             ";
				items_message += "Successful recycles: " + util::to_string(successfulRecycles) + "                          ";
				items_message += "Failed recycles: " + util::to_string(failedRecycles)+ "                                ";

				if (!total_winning_items.empty())
				{
				    items_message += "You got:";
				    for (const auto& entry : total_winning_items)
				    {
				        int item_id = entry.first;
				        int item_amount = entry.second;
				        const EIF_Data& item = character->world->eif->Get(item_id);

				        items_message += " " + util::to_string(item_amount) + " " + item.name + ",";
				        if (item_amount > 0)
				            character->GiveItem(item_id, item_amount);
				    }

				    items_message.pop_back(); // Remove the last comma
				}
				else
				{
				    items_message += "Material extraction failed. You lost your item(s) in the recycling process.";
				}

				builder.AddBreakString(items_message);

				character->Send(builder);				

			}
        }
        else
        {
			    PacketBuilder builder(PACKET_QUEST, PACKET_DIALOG, 150);
				builder.AddChar(1); // quest count
				builder.AddShort(445); // ?
				builder.AddShort(445);
	            builder.AddInt(0);
	            builder.AddByte(255);
	            builder.AddShort(445); // Quest ID

	            builder.AddBreakString("Recycler");

	            builder.AddShort(1);
	            builder.AddBreakString("This item cannot be recycled!");

	            character->Send(builder);
        }


	}
}

// Taking an item from a chest
void Chest_Take(Character *character, PacketReader &reader)
{
	int x = reader.GetChar();
	int y = reader.GetChar();
	int id = reader.GetShort();

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::Chest)
		{
			UTIL_FOREACH(character->map->chests, chest)
			{
				if (chest->x == x && chest->y == y)
				{
					int amount = chest->HasItem(id);
					int taken = character->CanHoldItem(id, amount);

					if (taken > 0)
					{
						chest->DelSomeItem(id, taken);
						character->AddItem(id, taken);

						PacketBuilder reply(PACKET_CHEST, PACKET_GET, 7 + (chest->items.size() + 1) * 5);
						reply.AddShort(id);
						reply.AddThree(taken);
						reply.AddChar(character->weight);
						reply.AddChar(character->maxweight);

						UTIL_CIFOREACH(chest->items, item)
						{
							if (item->id != 0)
							{
								reply.AddShort(item->id);
								reply.AddThree(item->amount);
							}
						}

						character->Send(reply);

						chest->Update(character->map, character);
						break;
					}
				}
			}
		}
	}
}

// Opening a chest
void Chest_Open(Character *character, PacketReader &reader)
{
	int x = reader.GetChar();
	int y = reader.GetChar();

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::Chest)
		{
			PacketBuilder reply(PACKET_CHEST, PACKET_OPEN, 2);
			reply.AddChar(x);
			reply.AddChar(y);

			UTIL_FOREACH(character->map->chests, chest)
			{
				if (chest->x == x && chest->y == y)
				{
					reply.ReserveMore(chest->items.size() * 5);

					UTIL_CIFOREACH(chest->items, item)
					{
						if (item->id != 0)
						{
							reply.AddShort(item->id);
							reply.AddThree(item->amount);
						}
					}

					character->Send(reply);
					break;
				}
			}
		}
	}
}

PACKET_HANDLER_REGISTER(PACKET_CHEST)
	Register(PACKET_ADD, Chest_Add, Playing);
	Register(PACKET_TAKE, Chest_Take, Playing);
	Register(PACKET_OPEN, Chest_Open, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_CHEST)

}
