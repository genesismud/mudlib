/*
 * /cmd/wiz/arch.c
 *
 * This object holds the commands reserved for archwizards.
 * The following commands are supported:
 *
 * - all_spells
 * - arch
 * - arche
 * - ateam
 * - delchar
 * - draft
 * - global
 * - mailadmin
 * - mkdomain
 * - mudstatus
 * - namechange
 * - newchar
 * - nopurge
 * - pingmud
 * - purge
 * - resetpassword
 * - rmdomain
 * - siteban
 * - startloc
 * - storemuds
 * - suspend
 * - trace
 * - traceprefix
 * - vip
 * - xpclear
 */

#pragma no_clone
#pragma no_inherit
#pragma save_binary
#pragma strict_types

inherit "/cmd/std/command_driver";

#include <adverbs.h>
#include <composite.h>
#include <files.h>
#include <macros.h>
#include <mail.h>
#include <log.h>
#include <stdproperties.h>
#include <std.h>

#define CHECK_SO_ARCH   if (WIZ_CHECK < WIZ_ARCH) return 0; \
                        if (this_interactive() != this_player()) return 0

#define NOPURGE(s)      ("/syslog/nopurge/" + extract((s), 0, 0) + "/" + (s))

/* **************************************************************************
 * Return a list of which souls are to be loaded.
 * The souls are listed in order of command search.
 */
nomask string *
get_soul_list()
{
    return ({ WIZ_CMD_ARCH,
              WIZ_CMD_LORD,
              WIZ_CMD_NORMAL,
              WIZ_CMD_HELPER,
              WIZ_CMD_PILGRIM,
              WIZ_CMD_APPRENTICE,
              MBS_SOUL });
}

/* **************************************************************************
 * Return a proper name of the soul in order to get a nice printout.
 */
nomask string
get_soul_id()
{
    return WIZNAME_ARCH;
}

/* **************************************************************************
 * The list of verbs and functions. Please add new in alfabetical order.
 */
nomask mapping
query_cmdlist()
{
    return ([
             "all_spells":"all_spells",
             "arch":"arch",
             "arche":"arch",
             "ateam":"ateam",

             "delchar":"delchar",
             "draft":"draft",

             "global":"global",

             "mailadmin":"mailadmin",
             "mkdomain":"mkdomain",
             "mudstatus":"mudstatus",

             "namechange":"namechange",
             "newchar":"newchar",
             "nopurge":"nopurge",

             "pingmud":"pingmud",
             "purge":"purge",

             "resetpassword":"resetpassword",
             "rmdomain":"rmdomain",

             "siteban":"siteban",
             "storemuds":"storemuds",
             "suspend":"suspend",

             "trace":"set_trace",
             "traceprefix":"set_traceprefix",

             "vip":"vip",

             "xpclear":"xpclear",
         ]);
}

/* **************************************************************************
 * Here follows the actual functions. Please add new functions in the 
 * same order as in the function name list.
 * **************************************************************************/

/* **************************************************************************
 * all_spells - list all active spells
 */
nomask int
all_spells()
{
    CHECK_SO_ARCH;

    SECURITY->list_spells();
    
    return 1;
}

/* **************************************************************************
 * arch  - send a message on the archline
 * arche - emote a message on the archline
 */
nomask int
arch(string str)
{
    if (!stringp(str))
    {
        notify_fail(capitalize(query_verb()) + " what?\n");
        return 0;
    }

    return WIZ_CMD_APPRENTICE->line((WIZNAME_ARCH + " " + str),
        (query_verb() == "arche"));
}

/* **************************************************************************
 * ateam - maintain the list of admin teams
 */
