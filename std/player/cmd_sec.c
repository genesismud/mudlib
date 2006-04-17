/*
 * player/cmd_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * Some standard commands that should always exist are defined here.
 * This is also the place for the quicktyper command hook.
 */

#include <composite.h>
#include <files.h>
#include <macros.h>
#include <std.h>
#include <stdproperties.h>
#include <time.h>

/*
 * Prototypes.
 */
#ifndef NO_SKILL_DECAY
public nomask int query_skill_decay();
static nomask void decay_skills();
#endif NO_SKILL_DECAY
public nomask void save_me(int value_items);
nomask int quit(string str);
public int save_character(string str);
static nomask int change_password(string str);

/*
 * Global variables, they are static and will not be saved.
 */
static int save_alarm;           /* The id of the autosave-alarm */
static private string password2; /* Used when someone changes his password. */

/*
 * Function name: start_autosave
 * Description  : Call this function to start autosaving. Only works for
 *                mortal players.
 */
static nomask void
start_autosave()
{
    /* Do not autosave wizards. */
    if (query_wiz_level())
    {
	return;
    }

    /* Only autosave on interactives, not on linkdead players. */
    remove_alarm(save_alarm);
    if (interactive())
    {
	save_alarm = set_alarm(300.0, 0.0, &save_me(1));
    }
    else
    {
	save_alarm = 0;
    }
}

/*
 * Function name: stop_autosave
 * Description  : Call this function to stop autosaving.
 */
static nomask void
stop_autosave()
{
    remove_alarm(save_alarm);
    save_alarm = 0;
}

/*
 * Function name: cmd_sec_reset
 * Description  : When the player logs in, this function is called to link
 *                some essential commands to him.
 */
static nomask void
cmd_sec_reset()
{
    add_action(quit,            "quit");
    add_action(save_character,  "save");
    add_action(change_password, "password");

    init_cmdmodify();

    /* Start autosaving. */
    start_autosave();
}

/*
 * Function name:   compute_values
 * Description:     Recursively compute the values of the given list of
 *                  objects. If the objects contain objects with a value
 *                  that value is also taken into account. Only 2/3 of the
 *		    actual value will be stored. Also, coins aren't included
 *		    in the value summed up. Don't figure objects that can
 *		    be recovered into this list.
 * Arguments:       ob_list: The list of objects to sum up.
 */
public nomask int
compute_values(object *ob_list)
{
    int v, i, size, tmp;

    if (!pointerp(ob_list))
	return 0;

    v = 0;
    i = -1;
    size = sizeof(ob_list);
    while(++i < size)
    {
	if (ob_list[i]->query_recover() || ob_list[i]->query_auto_load())
	    continue;
	if (function_exists("create_heap", ob_list[i]) == COINS_OBJECT)
	    continue;
	tmp = ob_list[i]->query_prop(OBJ_I_VALUE);
	if (tmp > 1000)
	    tmp = 1000;
	v += tmp;
    }

    return ((2 * v) / 3);
}

/*
 * Function name: compute_auto_str
 * Description  : Walk through the inventory and check all the objects for
 *                the function query_auto_load(). Constructs an array with
 *                all returned strings. query_auto_load() should return
 *                a string of the form "<file>:<argument>".
 */
static nomask void
compute_auto_str()
{
    object *ob = deep_inventory(this_object());
    string str;
    string *auto = ({ });
    int i = sizeof(ob);

    while(--i >= 0)
    {
	str = ob[i]->query_auto_load();
	if (strlen(str))
	{
	    auto += ({ str });
	}
    }
    set_auto_load(auto);
}

/*
 * Function name: check_recover_loc
 * Description:   This function checks if the player is standing on a spot
 *                where recover may happend
 * Returns:       1 / 0 depending on outcome.
 */
nomask int
check_recover_loc()
{
    object tmp;
    int i;
    string *list, env;

    /* Check for armageddon */
    if (ARMAGEDDON->shutdown_active())
	return 1;

    /* Wizards always recover. */
    if (query_wiz_level())
        return 1;

    /* Check for recoverable surroundings */
    if (objectp(tmp = environment(this_object())))
	env = file_name(tmp);
    else
	return 0;

    list = SECURITY->query_list_temp_start() +
	SECURITY->query_list_def_start();

    if (member_array(env, list) != -1)
    {
	return 1;
    }

    return 0;
}

