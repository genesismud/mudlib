/*
 * /cmd/wiz/lord.c
 *
 * This object holds the lord commands. Some commands may also be performed
 * by stewards. The following commands are supported:
 *
 * - accept   [stewards too]
 * - demote   [stewards too]
 * - deny     [stewards too]
 * - expel    [stewards too]
 * - liege
 * - liegee
 * - limit
 * - mentor   [stewards too]
 * - promote  [stewards too]
 * - short
 */

#pragma no_clone
#pragma no_inherit
#pragma save_binary
#pragma strict_types

inherit "/cmd/std/command_driver";

#include <flags.h>
#include <log.h>
#include <macros.h>
#include <std.h>
#include <stdproperties.h>
#include <composite.h>

#define CHECK_SO_LORD 	 if (WIZ_CHECK < WIZ_LORD) return 0; \
			 if (this_interactive() != this_player()) return 0
#define CHECK_SO_STEWARD if (WIZ_CHECK < WIZ_STEWARD) return 0; \
			 if (this_interactive() != this_player()) return 0

/* **************************************************************************
 * Return a list of which souls are to be loaded.
 * The souls are listed in order of command search.
 */
nomask string *
get_soul_list()
{
    return ({ WIZ_CMD_LORD,
	      WIZ_CMD_NORMAL,
              WIZ_CMD_HELPER,
	      WIZ_CMD_APPRENTICE,
	      MBS_SOUL });
}

/* **************************************************************************
 * Return a proper name of the soul in order to get a nice printout.
 */
nomask string
get_soul_id()
{
    return WIZNAME_LORD;
}

/* **************************************************************************
 * The list of verbs and functions. Please add new in alfabetical order.
 */
nomask mapping
query_cmdlist()
{
    return ([
	     "accept":"accept",

	     "demote":"demote",
	     "deny":"deny",

	     "expel":"expel",

	     "liege":"liege",
	     "liegee":"liege",
	     "limit":"limit",

	     "mentor":"mentor_fun",

	     "promote":"promote",

	     "short":"short",
	     "startloc":"startloc",

	     ]);
}

/* **************************************************************************
 * Here follows the actual functions. Please add new functions in the 
 * same order as in the function name list.
 * **************************************************************************/

/* **************************************************************************
 * accept - accept someone into a domain.
 */
nomask int
accept(string name)
{
    CHECK_SO_STEWARD;

    return SECURITY->accept_application(name);
}

/* **************************************************************************
 * demote - demote a player to a lower level.
 */
nomask int
demote(string str)
{
    string name;
    int    level;

    CHECK_SO_STEWARD;

    if (!stringp(str))
    {
#ifdef USE_WIZ_LEVELS
	notify_fail("Syntax: demote <name> <rank> / <level>\n");
#else
        notify_fail("Syntax: demote <name> <rank>\n");
#endif USE_WIZ_LEVELS
	return 0;
    }

    /* Demotion to a new level with the same rank. */
    if (sscanf(str, "%s %d", name, level) == 2)
    {
#ifdef USE_WIZ_LEVELS
	name = lower_case(name);
	if (level >= SECURITY->query_wiz_level(name))
	{
	    notify_fail("Demotions should be used to lower the level.\n");
	    return 0;
	}

	return SECURITY->wizard_change_level(name, level);
#else
        notify_fail("Wizard levels are not supported right now.\n");
        return 0;
#endif USE_WIZ_LEVELS
    }

    /* Demotion to a new rank. */
    if (sscanf(str, "%s %s", name, str) != 2)
    {
	notify_fail("Syntax: demote <name> <rank> / <level>\n");
	return 0;
    }

    if ((level = member_array(str, WIZ_N)) == -1)
    {
	if ((level = member_array(str, WIZ_S)) == -1)
	{
	    notify_fail("There is no rank called '" + str + "'.\n");
	    return 0;
	}
    }

    name = lower_case(name);
    level = WIZ_R[level];

    if (level >= SECURITY->query_wiz_rank(name))
    {
	notify_fail("Demotions should be used to lower the rank.\n");
	return 0;
    }

    return SECURITY->wizard_change_rank(name, level);
}

