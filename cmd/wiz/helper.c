/*
 * /cmd/wiz/helper.c
 *
 * This special soul contains some commands useful for wizards with a
 * helper function. The following commands are supported:
 *
 * - elog
 * - msecond
 * - pinfo
 */

#pragma no_clone
#pragma no_inherit
#pragma strict_types
#pragma save_binary

inherit "/cmd/std/command_driver";

#include <log.h>
#include <macros.h>
#include <std.h>
#include <composite.h>

public nomask mixed compile_dates(int back);
public nomask void find_log(mixed data);

#define ELOG_REPEAT_TIME	2.0
#define ELOG_NUM_REPEATS	50

#define ALLOWED_LIEGE_COMMANDS ({ "pinfo" })
#define PINFO_EDIT_DONE     "pinfo_edit_done"
#define PINFO_WRITE_DONE    "pinfo_write_done"

#define CHECK_ALLOWED       if (!valid_user()) return 0;

/*
 * Global variables.
 */
private static mapping pinfo_edit = ([ ]);
private static mapping elog_mess = ([]);

/*
 * Function name: create
 * Description  : Constructor. Called at creation.
 */
nomask void
create()
{
    string *lines;
    int    index;
    string *args;

    SECURITY->set_helper_soul_euid();
}

/*
 * Function name: get_soul_id
 * Description  : Return a proper name of the soul in order to get a nice
 *                printout. People can also use this name with the "allcmd"
 *                command.
 * Returns      : string - the name.
 */
nomask string
get_soul_id()
{
    return "helper";
}

/*
 * Function name: query_cmd_soul
 * Description  : Identify this as a command soul.
 * Returns      : int 1 - always.
 */
nomask int
query_cmd_soul()
{
    return 1;
}

/*
 * Function name: using_soul
 * Description  : Called when a wizard hooks onto the soul.
 * Arguments    : object wizard - the wizard hooking onto the soul.
 */
public void
using_soul(object wizard)
{
    SECURITY->set_helper_soul_euid();
}

/*
 * Function name: query_cmdlist
 * Description  : The list of verbs and functions. Please add new in
 *                alphabetical order.
 * Returns      : mapping - ([ "verb" : "function name" ])
 */
nomask mapping
query_cmdlist()
{
    return ([
	"elog":"elog",
	"msecond":"msecond",
        "pinfo" : "pinfo",
        ]);
}

/*
 * Function name: valid_user
 * Description  : Tests whether a particular wizard may use a the command.
 *                Arches++ can use all. The function operates on
 *                this_interactive() and query_verb().
 * Returns      : int 1/0 - allowed/disallowed. 
 */
nomask int
valid_user()
{
    string verb = query_verb();
    string name = this_interactive()->query_real_name();

    /* No name, or no verb, no show. */
    if (!strlen(verb) ||
	!strlen(name))
    {
	return 0;
    }

    switch(SECURITY->query_wiz_rank(name))
    {
    case WIZ_ARCH:
    case WIZ_KEEPER:
	/* Arches and keepers do all. */
	return 1;

    case WIZ_LORD:
	/* Lieges have some special commands. */
	if (member_array(verb, ALLOWED_LIEGE_COMMANDS) >= 0)
	{
	    /* Set the euid of the object right. */
	    return 1;
	}

	/* Intentionally no break; */

    default:
	/* Wizard may have been allowed on the commands. */
	if (SECURITY->query_team_member("aod", name) ||
	    SECURITY->query_team_member("aop", name))
	{
	    /* Set the euid of the object right. */
	    return 1;
	}
    }

    /* All others, no show. */
    return 0;
}

/* ***************************************************************************
 *  elog - Examine the login log
 */

/*
 * Function name: elog
 * Description  : Examine the login log
 * Arguments	: arg - arguments
 */