nomask int
ateam(string str)
{
    string *tms, *helps, *args = ({});
    int i, sz;

    if (stringp(str))
        args = explode(lower_case(str), " ");
    else
        args = ({ "" });

    if (args[0] == "?")
    {
        notify_fail("Syntax: ateam\n" +
                    "        ateam add <team> <member>\n" +
                    "        ateam remove <team> <member>\n");
        return 0;
    }

    switch (args[0])
    {
    case "add":
        if (sizeof(args) != 3)
        {
            ateam("?");
            return 0;
        }
        if (SECURITY->query_wiz_rank(args[2]) < WIZ_NORMAL)
        {
            notify_fail("The member must be a full wizard.\n");
            return 0;
        }

        SECURITY->add_team_member(args[1], args[2]);
        write("Ok.\n");
        break;

    case "remove":
        if (sizeof(args) != 3)
        {
            ateam("?");
            return 0;
        }

        SECURITY->remove_team_member(args[1], args[2]);
        write("Ok.\n");
        break;

    default:
        tms = sort_array(SECURITY->query_teams());
        if (sizeof(tms))
        {
            for (i = 0, sz = sizeof(tms) ; i < sz ; i++)
            {
                args = sort_array(SECURITY->query_team_list(tms[i]));
                write(sprintf("%-10s", capitalize(tms[i])) + 
                    (sizeof(args) ? COMPOSITE_WORDS(map(args, capitalize)) :
                    "No members") + ".\n");
            }
        }
        else
            write("No admin teams exist.\n");
        break;
    }

    return 1;
}

/* **************************************************************************
 * delchar - remove a playerfile
 */
nomask int
delchar(string str)
{
    string who, reason;

    CHECK_SO_ARCH;

    if ((!stringp(str)) ||
        (sscanf(str, "%s %s", who, reason) != 2))
        return notify_fail("Usage: delchar <player> <reason>\n");

    who = lower_case(who);

    if (objectp(find_player(who)))
    {
        notify_fail("The player " + capitalize(who) + " is logged in. It is " +
            "useless to remove the file now.\n");
        return 0;
    }
    
    if (SECURITY->query_wiz_rank(who))
    {
        notify_fail("Wizards should be demoted.\n");
        return 0;
    }

    if (!SECURITY->exist_player(who))
    {
        notify_fail("Someone beat you to it. There is no player " +
            capitalize(who) + " anymore.\n");
        return 0;
    }

    if (!SECURITY->remove_playerfile(who, reason))
    {
        notify_fail("Failed to delete the player.\n");
        return 0;
    }

    write("The player " + capitalize(who) + " has been removed.\n");
    return 1;
}

/* **************************************************************************
 * draft - add someone to a domain
 */
nomask int
draft(string str)
{
    string dname;
    string pname;

    CHECK_SO_ARCH;

    if ((!stringp(str)) ||
        (sscanf(str, "%s %s", dname, pname) != 2))
    {
        notify_fail("Usage: draft Domain player\n");
        return 0;
    }

    return SECURITY->draft_wizard_to_domain(dname, pname);
}


/* **************************************************************************
 * global - add/remove people from the global list
 */
nomask int
global(string str)
{
    string *cmds;
    string *wnames;
    int     index;
    int     size;
    mapping gread;

    CHECK_SO_ARCH;

    if (!stringp(str))
    {
        gread = SECURITY->query_global_read();
        wnames = sort_array(m_indices(gread));
        size = sizeof(wnames);

        if (!size)
        {
            write("There are no wizards with global read rights.\n");
            return 1;
        }

        index = -1;
        while(++index < size)
        {
            write(sprintf("%-11s added by %-11s", capitalize(wnames[index]),
                capitalize(gread[wnames[index]][0])) + " (" +
                gread[wnames[index]][1] + ")\n");
        }
        return 1;
    }

    cmds = explode(str, " ");

    switch(cmds[0])
    {
    case "add":
        return SECURITY->add_global_read(cmds[1], implode(cmds[2..], " "));

    case "remove":
        return SECURITY->remove_global_read(cmds[1]);

    default:
        break;
    }

    notify_fail("Do which 'global' operation?\n");
    return 0;
}

/* **************************************************************************
 * mailadmin - manage the mail system
 */
nomask int
mailadmin(string str)
{
    CHECK_SO_ARCH;

    return SECURITY->mailadmin(str);
}

/* **************************************************************************
 * mkdomain - make a new domain
 */