/* **************************************************************************
 * deny - deny the request from an apprentice.
 */
nomask int
deny(string name)
{
    CHECK_SO_STEWARD;

    return SECURITY->deny_application(name);
}

/* **************************************************************************
 * expel - expel a wizard from a domain.
 */
nomask int
expel(string wizname)
{
    CHECK_SO_STEWARD;

    /* We have to make sure the steward does not expel his Lord. */
    if ((SECURITY->query_wiz_rank(wizname) == WIZ_LORD) &&
	(WIZ_CHECK == WIZ_STEWARD))
    {
	notify_fail("You cannot expel your Lord from the domain.\n");
	return 0;
    }

    return SECURITY->expel_wizard_from_domain(wizname);
}

/* **************************************************************************
 * liege  - send a message on the liege-line.
 * liegee - emote a message on the liege-line.
 */
nomask int
liege(string str)
{
    int busy;

    CHECK_SO_LORD;

    if (!stringp(str))
    {
        notify_fail(capitalize(query_verb()) + " what?\n");
        return 0;
    }

    busy = this_interactive()->query_prop(WIZARD_I_BUSY_LEVEL);
    if (busy & BUSY_F)
    {
	write("WARNING: You are currently 'busy F'.\n");
    }
    else if (busy & BUSY_L)
    {
	write("WARNING: Your 'busy L' state is automatically switched off.\n");
	this_interactive()->add_prop(WIZARD_I_BUSY_LEVEL, (busy ^ BUSY_L));
    }

    return WIZ_CMD_APPRENTICE->line((WIZNAME_LORD + " " + str),
	(query_verb() == "liegee"), BUSY_L);
}

/* **************************************************************************
 * limit - limit the number of players in a domain.
 */
nomask int
limit(string str)
{
    string dname;
    string name;
    int    max;

    CHECK_SO_LORD;

    name = this_interactive()->query_real_name();
    dname = SECURITY->query_wiz_dom(name);

    /* No argument, default to the wizards own domain. */
    if (!stringp(str))
    {
	if (!strlen(dname))
	{
	    notify_fail("You are not in any domain. Strange!\n");
	    return 0;
	}

	str = dname;
    }

    /* Argument only a domain name: print the current limit for that domain. */
    if ((max = SECURITY->query_domain_max(str)) > 0)
    {
	write("Maximum number of members in " + capitalize(str) + " is " +
	    max + ".\n");
	return 1;
    }

    /* Lords may only set their own domain. */
    if (SECURITY->query_wiz_rank(name) <= WIZ_LORD)
    {
	if (sscanf(str, "%d", max) != 1)
	{
	    notify_fail("Syntax: limit <maximum>\n");
	    return 0;
	}

	if ((max > SECURITY->query_domain_max(dname)) &&
	    (max > SECURITY->query_default_domain_max()))
	{
	    notify_fail("You may not raise the limit beyond the default " +
		"maximum (" + SECURITY->query_default_domain_max() + ").\n");
	    return 0;
	}
    }
    else
    {
	if (sscanf(str, "%s %d", dname, max) != 2)
	{
	    notify_fail("Syntax: limit <domain> <maximum>\n");
	    return 0;
	}
    
	dname = capitalize(dname);
	if (SECURITY->query_dom_num(dname) == -1)
	{
	    notify_fail("No domain " + dname + ".\n");
	    return 0;
	}
    }

    if (sizeof(SECURITY->query_domain_members(dname)) > max)
    {
	notify_fail("Cannot set the maximum to " + max + " as there are " +
	    "already " + sizeof(SECURITY->query_domain_members(dname)) +
	    " wizards in " + str + ".\n");
	return 0;
    }

    if (SECURITY->set_domain_max(dname, max))
    {
	write("Maximum number of wizards for " + dname + " set to " + max +
	    ".\n");
    }
    else
    {
	write("Failed to set maximum number of wizards for " + dname +
	    " to " + max + ". This is impossible. Please report this.\n");
    }

    return 1;
}