public nomask int
elog(string arg)
{
    string *args = ({ "" }), who = this_interactive()->query_real_name();
    string *loggers;
    int i, sz;
    mixed data;

    CHECK_ALLOWED;

    if (strlen(arg))
	args = explode(arg, " ");

    if (args[0] == "?")
    {
	notify_fail("Syntax: elog\n" +
		    "        elog break\n" +
		    "        elog status\n" +
		    "        elog <name> <# days>\n" +
		    "        elog address <site address> <# days>\n");
	return 0;
    }

    // Make sure the private dir exists
    if (file_size(SECURITY->query_wiz_path(who) + "/private") == -1)
	mkdir(SECURITY->query_wiz_path(who) + "/private");

    switch (args[0])
    {
	// Break the active search
    case "break":
	if (!sizeof(elog_mess[who]))
	{
	    notify_fail("You have no active search.\n");
	    return 0;
	}
	elog_mess[who] += ({ "break" });
	break;

	// Give info on the active search
    case "status":
	if (!sizeof(elog_mess[who]))
	{
	    notify_fail("You have no active search.\n");
	    return 0;
	}
	elog_mess[who] += ({ "status" });
	break;

	// List all active searches
    case "":
	loggers = m_indices(elog_mess);
	if ((sz = sizeof(loggers)))
		write("Active searches from " +  COMPOSITE_WORDS(map(loggers, capitalize)) + ".\n");
	else
	    write("No active searches.\n");
	break;

	// Do an address search
    case "address":
	if (sizeof(args) != 3)
	{
	    elog("?");
	    return 0;
	}
	if (atoi(args[2]) > 30 || atoi(args[2]) < 1)
	{
	    notify_fail("You can only search 1 - 30 days back.\n");
	    break;
	}
	data = compile_dates(atoi(args[2]));
	elog_mess[who] = ({ set_alarm(ELOG_REPEAT_TIME, 0.0, &find_log(({ who, 0, args[1] }) + ({ data }) + ({ 0 }))) });
	write_file(SECURITY->query_wiz_path(who) + "/private/ENTER_SEARCH", "Search started " + ctime(time()) + "\n");
	break;

	// Do a name search
    default:
	if (sizeof(args) != 2)
	{
	    elog("?");
	    return 0;
	}
	args[0] = lower_case(args[0]);
	if (!SECURITY->exist_player(args[0]))
	{
	    notify_fail("The player " + capitalize(args[0]) + " doesn't exist.\n");
	    return 0;
	}
	if (atoi(args[1]) > 30 || atoi(args[1]) < 1)
	{
	    notify_fail("You can only search 1 - 30 days back.\n");
	    break;
	}
	data = compile_dates(atoi(args[1]));
	elog_mess[who] = ({ set_alarm(ELOG_REPEAT_TIME, 0.0, &find_log(({ who, 1, capitalize(args[0]) }) + ({ data }) + ({ 0 }))) });
	write_file(SECURITY->query_wiz_path(who) + "/private/ENTER_SEARCH", "Search started " + ctime(time()) + "\n");
	break;
    }
	
    write("Ok.\n");
    return 1;
}

/*
 * Function name: compile_dates
 * Description  : Compile a list of dates going backwards in time
 * Arguments	: back - where to start
 * Returns	: A list of decreasing dates.
 */
public nomask mixed
compile_dates(int back)
{
    int date;
    mixed rval = ({});

    date = atoi(ctime(time())[8..10]);

    while (back--)
    {
	rval += ({ date-- });
	if (date == 0)
	    date = 31;
    }

    return rval;
}

/*
 * Function name: find_log
 * Description  : The actual routine grepping the logs
 * Arguments	: data - a list of search data.
 */
public nomask void
find_log(mixed data)
{
    string ldata;
    mixed args = ({ data[0], data[1], data[2] });
    int *mdate = data[3];
    int mline = data[4];
    string fdate, fwho, fwhat, fsite, *frest;
    int i;
    object pl = find_player(args[0]);

    // Handle any messages
    if (sizeof(elog_mess[args[0]]) > 1)
    {
	switch(elog_mess[args[0]][1])
	{
	case "break":
	    write_file(SECURITY->query_wiz_path(args[0]) + "/private/ENTER_SEARCH", "Search broken " + ctime(time()) + "\n");
	    if (objectp(pl))
		pl->catch_msg("Search broken.\n");
	    elog_mess = m_delete(elog_mess, args[0]);
	    break;

	case "status":
	    if (objectp(pl))
		pl->catch_msg("Status: File date " + mdate[0] + ", Line " + mline + ".\n");
	    elog_mess[args[0]] = ({ elog_mess[args[0]][0] });
	    break;

	default:
	    break;
	}
    }

    for (i = mline ; i < mline + ELOG_NUM_REPEATS ; i++)
    {
	ldata = read_file("/syslog/log/enter/ENTER." + mdate[0], i, 1);
	if (!strlen(ldata))
	    break;

	fdate = ldata[0..23];
	fwho = explode(ldata[25..35], " ")[0];
	frest = (explode(ldata[38..], " ") - ({""})) + ({" "});
	fwhat = frest[0];
	fsite = frest[1][0..(strlen(frest[1]) - 2)];

	if (fwhat == "login")
	{
	    if ((args[1] == 0 && wildmatch(args[2], fsite)) ||
		(args[1] == 1 && fwho == args[2]))
		write_file(SECURITY->query_wiz_path(args[0]) + "/private/ENTER_SEARCH", sprintf("%-10s : ", fwho) + fdate + " : " + fsite + "\n");
	}
    }

    if (!stringp(ldata))
    {
	mdate = mdate[1..];
	mline = 0;
    }
    else
	mline += ELOG_NUM_REPEATS;

    if (sizeof(mdate))
	elog_mess[args[0]] = ({ set_alarm(ELOG_REPEAT_TIME, 0.0, &find_log(args + ({ mdate, mline }))) });
    else
    {
	elog_mess = m_delete(elog_mess, args[0]);
	write_file(SECURITY->query_wiz_path(args[0]) + "/private/ENTER_SEARCH", "Search finished " + ctime(time()) + "\n");
    }
}


