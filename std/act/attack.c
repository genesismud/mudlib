/* 
 *  /std/act/attack.c
 *
 *  Selective attacking: Standard action module for mobiles
 *
 *  set_aggressive(int)       1 - will make this mobile aggressive
 *  set_attack_chance(int)    % chance that this monster will attack a player
 *                            0 will have the same affect has 100 so no need
 *			      to set this as well
 *
 *  Currently you can make a monster aggressive and attack the entering player
 *  on sight or at a %chance. No npc's will be attacked. There is a lot of
 *  improvement that can be done.
 */

#pragma save_binary
#pragma strict_types

#include <macros.h>

/*
 * Global variables. They are not saved.
 */
static	int	monster_attack_chance;   /* %attack chance */
static	mixed	monster_aggressive;   	 /* aggressive  */

/*
 * Function name: set_aggressive
 * Description:   Make the monster aggressive
 * Arguments:	  i - 1 aggressive, 0 - not aggressive; i can also be set to
 *                a VBFC-call with your own aggressive-detection code.
 */
public void
set_aggressive(mixed i)
{
    monster_aggressive = i;
}

/*
 * Function name: set_attack_chance
 * Description:   Set chance the monster will attack
 * Arguments:	  i - the % chance it will attack, 0 = 100 %
 */
public void
set_attack_chance(int i)
{
    monster_attack_chance = i;
}

/*
 * Function name: aggressive_attack
 * Description  : We are aggressive, let us attack anyone in sight.
 * Arguments    : object ob - the object to attack.
 */
public void
aggressive_attack(object ob)
{
    if (!objectp(ob) || !present(ob, environment(this_object())))
    {
	return;
    }

    command("kill " + OB_NAME(ob));
}

/*
 * Function name: init_attack
 * Description:   Called from init_living() i monster.c
 */
public void
init_attack()
{
    if ((!this_object()->check_call(monster_aggressive)) ||
	(this_player()->query_npc()))
    {
	return;
    }

    if ((monster_attack_chance == 0) || (random(100) < monster_attack_chance))
    {
	set_alarm(2.0, 0.0, &aggressive_attack(this_player()));
    }
}