/* **************************************************************************
 * mentor - handle the mentor settings in a wizard
 */
nomask int
mentor_fun(string arg)
{
    string *args, *wlist;
    string student, mentor;
    int i, sz, err = 0;
    object pl;

    CHECK_SO_STEWARD;

    if (arg == "?")
    {
	notify_fail("Syntax: mentor\n" +
		    "        mentor assign <student> to <mentor>\n" +
		    "        mentor graduate <student> from <mentor>\n" +
		    "        mentor clear <student> from <mentor>\n");
	return 0;
    }

    if (!strlen(arg))
    {
	wlist = sort_array(filter(SECURITY->query_wiz_list(-1), 
				  &operator(!=)(0) @ sizeof @ SECURITY->query_students));
	for (i = 0, sz = sizeof(wlist) ; i < sz ; i++)
	    write(sprintf("%-11s%-11s- ", capitalize(wlist[i]), SECURITY->query_wiz_dom(wlist[i])) + COMPOSITE_WORDS(map(sort_array(SECURITY->query_students(wlist[i])), capitalize)) + ".\n");

	return 1;
    }
    else
    {
	if (sscanf(arg, "assign %s to %s", student, mentor) == 2)
	{
	    if (SECURITY->query_wiz_rank(student) < WIZ_NORMAL)
	    {
		notify_fail("mentor: The student must at least be a full wizard.\n");
		return 0;
	    }

	    if (SECURITY->query_wiz_rank(mentor) < WIZ_NORMAL)
	    {
		notify_fail("mentor: The mentor must at least be a full wizard.\n");
		return 0;
	    }

	    if ((WIZ_CHECK < WIZ_ARCH)
		&& ((SECURITY->query_wiz_dom(student) !=
		     SECURITY->query_wiz_dom(mentor)) &&
		    SECURITY->query_wiz_rank(mentor) < WIZ_ARCH))
	    {
		notify_fail("mentor: The mentor and student must be in the same domain.\n");
		return 0;
	    }

	    if (strlen(SECURITY->query_mentor(student)) > 0)
	    {
		notify_fail("mentor: The student already has an assigned mentor.\n");
		return 0;
	    }

	    if (strlen(SECURITY->query_mentor(mentor)) >  0)
	    {
		notify_fail("mentor: The intended mentor actually is a student.\n");
		return 0;
	    }

	    if (member_array(student, SECURITY->query_students(mentor)) >= 0)
	    {
		notify_fail("mentor: The student already is assigned to the mentor.\n");
		return 0;
	    }

	    if (SECURITY->add_student(mentor, student) == 0)
	    {
		notify_fail("mentor: Assigning the student to the mentor failed.\n");
		return 0;
	    }
	    if (objectp(pl = find_player(student)))
            {
		tell_object(pl, "You have been assigned as student to " + capitalize(mentor) + ".\n");
            }
	    if (SECURITY->set_mentor(mentor, student) == 0)
	    {
		notify_fail("mentor: Assigning the mentor to the student failed.\n");
		return 0;
	    }
	    if (objectp(pl = find_player(mentor)))
            {
		tell_object(pl, "You have been assigned as mentor to " + capitalize(student) + ".\n");
            }
	}
	else if (sscanf(arg, "graduate %s from %s", student, mentor) == 2)
	{
	    if (SECURITY->set_mentor("none", student) == 0)
	    {
		notify_fail("mentor: Removing the mentor from the student failed.\n");
		err = 1;
	    }
	    if (objectp(pl = find_player(student)))
            {
		tell_object(pl, "You have been graduated from " + capitalize(mentor) + ".\n");
            }
	    if (SECURITY->remove_student(mentor, student) == 0)
	    {
		notify_fail("mentor: Removing the student from the mentor failed.\n");
		err = 1;
	    }
	    if (objectp(pl = find_player(mentor)))
            {
		tell_object(pl, "Your student " + capitalize(student) + " has graduated.\n");
            }

	    if (err)
		return 0;
	}
	else if (sscanf(arg, "clear %s from %s", student, mentor) == 2)
	{
	    if (SECURITY->set_mentor("none", student) == 0)
	    {
		notify_fail("mentor: Removing the mentor from the student failed.\n");
		err = 1;
	    }
	    if (objectp(pl = find_player(student)))
            {
		tell_object(pl, "The mentor entry for \"" + capitalize(mentor) + "\" has been stricken.\n");
            }
	    if (SECURITY->remove_student(mentor, student) == 0)
	    {
		notify_fail("mentor: Removing the student from the mentor failed.\n");
		err = 1;
	    }
	    if (objectp(pl = find_player(mentor)))
            {
		tell_object(pl, "The student entry for \"" + capitalize(student) + "\" has been stricken.\n");
            }

	    if (err)
		return 0;
	}
	else
	{
	    write("mentor: Syntax error.\n\n");
	    mentor_fun("?");
	    return 0;
	}
    }

    write("Ok.\n");
    return 1;
}

