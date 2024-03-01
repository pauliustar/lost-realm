/* handlers/Door.cpp
 * EOSERV is released under the zlib license.
 * See LICENSE.txt for more info.
 */

#include "handlers.hpp"

#include "../character.hpp"
#include "../map.hpp"
#include "../packet.hpp"
#include "../world.hpp"


namespace Handlers
{

// User opening a door
void Door_Open(Character *character, PacketReader &reader)
{
	int x = reader.GetChar();
	int y = reader.GetChar();

	character->map->OpenDoor(character, x, y);

    if (character->map->GetSpec(x, y) == Map_Tile::Door)
	{
		character->board = character->world->boards[9];

        Database_Result res = character->world->db.Query("SELECT `name`, `level`, `exp`, `fame`, `bestfame` FROM `characters` WHERE `admin` <= $ ORDER BY `bestfame` DESC, `fame` DESC LIMIT 10", std::string(character->world->config["TopPlayerAccess"]).c_str());

        char res_size = 0;

        res_size = res.size();

		PacketBuilder builder(PACKET_BOARD, PACKET_OPEN, res_size + 1000);

        builder.AddChar(9);
        builder.AddChar(res_size + 2);
        builder.AddShort(0);
        builder.AddByte(255);
        builder.AddBreakString(" ");
        builder.AddBreakString("                Top 10 Players");
        builder.AddShort(1);
        builder.AddByte(255);
        builder.AddBreakString("(Name)");
        builder.AddBreakString("(Best Fame)             (Current Fame)     (Level)");

        int idcount = 1;

		character->board->last_id = 1;

        UTIL_FOREACH_REF(res, row)
        {
            ++idcount;

            builder.AddShort(idcount);
            builder.AddByte(255);

            std::string resname = row["name"];

            int reslvl = row["level"];
            int resexp = row["exp"];
            int resfame = row["fame"];
            int resbestfame = row["bestfame"];


            std::string fillerlvl = "00000";
            std::string fillerbestfame = "00000";
            std::string fillerfame = "00000";

            std::string rlvl = fillerlvl.substr(0, fillerlvl.length() - util::to_string(reslvl).length()) + util::to_string(reslvl);
            std::string rbestfame = fillerfame.substr(0, fillerbestfame.length() - util::to_string(resbestfame).length()) + util::to_string(resbestfame);
            std::string rfame = fillerfame.substr(0, fillerfame.length() - util::to_string(resfame).length()) + util::to_string(resfame);

            std::string resbody = util::ucfirst(resname)+ " ";
            resbody[resbody.length() - 1] = 13;
            resbody += "Level: " + util::to_string(reslvl) + " ";
            resbody[resbody.length() - 1] = 13;
            resbody += "Exp: " + util::to_string(resexp) + " ";
            resbody[resbody.length() - 1] = 13;
            resbody += "Current Fame: " + util::to_string(resfame) + " ";
            resbody[resbody.length() - 1] = 13;
            resbody += "Best Fame: " + util::to_string(resbestfame) + " ";
            resbody[resbody.length() - 1] = 13;

            resbody[resbody.length() - 1] = 13;


            std::string displaytext = rbestfame + "                      " + rfame + "                  " + rlvl;

            Board_Post *newpost = new Board_Post;

            newpost->id = ++character->board->last_id;
            newpost->author = util::ucfirst(resname);
            newpost->author_admin = ADMIN_PLAYER;
            newpost->subject = displaytext;
            newpost->body = resbody;
            newpost->time = Timer::GetTime();

            character->board->posts.push_front(newpost);

            builder.AddBreakString(util::ucfirst(resname));
            builder.AddBreakString(displaytext);
        }

        character->Send(builder);

	}

}

PACKET_HANDLER_REGISTER(PACKET_DOOR)
	Register(PACKET_OPEN, Door_Open, Playing);
PACKET_HANDLER_REGISTER_END(PACKET_DOOR)

}