nomask int
mkdomain(string arg)
{
    string *args;

    CHECK_SO_ARCH;

    if (!stringp(arg) ||
        sizeof(args = explode(arg, " ")) != 3)
    {
        notify_fail("Syntax: mkdomain <Domain> <short> <liege>\n");
        return 0;
    }

    return SECURITY->make_domain(capitalize(args[0]),
                                 lower_case(args[1]),
                                 lower_case(args[2]));
}

/* **************************************************************************
 * mudstatus - Turn on and off /MUDstatistics
 */
nomask int
mudstatus(string arg)
{
    int ev, ti;
    string fl;

    CHECK_SO_ARCH;

    if (!stringp(arg) ||
        (sscanf(arg,"%s %d %d", fl, ev, ti) != 3))
    {
        if (fl != "off")
        {
            notify_fail("SYNTAX: mudstatus on/off eval_limit time_limit(ms)\n");
            return 0;
        }
        ev = 0; 
        ti = 0;
    }

    SECURITY->do_debug("mudstatus", fl, ev, ti / 10);
    write("Ok.\n");
    return 1;
}

/* **************************************************************************
 * namechange - change someones name.
 */
nomask int
namechange(string str)
{
    string  *words;
    string  oldname;
    string  newname;
    object  player;

    CHECK_SO_ARCH;

    if ((!stringp(str)) ||
        (sizeof(words = explode(lower_case(str), " ")) != 2))
    {
        notify_fail("Syntax: namechange <oldname> <newname>\n");
        return 0;
    }

    oldname = words[0];
    if (!SECURITY->exist_player(oldname))
    {
        notify_fail("There is no player named " + capitalize(oldname) +
            ".\n");
        return 0;
    }

    newname = words[1];
    if (SECURITY->exist_player(newname))
    {
        notify_fail("There already is a player named " +
            capitalize(newname) + ".\n");
        return 0;
    }

    /* Check only for banished names. Let's not bother about domain names,
     * or the lenght or validity of the name. Arches should be careful about
     * that sort of stuff.
     */
    if (file_size(BANISH_FILE(newname)) > 0)
    {
        notify_fail("The name " + capitalize(newname) + " is banised.\n");
        return 0;
    }

    /* Player found? Make him quit. */
    if (objectp(player = find_player(oldname)))
    {
        tell_object(player, "\nYour name will be changed to " +
            capitalize(newname) + ". This will take five seconds.\n\n");
        player->quit();
        if (objectp(player))
        {
            player->save_character();
            player->remove_object();
        }
    }

    return SECURITY->rename_playerfile(oldname, newname);
}

/* **************************************************************************
 * newchar - create a new character
 */
nomask int
newchar(string str)
{
    string *args, name;
    string passwd;
    mapping tmp_char;

    if (!strlen(str))
    {
        notify_fail("Usage: newchar <name> <email address>.\n");
        return 0;
    }

    args = explode(str, " ");
    if (sizeof(args) != 2)
    {
        notify_fail("Usage: newchar <name> <email address>.\n");
        return 0;
    }

    name = lower_case(args[0]);
    if (file_size(PLAYER_FILE(name) + ".o") != -1)
    {
        notify_fail("The player '" + capitalize(name) + "' already exists.\n");
        return 0;
    }

    if (file_size(BANISH_FILE(name)) != -1)
    {
        notify_fail("The player '" + capitalize(name) + "' is banished.\n");
        return 0;
    }

    if (file_size("/players/saved/" + name + ".o") != -1)
    {
        notify_fail("The player '" + capitalize(name) + "' is saved.\n");
        return 0;
    }

    tmp_char = restore_map("/secure/proto_char");
    tmp_char["name"] = name;
    passwd = SECURITY->generate_password();
    tmp_char["password"] = crypt(passwd, 0);
    tmp_char["mailaddr"] = args[1];
    tmp_char["password_time"] = time();
    tmp_char["login_time"] = time();

    /*
     * NOTA BENE!
     * 
     * DO NOT CHANGE THE FORMAT OF THE NEWCHAR_LIST FILE!!!!
     * This file is read automatically by an external service which mails
     * the recipiants of the charater, messing with this will mess up that
     * service.
     */
    write_file("/syslog/log/NEWCHAR_LIST", args[1] + "#" + name + "#" +
        passwd + "\n");

    save_map(tmp_char, PLAYER_FILE(name));

    write("The player '" + capitalize(name) + "' is created. Password: " +
        passwd + "\n");
    write_file(OPEN_LOG_DIR + "/CREATE_PLAYER", ctime(time()) + " " +
        capitalize(name) + " created by " +
        capitalize(this_interactive()->query_real_name()) + ".\n");

    return 1;
}

