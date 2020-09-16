/*
 * /std/player.c
 *
 * This file is the basis of all players.
 */

#pragma save_binary
#pragma strict_types

inherit "/std/living";

/* This order is on purpose to limit the number of prototypes necessary. */
#include "/std/player/savevars_sec.c"
#include "/std/player/quicktyper.c"
#include "/std/player/getmsg_sec.c"
#include "/std/player/cmd_sec.c"
#include "/std/player/stats_sec.c"
#include "/std/player/death_sec.c"
#include "/std/player/querys_sec.c"
#include "/std/player/pcombat.c"
#include "/std/player/more.c"

#include <const.h>
#include <files.h>
#include <formulas.h>
#include <language.h>
#ifndef OWN_STATUE
#include <living_desc.h>
#endif
#include <login.h>
#include <macros.h>
#include <mail.h>
#include <money.h>
#include <options.h>
#include <ss_types.h>
#include <std.h>
#include <stdproperties.h>

#define LINKDEATH_TIME          (180.0) /* three minutes */
#define RETURN_TO_LOCATION_TIME (60 * 15)       /* 15 minutes */
#define AUTOLOAD_INTERVAL       (0.0)
#define RECOVERY_INTERVAL       (0.20)

/*
 * Global variables. They are not saved.
 */
private static int    ld_alarm;    /* The alarm used when a player linkdies. */
#ifndef NO_SKILL_DECAY
private static int do_skill_decay = 0; /* Flag to control skill decay */
#endif NO_SKILL_DECAY
private static object magic_map;  /* the magic map object. */

/*
 * Function name: query_def_start
 * Description  : This function returns the very basic starting location
 *                for people of this race. Notice that we use query_race
 *                and not query_race_name since query_race always returns
 *                a valid race name. To alter the starting location of the
 *                player, use set_default_start_location.
 */
public nomask string
query_def_start()
{
    if (query_wiz_level())
    {
        return WIZ_ROOM;
    }
    return RACESTART[query_race()];
}

/*
 * Function name: query_orig_learn
 * Description:   Return the default starting stats of a player
 *                This function is supposed to be replaced in inheriting
 *                player objects.
 */
public int *
query_orig_learn()
{
    /*        STR, DEX, CON, INT, WIS, DIS */
    return ({  17,  16,  16,  17,  17,  17 });
}

#ifndef NO_ALIGN_TITLE
/*
 * Function name: query_new_al_title
 * Description:   Return the default starting title of a player
 *                This function is supposed to be replaced in inheriting
 *                player objects.
 */
public string
query_new_al_title()
{
    return "neutral";
}
#endif NO_ALIGN_TITLE

/*
 * Function name: fixup_screen
 * Description:   Restore the players screen width. Normally called
 *                during login.
 */
public nomask void
fixup_screen()
{
    int width = query_option(OPT_SCREEN_WIDTH);

    /* Value 0 means unset, so default to 80. */
    if (!width)
    {
        width = 80;
    }
    /* Value -1 means no screen width, ergo no wrapping. */
    if (width == -1)
    {
        width = 0;
    }

    set_screen_width(width);
}

#ifndef NO_SKILL_DECAY
/*
 * Function name:   query_skill_decay
 * Description:     Gives back the skill decay status
 * Returns:         The skill decay status
 */
public nomask int
query_skill_decay()
{
    return do_skill_decay;
}

/*
 * Function name: get_train_max
 * Description:   Return the max value of a skill that a trainer trains.
 * Arguments:     skill - the skill to be examined.
 *                ob - the object defining the skill
 * Returns:       See above.
 */
static nomask int
get_train_max(int skill, mixed ob)
{
    int rval = 0;

#ifdef LOG_BAD_TRAIN
    if (catch(rval = ob->sk_query_max(skill, 1)))
        log_file(LOG_BAD_TRAIN, ctime(time()) + ": " + ob + "\n");
#else
    catch(rval = ob->sk_query_max(skill, 1));
#endif LOG_BAD_TRAIN

    return rval;
}

/*
 * Function name: query_decay_skill
 * Description:   Return 1 if a skill should be decayed, 0 if not.
 * Arguments:     list - the list of objects defining the skill train max
 *                skill - the skill to be examined.
 * Returns:       See above.
 */
