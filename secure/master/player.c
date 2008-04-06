/*
 * /secure/master/player.c
 *
 * This file is a sub-part of SECURITY related to player matters, most notably
 * registration of seconds.
 */

#include "/sys/log.h"

#define SECONDS_SAVE "/players/seconds"

/* Indices to the m_seconds mapping. */
#define SNDS_REPORTER 0
#define SNDS_TIME     1

/*
 * Global variable in the save-file:
 *
 * The record consists of a first player with a number of associated seconds.
 * This prevents a double/duplex bookkeeping. For each second, we remember who
 * reported the second, and when.
 *
 * m_seconds = ([ (string) player :
 *                ([ (string) second :
 *                   ({ (string) reporter, (int) time }) ]) ])
 */
static private mapping m_seconds = ([ ]);

/*
 * Global run-time variable.
 *
 * At run-time we maintain a simple index from each second to the first.
 *
 * m_firsts = ([ (string) player : (string) first ])
 */
static private mapping m_firsts = ([ ]);

/*
 * Function name: save_seconds
 * Description  : Since the seconds information is stored in a separate
 *                place, we have a separate routine for it.
 */
static void
save_seconds()
{
    set_auth(this_object(), "root:root");
    save_map(m_seconds, SECONDS_SAVE);
}

/*
 * Function name: query_find_first
 * Description  : Find out who is the first of the second.
 * Arguments    : string name - the lower case name of the second.
 * Returns      : string - the first associated with the second. Note this
 *                    may be the second itself is it really is the first.
 */
public string
query_find_first(string name)
{
    string first = (m_firsts[name] ? m_firsts[name] : name);

    if (!valid_player_info(previous_object(), first, "second"))
    {
        return 0;
    }

    return (m_seconds[first] ? first : 0);
}

/*
 * Function name: query_is_second
 * Description  : Find out whether two players are seconds. Note that the
 *                order of the arguments is irrelevant.
 * Arguments    : string first - the lower case name of the first player.
 *                string second - the lower case name of the second player.
 * Returns      : int 1/0 - if true, they are seconds.
 */
public int
query_is_second(string first, string second)
{
    if (!valid_player_info(previous_object(), first, "second"))
    {
        return 0;
    }

    /* The order of the arguments doesn't matter. */
    return (m_firsts[second] == first) || (m_firsts[first] == second);
}

/*
 * Function name: query_seconds
 * Description  : Find out the names of the seconds of a certain player.
 * Arguments    : string name - the lower case name of the player.
 * Returns      : string *names - the names of the seconds, 0 means no access.
 */
public string *
query_seconds(string name)
{
    string first = (m_firsts[name] ? m_firsts[name] : name);
    string *names;

    if (!valid_player_info(previous_object(), first, "second"))
    {
        return 0;
    }

    /* Seconds found. */
    if (sizeof(names = m_indices(m_seconds[first])))
    {
        /* Add the first to the seconds. */
        names = ({ first }) + names;
        /* But don't return the queried player. */
        return names - ({ name });
    }
    return ({ });
}

/* 
 * Function name: query_player_seconds
 * Description  : Find out the names of the seconds of a player that were
 *                registered by that player. This routine operates on
 *                this_interactive().
 * Returns      : string *names - the names of the seconds.
 */
public string *
query_player_seconds()
{
    string name = this_interactive()->query_real_name();
    string first = (m_firsts[name] ? m_firsts[name] : name);
    string *seconds = m_indices(m_seconds[first]);

    /* Only return those seconds listed by the player. */
    foreach(string second: seconds)
    {
        if (m_seconds[first][second][SNDS_REPORTER] != first)
        {
            seconds -= ({ second });
        }
    }
    
    if (sizeof(seconds))
    {
        /* Add the first to the seconds. */
        seconds = ({ first }) + seconds;
        /* But don't return the querying player. */
        return seconds - ({ name });
    }
    return ({ });
}

