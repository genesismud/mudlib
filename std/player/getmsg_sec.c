/*
 * /std/player/getmsg_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * All incoming messages to the player should come through here.
 * All non combat interaction with other players are also here.
 */

#include <gmcp.h>
#include <options.h>
#include <macros.h>
#include <mail.h>
#include <living_desc.h>
#include <state_desc.h>
#include <stdproperties.h>

static mapping m_introduced_name = ([ ]); /* People who introduced themselves */
static mapping m_gift_today = ([ ]); /* Gifts accepted from people today */
static mapping m_gmcp = ([ ]); /* Subscribed to GMCP packages. */
static mapping m_gmcp_options = GMCP_DEFAULT_OPTIONS; /* GMCP options. */
static string  gmcp_client,      /* GMCP client name */
               gmcp_version,     /* GMCP client version */
               gmcp_mapfile,     /* last loaded mapfile */
               gmcp_section;     /* last loaded mapsection */

nomask public void gmcp_team();

/************************************************************************
 *
 * Introduction and met routines
 */

/*
 * Function name: query_met
 * Description  : Tells if we know a certain living's name.
 * Arguments    : mixed who: name or object of living.
 * Returns      : int 1/0 - if true, we know the person.
 */
public int
query_met(mixed who)
{
    string name;

#ifndef MET_ACTIVE
    return 1;
#else
    if (objectp(who))
    {
	name = (string)who->query_real_name();
    }
    else if (stringp(who))
    {
       	name = who;
	who = find_living(who);
    }
    else
	return 0;

    if (who->query_prop(LIVE_I_NEVERKNOWN))
	return 0;

    if (who->query_prop(LIVE_I_ALWAYSKNOWN))
	return 1;

    /* Wizards know everyone, unless they don't want to. */
    if (query_wiz_level())
    {
        switch(query_wiz_unmet())
        {
        case 0:
            /* Wizard wants to know everybody. */
            return 1;
        case 1:
            /* Wizard doesn't want to know anyone, but let's check for recent
             * introductions.
             */
            return query_introduced(name);
        case 2:
            /* Wizard wants to know players and not NPC's. Let's be nice and
             * check for recent introductions on NPC's.
             */
            return who->query_npc() ? query_introduced(name) : 1;
        }
    }

    /* Know thyself!*/
    if (name == query_real_name())
	return 1;

    if (query_remembered(name) || query_introduced(name))
	return 1;
    
    return 0;
#endif MET_ACTIVE
}

/*
 * Function name:   add_introduced
 * Description:     Add the name of a living who has introduced herself to us
 * Arguments:       str: Name of the introduced living
 */
public void
add_introduced(string str)
{
    if (query_met(str))
        return;  /* Don't add if already present */

    m_introduced_name[str] = 1;
}

/*
 * Function Name:   remove_introduced
 * Description  :   Removes someone from our introduce list.
 * Arguments    :   str - the name
 */
public int
remove_introduced(string str)
{
    if (!m_introduced_name[str])
        return 0;

    m_delkey(m_introduced_name, str);
    return 1;
}

/*
 * Function name: query_introduced
 * Description  : This routine has a double function. It will return a mapping
 *                with all people we have been introduced to during this
 *                session, or return whether a person introduced himself to us.
 * Arguments    : string name - the name of the person to verify. If omitted,
 *                    it will return the mapping of introductions.
 * Returns      : int 1/0 - whether this person introduced to us or not.
 *                mapping - the mapping with all introduced people.
 */
public varargs mixed
query_introduced(string name)
{
    if (name)
        return m_introduced_name[name];
    
    return ([ ]) + m_introduced_name;
}

/*
 * Function name: query_gift_today
 * Description  : Find out whether we accept gifts from a person or find out
 *                the names of all persons we accept gifts from today.
 * Arguments    : string name - the lower case name of the player, if omitted,
 *                    gives all names.
 * Returns      : int 1/0 - if true, gifts are accepted, or an array of names.
 */
public mixed
query_gift_today(string name)
{
    if (name)
        return m_gift_today[name];
    else
        return m_indices(m_gift_today);
}

