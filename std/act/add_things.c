/* std/act/add_things.c
 *
 * This is a file inherited into monster.c that enables the player to
 * add weapons and armour to the monster in a very convinient way. The
 * monster will automaticly wield/wear the things. 
 *
 * just do add_armour(string filename);        or
 *         add_weapon(string filename); 
 * 
 * and the monster will clone, move and wield/wear the things.
 *      (The functions return the objects)
 *
 *     Dimitri, the mighty Avatar !
 *
 * We thank PaderMUD for this File
 */

#pragma save_binary
#pragma strict_types

#include <macros.h>
#include <stdproperties.h>

void
move_and_wield(object weapon)
{
    if (weapon->move(this_object()))
    {
	weapon->remove_object();
    }
    this_object()->seq_addlast("add_weapon",
		({ "wield " + weapon->query_name() }) );
}

/*
 * Function name:  add_weapon
 * Description:    clones a weapon to the monster and makes the monster wield it.
 * Arguments:      filename:  The filename of the weapon. [string]
 * Returns:        the weapon
 */
public object
add_weapon(string file)
{
    object weapon;

    if (!file)
    	return 0;

    if (!this_object()->seq_query("add_weapon"))
        this_object()->seq_new("add_weapon");

    seteuid(getuid(this_object()));
    weapon = clone_object(file);
    if (!weapon)
    	return 0;

    set_alarm(1.0, 0.0, &move_and_wield(weapon));
    return weapon;
}


void
move_and_wear(object armour)
{
    if (armour->move(this_object()))
    {
	armour->remove_object();
    }
    this_object()->seq_addlast("add_armour",
	({ "wear " + armour->query_name() }) );
}

/*
 * Function name:  add_armour
 * Description:    clones an armour to the monster and makes the monster wear it.
 * Arguments:      filename: The filename of the armour. [string]
 * Returns:        the armour
 */
public object
add_armour(string file)
{
    object armour;
  
    if (!file)
    	return 0;

    if (!this_object()->seq_query("add_armour"))
	this_object()->seq_new("add_armour");

    seteuid(getuid(this_object()));
    armour = clone_object(file);
    if (!armour)
    	return 0;

    set_alarm(1.0, 0.0, &move_and_wear(armour));
    return armour;
}
