/*
 * /lib/guild_support.c
 *
 * Some support for meditating code.
 *
 * To use this code, take the following steps:
 *
 * - Inherit it into your room code.
 * - Place a call to the function init_guild_support() in your init() function.
 * - Place a call to the function gs_leave_inv(object ob, object to) in your
 *   leave_inv(object ob, object to)  function.
 * - You may redefine the gs_hook*() functions to create your own messages.
 */

#pragma save_binary
#pragma strict_types

#include <composite.h>
#include <files.h>
#include <macros.h>
#include <language.h>
#include <ss_types.h>
#include <state_desc.h>
#include <std.h>
#include <stdproperties.h>

/*
 * Prototypes.
 */
int gs_catch_all(string arg);

/*
 * Function name: create_guild_support
 * Description  : Obsolete function. Kept for backward compatibility so far.
 */
void
create_guild_support()
{
}

/*
 * Function name: get_prefs
 * Description  : When the player uses the "set" command, this function is used
 *                to parse the input.
 * Arguments    : string str - input text.
 */
void
get_prefs(string str)
{
    string *words;
    int    *prefs = allocate(SS_NO_EXP_STATS);
    int     index;
    int     primary;

    /* Start initialize variable pref. */
    index = -1;
    while(++index < SS_NO_EXP_STATS)
    {
        prefs[index] = GS_DEFAULT_FOCUS;
    }

    if (!strlen(str))
    {
        write("Please use \"<stat> [and] <stat>\", \"evenly\" or \"abort\".\n" +
            "Example: \"dexterity and wisdom\"\nPlease try again: ");
        input_to(get_prefs);
        return;
    }

    if (str == "abort")
    {
        write("Leaving your preferences unchanged.\n");
        return;
    }

    if (str == "evenly")
    {
        this_player()->set_learn_pref(prefs);
        write("Distributing your focus evenly over all stats.\n");
        return;
    }

    words = explode(lower_case(str), " ") - ({ "" });

    /* Is it the correct number of arguments? */
    switch (sizeof(words))
    {
    case 2:
        break;

    case 3:
        if (words[1] == "and")
        {
            words[1] = words[2];
            break;
        }
        /* Intentional fallthrough. */

    default:
        write("Use \"<stat> and <stat>\", \"evenly\" or \"abort\".\n" +
            "Please try again: ");
        input_to(get_prefs);
        return;
    }

    if ((index = member_array(words[0], SD_LONG_STAT_DESC)) == -1)
    {
        index = member_array(words[0], SD_STAT_DESC);
    }
    if ((index < 0) || (index >= SS_NO_EXP_STATS))
    {
        write("Unknown stat " + words[0] + ". Use either " +
            COMPOSITE_WORDS(SD_LONG_STAT_DESC[..(SS_NO_EXP_STATS-1)]) +
            " (or their known abbreviations).\nPlease try again: ");
        input_to(get_prefs);
        return;
    }
    prefs[index] = GS_PRIMARY_FOCUS;
    write("You put your primary focus on " + SD_LONG_STAT_DESC[index] + ".\n");
    primary = index;

    if ((index = member_array(words[1], SD_LONG_STAT_DESC)) == -1)
    {
        index = member_array(words[1], SD_STAT_DESC);
    }
    if ((index < 0) || (index >= SS_NO_EXP_STATS))
    {
        write("Unknown stat " + words[0] + ". Use either " +
            COMPOSITE_WORDS(SD_LONG_STAT_DESC[..(SS_NO_EXP_STATS-1)]) +
            " (or their known abbreviations).\nPlease try again: ");
        input_to(get_prefs);
        return;
    }
    if (index == primary)
    {
        write("Select two different stats to focus on.\nPlease try again: ");
        input_to(get_prefs);
        return;
    }
    prefs[index] = GS_SECONDARY_FOCUS;
    write("You put your secondary focus on " + SD_LONG_STAT_DESC[index] + ".\n");

    this_player()->set_learn_pref(prefs);
}

/*
 * Function name: set_prefs
 * Description  : When the player wants to change the learn preferences, this
 *                function prompts the player for the new values.
 * Returns      : int 1 - always.
 */