/* **************************************************************************
 * promote - promote a wizard to a higher level.
 */
nomask int
promote(string str)
{
    string name;
    int    level;

    CHECK_SO_STEWARD;

    if (!strlen(str))
    {
#ifdef USE_WIZ_LEVELS
	notify_fail("Syntax: promote <name> <rank> / <level>\n");
#else
        notify_fail("Syntax: promote <name> <rank>\n");
#endif USE_WIZ_LEVELS
	return 0;
    }

    /* Promotion to a new level with the same rank. */
    if (sscanf(str, "%s %d", name, level) == 2)
    {
#ifdef USE_WIZ_LEVELS
	name = lower_case(name);
	if (level <= SECURITY->query_wiz_level(name))
	{
	    notify_fail("Promotions should be used to raise the level.\n");
	    return 0;
	}

	return SECURITY->wizard_change_level(name, level);
#else
        notify_fail("Wizard levels are not supported right now.\n");
        return 0;
#endif USE_WIZ_LEVELS
    }

    /* Promotion to a new rank. */
    if (sscanf(str, "%s %s", name, str) != 2)
    {
	notify_fail("Syntax: promote <name> <rank> / <level>\n");
	return 0;
    }

    if ((level = member_array(str, WIZ_N)) == -1)
    {
	if ((level = member_array(str, WIZ_S)) == -1)
	{
	    notify_fail("There is no rank called '" + str + "'.\n");
	    return 0;
	}
    }

    name = lower_case(name);
    level = WIZ_R[level];

    if (level <= SECURITY->query_wiz_rank(name))
    {
	notify_fail("Promotions should be used to raise the rank.\n");
	return 0;
    }

    return SECURITY->wizard_change_rank(name, level);
}

/* **************************************************************************
 * short - set the short name of a domain.
 */
