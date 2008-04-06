/*
 * /std/player/getmsg_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * All incoming messages to the player should come through here.
 * All non combat interaction with other players are also here.
 */

#include <stdproperties.h>

static mapping introduced_name = ([ ]); /* People who introduced themselves */

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

    introduced_name[str] = 1;
}

/*
 * Function Name:   remove_introduced
 * Description  :   Removes someone from our introduce list.
 * Arguments    :   str - the name
 */
public int
remove_introduced(string str)
{
    if (!introduced_name[str])
        return 0;

    m_delkey(introduced_name, str);
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
        return introduced_name[name];
    
    return ([ ]) + introduced_name;
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