static nomask int
query_decay_skill(mixed list, int skill)
{
    int *sklist;
    int index;
    int size;
    int maximum;
    int sk;

    /* Load all trainers first */
    catch(list->teleledningsanka());

    /* Check the contents */
    sklist = ({ }) + map(list, &get_train_max(skill, ));
    sk = (sizeof(SS_SKILL_DESC[skill]) ? SS_SKILL_DESC[skill][4] : 0);
    sklist += ({ ((sk > MIN_SKILL_LEVEL) ? sk : MIN_SKILL_LEVEL) });

    maximum = applyv(max, sklist);

    return (query_base_skill(skill) > maximum);
}

/*
 * Function name: decay_skills
 * Description:   Do skill decay in the player
 *                Call this function ONLY if it's necessary, as when
 *                entering the game or entering/leaving a guild as
 *                it's a bit costly.
 */
static nomask void
decay_skills()
{
    mixed obs;
    mixed otmp;
    int *skills, i, sz;
    string str, tmp;

    /* Only do this on the proper interval, and wizards pass by, of course */
    if ((query_decay_time() < SKILL_DECAY_INTERVAL) ||
        query_wiz_level())
    {
        return;
    }

    set_this_player(this_object());

    /* Reset the time for next call. */
    reset_decay_time();

    /* Get the list of trainer objects */
    obs = ({});
    otmp = this_object()->query_guild_trainer_occ();
    obs += pointerp(otmp) ? otmp : ({ otmp });
    otmp = this_object()->query_guild_trainer_race();
    obs += pointerp(otmp) ? otmp : ({ otmp });
    otmp = this_object()->query_guild_trainer_lay();
    obs += pointerp(otmp) ? otmp : ({ otmp });
    otmp = this_object()->query_guild_trainer_craft();
    obs += pointerp(otmp) ? otmp : ({ otmp });
    obs -= ({ 0 });

    /* Filter all relevant skills */
    skills = filter(query_all_skill_types(), &operator(>)(99999));

    /* Find out what skills need decay */
    skills = filter(skills, &query_decay_skill(obs, ));

    /* Do decay */
    if (sizeof(skills))
    {
        tmp = ((tmp = this_object()->query_guild_name_occ()) ? tmp : "") + ", " +
            ((tmp = this_object()->query_guild_name_lay()) ? tmp : "") + ", " +
	    ((tmp = this_object()->query_guild_name_craft()) ? tmp : "") + ", " +
            ((tmp = this_object()->query_guild_name_race()) ? tmp : "");

        str = sprintf("%s\t\t%s\n%s\t\t", this_object()->query_name(), tmp,
            ctime(time()));

        sz = sizeof(skills);
        for (i = 0; i < sz; i++)
        {
            str += sprintf("%i ", skills[i]);
            set_skill(skills[i], query_base_skill(skills[i]) - 1);
        }
        log_file("DECAY_LOG", str + "\n", 50000);
    }
    else
    {
        do_skill_decay = 0;
    }
}

/*
 * Function name: move_living
 * Description  : Redefinition of move_living to handle the player exclusive
 *                interface to the magic map.
 * Arguments    : see 'sman /std/living move_living'
 */
public varargs int
move_living(string how, mixed to_dest, int dont_follow, int no_glance)
{
    int result = ::move_living(how, to_dest, dont_follow, no_glance);

    if (!result)
    {
        if (!objectp(magic_map))
        {
	    magic_map = present(MAGIC_MAP_ID);
        }
        catch(magic_map->notify_new_room(to_dest));
    }

    return result;
}

/*
 * Function name:   setup_skill_decay()
 * Description:     setup the skill decay flag.
 */
public nomask void
setup_skill_decay()
{
    if (query_wiz_level())
        return;

    do_skill_decay = 1;
    set_alarm(90.0, 0.0, decay_skills);
}
#endif NO_SKILL_DECAY

/*
 * Function name: reset_userids
 * Description  : Called to set the euid of this player. Wizards get their own
 *                name as effective user id.
 */
public void
reset_userids()
{
    seteuid(0);

    if (SECURITY->query_wiz_rank(query_real_name()))
    {
        SECURITY->reset_wiz_uid(this_object());
    }

    seteuid(getuid());
}