/* ***************************************************************************
 *  msecond - Modify seconds entry in mortals
 */

/*
 * Function name: patch_second
 * Description  : edit a stored player second entry
 * Arguments	: what - 1 - add, 0 - remove
 *		  who - whom to edit
 *		  name - the name to add/remove
 */
public void
patch_second(int what, string who, string name)
{
    string file = PLAYER_FILE(who);
    mapping plmap;
    string query = this_interactive()->query_real_name();
    
    if (!SECURITY->exist_player(who))
    {
        return;
    }

    if (SECURITY->query_wiz_rank(query) < WIZ_ARCH)
    {
        write("This command is not available for non-arches when the " +
            "player is not present in the realms.\n");
        return;
    }

    plmap = restore_map(file);

    if (!m_sizeof(plmap["m_seconds"]))
    {
        plmap["m_seconds"] = ([]);
    }
    if (what == 1)
    {
	if (sizeof(plmap["m_seconds"][name]) == 0)
	{
	    plmap["m_seconds"][name] = ({ query, time() });
	}
    }
    else
    {
	plmap["m_seconds"] = m_delete(plmap["m_seconds"], name);
    }
#ifdef LOG_SECONDS
    SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " + capitalize(query) + (what == 1 ? " added " : " removed ") + capitalize(name) + (what == 1 ? " to " : " from ") + capitalize(who) + ".\n"));
#endif LOG_SECONDS

    save_map(plmap, file);
}

/*
 * Function name: msecond
 * Description  : modify seconds entry in mortal
 */