/*
 * Function name: add_gift_today
 * Description  : Add a person to the list of people whose gifts to accept today.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was added (0 = already added).
 */
public int
add_gift_today(string name)
{
    name = lower_case(name);
    if (m_gift_today[name])
        return 0;

    m_gift_today[name] = 1;
    return 1;
}

/*
 * Function name: remove_gift_today
 * Description  : Remove a person from the list of people whose gifts to accept today.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was removed (0 = was not added).
 */
public int
remove_gift_today(string name)
{
    name = lower_case(name);
    if (!m_gift_today[name])
        return 0;

    m_delkey(m_gift_today, name);
    return 1;
}

/* 
 * Function name: query_reject_gift
 * Description  : Use this routine to test whether a person will reject a gift
 *                from another person. Should only be used for players.
 * Arguments    : object giver - the object trying to give you something.
 * Returns      : int 1/0 - if true, reject the gift.
 */
varargs public
query_reject_gift(object giver = this_player())
{
    string name;

    /* Always accept gifts from wizards and NPC's */
    if (!giver || giver->query_wiz_level() || giver->query_npc())
    {
        return 0;
    }

    name = giver->query_real_name();
    if (query_option(OPT_GIFT_FILTER))
    {
        /* Always allow gifts within the team. */
        if (IN_ARRAY(giver, query_team_others()))
            return 0;

	/* Don't accept gifts from people we don't know. */
	if (!query_met(name))
	    return 1;

	/* Person can be on the 'white' list of accepted givers. */
        if (query_gift_accept(name))
            return 0;

        return !query_gift_today(name);
    }
    else
    {
	/* Always accept gifts from people we don't know. */
	if (!query_met(name))
	    return 0;

	/* Person can be on the 'black' list of rejected givers. */
        return query_gift_reject(name);
    }
}

/*
 * Function Name: catch_gmcp
 * Description  : All out of band GMCP messages are sent through here.
 * Arguments    : string package - Message identifier
 *                mixed data     - The message data (val2json rules apply)
 */
nomask public void
catch_gmcp(string package, mixed data)
{
    write_socket_gmcp(package, data);
}

/*
 * Function name: query_gmcp
 * Description  : Find out whether a GMCP package is supported.
 * Arguments    : string package - the package to test.
 * Returns      : int - the version level subscribed to.
 */
nomask public int
query_gmcp(string package)
{
    return m_gmcp[package];
}

/*
 * Function name: query_gmcp_list
 * Description  : Find out the names of the packages we are subscribed to.
 * Returns      : string * - the list of names.
 */
nomask public string *
query_gmcp_list()
{
    return m_indices(m_gmcp);
}

/*
 * Function name: set_gmcp
 * Description  : Sets or removes GMCP packages.
 * Arguments    : string package - the package (set/add/remove).
 *                mixed data - the packages to set/add/remove.
 */