/* **************************************************************************
 * nopurge - prevent someone from being purged.
 */
nomask int
nopurge(string str)
{
    string *words;
    string name;

    CHECK_SO_ARCH;

    if (!stringp(str))
    {
        notify_fail("No argument to nopurge. See the help page.\n");
        return 0;
    }

    words = explode(str, " ");

    switch(words[0])
    {
    case "-i":
    case "-info":
        /* Strip the first argument. */
        words = words[1..];
        break;

    case "-a":
    case "-add":
        if (sizeof(words) < 3)
        {
            write("No reason added to 'purge -a[dd]'.\n");
            return 1;
        }

        name = lower_case(words[1]);
        if (SECURITY->query_no_purge(name))
        {
            write("Player '" + capitalize(name) +
                "' is already protected against purging.\n");
            return 1;
        }

        if (!write_file(NOPURGE(name), implode(words[2..], " ")))
        {
            write("Failed to write purge protection on '" + capitalize(name) +
                "'.\n");
            return 1;
        }

        write("Purge protection added to '" + capitalize(name) + "'.\n");
        return 1;

    case "-r":
    case "-remove":
        name = lower_case(words[1]);
        if (!(SECURITY->query_no_purge(name)))
        {
            write("Players '" + capitalize(name) +
                "' is not protected against purging.\n");
            return 1;
        }

        if (!rm(NOPURGE(name)))
        {
            write("Failed to remove purge protection from '" +
                capitalize(name) + "'.\n");
            return 1;
        }

        write("Removed purge protection from '" + capitalize(name) + "'.\n");
        return 1;

    default:
        break;
    }

    name = lower_case(words[0]);
    if (!(SECURITY->query_no_purge(name)))
    {
        write("Player '" + name + "' is not purge-protected.\n");
        return 1;
    }

    write("Purge protection on '" + name + "':\n");

    if (!strlen(str = read_file(NOPURGE(name))))
    {
        write("    Error: unable to read purge protection.\n");
        return 1;
    }

    words = explode(str, "\n");
    write("Wizard: " + words[0] +
        "\nReason: " + words[1] +
        "\nDate  : " + ctime(file_time(NOPURGE(name)))+ "\n");
    return 1;
}
 
/* **************************************************************************
 * pingmud - send a udp ping to another mud
 */
nomask int
pingmud(string arg)
{
    CHECK_SO_ARCH;

#ifdef UDP_MANAGER    
    return UDP_MANAGER->cmd_ping(arg);
#else
    notify_fail("No udp manager active.\n");
    return 0;
#endif UDP_MANAGER
}

/* **************************************************************************
 * purge - remove all players that have been idle too long or remove one
 *         individual mortal.
 */
nomask int
purge(string str)
{
    CHECK_SO_ARCH;

    notify_fail("Currently not operational.\n");
    return 0;

    if (str == "players")
    {
        if (objectp(PURGE_OBJECT))
        {
            notify_fail("The general purger is already active.\n");
            return 0;
        }

        PURGE_OBJECT->purge_players();
        return 1;
    }

    return SECURITY->purge(lower_case(str));
}

/* **************************************************************************
 * resetpassword - (re)set the password of a player.
 */
