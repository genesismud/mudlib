/*
 * /cmd/wiz/junior_shadow.c
 *
 * This shadow redefines the function do_die() in a junior player to a
 * wizard. It prevents them from death if they so choose.
 *
 * There is some protection in the shadow for we do not want ordinary
 * mortal players to get this protection. I bet they would want to have it!
 *
 * /Mercade, 6 Februari 1994
 */

#pragma no_inherit
#pragma strict_types
#pragma save_binary

inherit "/std/shadow";

#include <macros.h>
#include <std.h>

/*
 * Function name: remove_death_protection
 * Description  : Remove this shadow from a player.
 * Returns      : 1 - always
 */
nomask public int
remove_death_protection()
{
    tell_object(query_shadow_who(), break_string("You are no longer " +
	"protected from the horrors of death. Take care! It is a rough " +
	"world out there.", 74) + "\n");

    destruct();
    return 1;
}

/*
 * Function name: illegal_shadow_use
 * Description  : If you are not allowed to use this shadow, this function
 *                will take care of it.
 */
nomask private void
illegal_shadow_use()
{
    setuid();
    seteuid(getuid());

    query_shadow_who()->command("sysbug This is an automatic sysbug to " +
	"let the administration know that I got the junior shadow without " +
	"being entitled to have it.");
}

/*
 * Function name: do_die
 * Description  : This function is called whenever someone suspects that
 *                we have died.
 * Arguments    : killer - the object that killed us.
 */
public void
do_die(object killer)
{
    string  name;
    object *enemies;

    if ((query_shadow_who()->query_hp() > 0) ||
	(query_shadow_who()->query_ghost()))
    {
	return;
    }

    if (!objectp(killer))
    {
	killer = previous_object();
    }

    if (sscanf((string)query_shadow_who()->query_real_name(),
	"%sjr", name) != 1)
    {
	illegal_shadow_use();
	set_alarm(0.1, 0.0, "remove_death_protection");
	query_shadow_who()->do_die(killer);
	return;
    }

    if (SECURITY->query_wiz_rank(name) <= WIZ_RETIRED)
    {
	illegal_shadow_use();
	set_alarm(0.1, 0.0, "remove_death_protection");
	query_shadow_who()->do_die(killer);
	return;
    }

    enemies = (object *)query_shadow_who()->query_enemy(-1) - ({ 0 });

    if (sizeof(enemies))
    {
	query_shadow_who()->stop_fight(enemies);
	enemies->stop_fight(query_shadow_who());
    }

    /* Give him/her at least one hitpoint so this function won't be called
     * over and over again.
     */
    query_shadow_who()->set_hp(1);

    tell_room(environment(query_shadow_who()),
	"\n" + QCTNAME(query_shadow_who()) + " dies ....\n" +
	" .... and yet " + query_shadow_who()->query_pronoun() +
	" fails to die!\n" +
	"Even though all life is drained from " +
	query_shadow_who()->query_objective() +
	", the Gods refuse to take them away.\n",
	query_shadow_who());
    tell_object(killer, "You killed " +
	query_shadow_who()->query_the_name(killer) + ".\n");
    tell_object(query_shadow_who(), "\nYOU DIE !!!\n\n" +
	"The junior tool prevented you from actually dying.\n" +
	"You stop fighting all known enemies.\n\n");
}

/*
 * Function name: query_death_protection
 * Description  : This function will return whether the player is protected
 *                from Death.
 * Returns      : 1 (always)
 */
nomask public int
query_death_protection()
{
    return 1;
}

/*
 * Function name: shadow_me
 * Description  : This function is called to make this shadow shadow the
 *                player. It add the autoloading feature to the player.
 * Arguments    : player - the player
 * Returns      : 1 - everything went right
 *                0 - no player or player already shadowed
 */
nomask public int
shadow_me(object player)
{
    string name;

    if (sscanf((string)player->query_real_name(), "%sjr", name) != 1)
    {
	tell_object(player,
	    "ACK! You should have never gotten this shadow!\n");
	illegal_shadow_use();
	return 0;
    }

    if (SECURITY->query_wiz_rank(name) <= WIZ_RETIRED)
    {
	tell_object(player,
	    "ACK! You should have never gotten this shadow!\n");
	illegal_shadow_use();
	return 0;
    }

    if (!::shadow_me(player))
    {
	tell_object(player,
	    "Something is very wrong with the shadow preventing junior " +
	    "players from death!\n");
	return 0;
    }

    tell_object(player, "From now on you will be protected from Death!\n");
    return 1;
}