public int
msecond(string str)
{
    string *slist, *args = ({});
    int i, sz;
    mixed info;
    object plob;

    CHECK_ALLOWED;

    if (stringp(str))
    {
	args = explode(lower_case(str), " ");
    }

    if (!sizeof(args) || sizeof(args) == 0)
    {
	notify_fail("Syntax: msecond <mortal>\n" +
		    "        msecond a[dd] <name> to <mortal>\n" +
		    "        msecond r[emove] <name> from <mortal>\n");
	return 0;
    }

    switch (args[0])
    {
    case "a":
    case "add":
	if (sizeof(args) != 4 || args[2] != "to")
	{
	    msecond("");
	    return 0;
	}
	if (!SECURITY->exist_player(args[1]))
	{
	    notify_fail("The player " + capitalize(args[1]) + " does not exist.\n");
	    return 0;
	}
	if (!SECURITY->exist_player(args[3]))
	{
	    notify_fail("The player " + capitalize(args[3]) + " does not exist.\n");
	    return 0;
	}

	// Do the actual adding

	// Make a list of all seconds who need to be notified/added
	if (!objectp(plob = find_player(args[3])))
	    plob = SECURITY->finger_player(args[3]);
	slist = ({ args[3] }) + plob->query_seconds();
	if (plob->query_finger_player())
	{
	    plob->remove_object();
	}

	// First add as per the command, including all other seconds
	for (i = 0, sz = sizeof(slist) ; i < sz ; i++)
	{
	    if (args[1] == slist[i])
		continue;
	    if (objectp(plob = find_player(slist[i])))
		plob->add_second(args[1]);
	    else
		patch_second(1, slist[i], args[1]);
	}

	// Then add the reverse (all seconds to the first name)
	plob = find_player(args[1]);
	for (i = 0, sz = sizeof(slist) ; i < sz ; i++)
	{
	    if (args[1] == slist[i])
		continue;
	    if (objectp(plob))
		plob->add_second(slist[i]);
	    else
		patch_second(1, args[1], slist[i]);
	}
	
	write("Added second " + capitalize(args[1]) + " to " + COMPOSITE_WORDS(map(slist, capitalize)) + ".\n");
	break;

    case "r":
    case "remove":
	if (sizeof(args) != 4 || args[2] != "from")
	{
	    msecond("");
	    return 0;
	}
	if (!SECURITY->exist_player(args[3]))
	{
	    notify_fail("The player " + capitalize(args[3]) + " does not exist.\n");
	    return 0;
	}

	// Make a list of all seconds who need to be notified/removed
	if (!objectp(plob = find_player(args[3])))
	{
	    plob = SECURITY->finger_player(args[3]);
	}
	slist = ({ args[3] }) + plob->query_seconds();
	if (plob->query_finger_player())
	{
	    plob->remove_object();
	}

	// First remove as per the command, including all other seconds
	for (i = 0, sz = sizeof(slist) ; i < sz ; i++)
	{
	    if (objectp(plob = find_player(slist[i])))
		plob->remove_second(args[1]);
	    else
		patch_second(0, slist[i], args[1]);
	}

	// Then remove the reverse (all seconds from the first name)
	plob = find_player(args[1]);
	for (i = 0, sz = sizeof(slist) ; i < sz ; i++)
	{
	    if (objectp(plob))
		plob->remove_second(slist[i]);
	    else
		patch_second(0, args[1], slist[i]);
	}
	
	write("Removed second " + capitalize(args[1]) + " from " + COMPOSITE_WORDS(map(slist, capitalize)) + ".\n");
	break;

    default:
	if (sizeof(args) != 1)
	{
	    msecond("");
	    return 0;
	}
	if (!SECURITY->exist_player(args[0]))
	{
	    notify_fail("The player " + capitalize(args[0]) + " does not exist.\n");
	    return 0;
	}
	if (!objectp(plob = find_player(args[0])))
	{
	    plob = SECURITY->finger_player(args[0]);
	}
	str = args[0];
	args = sort_array(plob->query_seconds());

	if (!sizeof(args))
	{
	    write(sprintf("%-10s: ", capitalize(str)) + "No seconds\n");
	}
	else
	{
	    write(sprintf("%-10s: ", capitalize(str)));
	    for (i = 0, sz = sizeof(args) ; i < sz ; i++)
	    {
		if (i > 0)
		    write("          : ");
		info = plob->query_second_info(args[i]);
		write(sprintf("%-10s added %s by %s\n", capitalize(args[i]), ctime(info[1]), capitalize(info[0])));
	    }
	}
	if (plob->query_finger_player())
	{
	    plob->remove_object();
	}
	break;
    }
    return 1;
}

/* ***************************************************************************
 *  pinfo - Edit/view the information file on a player.
 */

/*
 * Function name: pinfo_write_done
 * Description  : Called from the editor when the wizard is done writing
 *                the text for the file on the player.
 * Arguments    : string text - the text to add to the file.
 */
public void
pinfo_write_done(string text)
{
    string wname = this_player()->query_real_name();

    if (MASTER_OB(previous_object()) != EDITOR_OBJECT)
    {
	write("Illegal call to pinfo_edit_done().\n");
	return;
    }

    if (!stringp(pinfo_edit[wname]))
    {
	write("No pinfo_edit information. Impossible! Please report!\n");
	return;
    }

    if (!strlen(text))
    {
        write("Pinfo aborted.\n");
        return;
    }

    /* Make sure we have the proper euid. */
    SECURITY->set_helper_soul_euid();

    write_file(pinfo_edit[wname], ctime(time()) + " " + capitalize(wname) +
	       " (" + capitalize(WIZ_RANK_NAME(SECURITY->query_wiz_rank(wname))) +
	       "):\n" + text + "\n");
    pinfo_edit = m_delete(pinfo_edit, wname);
    write("Information saved.\n");
}

/*
 * Function name: pinfo_edit_done
 * Description  : Called from the editor when the wizard is done editing
 *                the text for the file on the player.
 * Arguments    : string text - the text to add to the file.
 */
public void
pinfo_edit_done(string text)
{
    string wname = this_player()->query_real_name();

    if (MASTER_OB(previous_object()) != EDITOR_OBJECT)
    {
	write("Illegal call to pinfo_edit_done().\n");
	return;
    }

    if (!stringp(pinfo_edit[wname]))
    {
	write("No pinfo_edit information. Impossible! Please report!\n");
	return;
    }

    if (!strlen(text))
    {
        write("Pinfo aborted.\n");
        return;
    }

    /* Make sure we have the proper euid. */
    SECURITY->set_helper_soul_euid();

    rm(pinfo_edit[wname]);
    write_file(pinfo_edit[wname], text + "\n" + ctime(time()) + " " +
	       capitalize(wname) + " (" +
	       capitalize(WIZ_RANK_NAME(SECURITY->query_wiz_rank(wname))) +
	       "):\nRe-edited the previous text.\n\n");
    pinfo_edit = m_delete(pinfo_edit, wname);
    write("Information saved.\n");
}

