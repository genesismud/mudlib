/*
 * player/cmd_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * Some standard commands that should always exist are defined here.
 * This is also the place for the quicktyper command hook.
 */

#include <cmdparse.h>
#include <composite.h>
#include <files.h>
#include <log.h>
#include <macros.h>
#include <options.h>
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
public nomask varargs void save_me(int display);
nomask int quit(string str);
public int save_character(string str);
static nomask int change_password(string str);

/*
 * Global variables, they are static and will not be saved.
 */
static int save_alarm;           /* The id of the autosave-alarm */

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
    save_alarm = 0;
    if (interactive())
    {
	save_alarm = set_alarm(300.0, 0.0, save_me);
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
 * Function name: compute_auto_str
 * Description  : Walk through the inventory and check all the objects for
 *                the function query_auto_load(). Constructs an array with
 *                all returned strings. query_auto_load() should return
 *                a string of the form "<file>:<argument>".
 */
static nomask void
compute_auto_str()
{
    string *list;

    list = map(deep_inventory(this_object()), &->query_auto_load());
    list = filter(list, stringp);

    set_auto_load(list);
}

/*
 * Function name: check_valid_startloc
 * Description  : This function checks if the player is standing on a spot
 *                where recover may happend
 * Returns      : int - if TRUE, it's allowed to recover.
 *                 (1 start location, 2 armageddon, 3 wizard)
 */
nomask int
check_valid_startloc()
{
    object obj;
    string env;

    /* Check for armageddon */
    if (ARMAGEDDON->shutdown_active())
	return 2;

    /* Wizards always recover. */
    if (query_wiz_level())
        return 3;

    /* Check for recoverable surroundings */
    if (objectp(obj = environment(this_object())))
	env = MASTER_OB(obj);
    else
	return 0;

    if (IN_ARRAY(env, SECURITY->query_list_def_start()) ||
        IN_ARRAY(env, SECURITY->query_list_temp_start()))
    {
	return 1;
    }

    return 0;
}

/*
 * Function name: compute_recover_str
 * Description  : Walk through the inventory and check all the objects for
 *                the recover property.
 * Arguments    : int display - if true, player manually typed save and we
 *                    show the glowing status.
 */
static nomask void
compute_recover_str(int display)
{
    object *glowing, *failing;
    string str;
    string *recover = ({ });
    int index, size;

    /* Find all recoverable items on the player. */
    glowing = deep_inventory(this_object());
    glowing = filter(glowing, &->check_recoverable(1));

    /* If the game reboots automatically, some items may fail to glow. */
    if (!(ARMAGEDDON->query_manual_reboot()))
    {
        failing = filter(glowing, &->may_not_recover());
        glowing -= failing;
    }

    index = sizeof(glowing);
    while(--index >= 0)
    {
        /* If it's recovering, add it to the recover array, otherwise remove
         * it from the glowing list. */
        if (strlen(str = glowing[index]->query_recover()))
        {
            recover += ({ str });
	}
	else
	{
	    glowing[index] = 0;
	}
    }

    set_recover_list(recover);

    /* Display message if player manually saved. */
    if (display)
    {
        /* Remove cleared items from the array. */
        glowing = filter(glowing, objectp);
        if (size = sizeof(glowing))
        {
	    tell_object(this_object(), capitalize(COMPOSITE_DEAD(glowing)) +
	        ((size == 1) ? " glows" : " glow") + " briefly.\n");
        }
        if (size = sizeof(failing))
        {
	    tell_object(this_object(), capitalize(COMPOSITE_DEAD(failing)) +
	        ((size == 1) ? " fails" : " fail") + " to glow briefly.\n");
	}
    }
}

/*
 * Function name: save_me
 * Description  : Save all internal variables of a character to disk.
 * Arguments    : int display - if true, display recovery.
 */
public nomask varargs void
save_me(int manual)
{
    /* Do some queries to make certain time-dependent 
     * vars are updated properly.
     */
    this_object()->query_mana();
    query_fatigue();
    query_hp();
    query_stuffed();
    query_soaked();
    query_intoxicated();
    query_age();
    compute_auto_str();
    compute_recover_str(manual);
#ifndef NO_SKILL_DECAY
    query_decay_time();
#endif NO_SKILL_DECAY
    set_logout_time();
    set_logout_location();
    store_saved_props();

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
 * Function name: save_character
 * Description  : Saves all internal variables of a character to disk
 * Arguments    : string str - the command line argument.
 * Returns      : int 1
 */
public int
save_character(string str) 
{
    write("Saving " + query_name() + ".\n");
    /* Display recovery for any argument other than "silent". */
    save_me(str != "silent");
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
    object *inv, *dropped;
    int    startloc, seconds, manual;
    string pname;

    if ((seconds = (query_prop(PLAYER_I_AUTOLOAD_TIME) - time())) > 0)
    {
        write("To prevent loss of inventory, you cannot quit just yet.\n");
        write("Please wait another " + CONVTIME(seconds) + ".\n");
        return 1;
    }

    /* No way to chicken out of combat like that, but do allow it when
     * the game is being shut down.
     */
    startloc = check_valid_startloc();
    if (!startloc)
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

    /* If you quit while in a team, switch off the auto-brief. */
    if (query_prop(TEMP_BACKUP_BRIEF_OPTION))
    {
        tell_object(this_object(), "As you quit, you switch back to " +
            "verbose mode.\n");
        remove_prop(TEMP_BACKUP_BRIEF_OPTION);
        set_option(OPT_BRIEF, 0);
    }

    /* Consider all items in the deep inventory of the player. If something is
     * in a container, it's considered individually and moved to the top of
     * the inventory if it needs to be dropped. */
    inv = deep_inventory(this_object());

    /* Find out which objects are non-recoverable, non-recoverable and
     * (not un)droppable. For mortals, we try to drop those. */
    if (!query_wiz_level())
    {
        dropped = filter(inv, &not() @ &->query_auto_load());
        /* During a manual reboot, no recoverable object will fail to glow. */
        manual = ARMAGEDDON->query_manual_reboot();
        dropped = filter(dropped, &not() @ &->check_recoverable(manual));
        dropped = filter(dropped, &not() @ &->query_prop(OBJ_M_NO_DROP));

        foreach(object item: dropped)        
        {
            /* Move to the top of the inventory, otherwise it cannot be dropped. */
            if (environment(item) != this_object())
            {
                item->move(this_object(), 1);
            }

            /* For heaps we have to drop the plural items. */
            if (IS_HEAP_OBJECT(item))
            {
		pname = OB_NAME(item) + "s";
                item->add_pname(pname);
		command("$drop " + pname);
                item->remove_pname(pname);
            }
	    else
            {
		command("$drop " + OB_NAME(item));
	    }
        }

        /* Everything that wasn't explicitly dropped is ready for destruction,
         * so that also the recoverable content from a non-recoverable bag
         * is destroyed. */
        inv -= dropped;
    }

    /* Give the message before resetting the race name (but after dropping of
     * items). */
    say( ({ METNAME + " leaves the realms.\n",
	    TART_NONMETNAME + " leaves the realms.\n",
	    "" }) );

    /* For mortals, always save the 'true' race. */
    if (!query_wiz_level())
    {
        reset_race_name();
    }

    /* Save whatever needs to be saved. */
    tell_object(this_object(), "Saving " + query_name() + ".\n");
    save_me(1);
    tell_object(this_object(), "Goodbye. Until next time.\n");
    catch_gmcp(GMCP_CORE_GOODBYE, "Goodbye. Until next time.");

    /* Inform the statistics module. */
    catch(WEBSTATS_CENTRAL->player_logout(this_object()));

    /* Remove the objects. If there are some persistant objects left,
     * hammer hard and they will go away eventually.
     */
    inv->remove_object();
    inv = filter(inv, objectp);

    foreach(object item: inv)
    {
        /* This is the hammer. */
        SECURITY->do_debug("destroy", item);
    }

    this_object()->add_prop("_mark_quit", 1);
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
 *                string password1 - the password given on the 1st pass.
 */
static nomask varargs void
change_password_new(string str, string password1 = 0)
{
    write("\n");
    if (!strlen(str))
    {
	write("No password typed, so it was not changed.\n");
	return;
    }

    /* The first time the player types the new password. */
    if (password1 == 0)
    {
	if (strlen(str) < 8)
	{
	    write("New password must be at least 8 characters long.\n");
	    return;
	}

	if (!(SECURITY->proper_password(str)))
	{
	    write("The new password must contain atleast two digits or special characters.\n"); 
	    return;
	}

	input_to(&change_password_new(, str), 1);
	write("Please type your password again to check.\n");
	write("Password (again): ");
	return;
    }

    /* Second password doesn't match the first one. */
    if (str != password1)
    {
	write("New passwords don't match! Password not changed.\n");
	return;
    }

    /* Generate new seed */
    set_password(crypt(password1, CRYPT_METHOD));
    /* Save the new password. */
    save_me(0);
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

    input_to(change_password_new, 1);
    write("To prevent people from breaking your password, we feel the need\n" +
	  "require your password to match certain criteria:\n" +
	  "- the password must be at least 8 characters long;\n" +
	  "- the password must at least contain two 'special characters';\n" +
	  "- a 'special character' is anything other than a-z and A-Z;\n\n" +
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
    write("To change your password, first enter the old password.\n" +
        "Old password: ");
    input_to(change_password_old, 1);
    return 1;
}

/*
 * Function name: block_action
 * Description:   Players can block certain actions on being performed on them.
 * Arguments:     string cmd    - the name of the executed command
 *                object target - the target in your inventory, if 0 it's you.
 *                object actor  - the command performer
 *                int cmd_type  - the command attributes (from cmdparse.h) 
 * Returns:       0 - command allowed
 *                1 - command blocked, no error message provided
 *                string - command blocked, use string as error message.
 */
public mixed
block_action(string cmd, object target, object actor, int cmd_type)
{
    if ((cmd_type & ACTION_INTIMATE) && query_option(OPT_BLOCK_INTIMATE) &&
        !query_friendship(actor->query_real_name()))
    {
        return query_The_name(actor) + " is not in the mood for intimate behaviour.\n";
    }
    return 0;
}