/*
 * Function:     init_vars_before_load
 * Description:  Initialises all variables to default conditions.
 */
static nomask void
init_vars_before_load()
{
    int i;
    int *ostat = RACESTAT[query_race()];

    if (!pointerp(ostat))
    {
	return;
    }

    i = -1;
    while(++i < SS_NO_EXP_STATS)
    {
        set_base_stat(i, ostat[i]);
    }

    stats_to_acc_exp();

    set_learn_pref(query_orig_learn());

#ifndef NO_ALIGN_TITLE
    set_al_title(query_new_al_title());
#endif NO_ALIGN_TITLE
}

static object
clone_recover_object(string entry, string type, function init)
{
    string file, argument;

    if (!stringp(entry) || !strlen(entry))
        return 0;

    if (sscanf(entry, "%s:%s", file, argument) != 2)
    {
        file = entry;
        argument = 0;
    }

    set_this_player(this_object());

    object ob;
    catch(ob = clone_object(file));
    if (!objectp(ob))
    {
#ifdef LOG_FAILED_RECOVERY
        SECURITY->log_syslog(LOG_FAILED_RECOVERY,
            sprintf("%s %-11s %-8s %s\n", ctime(time()),
                capitalize(this_object()->query_real_name()), type, entry));
#endif
        return 0;
    }

    /* Note that we don't check for strlen() since we also want to call
     * init_recover() if the format is 'filename:'.
     */
    if (stringp(argument))
    {
        /*
         * Call the provided init function
         * If init returns true the object is removed. If the return value is
         * an object that object is return instead, allowing recovered items to
         * be replaced.
         */
        mixed ret;

        try {
            if (ret = init(ob, argument))
            {
                ob->remove_object();

                if (objectp(ret)) {
                    return ret;
                }

                return 0;
            }
        } catch (mixed ex) {
#ifdef LOG_FAILED_RECOVERY
            SECURITY->log_syslog(LOG_FAILED_RECOVERY,
                    sprintf("%s %-11s %-8s %s\n", ctime(time()),
                        capitalize(this_object()->query_real_name()), type, entry));
#endif
            ob->remove_object();
            return 0;
        }
    }

    return ob;
}

/*
 * Function name: slow_load_auto_files
 * Description  : Loads one autoloaded object. We use an alarm to make sure
 *                that people always get their stuff, even when they carry too
 *                much for their own good.
 * Arguments    : string *auto_files - the autoloading files still to load.
 */
nomask static void
slow_load_auto_files()
{
    string *auto_files = query_pending_auto_load();

    if (sizeof(auto_files) > 1)
    {
        set_alarm(AUTOLOAD_INTERVAL, 0.0, &slow_load_auto_files());
    }
    else
    {
        remove_prop(PLAYER_I_AUTOLOAD_TIME);
    }

    /* Pop the one we're loading of the list and save the new pending list */
    string entry = auto_files[0];
    set_pending_auto_load(auto_files[1..]);

    object ob = clone_recover_object(entry, "AUTO", &->init_arg());

    if (!objectp(ob))
    {
        return;
    }

    ob->move(this_object(), 1);
}

/*
 * Function name: load_auto_files
 * Description  : Loads all autoloaded objects. We use an alarm to make sure
 *                that people always get their stuff, even when they carry too
 *                much for their own good.
 */
nomask static void
load_auto_files()
{
    string *auto_files = query_auto_load() + query_pending_auto_load();

    set_pending_auto_load(auto_files);
    set_auto_load(({ }));

    if (!sizeof(auto_files))
    {
        return;
    }

    add_prop(PLAYER_I_AUTOLOAD_TIME, (time() + 5 + sizeof(auto_files)));
    set_alarm(AUTOLOAD_INTERVAL, 0.0, &slow_load_auto_files());
}

/*
 * Function name: slow_load_recover_files
 * Description  : Loads one recoverable object. We use an alarm to make sure
 *                that people always get their stuff, even when they carry too
 *                much for their own good.
 * Arguments    : string *recover_files - the recoverable files still to load.
 */