/*
 * Function name: compute_recover_str
 * Description  : Walk through the inventory and check all the objects for
 *                the recover property.
 * Arguments    : int - if true the player manually typed 'save'. It means
 *                      we compute the string even though the player may
 *                      not be in a recover location.
 */
static nomask void
compute_recover_str(int verb)
{
    object *ob;
    object *glowing = ({ });
    object *failing = ({ });
    string str, *recover;
    int i, loc, size;

    recover = ({ });

    loc = check_recover_loc();

    if (!(loc || verb))
    {
	set_recover_list(recover);
	return;
    }

    ob = deep_inventory(this_object());

    i = -1;
    size = sizeof(ob);
    while(++i < size)
    {
	if (ob[i]->check_recoverable(1))
	{
	    if (ob[i]->may_not_recover())
	    {
		failing += ({ ob[i] });
	    }
	    else
	    {
		if (!strlen(str = ob[i]->query_recover()))
		    continue;
		recover += ({ str });
		glowing += ({ ob[i] });
	    }
	}
    }

    set_recover_list(recover);

    if (loc)
    {
	if (i = sizeof(glowing))
	{
	    write(capitalize(COMPOSITE_DEAD(glowing)) +
		((i == 1) ? " glows" : " glow") + " briefly.\n");
	}
	if (i = sizeof(failing))
	{
	    write(capitalize(COMPOSITE_DEAD(failing)) +
		((i == 1) ? " fails" : " fail") + " to glow briefly.\n");
	}
    }
}

/*
 * Function name: save_me
 * Description  : Save all internal variables of a character to disk.
 * Arguments    : int value_items - If 0, set total value of money to 0,
 *                      otherwise calculate the total value of all the
 *                      stuff a character carries. It is true when the
 *                      player typed save and when he is autosaving.
 */
public nomask void
save_me(int value_items)
{
    if (value_items)
	set_tot_value(compute_values(deep_inventory(this_object())));
    else
	set_tot_value(0);

    /* Do some queries to make certain time-dependent 
     * vars are updated properly.
     */
    query_mana();
    query_fatigue();
    query_hp();
    query_stuffed();
    query_soaked();
    query_intoxicated();
    query_age();
    compute_auto_str();
    compute_recover_str(value_items);
#ifndef NO_SKILL_DECAY
    query_decay_time();
#endif NO_SKILL_DECAY
    seteuid(0);
    SECURITY->save_player();
    seteuid(getuid(this_object()));

    /* If the player is a mortal, we will restart autosave. */
    start_autosave();

#ifndef NO_SKILL_DECAY
    if (!query_wiz_level())
    {
	/* Handle decay here as this function is called regularly and often */
	if (query_skill_decay())
	{
	    set_alarm(1.0, 0.0, decay_skills);
	}
    }
#endif NO_SKILL_DECAY
}

/*
 * Function name:   save_character
 * Description:     Saves all internal variables of a character to disk
 * Returns:         Always 1
 */
public int
save_character(string str) 
{
    save_me(1);
    write("Ok.\n");
    return 1;
}

/*
 * Function name: quit
 * Description:	  The standard routine when quitting. You cannot quit while
 *                you are in direct combat.
 * Returns:	  1 - always.
 */
