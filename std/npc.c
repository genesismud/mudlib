/*
  /std/npc.c

  This is the base for all NPC creatures that need to use combat tools.

*/
#pragma save_binary
#pragma strict_types

inherit "/std/creature";

#include <money.h>
#include <stdproperties.h>

void
create_npc()
{
    ::create_creature();
}

nomask void
create_creature() 
{
    if (!random(5))
	add_leftover("/std/leftover", "tooth", random(5) + 1, 0, 1, 0);
    if (!random(5))
	add_leftover("/std/leftover", "skull", 1, 0, 1, 1);
    if (!random(5))
	add_leftover("/std/leftover", "thighbone", 2, 0, 1, 1);
    if (!random(5))
        add_leftover("/std/leftover", "kneecap", 2, 0, 0, 1);
    if (!random(5))
	add_leftover("/std/leftover", "rib", 2, 0, 1, 1);

    if (query_prop(LIVE_I_UNDEAD))
    {
	create_npc();
        MONEY_CONDENSE(this_object());
	return;
    }

    if (!random(5))
        add_leftover("/std/leftover", "ear", 2, 0, 0, 0);
    if (!random(5))
        add_leftover("/std/leftover", "scalp", 1, 0, 0, 1);
    if (!random(5))
        add_leftover("/std/leftover", "nail", random(5) + 1, 0, 0, 0);
    if (!random(5))
	add_leftover("/std/leftover", "heart", 1, 0, 0, 1);
    if (!random(5))
	add_leftover("/std/leftover", "nose", 1, 0, 0, 0);
    if (!random(5))
	add_leftover("/std/leftover", "eye", 2, 0, 0, 0);
    if (!random(5))
	add_leftover("/std/leftover", "kidney", 2, 0, 0, 1);
    if (!random(5))
	add_leftover("/std/leftover", "intestine", 2, 0, 0, 1);

    MONEY_CONDENSE(this_object());
    create_npc(); 
}

void
reset_npc()
{
    ::reset_creature();
}

nomask void
reset_creature()
{
    reset_npc();
}

/*
 * Description:  Use the combat file for generic tools
 */
public string 
query_combat_file() 
{
    return "/std/combat/ctool"; 
}

/*
 * Function name:  default_config_npc
 * Description:    Sets all neccessary values for this npc to function
 */
varargs public void
default_config_npc(int lvl)
{
    default_config_creature(lvl);
}