/*
 * Function name: query_second_info
 * Description  : Obtain more information about a second, namely the person
 *                who registered it, and when.
 * Arguments    : string first - the lower case name of the first player.
 *                string second - the lower case name of the second player.
 * Returns      : mixed - ({ string name, int time })
 */
public mixed
query_second_info(string first, string second)
{
    string wname = geteuid(previous_object());

    if (!valid_player_info(wname, first, "second"))
    {
        return 0;
    }

    if (!m_seconds[first])
    {
        return 0;
    }

    return secure_var(m_seconds[first][second]);
}

/*
 * Function name: add_second
 * Description  : Called (from the wizard soul) to add a second to a certain
 *                first. Note that the order is relevant.
 * Arguments    : string first - the lower case name of the first player.
 *                string second - the lower case name of the second player.
 * Returns      : int 1/0 - success/failure.
 */
public int
add_second(string first, string second)
{
    string wname = geteuid(previous_object());

    first = lower_case(first);
    second = lower_case(second);
    /* If the first is a second, find the real first. */
    first = (m_firsts[first] ? m_firsts[first] : first);

    if (!valid_player_info(wname, first, "second"))
    {
        return 0;
    }

    /* Second is really a first. */
    if (m_seconds[second])
    {
        notify_fail("The player " + capitalize(second) +
            " already has seconds registered.\n");
        return 0;
    }

    /* Second already has a first. */
    if (m_firsts[second])
    {
        notify_fail("The player " + capitalize(second) +
            " is already a second of " + capitalize(m_firsts[second]) + ".\n");
        return 0;
    }

    /* The actual addition. */
    if (!mappingp(m_seconds[first]))
    {
        m_seconds[first] = ([ ]);
    }
    m_seconds[first] += ([ second : ({ wname, time() }) ]);
    m_firsts[second] = first;
    save_seconds();
#ifdef LOG_SECONDS
    SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
        capitalize(wname) + " added " + capitalize(second) +
        " to " + capitalize(first) + ".\n"));
#endif LOG_SECONDS
    return 1;
}

/*
 * Function name: remove_second
 * Description  : Called (from the wizard soul) to remove a second from a
 *                certain first. Note that the order is relevant.
 * Arguments    : string first - the lower case name of the first player.
 *                string second - the lower case name of the second player.
 * Returns      : int 1/0 - success/failure.
 */
public int
remove_second(string first, string second)
{
    string wname = geteuid(previous_object());

    first = lower_case(first);
    second = lower_case(second);
    /* If the first is a second, find the real first. */
    first = (m_firsts[first] ? m_firsts[first] : first);

    if (!valid_player_info(wname, first, "second"))
    {
        return 0;
    }

    if (m_firsts[second] != first)
    {
        notify_fail("The player " + capitalize(second) + " is not a second of " +
            capitalize(first) +
            (m_firsts[second] ? " (but of " + capitalize(m_firsts[second]) + ") ": "") +
            ".\n");
        return 0;
    }

    /* The actual removal. If there is no second left, remove the first too. */
    m_delkey(m_seconds[first], second);
    if (!m_sizeof(m_seconds[first]))
    {
        m_delkey(m_seconds, first);
    }
    m_delkey(m_firsts, second);
    save_seconds();
#ifdef LOG_SECONDS
    SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
        capitalize(wname) + " removed " + capitalize(second) +
        " from " + capitalize(first) + ".\n"));
#endif LOG_SECONDS
    return 1;
}

/*
 * Function name: register_second
 * Description  : Called (from the player soul) for a player to register a
 *                second player. Note we don't do notify_fail() in this
 *                routine since it isn't called directly from the command.
 * Arguments    : string second - the lower case name of the second to add.
 *                string password - the password of teh second to add.
 * Returns      : int 1/0 - success/failure.
 */