int
set_prefs()
{
    write("Deep in trance you can select to concentrate on improving your " +
        "different stats. Simply type \"abort\" to keep your preferences " +
        "unchanged or \"evenly\" to distribute your focus evenly over all " +
        "stats.\n");
    write("The syntax is \"<stat> [and] <stat>\" using the stats " +
        COMPOSITE_WORDS(SD_LONG_STAT_DESC[..(SS_NO_EXP_STATS-1)]) +
        " (or their known abbreviations).\nOn which stats do you wish " +
        "to focus? ");

    input_to(get_prefs);
    return 1;
}

/*
 * Function name: gs_leave_inv
 * Description  : Should be called if someone leaves the room. if that person
 *                was meditating, better do something. You should call this
 *                function from leave_inv(ob, to) in your room.
 * Arguments    : object ob - the object that is leaving.
 *                object to - the new destination of the object.
 */
void
gs_leave_inv(object ob, object to)
{
    if (ob->query_prop(LIVE_I_MEDITATES))
    {
        ob->remove_prop(LIVE_I_MEDITATES);
        ob->remove_prop(LIVE_S_EXTRA_SHORT);
    }
}

/*
 * Function name: list
 * Description  : With this command, players can get information on their
 *                guilds.
 * Arguments    : string str - the command line argument.
 * Returns      : int 1/0 - success/failure.
 */
nomask int
gs_list(string str)
{
    string long_name;
    string short_name;
    int    index = -1;
    int    total_tax = 0;
    string *not_found = SD_GUILD_FULL_NAMES;
    string *masters;

    /* Access failure. The player probably wanted a different list. */
    if (str != "guilds")
    {
        notify_fail("List what? Here you can only list your \"guilds\".\n");
        return 0;
    }

    /* Loop over all guilds to try and find information about them. */
    while(++index < SD_NUM_GUILDS)
    {
        if (!strlen(long_name = call_other(this_player(), "query_guild_name_" +
            SD_GUILD_EXTENSIONS[index])))
        {
            continue;
        }

        not_found -= ({ SD_GUILD_FULL_NAMES[index] });
        total_tax += this_player()->query_learn_pref(SS_NO_EXP_STATS + index);

        short_name = SECURITY->query_guild_short_name(long_name);
        if (!strlen(short_name))
        {
            write("Your " + SD_GUILD_FULL_NAMES[index] + " guild named \"" +
                long_name + "\" has not been registered! This is not good! " +
                "Please report this to the playerarch team and inform your " +
                "guildmaster so he/she can correct this.\n\n");
            short_name = "unknown";
        }

        write(capitalize(SD_GUILD_FULL_NAMES[index]) + " guild: " +
            long_name + " (short: " + capitalize(short_name) + ").\n");

        masters = SECURITY->query_guild_masters(short_name);
        if (!sizeof(masters))
        {
            write("No guildmasters are registered for the " +
                capitalize(short_name) + " guild. This is not good!\n");
        }
        else
        {
            write("Guildmaster(s): " +
                COMPOSITE_WORDS(map(masters, capitalize)) + ".\n");
        }
        write("\n");
    }

    switch(sizeof(not_found))
    {
    case 0:
        break;

    case SD_NUM_GUILDS:
        write("You are not a member of any guild, be it racial, layman or " +
            "occupational.\n");
        break;

    default:
        write("You are not a member of any " +
	    COMPOSITE_WORDS_WITH(not_found, "or") + " guild.\n");
        break;
    }

    /* See whether the total tax payed is the tax we expect for the guilds the
     * player is in.
     */
    while(--index >= 0)
    {
        total_tax -= this_player()->query_learn_pref(SS_NO_EXP_STATS + index);
    }

    if (total_tax)
    {
        write("The total tax you currently pay is not the amount you " +
            "should be paying. It seems that a guild you left did not " +
            "clean up after itself properly. Please contact the playerarch " +
            "team to have this corrected.\n");
    }

    return 1;
}

/*
 * Function name: gs_hook_already_meditate
 * Description  : This hook is called when player is already meditating and
 *                tries to mediate again. You can mask this function to give
 *                a different message.
 * Returns      : int 1 - always.
 */
