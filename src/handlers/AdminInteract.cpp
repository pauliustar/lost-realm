/* handlers/AdminInteract.cpp
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../config.hpp"
#include "../i18n.hpp"
#include "../world.hpp"
#include "../npc.hpp"
#include "../npc_data.hpp"

#include <string>

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

// "Talk to admin" message
void AdminInteract_Tell(Character *character, PacketReader &reader)
{
	std::string message = reader.GetEndString();

	if (character->world->config["OldReports"])
	{
		message = character->world->i18n.Format("admin_request", message);
		character->world->AdminMsg(character, message, static_cast<int>(character->world->admin_config["reports"]));
	}
	else
	{
		character->world->AdminRequest(character, message);
	}
}

void AdminInteract_Info(Character *character, PacketReader &reader)
{
	int lookupType = reader.GetByte();
	int ident = reader.GetByte();

	//NPC Lookup
	if (util::to_string(lookupType) == "3")
	{
        ENF_Data value = character->world->enf->Get(ident - 1);

        std::vector<ENF_Data> enfList = character->world->enf->data;
        std::string name = util::ucfirst(value.name);
        std::vector<std::string> lines = {
                "HP: " + std::to_string(value.hp),
                "EXP: " + std::to_string(value.exp * (int)character->map->world->config["ExpRate"]),
                " ",
                "Min. Damage: " + std::to_string(value.mindam),
                "Max. Damage: " + std::to_string(value.maxdam),
                "Accuracy: " + std::to_string(value.accuracy),
                "Evade: " + std::to_string(value.evade),
                "Armor: " + std::to_string(value.armor),
                " ",
                "Drops:"
        };
        for(auto &nData : character->world->npc_data){
            if(nData->id == value.id){
                for(auto &drop : nData->drops){
                    EIF_Data dropData = character->world->eif->Get(drop->id);
                    std::string str = std::to_string (drop->chance);
                    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                    str.erase(str.find_last_not_of('.') + 1, std::string::npos);
                    lines.push_back(dropData.name + " "+ std::to_string(drop->min) + "-"+ std::to_string(drop->max) +": " + str + "%");
                }
                break;
            }
        }
        std::string title = name + " (" + std::to_string(value.id) + ")";
        std::string message = util::lines_to_string(lines);
        PacketBuilder reply(PACKET_MESSAGE, PACKET_ACCEPT, title.length() + message.length() + 2);
        reply.AddBreakString(title);
        reply.AddBreakString(message);
        character->Send(reply);
	}
	//Item Lookup
	if (util::to_string(lookupType) == "2")
	{
        int str = 0, intl = 0, wis = 0, agi = 0, con = 0, cha = 0;

        EIF_Data value = character->world->eif->Get(ident - 1);

        if (!value || value.id == 0)
            return;

        std::string name = value.name + " (" + std::to_string(value.id) + ")";
        std::vector<std::string> lines, droppedBy = {" ", "Dropped by:"}, soldAt = {" ", "Sold at:"}, sellsTo = {" ", "Sells to:"}, craftedAt = {" ", "Crafted at:"}, crafts {" ", "Used to craft:"}, recycles = {" ", "Recycled for:"};
        lines.push_back("Weight: " + std::to_string(value.weight));
        if(value.type == EIF::Heal)
    	{
        	if (value.id == 44 || value.id == 74)
        	{
        		lines.push_back(" ");
        		lines.push_back("Description: Loot bags have a percental chance to drop one of the items from the list below");
        		lines.push_back(" ");
        		if (value.id == 44)
        		{
			        for (int ii = 1;; ++ii)
			        {
			        	std::string reward_id_str = character->world->giftbox_config["1.RewardID" + util::to_string(ii)];
						if (reward_id_str == "0")			            
						{
			                break; // End of rewards for this GiftID
			            }
			        	EIF_Data item = character->world->eif->Get(std::stoi(character->world->giftbox_config["1.RewardID" + util::to_string(ii)]));
			        	int chance = std::stoi(character->world->giftbox_config["1.ChanceID" + util::to_string(ii)]);
			        	int amount = std::stoi(character->world->giftbox_config["1.AmountID" + util::to_string(ii)]);
			            lines.push_back("Item: " + item.name);
			            if (amount > 1)
			            {
			            	lines.push_back("Amount: " + util::to_string(amount));
			            }
			            lines.push_back("Chance: " + util::to_string(chance) + "%");
			            lines.push_back(" ");			        	
			        }
        		}
        		else if (value.id == 74)
        		{
			        for (int ii = 1;; ++ii)
			        {
			        	std::string reward_id_str = character->world->giftbox_config["2.RewardID" + util::to_string(ii)];
						if (reward_id_str == "0")			            
						{
			                break; // End of rewards for this GiftID
			            }
			        	EIF_Data item = character->world->eif->Get(std::stoi(character->world->giftbox_config["2.RewardID" + util::to_string(ii)]));
			        	int chance = std::stoi(character->world->giftbox_config["2.ChanceID" + util::to_string(ii)]);
			        	int amount = std::stoi(character->world->giftbox_config["2.AmountID" + util::to_string(ii)]);
			            lines.push_back("Item: " + item.name);
			            if (amount > 1)
			            {
			            	lines.push_back("Amount: " + util::to_string(amount));
			            }
			            lines.push_back("Chance: " + util::to_string(chance) + "%");
			            lines.push_back(" ");			        	
			        }
        		}
    		}
        	else if(value.id != 44 && value.id != 74)
        	{
	            lines.push_back("HP gain: " + std::to_string(value.hp));
	            lines.push_back("TP gain: " + std::to_string(value.tp));
        	}
        }
        if(value.type == EIF::Weapon || value.type == EIF::Shield || value.type == EIF::Armor || value.type == EIF::Hat || value.type == EIF::Boots || value.type == EIF::Gloves || value.type == EIF::Accessory || value.type == EIF::Belt || value.type == EIF::Necklace || value.type == EIF::Ring || value.type == EIF::Armlet || value.type == EIF::Bracer)
        {
            lines.push_back("HP gain: " + std::to_string(value.hp));
            lines.push_back("TP gain: " + std::to_string(value.tp));
            lines.push_back(" ");
            lines.push_back("Str.: " + std::to_string(value.str));
            lines.push_back("Int.: " + std::to_string(value.intl));
            lines.push_back("Wis.: " + std::to_string(value.wis));
            lines.push_back("Agi.: " + std::to_string(value.agi));
            lines.push_back("Con.: " + std::to_string(value.con));
            lines.push_back("Cha.: " + std::to_string(value.cha));
            lines.push_back(" ");
            lines.push_back("Damage: " + std::to_string(value.mindam) + " - " +std::to_string(value.maxdam) );
            lines.push_back("Accuracy: " + std::to_string(value.accuracy));
            lines.push_back("Evade: " + std::to_string(value.evade));
            lines.push_back("Armor: " + std::to_string(value.armor));
            lines.push_back(" ");
        }
		UTIL_FOREACH_CREF(character->world->npc_data, nData) {
		    UTIL_FOREACH_CREF(nData->drops, drop)
		    {
            	if(drop->id == value.id)
                {
                    std::string str = std::to_string (drop->chance);
                    str.erase(str.find_last_not_of('0') + 1, std::string::npos );
                    str.erase(str.find_last_not_of('.') + 1, std::string::npos );
                    droppedBy.push_back(character->world->enf->Get(nData->id).name + ": "+ std::to_string(drop->min) + "-"+ std::to_string(drop->max) +" " + str + "%");
                }
        	}
	        UTIL_FOREACH_CREF(nData->shop_trade, shopItem)
	        {
	            if(shopItem->id == value.id)
	            {
	                if(shopItem->buy)
	                    soldAt.push_back(nData->shop_name + ": " + std::to_string(shopItem->buy) + "g");
	                if(shopItem->sell)
	                    sellsTo.push_back(nData->shop_name + ": " + std::to_string(shopItem->sell) + "g");
	            }
	        }
            UTIL_FOREACH_CREF(nData->shop_craft, shopCraft)
            {
                for(auto& ing : shopCraft->ingredients){
                    EIF_Data itemData = character->world->eif->Get(shopCraft->id);
                    if(ing.id == value.id){
                        crafts.push_back(itemData.name);
                    }
                }
                if(shopCraft->id == value.id){
                    craftedAt.push_back(nData->shop_name + ": ");
                    for(auto& ing : shopCraft->ingredients){
                        int in_bank = 0;
                        UTIL_FOREACH(character->bank, char_item){
                            if(char_item.id == value.id)
                                in_bank += char_item.amount;
                        }
                        EIF_Data itemData = character->world->eif->Get(ing.id);
                        if(itemData)
                            craftedAt.push_back(itemData.name + " - " + std::to_string(character->HasItem(ing.id + in_bank)) + "/" + std::to_string((int)ing.amount));
                    }
                }
            }
		}

		std::list<int> allowed_list = ExceptUnserialize(character->world->recycle_config["RecycleIDs"]);

        //Metal
        if (value.id == 54)
        {
            lines.push_back(" ");
            lines.push_back("Items that can be recycled for Metal: ");
            for(int item_id : allowed_list)
            {
                EIF_Data item = character->world->eif->Get(item_id);
                int chance = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".metalchance"]);
                int min_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".metalminamount"]);
                int max_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".metalmaxamount"]);
                if (chance > 0)
                {
                    lines.push_back(" ");
                    lines.push_back(item.name);
                    lines.push_back("Chance: " + util::to_string(chance) + "%");
                    lines.push_back("Min: " + util::to_string(min_amount));
                    lines.push_back("Max: " + util::to_string(max_amount));                    
                }

            }
        }
        //Cloth
        if (value.id == 56)
        {
            lines.push_back(" ");
            lines.push_back("Items that can be recycled for Cloth: ");
            for(int item_id : allowed_list)
            {
                EIF_Data item = character->world->eif->Get(item_id);
                int chance = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".clothchance"]);
                int min_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".clothminamount"]);
                int max_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".clothmaxamount"]);
                if (chance > 0)
                {
                    lines.push_back(" ");
                    lines.push_back(item.name);
                    lines.push_back("Chance: " + util::to_string(chance) + "%");
                    lines.push_back("Min: " + util::to_string(min_amount));
                    lines.push_back("Max: " + util::to_string(max_amount));                    
                }

            }        
        }
        //Wood
        if (value.id == 57)
        {
            lines.push_back(" ");
            lines.push_back("Items that can be recycled for Wood: ");
            for(int item_id : allowed_list)
            {
                EIF_Data item = character->world->eif->Get(item_id);
                int chance = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".woodchance"]);
                int min_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".woodminamount"]);
                int max_amount = std::stoi(character->world->recycle_config[util::to_string(item_id) + ".woodmaxamount"]);
                if (chance > 0)
                {
                    lines.push_back(" ");
                    lines.push_back(item.name);
                    lines.push_back("Chance: " + util::to_string(chance) + "%");
                    lines.push_back("Min: " + util::to_string(min_amount));
                    lines.push_back("Max: " + util::to_string(max_amount));                    
                }

            }   
        }

    	if (std::find(allowed_list.begin(), allowed_list.end(), value.id) != allowed_list.end())
        {
        	recycles.push_back(" ");
        	// Metal
        	if (std::stoi(character->world->recycle_config[util::to_string(value.id) + ".metalchance"]) > 0)
        	{	
        		int chance = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".metalchance"]);
        		int min_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".metalminamount"]);
        		int max_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".metalmaxamount"]);
        		recycles.push_back("Metal Chance: " + util::to_string(chance) + "%");
        		recycles.push_back("Min Amount: " + util::to_string(min_amount));
        		recycles.push_back("Max Amount: " + util::to_string(max_amount));
        		recycles.push_back(" ");
        	}
        	
        	// Wood
        	if (std::stoi(character->world->recycle_config[util::to_string(value.id) + ".woodchance"]) > 0)
        	{	
        		int chance = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".woodchance"]);
        		int min_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".woodminamount"]);
        		int max_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".woodmaxamount"]);
        		recycles.push_back("Wood Chance: " + util::to_string(chance) + "%");
        		recycles.push_back("Min Amount: " + util::to_string(min_amount));
        		recycles.push_back("Max Amount: " + util::to_string(max_amount));
        		recycles.push_back(" ");
        	}
        	// Cloth
        	if (std::stoi(character->world->recycle_config[util::to_string(value.id) + ".clothchance"]) > 0)
        	{	
        		int chance = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".clothchance"]);
        		int min_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".clothminamount"]);
        		int max_amount = std::stoi(character->world->recycle_config[util::to_string(value.id) + ".clothmaxamount"]);
        		recycles.push_back("Cloth Chance: " + util::to_string(chance) + "%");
        		recycles.push_back("Min Amount: " + util::to_string(min_amount));
        		recycles.push_back("Max Amount: " + util::to_string(max_amount));
        		recycles.push_back(" ");
        	}
        }

        std::string title = name;
        std::string message = util::lines_to_string(lines);
        if(droppedBy.size() > 2)
            message += util::lines_to_string(droppedBy);
        if(sellsTo.size() > 2)
            message += util::lines_to_string(sellsTo);
        if(soldAt.size() > 2)
            message += util::lines_to_string(soldAt);
        if(craftedAt.size() > 2)
            message += util::lines_to_string(craftedAt);
        if(crafts.size() > 2)
            message += util::lines_to_string(crafts);
        if(recycles.size() > 2)
   			message += util::lines_to_string(recycles);


        PacketBuilder reply(PACKET_MESSAGE, PACKET_ACCEPT, title.length() + message.length() + 2);
        reply.AddBreakString(title);
        reply.AddBreakString(message);
        character->Send(reply);
	}
}

// User report
void AdminInteract_Report(Character *character, PacketReader &reader)
{
	std::string reportee = reader.GetBreakString();
	std::string message = reader.GetEndString();

	if (character->world->config["OldReports"])
	{
		message = character->world->i18n.Format("admin_report", reportee, message);
		character->world->AdminMsg(character, message, static_cast<int>(character->world->admin_config["reports"]));
	}
	else
	{
		character->world->AdminReport(character, reportee, message);
	}
}

PACKET_HANDLER_REGISTER(PACKET_ADMININTERACT)
	Register(PACKET_TELL, AdminInteract_Tell, Playing | OutOfBand);
	Register(PACKET_REPORT, AdminInteract_Report, Playing | OutOfBand);
	Register(PACKET_TAKE, AdminInteract_Info, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_ADMININTERACT)

}