nomask public void
set_gmcp(string package, mixed data)
{
    string name;
    int version, index, stat;
    string *added = ({ });

    /* Sanity check. Argument must be an array. */
    if (!pointerp(data)) return;

    /* Set discards the currently set protocols. Add is additive. */
    if (package == GMCP_CORE_SUPPORTS_SET)
    {
        m_gmcp = ([ ]);
    }
    foreach(mixed line: data)
    {
        /* Argument must be a string. */
        if (!stringp(line)) continue;

        line = lower_case(line);
        if (!sscanf(line, "%s %d", name, version))
        {
            name = line;
            version = 1;
        }

        /* Only wizards may use debug information. */
        if ((name == GMCP_CORE_DEBUG) && !query_wiz_level() &&
            !wildmatch("*jr", query_real_name()))
        {
            catch_gmcp(GMCP_CORE_DEBUG, "Rejected.");
            continue;
        }

        switch(package)
        {
        case GMCP_CORE_SUPPORTS_ADD:
        case GMCP_CORE_SUPPORTS_SET:
            m_gmcp[name] = version;
            added += ({ name });
            break;
        case GMCP_CORE_SUPPORTS_REMOVE:
            m_delkey(m_gmcp, name);
        }
    }

    /* Send initial data for CHAR package. */
    if (IN_ARRAY(GMCP_CHAR, added))
    {
        /* Send the char.statusvars package. This is mostly meaningless in Genesis. */
        data = ([ GMCP_LEVEL : "Level", GMCP_RACE : "Race", GMCP_NAME : "Name",
            GMCP_GENDER : "Gender" ]);
        for(index = 0; index < SS_NO_EXP_STATS; index++)
        {
            data[SD_LONG_STAT_DESC[index]] = capitalize(SD_LONG_STAT_DESC[index]);
        }
        catch_gmcp(GMCP_CHAR_STATUSVARS, data);

        /* Send the char.status package. */
        data = ([ GMCP_NAME : query_name(), GMCP_RACE : query_race_name(),
            GMCP_LEVEL : GET_EXP_LEVEL_DESC(query_average_stat(), query_gender()),
	    GMCP_RANK : WIZ_RANK_NAME(SECURITY->query_wiz_rank(query_real_name())),
            GMCP_GENDER : LD_GENDER_MAP[query_gender()],
            GMCP_ALIGN : query_align_text(),
            GMCP_MAIL : MAIL_FLAGS[MAIL_CHECKER->query_mail(query_real_name())] ]);
        for(index = 0; index < SS_NO_EXP_STATS; index++)
        {
            stat = query_stat(index);
            if (stat >= SD_STATLEVEL_SUP)
                data[SD_LONG_STAT_DESC[index]] = "supreme";
            else if (stat >= SD_STATLEVEL_IMM)
                data[SD_LONG_STAT_DESC[index]] = "immortal";
            else if (stat >= SD_STATLEVEL_EPIC)
                data[SD_LONG_STAT_DESC[index]] = "epic";
            else
                data[SD_LONG_STAT_DESC[index]] = GET_STAT_LEVEL_DESC(index, stat);
        }
        catch_gmcp(GMCP_CHAR_STATUS, data);

        gmcp_team();

        /* Send the initial vitals with an alarm to ensure they are loaded. */
        set_alarm(1.0, 0.0, gmcp_char_vitals);
    }

    /* Send initial data for ROOM package. */
    if (IN_ARRAY(GMCP_ROOM, added))
    {
        environment()->gmcp_room_info(this_object());
    }
}

/*
 * Function name: rebuffer_gmcp
 * Description  : This routine will push all current GMCP settings into calls
 *                to incoming_gmcp() for 
 * Arguments    : object target - the object to buffer into.
 */
nomask public void
rebuffer_gmcp(object target)
{
    mixed data;

    /* Repack client name and version. */
    target->incoming_gmcp(GMCP_CORE_HELLO,
        ([ GMCP_CLIENT : gmcp_client, GMCP_VERSION : gmcp_version ]) );

    /* Repack supported packages. */
    data = ({ });
    foreach(string name: m_indices(m_gmcp))
    {
        data += ({ name + " " + m_gmcp[name] });
    }
    target->incoming_gmcp(GMCP_CORE_SUPPORTS_SET, data);

    /* Repack options. */
    data = ([ ]);
    foreach(string option: m_indices(m_gmcp_options))
    {
        data[option] = (m_gmcp_options[option] ? "on" : "off");
    }
    target->incoming_gmcp(GMCP_CORE_OPTIONS, data);
}

/*
 * Function name: query_gmcp_option
 * Description  : Find out the value of a GMCP option.
 * Arguments    : string option - the option to test.
 * Returns      : int - 1/0 (true/false).
 */
nomask public int
query_gmcp_option(string option)
{
    return m_gmcp_options[option];
}

/*
 * Function name: set_gmcp_options
 * Description  : Sets one or more GMCP options.
 * Arguments    : mapping data - ([ "option" : "on/off" ])
 */