nomask static void
slow_load_recover_files()
{
    string *recover_files = query_pending_recover();

    if (sizeof(recover_files) > 1)
    {
        set_alarm(RECOVERY_INTERVAL, 0.0, &slow_load_recover_files());
    }

    /* Pop the one we're loading of the list and save the new pending list */
    string entry = recover_files[0];
    set_pending_recover(recover_files[1..]);

    object ob = clone_recover_object(entry, "RECOVERY", &->init_recover());

    if (!objectp(ob))
    {
        return;
    }

    /* Tell the person. Little touch to tell when it's the last one. */
    write("You " + ((sizeof(recover_files) == 1) ? "finally " : "") +
        "recover your " + ob->short() + ".\n");
    ob->move(this_object(), 1);
}

/*
 * Function name: load_recover_files
 * Description  : Loads all recoverable objects. We use an alarm to make sure
 *                that people always get their stuff, even when they carry too
 *                much for their own good.
 */
nomask static void
load_recover_files()
{
    string *recover_files = query_recover_list() + query_pending_recover();

    /* Set up the list of files to recover and clear the list of recover_files */
    set_pending_recover(recover_files);
    set_recover_list(({ }));

    /* Do not restore after a certain time. */
    if (time() - query_logout_time() > F_RECOVERY_LIMIT)
    {
        return;
    }

    int size = sizeof(recover_files);
    if (size)
    {
        catch_tell("Preparing to recover " + LANG_WNUM(size) + " item" +
            ((size == 1) ? "" : "s") + ".\n");
        set_alarm(RECOVERY_INTERVAL, 0.0, &slow_load_recover_files());
    }
}

/*
 * Function name: load_auto_shadows
 * Description  : This function loads and initialises all shadows that the
 *                player should have when he logs in. No special measures are
 *                taken for shadows at login time.
 */
nomask static void
load_auto_shadows()
{
    string *load_arr;
    string file;
    string argument;
    object ob;
    int    index;
    int    size;

    load_arr = query_autoshadow_list();
    if (!sizeof(load_arr))
    {
        return;
    }

    index = -1;
    size = sizeof(load_arr);
    while(++index < size)
    {
        if (sscanf(load_arr[index], "%s:%s", file, argument) != 2)
        {
            write("Shadow load string corrupt: " + load_arr[index] + "\n");
            continue;
        }
        if (LOAD_ERR(file) ||
            !objectp(ob = find_object(file)))
        {
            write("Shadow not available: " + file + "\n");
            continue;
        }

        try {
            ob = clone_object(file);

            if (argument)
            {
                ob->autoload_shadow(argument);
            }
            else
            {
                ob->autoload_shadow(0);
            } 
        } 
        catch (mixed err) 
        {
            write("An error occured while loading a shadow.\n");
            SECURITY->log_syslog(LOG_FAILED_RECOVERY,
                    sprintf("%s %-11s %-8s %s / %s\n", ctime(time()),
                        capitalize(this_object()->query_real_name()), "SHADOW", load_arr[index], err));
        }
    }
}

/*
 * Function name: setup_player
 * Description:   Restore player variables from the player file and go through
 *                startup routines.
 * Arguments:     (string) pl_name - The player's name
 * Returns:       True if setup completed normally
 */