nomask int
resetpassword(string str)
{
    string pswd = 0;
    string name;
    mapping playerfile;

    if (!strlen(str))
    {
        notify_fail("Syntax: resetpassword <name> [<password>]\n");
        return 0;
    }
    if (sscanf(str, "%s %s", name, pswd) != 2)
    {
        name = str;
    }

    name = lower_case(name);
    if (objectp(find_player(name)))
    {
        notify_fail("The player " + capitalize(name) + " is in the realms. " +
            "Cannot (re)set the password.\n");
        return 0;
    }
    /* Test whether the name is the same as the saved name. A simple check. */
    playerfile = restore_map(PLAYER_FILE(name));
    if (!m_sizeof(playerfile) || (playerfile["name"] != name))
    {
        notify_fail("No valid player file found for " + capitalize(name) +
            ".\n");
        return 0;
    }

    /* Reset the password and also the time, forcing the player to change upon
       login if we set the password to a text. */
    playerfile["password"] = (strlen(pswd) ? crypt(pswd, 0) : 0);
    playerfile["password_time"] = 0;
    save_map(playerfile, PLAYER_FILE(name));

    if (pswd)
    {
        write("Reset the password of " + capitalize(name) + " to " + pswd +
            ".\n");
    }
    else
    {
        write("Cleared the password of " + capitalize(name) + ".\n");
    }
    return 1;
}

/* **************************************************************************
 * rmdomain - remove an old domain
 */
nomask int
rmdomain(string arg)
{
    CHECK_SO_ARCH;

    return SECURITY->remove_domain(arg);
}

/* **************************************************************************
 * siteban - (Dis)allow logins and or new characters from a site.
 */
nomask int
siteban(string str)
{
    CHECK_SO_ARCH;
    
    return SECURITY->siteban(str);
}

/* **************************************************************************
 * suspend - suspend a player from playing for a certain time.
 */
nomask int
suspend(string str)
{
    string *words;
    int number;
    int seconds;
    object player;
    mapping playerfile;

    CHECK_SO_ARCH;

    if (!strlen(str))
    {
        notify_fail("Syntax: suspend <name> off\n" +
            "        suspend <name> <number> hour/hours/day/days\n");
        return 0;
    }

    words = explode(str, " ");
    if (!SECURITY->exist_player(words[0]))
    {
        notify_fail("There is no player named " + capitalize(words[0]) +
            ".\n");
        return 0;
    }

    switch(sizeof(words))
    {
    case 2:
        if (words[1] != "off")
        {
            break;
        }

#ifdef LOG_SUSPENDED
        SECURITY->log_syslog(LOG_SUSPENDED,
            sprintf("%s %-11s: %-11s lifted.\n",
            ctime(time()),
            capitalize(this_interactive()->query_real_name()),
            capitalize(words[0])));
#endif /* LOG_SUSPENDED */
        write("Lifted the suspension from " + capitalize(words[0]) + ".\n");

        if (objectp(player = find_player(words[0])))
        {
            player->reset_restricted(0);
        }
        else
        {
            playerfile = restore_map(PLAYER_FILE(words[0]));
            playerfile["restricted"] = 0;
            save_map(playerfile, PLAYER_FILE(words[0]));
        }
        return 1;

    case 3:
        number = atoi(words[1]);
        if (number < 1)
        {
            notify_fail("The <number> of hours/days must be positive.\n");
            return 0;
        }

        switch(words[2])
        {
        case "hour":
        case "hours":
            str = "hour";
            seconds = (3600 * number);
            break;

        case "day":
        case "days":
            str = "day";
            seconds = (86400 * number);
            break;

        default:
            notify_fail("Suspension must be done for <number> " +
                "hour/hours/day/days only.\n");
            return 0;
        }

#ifdef LOG_SUSPENDED
        SECURITY->log_syslog(LOG_SUSPENDED,
            sprintf("%s %-11s: %-11s %s\n", ctime(time()),
            capitalize(this_interactive()->query_real_name()),
            capitalize(words[0]), ctime(time() + seconds)));
#endif /* LOG_SUSPENDED */

        write("Suspension on " + capitalize(words[0]) + " for " + number +
            " " + str + ((number > 1) ? "s" : "") + ".\nNo new login is " +
            "accepted until: " + ctime(time() + seconds) + "\n");

        if (objectp(player = find_player(words[0])))
        {
            tell_object(player, "\n\nYou have just been suspended from " +
                "playing for " + number + " " + str +
                ((number > 1) ? "s" : "") + ".\nNo new login from you is " +
                "accepted until: " + ctime(time() + seconds) + "\n");

            player->set_restricted(seconds, 0);
            player->save_me();
            player->command("$quit");
            if (objectp(player))
            {
                SECURITY->do_debug("destroy", player);
            }
        }
        else
        {
            playerfile = restore_map(PLAYER_FILE(words[0]));
            playerfile["restricted"] = -(time() + seconds);
            save_map(playerfile, PLAYER_FILE(words[0]));
        }
        return 1;
    }

    notify_fail("Syntax: suspend <name> off\n" +
        "        suspend <name> <number> hour/hours/day/days\n");
    return 0;
}