int
gs_hook_already_meditate()
{
    write("You are already in deep trance. If you wish to finish your " +
        "meditation\nyou can do so by typing \"rise\".\n");
    return 1;
}

/*
 * Function name: gs_hook_start_meditate
 * Description  : This hook is called when player starts to meditate. You can
 *                mask this function to give a different message.
 */
void
gs_hook_start_meditate()
{
    write("Slowly you sit down on the soft carpet and close your eyes. A " +
        "feeling of great ease and self control falls upon you. You block " +
        "off your senses and concentrate solely upon your own mind. You " +
        "find yourself able to <set> your different preferences at you own " +
        "desire. Just <rise> when you are done meditating. You estimate " +
        "your stats and the progress you make at them.\n");
    say(QCTNAME(this_player()) + " sits down on the carpet and starts " +
        "to meditate.\n");
}

/*
 * Function name: gs_hook_rise
 * Description  : This hook is called when player rises from the meditation.
 *                You can mask this function to give a different message.
 */
void
gs_hook_rise()
{
    write("As if ascending from a great depth, you rise to the surface of\n" +
        "your consciousness. You exhale and feel very relaxed as you get\n" +
        "up and leave the carpet.\n");
    say(QCTNAME(this_player()) + " rises from the carpet.\n");
}

/*
 * Function name: gs_hook_catch_error
 * Description  : This hook is called when a player tried to do something strange
 *                while meditating like examining things or leave the room. You
 *                can mask this function to give a different message.
 * Arguments    : string str - Argument the player tried to send to his command.
 * Returns      : int 1 - normally.
 */
int
gs_hook_catch_error(string str)
{
    write("You cannot do that while meditating. " +
        "You may <rise> to end your trance.\n");
    return 1;
}

/*
 * Function name: gs_meditate
 * Description  : Player wants to meditate.
 * Arguments    : string str - the command line argument.
 * Returns      : int 1/0 - success/failure.
 */
int
gs_meditate(string str)
{
    string primary_desc;
    string secondary_desc;
    int    index;
    int    stat;
    int    level;
    int    residue;
    int    spread;
    int   *prefs;
    int   *sorted_prefs;
    int    primary;

    this_player()->add_prop(LIVE_S_EXTRA_SHORT, " is meditating");
    if (this_player()->query_prop(LIVE_I_MEDITATES))
    {
        return gs_hook_already_meditate();
    }

    this_player()->add_prop(LIVE_I_MEDITATES, 1);

    this_player()->reveal_me(1);

    gs_hook_start_meditate();
    write("Here you may also <restrict> yourself from playing.\n\n");

    index = -1;
    while(++index < SS_NO_EXP_STATS)
    {
        stat = this_player()->query_stat(index);
        if (stat >= SD_STATLEVEL_SUP)
        {
            write("You have supreme " + SD_LONG_STAT_DESC[index] + ".\n");
            continue;
        }
        if (stat >= SD_STATLEVEL_IMM)
        {
            write("You have the " + SD_LONG_STAT_DESC[index] +
                " of an immortal.\n");
            continue;
        }
        if (stat >= SD_STATLEVEL_EPIC)
        {
            write("You have reached epic " + SD_LONG_STAT_DESC[index] +
                ".\n");
            continue;
        }

        level = SD_NUM_STATLEVS;
        while(--level >= 0)
        {
            if (stat >= SD_STATLEVELS[level])
            {
                break;
            }
        }
        residue = stat - SD_STATLEVELS[level];
        level++;
        spread = ((level >= SD_NUM_STATLEVS) ? SD_STATLEVEL_EPIC :
            SD_STATLEVELS[level]) - SD_STATLEVELS[level-1];

        write("You are " + GET_NUM_DESC(residue, spread, SD_ADVANCE_DESCS) +
            " advancing to " + ((level == SD_NUM_STATLEVS) ?
            ("epic " + SD_LONG_STAT_DESC[index]) :
            GET_STAT_INDEX_DESC(index, level)) + ".\n");
    }

    prefs = this_player()->query_learn_pref(-1)[..(SS_NO_EXP_STATS-1)];
    sorted_prefs = sort_array( ({ }) + prefs);
    if (sorted_prefs[SS_NO_EXP_STATS-1] <= sorted_prefs[SS_NO_EXP_STATS-2] + 1)
    {
        write("\nYour focus is distributed evenly over your stats.\n");
    }
    else
    {
        index = member_array(sorted_prefs[SS_NO_EXP_STATS-1], prefs);
        write("\nYour primary focus is on " + SD_LONG_STAT_DESC[index] + ".\n");

        if (sorted_prefs[SS_NO_EXP_STATS-2] > sorted_prefs[SS_NO_EXP_STATS-3] + 1)
        {
            index = member_array(sorted_prefs[SS_NO_EXP_STATS-2], prefs);
            write("Your secondary focus is on " + SD_LONG_STAT_DESC[index] + ".\n");
        }
        write("The focus on your other stats is distributed evenly.\n");
    }

    add_action(gs_catch_all, "", 1);
    return 1;
}