private static nomask int
setup_player(string pl_name)
{
    string *souls;

    set_name(pl_name);

    /* No adjectives and no default */
    ::set_adj(({ }));
    /* All variables to default condition before loading. */
    init_vars_before_load();

    seteuid(0);
    if (!SECURITY->load_player())
    {
        return 0;
    }
    reset_userids();

    /* Set the adjectives as loaded */
    set_adj(0);
    fixup_screen();

    /* Mortals should have one of the base races by default. */
    if (!query_wiz_level())
    {
        reset_race_name();
    }

    add_name(query_race_name());
    add_pname(LANG_PWORD(query_race_name()));

    if (query_wiz_level())
    {
        /* Wizards should have the term wizard as added name. */
        add_name("wizard");
        add_pname("wizards");
    }
    else
    {
        /* Mortals should not have altered entrance messages. */
        move_reset();
    }

    /* Transform domain bits if they still exist in the old format. */
    transform_domain_bits();

    /* Make some sanity things to guard against old and patched .o files */
    set_learn_pref(query_learn_pref(-1));

    set_living_name(pl_name);
    cmd_sec_reset();
    player_save_vars_reset();

    /* Check the accumulated experience and then set the stats. */
    check_acc_exp();
    acc_exp_to_stats();
    reset_exp_gain_desc();

    /* Restore the saved properties and add a default one. */
    add_prop(PLAYER_I_MORE_LEN, 20);
    init_saved_props();

    /* Restore the whimpy option into the internal variable. */
    ::set_whimpy(query_option(OPT_WHIMPY));

    /* Set the always known property if wanted. */
    if (query_wiz_level())
    {
        add_prop(LIVE_I_ALWAYSKNOWN, query_option(OPT_ALWAYS_KNOWN));
    }
    else
    /* Non wizards should not have a lot of souls */
    {
        if (sizeof(souls = query_cmdsoul_list()))
        {
	    foreach(string soul: souls)
            {
                remove_cmdsoul(soul);
            }
        }

        if (sizeof(souls = query_tool_list()))
        {
            foreach(string soul: souls)
            {
                this_object()->remove_toolsoul(soul);
            }
        }
    }

    /* Get the autoloading shadows and the autoloading objects. Start the
     * recovery with a little alarm to make it safe. */
    load_auto_shadows();
    load_auto_files();
    set_alarm(RECOVERY_INTERVAL, 0.0, load_recover_files);

    /* Set up skill decay now that the guild shadows are loaded. Do a first
     * decay as well, making it a bit more frequent for people who log
     * on/off/on all the time.
     */
#ifndef NO_SKILL_DECAY
    decay_time = time();
    setup_skill_decay();
#endif NO_SKILL_DECAY

    query_combat_object()->cb_configure();
}

/*
 * Function name: start_player
 * Description  : Final set starting the player.
 */
public void
start_player()
{
    /* Get the soul commands */
    this_object()->load_command_souls();
    command("$look");
    say(QCNAME(this_object()) + " enters the realms.\n");
}

#ifdef CHANGE_PLAYEROB_OBJECT
/*
 * Function name: change_player_object
 * Description:   Initialize this player object based on an existing one.
 * Arguments:     (object) old_plob - the player object to be used to initialize
 *                                   this one.
 * Returns:       True if initialization was successful
 */
public nomask int
change_player_object(object old_plob)
{
    if (MASTER_OB(previous_object()) != CHANGE_PLAYEROB_OBJECT)
    {
        return 0;
    }

    set_name(old_plob->query_real_name());

    setup_player(old_plob->query_real_name());

    this_object()->start_player();

    /* Reset the experience counters. */
    reset_exp_gain_desc();

    return 1;
}
#endif CHANGE_PLAYEROB_OBJECT

/*
 * Function name: try_start_location
 * Description  : Attempt to make the player start in a start location.
 * Arguments    : string path - the path to try.
 */
static nomask void
try_start_location(string path)
{
    object room;

    /* Sanity check. */
    if (!strlen(path))
    {
        return;
    }
    /* Strip any .c if present. */
    sscanf(path, "%s.c", path);
    if (file_size(path + ".c") <= 0)
    {
        return;
    }

    /* Try to load the room, and then try to move the player. */
    LOAD_ERR(path);
    if (objectp(room = find_object(path)))
    {
        catch(move_living(0, room));
    }
}

/*
 * Function name: enter_game
 * Description  : Enter the player into the game.
 * Arguments    : string pl_name - the name of the player.
 *                string pwd     - the password if it was changed.
 * Returns      : int 1/0 - login succeeded/failed.
 */
