/*
 * /std/player/getmsg_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * All incoming messages to the player should come through here.
 * All non combat interaction with other players are also here.
 */

#include <config.h>
#include <flags.h>
#include <language.h>
#include <ss_types.h>
#include <stdproperties.h>

static mapping introduced_name;   /* People who introduced themselves */
public int     wiz_unmet;         /* If wizards want to be unmet */

/************************************************************************
 *
 * Introduction and met routines
 */

/*
 * Function name:   query_met
 * Description:     Tells if we know a certain living's name.
 * Arguments:       name: Name of living or objectp of living
 * Returns:         True if we know this name otherwise false.
 */
public int
query_met(mixed name)
{
    string str;
    mapping rem;

#ifndef MET_ACTIVE
    return 1;
#else
    if (objectp(name))
	str = (string) name->query_real_name();
    else if (stringp(name))
    {
       	str = name;
	name = find_living(name);
    }
    else
	return 0;

    if (name && name->query_prop(LIVE_I_NEVERKNOWN))
	return 0;

    if (name && name->query_prop(LIVE_I_ALWAYSKNOWN))
	return 1;

    /* Wizards know everyone */
    if (query_wiz_level() > 0)
    {
	if (query_wiz_unmet() == 0 ||
            (query_wiz_unmet() == 2 && name && !(name->query_npc())))
    	    return 1;
	else
	    return 0; /* Unless they have said they don't want to. */
    }
    
    if (str == query_real_name()) /* I always know myself */
	return 1;

    if (query_introduced(str) || query_remembered(str))
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
    
    if (!mappingp(introduced_name))
	introduced_name = ([ ]);
    
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
    if (!mappingp(introduced_name))
        introduced_name = ([ ]);
    
    if (!introduced_name[str])
        return 0;
    
    m_delkey(introduced_name, str);
    return 1;
}

/*
 * Function name:   query_introduced
 * Description:     Return a mapping with all people we have been introduced
 *                  to during this session.
 * Returns:         The mapping with introduced people.
 */
public varargs mixed
query_introduced(mixed name)
{
    if (!mappingp(introduced_name))
        introduced_name = ([ ]);

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
