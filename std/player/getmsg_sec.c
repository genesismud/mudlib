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
#include <formulas.h>
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
 * Function name:   set_wiz_unmet
 * Description:     Marks if the wizard wants to see all as met or unmet
 * Arguments:       flag: 1 if see as unmet, 0 if see as met
 * Returns:         The current state    
 */
public int
set_wiz_unmet(int flag)
{
    wiz_unmet = flag;
    return wiz_unmet;
}

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
	if (wiz_unmet == 0 || (wiz_unmet == 2 && name && !(name->query_npc())))
    	    return 1;
	else
	    return 0; /* Unless they have said they don't want to. */

    if (str == query_real_name()) /* I always know myself */
	return 1;

    if (!introduced_name)
	introduced_name = ([ ]);

    rem = query_remember_name();
	
    if (introduced_name[str] || (mappingp(rem) && rem[str]))
	return 1;
     
    /* Default case */
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
    if (query_met(str))	return;  /* Don't add if already present */
    
    if (!mappingp(introduced_name))
	introduced_name = ([ ]);
    
    introduced_name[str] = 1;
}

/*
 * Function name:   query_introduced
 * Description:     Return a mapping with all people we have been introduced
 *                  to during this session.
 * Returns:         The mapping with introduced people.
 */
public mapping
query_introduced()
{
    return introduced_name;
}

/*
 * Function name:   add_remembered
 * Description:     Adds a living to those whom we want to remember.
 *                  The living must exist in our list of those we have been
 *                  introduced to.
 * Arguments:       str: Name of living that we want to remember.
 * Returns:         -1 if at limit for remember, 0 if not introduced, 
 *                  1 if remember ok, 2 if already known
 */
public int
add_remembered(string str)
{
    mapping tmp;
    int max;
    
    tmp = query_remember_name();

    if (mappingp(tmp) && tmp[str])
	return 2; /* Already known */
    
    if (!query_met(str) &&
		(!mappingp(introduced_name) || !introduced_name[str]))
	return 0;
    
    max = F_MAX_REMEMBERED(query_stat(SS_INT), query_stat(SS_WIS));

    if (!mappingp(tmp))
	tmp = ([ ]);

    if( m_sizeof(tmp) >= max)
	return -1;

    m_delkey(introduced_name, str);

    tmp[str] = 1;
	set_remember_name(tmp);
    
    return 1; /* Remember ok */
}

/*
 * Function name:   query_remembered
 * Description:     Gives back a mapping with all names of people we remember
 * Return:          A mapping with names
 */
public mapping
query_remembered()
{
    return query_remember_name();
}

/*
 * Function name:   remove_remembered
 * Description:     Removes a remembered or introduced person from our list.
 * Arguments:       name: Name of living to forget
 * Returns:         false if the name was not introduced or remembered,
 *                  true otherwise.
 */
public int
remove_remembered(string name)
{
    int pos, pos2;

    name = lower_case(name);
    
    if (mappingp(introduced_name))
    {
	pos = introduced_name[name];
	m_delkey(introduced_name, name);
    }
    
    pos2 = query_remember_name()[name];
    set_remember_name(m_delete(query_remember_name(), name));
    
    return (pos || pos2);
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