/*
 * Function name: gs_rise
 * Description  : Player rises from meditation.
 * Returns      : int 1 - always.
 */
int
gs_rise()
{
    gs_hook_rise();
    this_player()->remove_prop(LIVE_I_MEDITATES);
    return 1;
}

/*
 * Function name: gs_restrict
 * Description  : This allows a player to restrict himself from playing.
 * Arguments    : string str - the command line argument.
 * Returns      : int 1 - always.
 */
static nomask int
gs_restrict(string str)
{
    int number;
    int seconds;
    string *words;

    if (str == "off")
    {
        if (this_player()->query_restricted())
        {
            this_player()->reset_restricted(1);
            write("You lift your self-imposed restriction.\n");
        }
        else
        {
            write("You had not imposed a restriction on yourself.\n");
        }

        return 1;
    }

    if (!strlen(str) ||
        !parse_command(str, ({ }),
             "'myself' 'for' %d [hour] [hours] [day] [days]", number))
    {
        write("Syntax: restrict off\n" +
            "        restrict myself for <number> hour/hours/day/days\n");
        return 1;
    }

    words = explode(str, " ");
    switch(words[sizeof(words) - 1])
    {
    case "hour":
    case "hours":
        if (number > 720)
        {
            write("You may not restrict yourself for more than 720 hours.\n");
            return 1;
        }
        str = "hour";
        seconds = (3600 * number);
        break;

    case "day":
    case "days":
        if (number > 30)
        {
            write("You may not restrict yourself for more than 30 days.\n");
            return 1;
        }
        str = "day";
        seconds = (86400 * number);
        break;

    default:
        write("Syntax: restrict myself for <number> hour/hours/day/days\n");
        return 1;
    }

    write("You have imposed a playing restriction on yourself for " + number +
        " " + str + ((number > 1) ? "s" : "") + ". This period starts now, " +
        "but will not come into effect until you log out. To lift this " +
        "restriction, meditate and type \"restrict off\" before you next " +
        "quit.\n\nNo new login is accepted from you until: " +
        ctime(time() + seconds) + "\n");
    this_player()->set_restricted(seconds, 1);

    return 1;
}

/*
 * Function name: gs_catch_all
 * Description  : Catch all commands the player makes while meditating.
 * Returns      : int 1/0 - success/failure.
 */
int
gs_catch_all(string arg)
{
    if (!this_player()->query_prop(LIVE_I_MEDITATES))
    {
        return 0;
    }

    switch(query_verb())
    {
    case "meditate":
        return gs_meditate("");

    case "set":
        set_prefs();
        return 1;

    case "rise":
        this_player()->remove_prop(LIVE_S_EXTRA_SHORT);
        gs_rise();
        return 1;

    case "restrict":
        gs_restrict(arg);
        return 1;

    case "help":
    case "stats":
    case "quit":
    case "save":
    case "drop": /* For those that quit from meditation */
    case "commune":
    case "reply":
    case "bug":
    case "typo":
    case "idea":
    case "praise":
    case "sysbug":
    case "systypo":
    case "syspraise":
    case "sysidea":
        return 0;

    default:
        return gs_hook_catch_error(arg);
    }
}

/*
 * Function name: init_guild_support
 * Description  : Add the meditate command to the player. You must call this
 *                function from init() in your room.
 */
void
init_guild_support()
{
    add_action(gs_list    , "list");
    add_action(gs_meditate, "meditate");
}
