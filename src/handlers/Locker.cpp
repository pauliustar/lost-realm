/* handlers/Locker.cpp
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../eodata.hpp"
#include "../eoserver.hpp"
#include "../map.hpp"
#include "../packet.hpp"
#include "../world.hpp"

#include "../util.hpp"

#include <algorithm>
#include <cstddef>

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

namespace Handlers
{

static PacketBuilder add_common(Character *character, short item, int amount)
{
	character->DelItem(item, amount);

	character->CalculateStats();

	bool weaponBankOpen = character->weaponbankopen;
	bool shieldBankOpen = character->shieldbankopen;
	bool cosmeticsBankOpen = character->cosmeticsopen;

	int size = (weaponBankOpen) ? character->weaponbank.size() :
	           (shieldBankOpen) ? character->shieldbank.size() :
	           (cosmeticsBankOpen) ? character->cosmeticsbank.size() :
	           character->bank.size();

	PacketBuilder reply(PACKET_LOCKER, PACKET_REPLY, 8 + size * 5);
	reply.AddShort(item);
	reply.AddInt(character->HasItem(item));
	reply.AddChar(character->weight);
	reply.AddChar(character->maxweight);

    if (character->weaponbankopen)
    {
        UTIL_FOREACH(character->weaponbank, item)
        {
            reply.AddShort(item.id);
            reply.AddThree(item.amount);
        }
    }
    else if (character->shieldbankopen)
    {
        UTIL_FOREACH(character->shieldbank, item)
        {
            reply.AddShort(item.id);
            reply.AddThree(item.amount);
        }
    }	
    else if (character->cosmeticsopen)
    {
        UTIL_FOREACH(character->cosmeticsbank, item)
        {
            reply.AddShort(item.id);
            reply.AddThree(item.amount);
        }
    }	
    else
    {
		UTIL_FOREACH(character->bank, item)
		{
			reply.AddShort(item.id);
			reply.AddThree(item.amount);
		}
    }

	return reply;
}

// Placing an item in a bank locker
void Locker_Add(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	unsigned char x = reader.GetChar();
	unsigned char y = reader.GetChar();
	short item = reader.GetShort();
	int amount = reader.GetThree();

	if (item == 1 || item == 36 || item == 37 || item == 53) return;
	if (amount <= 0) return;
	if (character->HasItem(item) < amount) return;

	std::size_t lockermax = static_cast<int>(character->world->config["BaseBankSize"]) + character->bankmax * static_cast<int>(character->world->config["BankSizeStep"]);

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
		{
			UTIL_IFOREACH(character->bank, it)
			{
				if (it->id == item)
				{
					if (it->amount + amount < 0)
					{
						return;
					}

					amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]) - it->amount);

					it->amount += amount;

					PacketBuilder reply = add_common(character, item, amount);
					character->Send(reply);
					return;
				}
			}

			if (character->bank.size() >= lockermax)
			{
				return;
			}

			amount = std::min<int>(amount, static_cast<int>(character->world->config["MaxBank"]));

			Character_Item newitem;
			newitem.id = item;
			newitem.amount = amount;

			character->bank.push_back(newitem);

			PacketBuilder reply = add_common(character, item, amount);
			character->Send(reply);
		}
	}

	if (character->weaponbankopen)
	{

		std::list<int> allowed_list = ExceptUnserialize(character->world->socket_config["WeaponSocketIDs"]);
    	if (std::find(allowed_list.begin(), allowed_list.end(), item) != allowed_list.end())
        {
	        UTIL_IFOREACH(character->weaponbank, it)
	        {

		            if (it->id == item)
		            {
		                if (it->amount + amount < 0)
		                {
		                    return;
		                }

		                amount = std::min<int>(amount, 1 - it->amount);

		                it->amount += amount;

		                PacketBuilder reply = add_common(character, item, amount);
		                character->Send(reply);
		                return;
		            }               
	        }
        }
        else
        {
        	return;
        }

        if (character->weaponbank.size() >= 8)
        {
            return;
        }

        amount = std::min<int>(amount, 1);

        Character_Item newitem;
        newitem.id = item;
        newitem.amount = amount;

        character->weaponbank.push_back(newitem);

        PacketBuilder reply = add_common(character, item, amount);
        character->Send(reply);		
        character->CalculateStats();
        character->PacketRecover();
        character->UpdateStats();
	}
	if (character->shieldbankopen)
	{

		std::list<int> allowed_list = ExceptUnserialize(character->world->socket_config["ShieldSocketIDs"]);

    	if (std::find(allowed_list.begin(), allowed_list.end(), item) != allowed_list.end())
        {   
	        UTIL_IFOREACH(character->shieldbank, it)
	        {
	            
		            if (it->id == item)
		            {
		                if (it->amount + amount < 0)
		                {
		                    return;
		                }

		                amount = std::min<int>(amount, 1 - it->amount);

		                it->amount += amount;

		                PacketBuilder reply = add_common(character, item, amount);
		                character->Send(reply);
		                return;
		            }
	        }
        }
        else
        {
        	return;
        }        

        if (character->shieldbank.size() >= 8)
        {
            return;
        }

        amount = std::min<int>(amount, 1);

        Character_Item newitem;
        newitem.id = item;
        newitem.amount = amount;

        character->shieldbank.push_back(newitem);

        PacketBuilder reply = add_common(character, item, amount);
        character->Send(reply);		
        character->CalculateStats();
        character->PacketRecover();
        character->UpdateStats();
	}
	if (character->cosmeticsopen)
	{

		std::list<int> allowed_list = ExceptUnserialize(character->world->socket_config["CosmeticIDs"]);

    	if (std::find(allowed_list.begin(), allowed_list.end(), item) != allowed_list.end())
        {   
	        UTIL_IFOREACH(character->cosmeticsbank, it)
	        {
	            
		            if (it->id == item)
		            {
		                if (it->amount + amount < 0)
		                {
		                    return;
		                }

		                amount = std::min<int>(amount, 1 - it->amount);

		                it->amount += amount;

		                PacketBuilder reply = add_common(character, item, amount);
		                character->Send(reply);
		                return;
		            }
	        }
        }
        else
        {
        	return;
        }        

        amount = std::min<int>(amount, 1);

        Character_Item newitem;
        newitem.id = item;
        newitem.amount = amount;

        character->cosmeticsbank.push_back(newitem);

        PacketBuilder reply = add_common(character, item, amount);
        character->Send(reply);		
        character->CalculateStats();
        character->PacketRecover();
        character->UpdateStats();
	}
}

// Taking an item from a bank locker
void Locker_Take(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	unsigned char x = reader.GetChar();
	unsigned char y = reader.GetChar();
	short item = reader.GetShort();

	// TODO: Limit number of items withdrawn to under weight

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
		{
			UTIL_IFOREACH(character->bank, it)
			{
				if (it->id == item)
				{
					int amount = it->amount;
					int taken = character->CanHoldItem(it->id, amount);

					character->AddItem(item, taken);

					character->CalculateStats();

					PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->bank.size() * 5);
					reply.AddShort(item);
					reply.AddThree(taken);
					reply.AddChar(character->weight);
					reply.AddChar(character->maxweight);

					it->amount -= taken;

					if (it->amount <= 0)
						character->bank.erase(it);

					UTIL_FOREACH(character->bank, item)
					{
						reply.AddShort(item.id);
						reply.AddThree(item.amount);
					}
					character->Send(reply);

					break;
				}
			}
		}
	}

	if (character->weaponbankopen)
	{
		UTIL_IFOREACH(character->weaponbank, it)
		{
			if (it->id == item)
			{
				int amount = it->amount;
				int taken = character->CanHoldItem(it->id, amount);

				character->AddItem(item, taken);

				character->CalculateStats();

				PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->weaponbank.size() * 5);
				reply.AddShort(item);
				reply.AddThree(taken);
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				it->amount -= taken;

				if (it->amount <= 0)
					character->weaponbank.erase(it);

				UTIL_FOREACH(character->weaponbank, item)
				{
					reply.AddShort(item.id);
					reply.AddThree(item.amount);
				}
				character->Send(reply);
				character->CalculateStats();
		        character->PacketRecover();
		        character->UpdateStats();

				break;
			}
		}
	}

	if (character->shieldbankopen)
	{
		UTIL_IFOREACH(character->shieldbank, it)
		{
			if (it->id == item)
			{
				int amount = it->amount;
				int taken = character->CanHoldItem(it->id, amount);

				character->AddItem(item, taken);

				character->CalculateStats();

				PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->shieldbank.size() * 5);
				reply.AddShort(item);
				reply.AddThree(taken);
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				it->amount -= taken;

				if (it->amount <= 0)
					character->shieldbank.erase(it);

				UTIL_FOREACH(character->shieldbank, item)
				{
					reply.AddShort(item.id);
					reply.AddThree(item.amount);
				}
				character->Send(reply);
				    character->CalculateStats();
				    character->PacketRecover();
				    character->UpdateStats();

				break;
			}
		}
	}
	if (character->cosmeticsopen)
	{
		UTIL_IFOREACH(character->cosmeticsbank, it)
		{
			if (it->id == item)
			{
				int amount = it->amount;
				int taken = character->CanHoldItem(it->id, amount);

				character->AddItem(item, taken);

				PacketBuilder reply(PACKET_LOCKER, PACKET_GET, 7 + character->cosmeticsbank.size() * 5);
				reply.AddShort(item);
				reply.AddThree(taken);
				reply.AddChar(character->weight);
				reply.AddChar(character->maxweight);

				it->amount -= taken;

				if (it->amount <= 0)
					character->cosmeticsbank.erase(it);

				UTIL_FOREACH(character->cosmeticsbank, item)
				{
					reply.AddShort(item.id);
					reply.AddThree(item.amount);
				}
				character->Send(reply);

				character->CalculateStats();

				break;
			}
		}
	}
}

// Opening a bank locker
void Locker_Open(Character *character, PacketReader &reader)
{
	character->weaponbankopen = false;
	character->shieldbankopen = false;
	character->cosmeticsopen = false;
	unsigned char x = reader.GetChar();
	unsigned char y = reader.GetChar();

	if (util::path_length(character->x, character->y, x, y) <= 1)
	{
		if (character->map->GetSpec(x, y) == Map_Tile::BankVault)
		{
			PacketBuilder reply(PACKET_LOCKER, PACKET_OPEN, 2 + character->bank.size() * 5);
			reply.AddChar(x);
			reply.AddChar(y);
			UTIL_FOREACH(character->bank, item)
			{
				reply.AddShort(item.id);
				reply.AddThree(item.amount);
			}
			character->Send(reply);
		}
	}
}

// Purchasing a locker space upgrade
void Locker_Buy(Character *character, PacketReader &reader)
{
	if (character->trading) return;

	(void)reader;

	if (character->npc_type == ENF::Bank)
	{
		int cost = static_cast<int>(character->world->config["BankUpgradeBase"]) + character->bankmax * static_cast<int>(character->world->config["BankUpgradeStep"]);

		if (character->bankmax >= static_cast<int>(character->world->config["MaxBankUpgrades"]))
		{
			return;
		}

		if (character->HasItem(1) < cost)
		{
			return;
		}

		++character->bankmax;
		character->DelItem(1, cost);

		PacketBuilder reply(PACKET_LOCKER, PACKET_BUY, 5);
		reply.AddInt(character->HasItem(1));
		reply.AddChar(character->bankmax);
		character->Send(reply);
	}
}

PACKET_HANDLER_REGISTER(PACKET_LOCKER)
	Register(PACKET_ADD, Locker_Add, Playing);
	Register(PACKET_TAKE, Locker_Take, Playing);
	Register(PACKET_OPEN, Locker_Open, Playing);
	Register(PACKET_BUY, Locker_Buy, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_LOCKER)

}