/* **************************************************************************
 * trace - trace the mud
 */
nomask int
set_trace(string str)
{
    int n;
    int o;

    CHECK_SO_ARCH;

    if (stringp(str) &&
        sscanf(str, "%d", n) == 1)
    {
        o = SECURITY->do_debug("trace", n);
        write("Trace was " + o + ", now " +
              SECURITY->do_debug("trace", n) + "\n");
    }
    else
    {
        write("Bad argument to trace.\n");
    }
    return 1;
}

/* **************************************************************************
 * traceprefix - set the trace prefixes
 */
nomask int
set_traceprefix(string str)
{
    string o;

    CHECK_SO_ARCH;

    if (stringp(str))
        o = SECURITY->do_debug("traceprefix", str);
    else
        o = SECURITY->do_debug("traceprefix");
    write("Trace prefix was " + o + "\n");
    return 1;
}

/* **************************************************************************
 * storemuds - Store the mudlist currently in the UDP_MANAGER
 */
nomask int
storemuds(string arg)
{
    CHECK_SO_ARCH;

#ifdef UDP_MANAGER    
    if (UDP_MANAGER->update_masters_list())
    {
        write("Ok.\n");
        return 1;
    }
    notify_fail("Master did not allow the store.\n");
    return 0;
#else
    notify_fail("No udp manager active.\n");
    return 0;
#endif UDP_MANAGER
}

/* **************************************************************************
 * vip - Display the people with vip-access, grant vip access or revoke it.
 */
nomask int
vip(string str)
{
    string *vips = (string *)QUEUE->query_vip();

    CHECK_SO_ARCH;

    str = lower_case(str);
    if (!stringp(str))
    {
        if (!sizeof(vips))
        {
            write("No people with VIP-access.\n");
            return 1;
        }

        write("The following people have VIP-access: " +
            COMPOSITE_WORDS(vips) + ".\n");
        return 1;
    }

    /* Remove vip-access from someone. */
    if (sscanf(str, "-r %s", str) == 1)
    {
        if (member_array(str, vips) == -1)
        {
            write(capitalize(str) + " has no VIP access.\n");
            return 1;
        }

        if (QUEUE->unvip(str))
        {
            write("VIP access of " + capitalize(str) + " revoked.\n");
            return 1;
        }

        write("VIP access of " + capitalize(str) + " is NOT revoked.\n");
        return 1;
    }

    if (!(SECURITY->exist_player(str)))
    {
        notify_fail("There is no player called " + capitalize(str) + ".\n");
        return 0;
    }

    if (member_array(str, vips) >= 0)
    {
        write(capitalize(str) + " already has VIP access.\n");
        return 1;
    }

    if (QUEUE->set_vip(str))
    {
        tell_object(this_interactive(), "VIP access of " + capitalize(str) +
            " granted.\n");
        return 1;
    }

    write("VIP access of " + capitalize(str) + " was NOT granted.\n");
    return 1;    
}


/* **************************************************************************
 * xpclear - Clear the xp counters in master for a specific domain
 */
nomask int
xpclear(string dom)
{
    CHECK_SO_ARCH;

    return SECURITY->domain_clear_xp(dom);
}