nomask int
quit(string str)
{
    object *inv;
    int    index, loc, size;

    if ((index = (query_prop(PLAYER_I_AUTOLOAD_TIME) - time())) > 0)
    {
        write("To prevent loss of inventory, you cannot quit just yet.\n");
        write("Please wait another " + CONVTIME(index) + ".\n");
        return 1;
    }
    
    loc = check_recover_loc();
    
    /* No way to chicken out of combat like that, but do allow it when
     * the game is being shut down.
     */
    if (!loc)
    {
        if (objectp(query_attack()))
        {
            write("You cannot leave in such hasty abandon since you are in " +
                "combat.\n\n");
            return 1;
        }
        if (!query_relaxed_from_combat())
        {
            write("Following the recent fight, you must relax a bit more " +
                "before you can leave the realms.\n");
            return 1;
        }
    }

    tell_object(this_object(), "Saving " + query_name() + ".\n");
    say( ({ METNAME + " leaves the realms.\n",
	    TART_NONMETNAME + " leaves the realms.\n",
	    "" }) );
    
    /* For mortals, always save the 'true' race. */
    if (!query_wiz_level())
    {
        reset_race_name();
    }

    /* If you quit while in a team, switch off the auto-brief. */
    if (query_prop(TEMP_BACKUP_BRIEF_OPTION))
    {
        tell_object(this_object(), "As you quit, you switch back to " +
            "verbose mode.\n");
        remove_prop(TEMP_BACKUP_BRIEF_OPTION);
        set_option(OPT_BRIEF, 0);
    }
    
    save_me(0);
    
    inv = deep_inventory(this_object());
    
    /* Only mortals drop stuff when they quit. */
    if (!query_wiz_level())
    {
	index = -1;
	size = sizeof(inv);
	while(++index < size)
	{
	    /* Objects that are neither recoverable, nor autoloading
 	     * should not be destructed if they are droppable.
	     */
	    if (!((loc && inv[index]->check_recoverable()) ||
		  stringp(inv[index]->query_auto_load()) ))
	    {
		/* However, we only try to drop it if the player is
		 * carrying it on the top level.
		 */
		if ( (environment(inv[index]) == this_object()) &&
		    !(inv[index]->query_prop(OBJ_M_NO_DROP)) )
		{
		    command("$drop " + OB_NAME(inv[index]));
		    inv[index] = 0;
		}
	    }
	}
	
	inv = filter(inv, objectp);
    }
    
    /* Remove the objects. If there are some persistant objects left,
     * hammer hard and they will go away eventually.
     */
    size = sizeof(inv);
    index = -1;
    while(++index < size)
    {
	if (objectp(inv[index]))
	{
	    inv[index]->remove_object();
	}
	if (objectp(inv[index]))
	{
	    /* This is the hammer. */
	    SECURITY->do_debug("destroy", inv[index]);
	}
    }
    
    this_object()->remove_object();
    return 1;
}

/*
 * Function name: change_password_new
 * Description  : This function is called by change_password_old to catch the
 *                new password entered by the player. Calls itself again to
 *                verify the new entered password and makes sure the new
 *                password is somewhat secure.
 * Arguments    : string str - the new password.
 */
static nomask void
change_password_new(string str)
{
    write("\n");
    if (!strlen(str))
    {
	write("No password typed, so it was not changed.\n");
	return;
    }

    /* The first time the player types the new password. */
    if (password2 == 0)
    {
	if (strlen(str) < 6)
	{
	    write("New password must be at least 6 characters long.\n");
	    return;
	}

	if (!(SECURITY->proper_password(str)))
	{
	    write("New password must comply with the basic security rules " +
		  "we have for passwords.\n");
	    return;
	}

	password2 = str;
	input_to(change_password_new, 1);
	write("Please type your password again to check.\n");
	write("Password (again): ");
	return;
    }

    /* Second password doesn't match the first one. */
    if (password2 != str)
    {
	write("New passwords don't match! Password not changed.\n");
	return;
    }

    set_password(crypt(password2, 0));	/* Generate new seed */
    password2 = 0;
    write("Password changed.\n");
}

/*
 * Function name: change_password_old
 * Description  : Takes and checks the old password.
 * Arguments    : string str - the given (old) password.
 */
static nomask void 
change_password_old(string str)
{
    write("\n");
    if (!strlen(str) ||
	!match_password(str)) 
    {
	write("Wrong old password.\n");
	return;
    }

    password2 = 0;
    input_to(change_password_new, 1);
    write("To prevent people from breaking your password, we feel the need\n" +
	  "require your password to match certain criteria:\n" +
	  "- the password must be at least 6 characters long;\n" +
	  "- the password must at least contain one 'special character';\n" +
	  "- a 'special character' is anything other than a-z and A-Z;\n" +
	  "- the 'special character' may not be the first or the last\n" +
	  "  letter in the password, that is somewhere before and after a\n" +
	  "  'special character' there must be a normal letter.\n\n" +
	  "New password: ");
}

/*
 * Function name: change_password
 * Description  : Allow a player to change his old password into a new one.
 * Arguments    : string str - the command line argument.
 * Returns:     : int 1 - always.
 */
static nomask int
change_password(string str)
{
    write("Old password: ");
    input_to(change_password_old, 1);
    return 1;
}