nomask int
short(string str)
{
    string dname;
    string name;
    string sname;
    int    size;

    CHECK_SO_LORD;

    name = this_interactive()->query_real_name();
    dname = SECURITY->query_wiz_dom(name);

    if (!stringp(str))
    {
	if (!strlen(dname))
	{
	    notify_fail("You are not in any domain. Strange!\n");
	    return 0;
	}

	str = dname;
    }

    if (strlen(sname = SECURITY->query_domain_short(str)))
    {
	write("Short name of " + dname + " is " + sname + ".\n");
	return 1;
    }

    /* Lords may only set their own domain. */
    if (SECURITY->query_wiz_rank(name) == WIZ_LORD)
    {
	sname = str;
    }
    else
    {
	if (sscanf(str, "%s %s", dname, sname) != 2)
	{
	    notify_fail("Syntax: short <domain> <short>\n");
	    return 0;
	}

	dname = capitalize(dname);
	if (SECURITY->query_dom_num(dname) == -1)
	{
	    notify_fail("No domain " + dname + ".\n");
	    return 0;
	}
    }

    sname = lower_case(sname);
    if (strlen(sname) != 3)
    {
	notify_fail("The short name must be three letters long.\n");
	return 0;
    }

    /* Don't set the name if that name is already in use. */
    if (sizeof(filter(SECURITY->query_domain_list(),
	&operator(==)(sname) @ SECURITY->query_domain_short)))
    {
	notify_fail("The domain short name " + sname + " is already used.\n");
	return 0;
    }

    if (SECURITY->set_domain_short(dname, sname))
    {
	write("Short name for " + dname + " set to " + sname + ".\n");
    }
    else
    {
	write("Failed to set short name for " + dname + " to " + sname +
	    ". This is impossible. Please report this.\n");
    }

    return 1;
}

/* **************************************************************************
 * startloc - handle starting locations
 */
nomask int
startloc(string str)
{
    string *sstr;
    int what;

    CHECK_SO_LORD;

    notify_fail("Incorrect syntax for startloc.\n" +
        "Syntax: startloc list [def[ault]] / [temp[orary]]\n" +
        "        startloc add def[ault] / temp[orary] <loc>\n" +
        "        startloc rem[ove] def[ault] / temp[orary] <loc>\n");

    if (!stringp(str))
    {
	return 0;
    }

    sstr = explode(str, " ");

    switch(sstr[0])
    {
    case "list":
	if (sizeof(sstr) < 2)
	    what = 3;
	else
	{
	    switch (sstr[1])
	    {
	    case "default":
	    case "def":
		what = 1;
		break;

	    case "temporary":
	    case "temp":
		what = 2;
		break;

	    default:
		notify_fail("I don't know of any '" + sstr[1] +
			    "' start location.\n");
		return 0;
		break;
	    }
	}

	if (what == 1 ||
	    what == 3)
	{
	    write("Default start locations:\n");
	    write(sprintf("%-*#s\n", 76, 
                implode(sort_array(SECURITY->query_list_def_start(str)),
                "\n")) + "\n");
	}

	if (what == 2 ||
	    what == 3)
	{
	    write("Temporary start locations:\n");
	    write(sprintf("%-*#s\n", 76, 
                implode(sort_array(SECURITY->query_list_temp_start(str)),
                "\n")) + "\n");
	}
	break;
	
    case "add":
	if (sizeof(sstr) < 3)
	{
	    return 0;
	}

        if (extract(sstr[2], -2) == ".c")
        {
            sstr[2] = extract(sstr[2], 0, -3);
        }

	switch(sstr[1])
	{
	case "default":
	case "def":
	    SECURITY->add_def_start_loc(sstr[2]);
	    break;

	case "temporary":
	case "temp":
	    SECURITY->add_temp_start_loc(sstr[2]);
	    break;

	default:
	    notify_fail("I don't know of any '" + sstr[1] +
			"' start location.\n");
	    return 0;
	    break;
	}
	break;

    case "remove":
    case "rem":
	if (sizeof(sstr) < 3)
	{
	    notify_fail("Remove what?\n");
	    return 0;
	}

	switch(sstr[1])
	{
	case "default":
	case "def":
	    SECURITY->rem_def_start_loc(sstr[2]);
	    break;

	case "temporary":
	case "temp":
	    SECURITY->rem_temp_start_loc(sstr[2]);
	    break;

	default:
	    notify_fail("I don't know of any '" + sstr[1] +
			"' start location.\n");
	    return 0;
	    break;
	}
	break;

    default:
	return 0;
    }

    write("Ok.\n");
    return 1;
}