nomask public void
set_gmcp_options(mixed data)
{
    /* Sanity check. Argument must be a mapping. */
    if (!mappingp(data)) return;

    foreach(string option: m_indices(data))
    {
        if (!stringp(data[option]))
        {
            catch_gmcp(GMCP_CORE_OPTIONS, "Syntax: " + option + " on/off");
            continue;
        }
    
        switch(lower_case(data[option]))
        {
        case "on":
            m_gmcp_options[option] = 1;
            continue;
        case "off":
            m_delkey(m_gmcp_options, option);
            continue;
        default:
            catch_gmcp(GMCP_CORE_OPTIONS, "Syntax: " + option + " on/off");
        }
    }
}

/*
 * Function name: set_gmcp_client
 * Description  : Advises on the client parameters (height and width).
 * Arguments    : mapping data - ([ "height" : val ])
 */
nomask public void
set_gmcp_client(mixed data)
{
    /* Sanity check. Argument must be a mapping. */
    if (!mappingp(data)) return;

    if (!wildmatch("*jr", name)) return;

    if (intp(data[GMCP_HEIGHT]) && (data[GMCP_HEIGHT] > 0))
    {
	cl_height = max(20, data[GMCP_HEIGHT] - 5);
    }
    if (intp(data[GMCP_WIDTH]) && (data[GMCP_WIDTH] > 0))
    {
	cl_width = max(80, data[GMCP_WIDTH] - 0);
	fixup_screen();
    }
}

/*
 * Function name: gmcp_refresh
 * Description  : Called from SECURITY from time to time to update the
 *                vitals so that we can give messages going up as well.
 */
nomask public void
gmcp_refresh()
{
    if (query_gmcp(GMCP_CHAR))
    {
        this_object()->calculate_hp();
        this_object()->calculate_fatigue();
        calculate_mana();
        query_stuffed();
        query_soaked();
        query_intoxicated();
    }
}

/*
 * Function name: gmcp_char
 * Description  : Updates the char package with a new value.
 * Arguments    : string package - the package to update
 *                string name - the name of the variable
 *                mixed value - the new value
 */
nomask public void
gmcp_char(string package, string name, mixed value)
{
    if (m_gmcp[GMCP_CHAR])
    {
        catch_gmcp(package, ([ name : value ]) );
    }
}

/*
 * Function name: gmcp_comms
 * Description  : Sends a communication package.
 * Arguments    : string line - the name of the line/verb.
 *                string name - the name/description of the player, 0 for self.
 *                string message - the message (full text message, no newline).
 */
nomask public void
gmcp_comms(string line, string player, string message)
{
    string *parts;
    string  body = "";

    if (!m_gmcp[GMCP_COMM])
	return;

    /* For NPC's, allow gagging of the message. */
    if (this_player()->query_npc() && !query_gmcp_option(GMCP_NPC_COMMS))
    {
        return;
    }

    /* Construct the body itself by taking everything after the first semicolon. */
    parts = explode(message, ": ");
    if (sizeof(parts) > 1)
    {
        body = implode(parts[1..], ": ");
    }

    catch_gmcp(GMCP_COMM_CHANNEL, ([ GMCP_LINE : line,
        GMCP_NAME : (strlen(player) ? player : "You"),
        GMCP_BODY : body, GMCP_MESSAGE : message ]) );
}

/*
 * Function name: gmcp_comms_vbfc
 * Description  : Resolves the VBFC in a message and then passes it as
 *                communication package.
 * Arguments    : string line - the name of the line/verb.
 *                string message - the message (full text message, no newline)
 *                    including VBFC to be resolved.
 */
nomask public void
gmcp_comms_vbfc(string line, string message)
{
    string name;

    if (m_gmcp[GMCP_COMM])
    {
        name = this_player()->query_The_name(this_object());
        gmcp_comms(line, name, process_string(message, 1));
    }
}

/*
 * Function name: gmcp_room_map
 * Description  : Pushes the current map graphics to the client if you moved
 *                into a room with a different map.
 *                Note: if the player moves to a room without a map, we do NOT
 *                delete the map as the player may just move back to it.
 * Arguments    : string mapfile - the mapfile the player is in.
 *                string section - the section within the mapfile.
 */