public nomask int
enter_game(string pl_name, string pwd)
{
    string path;
    string wname;
    object room;
    int    savetime;

    if ((MASTER_OB(previous_object()) != LOGIN_OBJECT) &&
        (MASTER_OB(previous_object()) != LOGIN_NEW_PLAYER))
    {
        write("Bad login object: " + file_name(previous_object()) + "\n");
        return 0;
    }

    setup_player(pl_name);

    /* Tell the player when he was last logged in and from which site. */
    if (MASTER_OB(previous_object()) == LOGIN_OBJECT)
    {
        write("Last login at  : " + ctime(query_login_time()) +
            "\nLast login from: " + query_login_from() + "\n");
    }

    object location = query_logout_location();

    if (!query_ghost() && location && (time() - query_logout_time() < RETURN_TO_LOCATION_TIME) && 
        !location->query_prop(ROOM_M_NO_TELEPORT) && 
        !location->query_prop(ROOM_M_NO_TELEPORT_TO)) 
    {
        catch(move_living(0, location));
    }

    /* Try the temporary start location. */
    if (!environment() && VALID_TEMP_START_LOCATION(query_temp_start_location()))
    {
        try_start_location(query_temp_start_location());
	/* If you used a temporary start location, you will heal through
	 * your time away from the realms. Healing rate is 50% of normal. */
	savetime = (file_time(PLAYER_FILE(pl_name) + ".o") + time()) / 2;
	save_vars_reset(savetime);
        set_temp_start_location(0);
    }

    /* For juniors, try the wizard workroom, but not if they are dead. */
    if (!environment() && wildmatch("*jr", pl_name) && !query_ghost())
    {
        /* Jr may be a registered Jr with a funny name, or otherwise
         * default to the regular name WizardJr. */
        wname = SECURITY->query_find_first(pl_name);
        if (!wname)
        {
            wname = extract(pl_name, 0, -3);
        }
        try_start_location(SECURITY->wiz_home(wname));
    }

    /* Try the default start location if necessary. */
    if (!environment())
    {
	/* Reset the default if there is no (valid) current default location. */
        if (!query_wiz_level() &&
            !VALID_DEF_START_LOCATION(query_default_start_location()))
        {
            set_default_start_location(query_def_start());
        }
        try_start_location(query_default_start_location());
    }

    /* Default start location failed. Try the racial default. */
    if (!environment())
    {
        write("\nSERIOUS PROBLEM with your start location.\n" +
            "You revert to your default racial start location.\n\n");
        set_default_start_location(query_def_start());
        try_start_location(query_def_start());
    }

    /* Eeks! Racial default start location failed. */
    if (!environment())
    {
        write("\nSERIOUS PROBLEM!\nYou are starting in the void!\n" +
            "Report this to a knowledgeble wizard at once!\n\n");
    }

    /* Start him up */
    this_object()->start_player();

    /* Do this after startup, so we can use the address and time at startup. */
    set_login_time();
    set_login_from();

    /* Let players start even if their start location is bad */
    if (!environment() &&
        !query_wiz_level())
    {
        if (catch(move_living(0, query_def_start())))
        {
            /* If this start location is corrupt too, destruct the player */
            write("PANIC, your starting locations are corrupt!!\n");
            destruct();
        }
    }

    /* If a password was changed, set it. */
    if (strlen(pwd))
    {
        set_password(pwd);
        save_me();
    }

    return 1;
}

/*
 * Function name: open_player
 * Description  : This function may only be called by SECURITY or by the
 *                login object to reset the euid of this object.
 */
public nomask void
open_player()
{
    if ((previous_object() == find_object(SECURITY)) ||
        (MASTER_OB(previous_object()) == LOGIN_OBJECT))
    {
        seteuid(0);
    }
}

/*
 * Function name: save_player
 * Description  : This function actually saves the player object.
 * Arguments    : string pl_name - the name of the player
 * Returns      : int 1/0 - success/failure.
 */
nomask public int
save_player(string pl_name)
{
    if (!pl_name)
    {
        return 0;
    }

    seteuid(getuid(this_object()));
    save_object(PLAYER_FILE(pl_name));
    seteuid(getuid(this_object()));

    return 1;
}

/*
 * Function name: load_player
 * Description  : This function actually loads the player file into the
 *                player object.
 * Arguments    : string pl_name - the name of the player.
 * Returns      : int 1/0 - success/failure.
 */