public int
register_second(string second, string password)
{
    string pname = this_interactive()->query_real_name();
    string first = (m_firsts[pname] ? m_firsts[pname] : pname);
    string tmp;
    object player;
    int match;

    second = lower_case(second);
    
    /* Verify whether the first knows the password of the second. */
    player = finger_player(second);
    match = player->match_password(password);
    player->remove_object();
    if (!match)
    {
        write("The password of " + capitalize(second) + " is not correct.\n");
        return 0;
    }

    /* In principle the wizard is the first, unless he's a second. */
    if (query_wiz_level(second) && !m_firsts[second])
    {
        tmp = first; first = second; second = tmp;
    }

    /* Second is already a first. */
    if (m_seconds[second])
    {
        /* Both are firsts. This won't work. */
        if (m_seconds[first])
        {
#ifdef LOG_SECONDS
            SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
                capitalize(pname) + " tries to merge " +
                capitalize(second) + " and " + capitalize(first) + ").\n"));
#endif LOG_SECONDS
            write("Second " + capitalize(second) +
                " appears to have seconds as well. Please consult the AoP.\n");
            return 0;
        }
        /* Swap the first and the second. */
        tmp = first; first = second; second = tmp;
    }

    /* The second is already registered. */
    if (m_firsts[second])
    {
        /* Second is registered to someone else. */
        if (m_firsts[second] != first)
        {
#ifdef LOG_SECONDS
            SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
                capitalize(pname) + " tries to register " +
                capitalize(second) + " (first is " +
                capitalize(m_firsts[second]) + ").\n"));
#endif LOG_SECONDS
            write("Second " + capitalize(second) +
                " appears to be registered already. Please consult the AoP.\n");
            return 0;
        }

        /* Player has already registered this second. */
        if (m_seconds[first][second][SNDS_REPORTER] == first)
        {
            write("You already registered " + capitalize(second) + ".\n");
            return 0;
        }

        /* Player registers a second already listed by wizard. */
#ifdef LOG_SECONDS
        SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
            capitalize(pname) + " registers " + capitalize(second) +
            " (previously by " +
            capitalize(m_seconds[first][second][SNDS_REPORTER]) + ").\n"));
#endif LOG_SECONDS
        m_seconds[first][second] = ({ first, time() });
        save_seconds();
        write("You registered " + capitalize(second) + " as second.\n");
        return 1;
    }

    /* New registration. */
    if (!mappingp(m_seconds[first]))
    {
        m_seconds[first] = ([ ]);
    }
    m_seconds[first] += ([ second : ({ first, time() }) ]);
    m_firsts[second] = first;
    save_seconds();
#ifdef LOG_SECONDS
    SECURITY->log_syslog(LOG_SECONDS, (ctime(time()) + " " +
        capitalize(pname) + " registers " + capitalize(second) + ".\n"));
#endif LOG_SECONDS
    write("You registered " + capitalize(second) + " as second.\n");
    return 1;
}

/*
 * Function name: init_player_info
 * Description  : Fill the m_firsts mapping with a combination of all seconds
 *                pointing to the firsts they are a second to.
 */
static void
init_player_info()
{
    string *firsts;
    string *seconds;

    m_seconds = restore_map(SECONDS_SAVE);
    if (!mappingp(m_seconds))
    {
        m_seconds = ([ ]);
    }

    firsts = m_indices(m_seconds);
    foreach(string first: firsts)
    {
        seconds = m_indices(m_seconds[first]);
        foreach(string second: seconds)
        {
            /* See if the seconds still exist. */
            if (exist_player(second))
            {
                m_firsts[second] = first;
            }
            else
            {
                m_seconds[first] = m_delete(m_seconds[first], second);
            }
        }
        /* No seconds (left), remove the first. */
        if (!m_sizeof(m_seconds[first]))
        {
            m_delkey(m_seconds, first);
        }
        /* Todo: If the first doesn't exist, make a second into the first. */
    }
}