nomask public void
gmcp_room_map(string mapfile, string section)
{
    mapping data;

    if (!m_gmcp[GMCP_ROOM] || !mapfile)
	return;

    if ((gmcp_mapfile == mapfile) && (gmcp_section == section))
	return;

    gmcp_mapfile = mapfile;
    gmcp_section = section;

    data = ([ GMCP_MAP : MAP_CENTRAL->query_map(mapfile, section) ]);
    if (section != "main")
    {
	data[GMCP_ZOOM] = MAP_CENTRAL->query_map(mapfile, "main");
    }
    catch_gmcp(GMCP_ROOM_MAP, data);
}

/*
 * Function name: gmcp_map_reset
 * Description  : Called in case of revive or switch to send the map to the
 *                new client.
 */
nomask public void
gmcp_map_reset()
{
    gmcp_mapfile = 0;
    gmcp_section = 0;
    environment()->gmcp_room_info(this_object());
}

/*
 * Function name: gmcp_team
 * Description  : Lists the team through gmcp. Only works if you are subscribed
 *                to the gmcp.char package.
 */
nomask public void
gmcp_team()
{
    mapping data = ([ ]);
    object* members = ({ });

    /* NPC or not interested. */
    if (!query_gmcp(GMCP_CHAR))
    {
        return;
    }

    /* Not in a team. */
    if (!my_leader && !sizeof(my_team))
    {
	catch_gmcp(GMCP_CHAR_TEAM, ([ GMCP_LEADER : 0, GMCP_MEMBERS: ({ }) ]) );
	return;
    }
    /* Construct leader and members. */
    if (my_leader)
    {
        data[GMCP_LEADER] = my_leader->query_art_name(this_object());
        members = (object *)my_leader->query_team() - ({ this_object() });
	data[GMCP_FOLLOW] = (my_leader->query_prop(LIVE_I_TEAM_NO_FOLLOW) ? "stay" : "follow");
    }
    else
    {
        data[GMCP_LEADER] = "You";
	members = my_team;
	data[GMCP_FOLLOW] = (query_prop(LIVE_I_TEAM_NO_FOLLOW) ? "stay" : "follow");
    }
    data[GMCP_MEMBERS] = map(members, &->query_art_name(this_object()));

    catch_gmcp(GMCP_CHAR_TEAM, data);
}

/*
 * Function name: gmcp_team_update
 * Description  : Called in the team leader to update the char.team in him and
 *                all team members. It is done this way, rather than via the
 *                update routines in the team_join and team_leave because these
 *                may be called multiple times in certain team manipulations
 *                and we only want to call with the end result.
 */
public void
gmcp_team_update()
{
    gmcp_team();
    my_team->gmcp_team();
}

/*
 * Function name: query_gmcp_client
 * Description  : Find out the name of the client.
 * Returns      : string - the client name.
 */
nomask public string
query_gmcp_client()
{
    return gmcp_client;
}

/*
 * Function name: query_gmcp_version
 * Description  : Find out the version number of the client.
 * Returns      : string - the client version.
 */
nomask public string
query_gmcp_version()
{
    return gmcp_version;
}

/*
 * Function name: gmcp_hello
 * Description  : Processes the Core.Hello package.
 * Arguments    : mixed data - ([ "client" : "name", "version" : "value" ])
 */
nomask public void
gmcp_hello(mixed data)
{
    if (mappingp(data))
    {
        gmcp_client = data[GMCP_CLIENT];
        if (floatp(data[GMCP_VERSION]))
        {
            /* Some clients incorrectly use floats. */
            gmcp_version = sprintf("%.3f", data[GMCP_VERSION]);
        }
        else
        {
            /* Handles strings and ints. */
            gmcp_version = "" + data[GMCP_VERSION];
        }
    }
}

/*
 * Function name: catch_tell
 * Description  : All text printed to this living via either write() or
 *                tell_object() will end up here. Here we do the actual
 *                printing to the player in the form of a write_socket()
 *                that will send the message to the host.
 * Arguments    : string msg - the message to print.
 */
public void
catch_tell(string msg)
{
    write_socket(msg);
}