nomask public int
load_player(string pl_name)
{
    int ret;

    if (!pl_name)
    {
        return 0;
    }

    seteuid(getuid(this_object()));
    ret = restore_object(PLAYER_FILE(pl_name));

    /* Initialize mapping variables. */
    if (!mappingp(m_gift_accept))
        m_gift_accept = ([ ]);
    if (!mappingp(m_gift_reject))
        m_gift_reject = ([ ]);
    if (!mappingp(m_gift_today))
        m_gift_today = ([ ]);
    if (!mappingp(m_friends_list))
        m_friends_list = ([ ]);
    if (!mappingp(m_bits))
        m_bits = ([ ]);
    if (!mappingp(m_vars))
        m_vars = ([ ]);
    if (!m_alias_list)
        m_alias_list = DEFAULT_ALIASES;
    if (!m_nick_list)
        m_nick_list = ([ ]);
    if (!strlen(options))
	options = OPT_DEFAULT_STRING;

    seteuid(0);
    return ret;
}

/*
 * Function name: linkdeath_hook
 * Description  : This routine is called when the player linkdies. Do not mask
 *                it unless you really need to.
 * Arguments    : int linkdeath - 1/0 - if true, the player linkdied, else
 *                    he revives from linkdeath.
 */
public void
linkdeath_hook(int linkdeath)
{
}

/*
 * Function name: actual_linkdeath
 * Description  : This function is called when the player actually linkdies.
 *                If the player is in combat, this will be delayed, or else
 *                it is called directly.
 */
static nomask void
actual_linkdeath()
{
#ifdef STATUE_WHEN_LINKDEAD
#ifdef OWN_STATUE
    OWN_STATUE->linkdie(this_object());
#else
    tell_room(environment(), LD_STATUE_TURN(this_object()), ({ }) );
#endif OWN_STATUE
#endif STATUE_WHEN_LINKDEAD

    /* People should not autosave while they are linkdead. */
    stop_autosave();

    if (ld_alarm)
    {
        SECURITY->notify(this_object(), CONNECT_REAL_LD);
        remove_alarm(ld_alarm);
        ld_alarm = 0;
    }
    set_linkdead(1);

    /* Allow a shadow to take notice of the linkdeath. */
    this_object()->linkdeath_hook(1);
    /* Allow items in the top level inventory of the player to take notice
     * of the linkdeath.
     */
    all_inventory(this_object())->linkdeath_hook(this_object(), 1);
}

/*
 * Function name: linkdie
 * Description  : When a player linkdies, this function is called.
 */
nomask public void
linkdie()
{
    if (previous_object() != find_object(SECURITY))
    {
        return;
    }

    if (query_relaxed_from_combat())
    {
        actual_linkdeath();
    }
    else
    {
#ifdef STATUE_WHEN_LINKDEAD
#ifdef OWN_STATUE
        OWN_STATUE->nonpresent_linkdie(this_object());
#endif OWN_STATUE
#endif STATUE_WHEN_LINKDEAD

        tell_room(environment(), ({
            capitalize(query_real_name()) + " loses touch with reality.\n",
            "The " + query_nonmet_name() + " loses touch with reality.\n",
            "" }),
            ({ this_object() }) );

        ld_alarm = set_alarm(LINKDEATH_TIME, 0.0, actual_linkdeath);
    }

    /* Clear the GMCP data when someone linkdies. */
    set_gmcp(GMCP_CORE_SUPPORTS_SET, ({ }) );
    gmcp_hello( ([ GMCP_CLIENT : 0, GMCP_VERSION : "" ]) );
}

/*
 * Function name: query_linkdead_in_combat
 * Description  : This function returns true if the player is linkdead,
 *                but still in combat.
 * Returns      : int 1/0 - in combat while linkdead or not.
 */
nomask public int
query_linkdead_in_combat()
{
    return (ld_alarm != 0);
}

/*
 * Function name: revive
 * Description  : Called when a player revives from linkdeath.
 */