nomask int
pinfo(string str)
{
    string *args;
    string name;
    string wname = this_player()->query_real_name();
    int    rank = SECURITY->query_wiz_rank(wname);
    string cmd;
    string text;
    string file;
    object editor;

    CHECK_ALLOWED;

    if (!strlen(str))
    {
	notify_fail("Syntax: pinfo [r / t / w / d / e] <name> [<text>]\n");
	return 0;
    }

    args = explode(str, " ");
    args = ((sizeof(args) == 1) ? ( ({ "r" }) + args) : args);

    cmd = args[0];
    name = lower_case(args[1]);
    if (sizeof(args) > 2)
    {
	text = implode(args[2..], " ");
    }

    /*
     * Access check. The following applies:
     *
     * - arches/keepers can do all.
     * - lieges can only access information on their subject wizards.
     * - people allowed for the command can handle mortal players.
     */
    switch(rank)
    {
    case WIZ_ARCH:
    case WIZ_KEEPER:
        /* They can do all. */
	break;

    case WIZ_LORD:
        /* Can handle their subject wizards. */
	if ((SECURITY->query_wiz_dom(wname) ==
	     SECURITY->query_wiz_dom(name)) &&
	    (SECURITY->query_wiz_rank(name) < rank))
	{
	    break;
	}
        /* Can handle apprentices and retired wizards. */
        if (SECURITY->query_wiz_rank(name) < WIZ_NORMAL)
        {
            break;
        }

	/* Intentionally no break. Could be an allowed user. */

    default:
        /* May not handle wizards here. */
	if (SECURITY->query_wiz_rank(name))
	{
	    write("You may not handle the file on " + capitalize(name) +
		  " as that player is a wizard.\n");
	    return 1;
	}
    }

    /* Make sure we have the proper euid. */
    SECURITY->set_helper_soul_euid();

    file = PINFO_FILE(name);

    switch(cmd)
    {
    case "d":
	if (rank < WIZ_ARCH)
	{
	    notify_fail("Only an arch++ can delete pinfo.\n");
	    return 0;
	}

	if (file_size(file) == -1)
	{
	    write("There is no pinfo on " + capitalize(name) + ".\n");
	    return 1;
	}

	rm(file);
	write("Removed pinfo on " + capitalize(name) + ".\n");
	return 1;

    case "e":
	if (rank < WIZ_ARCH)
	{
	    notify_fail("Only an arch++ can edit pinfo.\n");
	    return 0;
	}

	if (file_size(file) == -1)
	{
	    write("There is no pinfo on " + capitalize(name) + ".\n");
	    return 1;
	}

	pinfo_edit[wname] = file;
	text = read_file(file);
	clone_object(EDITOR_OBJECT)->edit(PINFO_EDIT_DONE, text,
					  sizeof(explode(text, "\n")));
	return 1;

    case "m":
    case "r":
	if (file_size(file) == -1)
	{
	    write("There is no pinfo on " + capitalize(name) + ".\n");
	    return 1;
	}

	text = read_file(file);

	/* Automatically invoke more, or wizards request. */
	if ((cmd == "m") ||
	    (sizeof(explode(text, "\n")) > 100))
	{
	    this_player()->more(text);
	}
	else
	{
	    write(text);
	}

	return 1;

    case "t":
	if (file_size(file) == -1)
	{
	    write("There is no pinfo on " + capitalize(name) + ".\n");
	    return 1;
	}

	tail(file);
	return 1;

    case "w":
	if (file_size(file) == -1)
	{
	    write("Writing pinfo file on " + capitalize(name) + ".\n");
	}
	else
	{
	    write("Appending pinfo file on " + capitalize(name) + ".\n");
	}

	if (strlen(text))
	{
	    if (strlen(text) > 75)
	    {
		text = break_string(text, 75);
	    }

	    write_file(file, ctime(time()) + " " + capitalize(wname) + " (" +
		       capitalize(WIZ_RANK_NAME(SECURITY->query_wiz_rank(wname))) +
		       "):\n" + text + "\n\n");
	}
	else
	{
	    pinfo_edit[wname] = file;
	    clone_object(EDITOR_OBJECT)->edit(PINFO_WRITE_DONE);
	}

	return 1;

    default:
	notify_fail("Syntax: pinfo [r / t / w / d / e] <name> [<text>]\n");
	return 0;
    }

    write("Impossible end of pinfo(). Please report.\n");
    return 1;
}