nomask public void
revive()
{
    if (MASTER_OB(previous_object()) != LOGIN_OBJECT)
    {
        return;
    }

    tell_object(this_object(), "You sense that you have " +
        MAIL_FLAGS[MAIL_CHECKER->query_mail(query_real_name())] + ".\n\n");

    set_login_from();
    gmcp_map_reset();

    /* If the player is not in combat, revive him. Else, just give a
     * a message about the fact that the player reconnected.
     */
    if (!ld_alarm)
    {
        set_linkdead(0);

#ifdef OWN_STATUE
        OWN_STATUE->revive(this_object());
#else
        tell_room(environment(), QCTNAME(this_object()) + " " +
            STATUE_TURNS_ALIVE + ".\n", ({ this_object() }) );
#endif OWN_STATUE

        /* We reset these variables so the player does not gain mana or
         * hitpoints while in LD.
         */
        player_save_vars_reset();
        save_vars_reset();

        /* Start autosaving again. */
        start_autosave();

        /* Allow a shadow to take notice of the revival. */
        this_object()->linkdeath_hook(0);
        /* Allow items in the top level inventory of the player to take notice
         * of the revival.
         */
        all_inventory(this_object())->linkdeath_hook(this_object(), 0);
    }
    else
    {
        tell_room(environment(), ({ capitalize(query_real_name()) +
            " gets in touch with reality again.\n",
            "The " + query_nonmet_name() +
            " gets in touch with reality again.\n",
            "" }),
            ({ this_object() }) );

#ifdef OWN_STATUE
        OWN_STATUE->nonpresent_revive(this_object());
#endif OWN_STATUE
        remove_alarm(ld_alarm);
        ld_alarm = 0;
    }
}

/*
 * Function name: switch_connection
 * Description  : Called when a player makes a new connection to this body
 *                without being linkdead first.
 */
nomask public void
switch_connection()
{
    if (MASTER_OB(previous_object()) != LOGIN_OBJECT)
    {
        return;
    }

    gmcp_map_reset();
}

/*
 * Function name: linkdead_save_vars_reset
 * Description  : May be called externally while the player is linkdead to keep
 *                his save-vars reset, that is to prevent them from being
 *                updated during a save or quit action.
 */
public void
linkdead_save_vars_reset()
{
    if (!interactive())
    {
	player_save_vars_reset();
	save_vars_reset();
    }
}

/*
 * Function name: create_living
 * Description  : Called to create the player. It initializes some variables.
 */
public nomask void
create_living()
{
    /* Must initialize the variable before loading because it can be queried
     * in ghosts. */
    m_bits = ([ ]);

    player_save_vars_reset();
}

/*
 * Function name: reset_living
 * Description  : We don't want people to mask this function.
 */
public nomask void
reset_living()
{
    return;
}

/*
 * Function name: command
 * Description  : Makes the player object execute a command, as if it was typed
 *                on the command line. For wizards, we have to test whether the
 *                euid of the caller allows to force the person.
 * Arguments    : string cmd - the command with arguments to perform. For players
 *                    this should always be prefixed with a "$".
 * Returns      : int - the amount of eval-cost ticks if the command was
 *                    successful, or 0 if unsuccessfull.
 */
public nomask int
command(string cmd)
{
    /* Test permissions if you try to force a wizard. */
    if (query_wiz_level() &&
        objectp(previous_object()))
    {
        if (!SECURITY->wiz_force_check(geteuid(previous_object()),
            geteuid()))
        {
            return 0;
        }
    }

    /* Automatically add the "$" if it isn't added already to the command. This
     * to prevent people from using the quicktyper to circumvent being forced
     * to do particular commands.
     */
    if (!wildmatch("$*", cmd) &&
        (previous_object() != this_object()))
    {
        cmd = "$" + cmd;
    }

    return ::command(cmd);
}

/*
 * Function name: id
 * Description  : Returns whether this object can be identified by a certain
 *                name. That isn't the case if the player hasn't met you
 *                while the real name is used.
 * Arguments    : string str - the name to test
 * Returns      : int 1/0 - true if the name is valid.
 */
public int
id(string str)
{
    if ((str == query_real_name()) &&
        notmet_me(this_player()))
    {
        return 0;
    }

    return ::id(str);
}

/*
 * Function name: parse_command_id_list
 * Description  : Mask of player_command_id_list() in /std/object.c to make sure
 *                that players cannot use the name of an NPC or player when that
 *                person hasn't been introduced to them.
 * Returns      : string * - the original parse_command_id_list() without the
 *                    lower case name of the person.
 */
public string *
parse_command_id_list()
{
    string *ids;

    ids = ::parse_command_id_list();

    if (sizeof(ids) &&
        notmet_me(this_player()))
    {
        ids -= ({ query_real_name() });
    }

    return ids;
}
