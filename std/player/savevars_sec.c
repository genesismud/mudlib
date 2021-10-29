/*
 * /std/player/savevars_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * All the variables that are to be saved to the players save file
 * are defined here.
 */

#include <files.h>
#include <formulas.h>
#include <gmcp.h>
#include <language.h>
#include <log.h>
#include <macros.h>
#include <options.h>
#include <ss_types.h>
#include <std.h>

/*
 * Non-static global variables. These variables are stored in the players
 * savefile.
 */
private string  name,                   /* Name of the player */
                password,               /* Password of the player */
                player_file,            /* Name of file used as player file */
                path,                   /* Current directory path */
                mailaddr,               /* Email adress of the player */
                adj_desc,               /* The adjectives describing player */
                default_start_location, /* Start location of player */
                temp_start_location,    /* Temp start location */
                login_from,             /* Host wherefrom last logged in */
                options,                /* Options for the player */
                *auto_load,             /* Automatically loaded objects */
                *pending_auto_load,     /* Items currently being loaded */
                *auto_shadow,           /* Our loyal shadows */
                *recover_list,          /* The recover list */
                *pending_recover;       /* Items currently being recovered. */

private mixed   saved_props;            /* List of saved properties */

private int     *bit_savelist,          /* Saved bits */
                age_heart,              /* # of heartbeats (== 2 seconds) */
#ifdef FORCE_PASSWORD_CHANGE
                password_time,          /* Time passwords was changed */
#endif FORCE_PASSWORD_CHANGE
#ifndef NO_SKILL_DECAY
                decay_time_acc,         /* # of seconds since last decay */
#endif NO_SKILL_DECAY
		        creation_time,          /* Player creation time() */
                login_time,             /* Last time logging in */
                logout_time,            /* Last time logging out / saving */
                wiz_unmet;              /* If wizards want to be unmet */
private mapping m_remember_name,        /* Names of players we have met */
		m_gift_accept,		/* Players we accept gifts from */
		m_gift_reject,		/* Players we reject gifts from */
                m_bits,                 /* The domain bits of the player. */
		m_vars,                 /* Some less used variables. */
                m_friends_list,         /* Players we consider friends */
                m_alias_list,           /* Aliases for the quicktyper */
                m_nick_list;            /* Nick(name)s for the quicktyper */
private object  logout_location;        /* The last logout / saving location */

/*
 * Static global variables. These variables are used internally and are not
 * stored in the players savefile.
 */
private static string   cap_name;       /* Capitalized name of the player */
private static int      age_time,       /* the last time the age was updated */
			cl_width,       /* width of the client. */
			cl_height;      /* height of the client. */
#ifndef NO_SKILL_DECAY
private static int      decay_time;     /* the last time the decay was done */
#endif NO_SKILL_DECAY

/*
 * Prototypes.
 */
nomask public int remove_autoshadow(mixed shadowfile);
public nomask void fixup_screen();
#ifndef NO_SKILL_DECAY
nomask public int query_skill_decay();
#endif NO_SKILL_DECAY
public varargs mixed query_introduced(mixed name);
public int remove_introduced(string str);
nomask public void gmcp_char(string package, string name, mixed value);

/*
 * DOMAIN_BIT - transformation from old bit groups into single value.
 */
#define DOMAIN_BIT(group, bit) ((group) * 20 + (bit))


/*
 * Function name: player_save_vars_reset
 * Description  : Reset the private time-related functions.
 */
static nomask void
player_save_vars_reset()
{
    age_time = time();
}

/*
 * Name of the player.
 * This can only be called from player_sec.
 */
static nomask void
set_name(string n)
{
    name = n;
    cap_name = capitalize(n);

    ::set_name(name, 1);
}

static varargs void
add_name(mixed name, int noplural)
{
    ::add_name(name, noplural);
}

/*
 * Function name:   remove_name
 * Description:     Remove a certain name
 * Arguments:       n: A string or a pointer of strings of names
 */
nomask void
remove_name(mixed n)
{
    ::remove_name(n);
}

/*
 * Function name:   set_adj
 * Description:     'go through function' for storing the adjectives. Also
 *                  sets the adjectives to 'adj_desc' if no arg given.
 *                  No more than 2 adjectives of a total length of 35
 *                  characters is allowed.
 * Arguments:       str: Adjectives to pass to ::set_adj() or 0.
 * Returns:         1 if setting was succesfull, 0 otherwise
 */
public int
set_adj(string *arr)
{
    string *adj;
    int i, len;

    if (arr)
    {
        adj = query_adj(1);

        if (sizeof(adj) + sizeof(arr) > 2)
            return 0;

        for (i = 0; i < sizeof(adj); i++)
            len += strlen(adj[i]);
        for (i = 0; i < sizeof(arr); i++)
            len += strlen(arr[i]);
        if (len > 33)
            return 0;

        ::set_adj(arr);
        adj_desc = query_adj(1);
        return 1;
    }
    else
        ::set_adj(adj_desc);

    return 1;
}

/*
 * Function name:   remove_adj
 * Description:     'go through function' for removing the adjectives.
 * Arguments:       str: The adjective string to remove.
 */
public void
remove_adj(string str)
{
    ::remove_adj(str);

    adj_desc = query_adj(1);
}

/*
 * Function name: query_real_name
 * Description  : Gives back the real name of a player, e.g. "fatty". The
 *                capitalized version can be found with query_cap_name().
 *                Note that query_name() should not be used for identification
 *                purposes as it may return a name as "ghost of X".
 * Returns      : string - the lower case name of the player.
 */
nomask public string
query_real_name()
{
    return name;
}

/*
 * Function name: set_race_name
 * Description  : Mask of one in /std/living/savevars.c for GMCP purposes.
 */
public void
set_race_name(string str)
{
    ::set_race_name(str);

    gmcp_char(GMCP_CHAR_STATUS, GMCP_RACE, str);
}

/*
 * Function name: set_alignment
 * Description  : Mask of one in /std/living/savevars.c for GMCP purposes.
 */
public void
set_alignment(int a)
{
    ::set_alignment(a);

    gmcp_char(GMCP_CHAR_STATUS, GMCP_ALIGN, query_align_text());
}

/*
 * Function name: query_wiz_level
 * Description  : This function tests whether this player is a wizard of
 *                a mortal player. Note that it only returns 1/0. In order
 *                to figure out the rank of the player, check:
 *
 *                SECURITY->query_wiz_rank(string name);
 *
 *                and for the level within this rank, call:
 *
 *                SECURITY->query_wiz_level(string name);
 *
 * Returns      : int 1/0 - wizard/mortal player.
 */
public int
query_wiz_level()
{
    return (SECURITY->query_wiz_rank(query_real_name()) > WIZ_MORTAL);
}

/*
 * Password
 */

/*
 * Function name: set_password
 * Description  : Set the password of a player. This is an internal function
 *                and cannot be called externally.
 * Arguments    : string p - the new password string.
 */
nomask static void
set_password(string p)
{
    password = p;

#ifdef FORCE_PASSWORD_CHANGE
    password_time = time();
#endif FORCE_PASSWORD_CHANGE
}

/*
 * Function name: query_password
 * Description  : This function will reveal the password of the player,
 *                but _only_ to SECURITY. Otherwise you shall have to
 *                call the function match_password() to see whether the
 *                password matches.
 * Returns      : string - the password.
 */
nomask public string
query_password()
{
    if (previous_object() != find_object(SECURITY))
    {
        return "that's a secret ;-)";
    }

    return password;
}

/*
 * Function name: match_password
 * Description  : This function matches the password of a player with an
 *                arbitrary string someone claims is the password of this
 *                player. NOTE that if the player has NO password, it always
 *                matches.
 * Arguments    : string p - the password to check.
 * Returns      : int - true/false.
 */
nomask public int
match_password(string p)
{
    if (!strlen(password))
        return 1;

    return (password == crypt(p, password, 0));
}

/*
 * Function name: query_latest_crypt
 * Description  : Find out if this password is of the latest crypt generation
 *                or not. If not, the player may update his password.
 * Returns      : int 1/0 - if true, it uses the latest crypt function.
 */
public int
query_latest_crypt()
{
    return (extract(password, 0, strlen(CRYPT_METHOD)-1) == CRYPT_METHOD);
}

/*
 * Function name: query_password_time
 * Description  : This function will return the time the password was last
 *                changed/set.
 * Returns      : int - the time.
 */
#ifdef FORCE_PASSWORD_CHANGE
nomask int
query_password_time()
{
    return password_time;
}
#endif FORCE_PASSWORD_CHANGE

/*
 * Function name:   set_player_file
 * Description:     Set the playerfile that is loaded when the player enters
 *                  the game. The lagality of the file is checked against
 *                    legal_player in the 'login_new_player' file defined in
 *                    <config.h>
 * Arguments:       f: The filename-string.
 */
nomask public void
set_player_file(string f)
{
    object ob;

    catch(f->teleledningsanka());

    ob = find_object(f);
    if (ob && LOGIN_NEW_PLAYER->legal_player(ob))
        player_file = f;
}

/*
 * Function name:   query_player_file
 * Description:     Gives back the playerfile that is loaded when the player
 *                  enters the game.
 * Returns:         The string with the filename
 */
nomask public string
query_player_file()
{
    return player_file;
}

/*
 * Function name:   query_auto_load
 * Description:     Gives back the array of strings with objects to autoload
 *                  when the player enters the game.
 * Returns:         An array of strings describing what objects to load.
 */
nomask string *
query_auto_load()
{
    return auto_load;
}

/*
 * Function name:   query_pending_auto_load
 * Description:     The list of items for which recovery has begun but not yet
 *                  completed.
 * Returns:         An array of strings describing what objects to recover,
 */
nomask string *
query_pending_auto_load()
{
    if (!pointerp(pending_auto_load))
        return ({ });
    return pending_auto_load;
}

/*
 * Function name:   query_recover_list
 * Description:     Gives back the array of strings with objects to recover
 *                  when the player enters the game.
 * Returns:         An array of strings describing what objects to recover,
 */
nomask string *
query_recover_list()
{
    return recover_list;
}

/*
 * Function name:   query_pending_recover
 * Description:     The list of items for which recovery has begun but not yet
 *                  completed.
 * Returns:         An array of strings describing what objects to recover,
 */
nomask string *
query_pending_recover()
{
    if (!pointerp(pending_recover))
        return ({ });
    return pending_recover;
}


/*
 * Function name:   query_path
 * Description:     Gives back the path of the current directory.
 * Returns:         The path string
 */
public string
query_path()
{
    if (path)
        return path;
    else if (query_wiz_level())
        path = (string) SECURITY->query_wiz_path(query_real_name());
    return path;
}

/*
 * Function name:   query_mailaddr
 * Description:     Gives back the Email address of a player.
 * Returns:         The Email address string
 */
public nomask string
query_mailaddr()
{
    string ceuid = geteuid(calling_object());
    if (this_interactive() == this_object() ||
        ceuid == ROOT_UID || SECURITY->query_wiz_rank(ceuid) >= WIZ_ARCH ||
        SECURITY->query_team_member("aop", ceuid)) {
        return mailaddr;
    }
    return "";
}

/*
 * Function name: query_cap_name
 * Description  : Gives back the capitalized name of a player. By definition
 *                this is the same as capitalize(query_real_name()). The
 *                result is buffered for speed reasons.
 * Returns      : string - the capitalized name string.
 */
public nomask string
query_cap_name()
{
    return cap_name;
}

/*
 * Function name:   query_default_start_location
 * Description:     Gives back the default start location of a player when
 *                  she enters the game.
 * Returns:         The string with the filename of the startup-room.
 */
public string
query_default_start_location()
{
    return default_start_location;
}

/*
 * Function name:   query_temp_start_location
 * Description:     Gives back the temporary start location of a player when
 *                  she enters the game.
 * Returns:         The string with the filename of the startup-room.
 */
public nomask string
query_temp_start_location()
{
    return temp_start_location;
}

/*
 * Function name:   query_age
 * Description:     Gives the age of the living in heart_beats.
 * Returns:         The age.
 */
public nomask int
query_age()
{
    age_heart += ((time() - age_time) / 2);
    age_time = time();

    return age_heart;
}

#ifndef NO_SKILL_DECAY
/*
 * Function name:   query_decay_time
 * Description:     Gives the time since last decay of skills
 * Returns:         The time.
 */
public nomask int
query_decay_time()
{
    /* Due to a bug decay_time_acc has become negative in some players.
       This will fix that. */
    if (decay_time_acc < 0)
        decay_time_acc = 10000;

    /* Only increment if we're actually decaying skills */
    if (!query_skill_decay())
        return decay_time_acc;

    decay_time_acc += (time() - decay_time);
    decay_time = time();

    return decay_time_acc;
}

/*
 * Function name:   reset_decay_time
 * Description:     Reset the decay time to the remainder of time
 *                  after a full interval.
 */
static nomask void
reset_decay_time()
{
    /*
     * Use a loop since it the clock might have ticked untended during
     * the time the player wasn't decaying skills at all. Leave only a
     * reminder <= the total interval.
     */
    if (decay_time_acc < 0)
        decay_time_acc = 10000;

    if (decay_time_acc > 432000)
        decay_time_acc = 432200;

    while (decay_time_acc >= SKILL_DECAY_INTERVAL)
        decay_time_acc -= SKILL_DECAY_INTERVAL;
}
#endif NO_SKILL_DECAY

/*
 * Function name:   set_auto_load
 * Description:     Sets the array with autoload strings. Auytoload strings
 *                  look like "<file>:<arg>".
 * Arguments:       arr: An array with autoload strings.
 */
nomask void
set_auto_load(string *arr)
{
    auto_load = arr;
}

/*
 * Function name:   set_pending_auto_load
 * Description:     Sets the list array of currently loading auto load items.
 *                  look like "<file>:<arg>".
 * Arguments:       arr: An array with recover strings.
 */
nomask static void
set_pending_auto_load(string *arr)
{
    pending_auto_load = arr;
}

/*
 * Function name:   set_recover_list
 * Description:     Sets the array with recover strings. Recover strings
 *                  look like "<file>:<arg>".
 * Arguments:       arr: An array with recover strings.
 */
nomask void
set_recover_list(string *arr)
{
    recover_list = arr;
}

/*
 * Function name:   set_pending_recover
 * Description:     Sets the list array of currently loading recover items.
 *                  look like "<file>:<arg>".
 * Arguments:       arr: An array with recover strings.
 */
nomask static void
set_pending_recover(string *arr)
{
    pending_recover = arr;
}

/*
 * Function name:   set_path
 * Description:     Sets the current path of a player.
 * Arguments:       str: Pathstring
 */
public nomask void
set_path(string str)
{
    path = str;
}

/*
 * Function name:   set_mailaddr
 * Description:     Sets the Email address of a player
 * Arguments:       addr: The Email address string
 */
public nomask int
set_mailaddr(string addr)
{
    string *parts = explode(addr, "\n");

    if (!wildmatch("*@*.*", addr) && addr != "none") {
	write("The email address must be valid.\n");
	return 0;
    }

    if (sizeof(parts) > 1)
    {
        write("The email address should not contain spaces. Only the " +
             "element of the address before the (first) space is used.\n");
        addr = parts[0];
    }

    if (strlen(addr) > 65)
    {
        write("Too long address. Truncated.\n");
        addr = extract(addr, 0, 64);
    }

    write("Set to: " + addr + "\n");
    mailaddr = addr;
    return 1;
}

/*
 * Function name:   set_cap_name
 * Description:     Sets the capitalized name of a player. This is derived
 *                  from query_real_name(), which returns a lowercase name.
 */
public nomask void
set_cap_name()
{
    cap_name = capitalize(query_real_name());
}

/*
 * Function name:   set_default_start_location
 * Description:     Sets a new default startup location for a player. The
 *                  default startup-location must have been approved of by
 *                  an archwizard or keeper.
 *                  The path must be _without_ the trailing .c of the filename.
 * Arguments:       str: the startup room's filename string
 * Returns:         0 if the string was not an accepted location,
 *                  1 when set.
 */
public nomask int
set_default_start_location(string str)
{
    if (!query_wiz_level() &&
        !VALID_DEF_START_LOCATION(str))
    {
        return 0;
    }

    default_start_location = str;
    return 1;
}

/*
 * Function name:   set_temp_start_location
 * Description:     Sets a new temporary startup location for a player. The
 *                  next time the player logs in, she will enter the game at
 *                  the set temporary location, which is then discarded. The
 *                  temporary startup-location must have been approved of by
 *                  an archwizard or keeper.
 *                  The path must be _without_ the trailing .c of the filename.
 * Arguments:       str: the startup room's filename string
 * Returns:         0 if the string was not an accepted location,
 *                  1 when set.
 */
public nomask int
set_temp_start_location(string str)
{
    if (strlen(str) && !VALID_TEMP_START_LOCATION(str))
    {
        return 0;
    }
    temp_start_location = str;
    return 1;
}

/*
 * Function name: set_notify
 * Description  : Set notify status
 * Arguments    : int flag - The current status
 */
public nomask void
set_notify(int flag)
{
    if (flag)
        m_vars[SAVEVAR_NOTIFY] = flag;
    else
	m_delkey(m_vars, SAVEVAR_NOTIFY);
}

/*
 * Function name: query_notify
 * Description  : Query the notify status.
 * Returns      : int - the setting
 */
public nomask int
query_notify()
{
    return m_vars[SAVEVAR_NOTIFY];
}

/*
 * Function name: set_creation_time
 * Description  : Sets the creation time of the player to now.
 */
static nomask void
set_creation_time()
{
    if (!creation_time)
	creation_time = time();
}

/*
 * Function name: query_creation_time
 * Description  : Gives back the creation time of the player.
 * Returns      : int - the time.
 */
public nomask int
query_creation_time()
{
    return creation_time;
}

/*
 * Function name:   set_login_time
 * Description:     sets the time of the login
 * Arguments:       (int) t: the login time
 */
static nomask varargs void
set_login_time(int t = time())
{
    login_time = t;
}

/*
 * Function name:   query_login_time
 * Description:     Gives back the login-time.
 * Returns:         The login-time.
 */
public nomask int
query_login_time()
{
    return login_time;
}

/*
 * Function name:   set_logout_time
 * Description:     sets the time of the logout / save.
 * Arguments:       (int) t: the logout time
 */
static nomask varargs void
set_logout_time(int t = time())
{
    logout_time = t;
}

/*
 * Function name:   query_logout_time
 * Description:     Gives back the logout-time.
 * Returns:         The logout-time.
 */
public nomask int
query_logout_time()
{
    /* For the purpose of backward compatibility, we use the file time
     * if no logout_time was remembered.
     */
    return logout_time ? logout_time : file_time(PLAYER_FILE(name) + ".o");
}

/*
 * Function name: set_logout_location
 * Description  : Stores the current player location as the logout location
 */
static void
set_logout_location()
{
    logout_location = environment();
}

/*
 * Function name: query_logout_location
 * Descriptions : Returns the last saved logout location
 */
public nomask object
query_logout_location()
{
    return logout_location;
}


/*
 * Function name:   set_login_from
 * Description:     Sets from which site the player is logged in.
 */
public nomask void
set_login_from()
{
    login_from = query_ip_name(this_object());
}

/*
 * Function name:   query_login_from
 * Description:     shows from which site the player is logged in
 * Returns:         A string with the site name.
 */
public nomask string
query_login_from()
{
    return login_from;
}

/*************************************************************************
 *
 * Auto shadow routines.
 *
 */

/*
 * Function name: add_autoshadow
 * Description  : Add a shadow to the shadow list. In string form the shadow
 *                has the syntax <filename>:<argument> and the <argument> may
 *                not exceed a length of 80 characters.
 * Arguments    : mixed shadowfile - the shadow-object or the filename of the
 *                   shadow-object.
 * Returns      : int 1/0 - success/failure.
 */
nomask public int
add_autoshadow(mixed shadowfile)
{
    string *sh;

    if (query_wiz_level())
    {
        if (geteuid(previous_object()) != geteuid(this_object()) &&
            geteuid(previous_object()) != ROOT_UID)
            return 0;
    }

    if (objectp(shadowfile))
        shadowfile = MASTER_OB(shadowfile) + ":";

    sh = explode(shadowfile, ":");
    if (sizeof(sh) > 1 && strlen(sh[1]) > 80)
        return 0;

    /* There can only be one. */
    while(remove_autoshadow(shadowfile));

    if (!sizeof(auto_shadow))
        auto_shadow = ({ shadowfile });
    else
        auto_shadow += ({ shadowfile });
    return 1;
}

/*
 * Function name:   remove_autoshadow
 * Description:            Remove a shadow from the shadow list
 * Arguments:       shadowfile: The shadow-object or the filename of the
 *                              shadow_object.
 * Returns:         1 if the removing was successfull,
 *                  0 otherwise
 */
nomask public int
remove_autoshadow(mixed shadowfile)
{
    string *sh;
    int i;

    if (objectp(shadowfile))
        shadowfile = MASTER_OB(shadowfile);

    sh = explode(shadowfile, ":");

    for (i = 0 ; i < sizeof(auto_shadow) ; i++)
    {
        if (sh[0] == explode(auto_shadow[i], ":")[0])
        {
            auto_shadow = exclude_array(auto_shadow, i, i);
            return 1;
        }
    }
    return 0;
}

/*
 * Function name:   query_autoshadow_list
 * Description:            Gives back the list of autoshadow object filenames.
 * Returns:         The autoshadow list
 */
nomask public string *
query_autoshadow_list()
{
    return secure_var(auto_shadow);
}


/*************************************************************************
 *
 * Bit handling routines.
 *
 */

/*
 * Function name: set_domain_bit
 * Description  : Domains have a bit string in the playerfile. This routine
 *                sets a bit in the player for future reference. Authority
 *                is checked to see if the actor may set the bit.
 * Arguments    : string domain - the domain to use (capitalized name).
 *                int bit - the bit to set (range 0..149).
 * Returns      : int 1/0 - if true, the bit is set.
 */
public nomask int
set_domain_bit(string domain, int bit)
{
    /* Access failure. */
    if (bit < 0 || bit >= 150)
    {
        return 0;
    }
    /* Verify whether we have the authority to change the bit. */
    domain = capitalize(domain);
    if (!query_wiz_level() && !wildmatch("*jr", query_real_name()) &&
        !SECURITY->valid_domain_bit(previous_object(), domain, bit))
    {
        return 0;
    }
    /* Need to have a bit string to set a bit. */
    if (!m_bits[domain])
    {
        m_bits[domain] = "";
    }

    m_bits[domain] = efun::set_bit(m_bits[domain], bit);
    return 1;
}

/*
 * Function name: clear_domain_bit
 * Description  : Domains have a bit string in the playerfile. This routine
 *                clears a bit in the player for future reference. Authority
 *                is checked to see if the actor may clear the bit.
 * Arguments    : string domain - the domain to use (capitalized name).
 *                int bit - the bit to clear (range 0..149).
 * Returns      : int 1/0 - if true, the bit is cleared.
 */
public nomask int
clear_domain_bit(string domain, int bit)
{
    /* Access failure. */
    if (bit < 0 || bit >= 150)
    {
        return 0;
    }
    /* Verify whether we have the authority to change the bit. */
    domain = capitalize(domain);
    if (!query_wiz_level() && !wildmatch("*jr", query_real_name()) &&
        !SECURITY->valid_domain_bit(previous_object(), domain, bit))
    {
        return 0;
    }

    /* No bit string means it's cleared already. That's success too, no? */
    if (!m_bits[domain])
    {
        return 1;
    }

    string actor = file_name(previous_object());
    if (this_interactive() != this_object()) {
        actor += " [" + capitalize(this_interactive()->query_real_name()) + "]";
    }

    SECURITY->log_syslog("MODIFY_BIT",
        sprintf("%s %-13s %5s %-11s %3d by %s\n", ctime(),
            capitalize(query_real_name()), "CLEAR", domain, bit, actor),
        100000);
    m_bits[domain] = efun::clear_bit(m_bits[domain], bit);
    return 1;
}

/*
 * Function name: test_domain_bit
 * Description  : Find out whether a certain bit for a certain domain is
 *                set. If
 * Arguments    : string domain - the domain to verify (capitalized name).
 *                int bit - the bit to test (range 0..149).
 * Returns      : int 1/0 - if true, the bit is set.
 */
public nomask int
test_domain_bit(string domain, int bit)
{
    /* Access failure. */
    if (bit < 0 || bit >= 150)
    {
        return 0;
    }

    domain = capitalize(domain);
    return (m_bits[domain] ? efun::test_bit(m_bits[domain], bit) : 0);
}

/*
 * Function name: transform_domain_bits
 * Description  : This routine exists for backward compatibility. It will
 *                transform the traditional bit groups into the modern string
 *                based bit list per domain.
 */
static nomask void
transform_domain_bits()
{
    int index;
    int domain, group;
    string dname;

    /* Add transformation bits, don't wipe what's already there. Guard against
     * uninitialized variables. */
    if (!mappingp(m_bits))
    {
        m_bits = ([ ]);
    }

    /* Only transform if there are bits in the old format. */
    if (!sizeof(bit_savelist))
    {
	return;
    }

    /* Transform each of the bit groups in the save-list. */
    foreach(int bitgroup: bit_savelist)
    {
        /* Extract domain number and bit group. */
        domain = (bitgroup & 0xFFF) / 5;
        group = (bitgroup & 0xFFF) % 5;
        if (!(dname = SECURITY->query_domain_name(domain)))
        {
            dname = "unknown";
        }
        /* Test all bits per group. The actual bits start from the 12th bit. */
        for (index = 0; index < 20; index++)
        {
            if (bitgroup & (1 << (12 + index)))
            {
                if (!m_bits[dname])
                {
                    m_bits[dname] = "";
                }

                m_bits[dname] = set_bit(m_bits[dname], DOMAIN_BIT(group, index));
            }
        }
    }
    bit_savelist = 0;
}

/*
 * Function name:   set_bit
 * Description:     Set a given bit in a given group. The effective userid of
 *                  the object calling this function is used to find which
 *                  domain is setting the bits.
 *                  DEPRECIATED. Use set_domain_bit instead.
 * Arguments:       group: An integer 0-4
 *                  bit:   An integer 0-19
 * Returns:         1 if the bit was successfully set, 0 otherwise.
 */
public int
set_bit(int group, int bit)
{
    string euid = geteuid(previous_object());

    return set_domain_bit(euid, DOMAIN_BIT(group, bit));
}

/*
 * Function name:   clear_bit
 * Description:     Clear a given bit in a given group. The effective userid
 *                  of the object calling this function is used to find which
 *                  domain is clearing the bits.
 *                  DEPRECIATED. Use clear_domain_bit instead.
 * Arguments:       group: An integer 0-4
 *                  bit:   An integer 0-19
 * Returns:         1 if the bit was successfully cleared, 0 otherwise.
 */
public int
clear_bit(int group, int bit)
{
    string euid = geteuid(previous_object());

    return clear_domain_bit(euid, DOMAIN_BIT(group, bit));
}

/*
 * Function name:   test_bit
 * Description:     Test a given bit in a given group for a given domain.
 *                  DEPRECIATED. Use test_domain_bit instead.
 * Arguments:       dom:   Domain which bits are to be tested
 *                  group: An integer 0-4
 *                  bit:   An integer 0-19
 * Returns:         1 if the bit was set, 0 if unset or failed to test.
 */
public int
test_bit(string domain, int group, int bit)
{
    return test_domain_bit(domain, DOMAIN_BIT(group, bit));
}

/*
 * Function name: store_saved_props
 * Description  : Stores the saved properties into an array for the save file.
 */
nomask void
store_saved_props()
{
    int busy;

    m_vars[SAVEVAR_HEIGHT] = query_prop(CONT_I_HEIGHT);
    m_vars[SAVEVAR_WEIGHT] = query_prop(CONT_I_WEIGHT);
    m_vars[SAVEVAR_VOLUME] = query_prop(CONT_I_VOLUME);

    m_vars[SAVEVARS_LAST_HERB] = query_prop(LIVE_I_LAST_HERB);
    m_vars[SAVEVARS_LAST_STEAL] = query_prop(LIVE_I_LAST_STEAL);
    m_vars[SAVEVARS_LAST_PEEK] = query_prop(LIVE_I_LAST_PEEK);

    /* Only wizards may be busy. */
    if (query_wiz_level() && (busy = query_prop(WIZARD_I_BUSY_LEVEL)))
        m_vars[SAVEVAR_BUSY] = busy;
    else
	m_delkey(m_vars, SAVEVAR_BUSY);
}

/*
 * Function name: init_saved_props
 * Description  : Add the saved properties to the player.
 */
static void
init_saved_props()
{
    int size = sizeof(saved_props);

    add_prop(CONT_I_HEIGHT, m_vars[SAVEVAR_HEIGHT]);
    add_prop(CONT_I_VOLUME, m_vars[SAVEVAR_VOLUME]);
    add_prop(CONT_I_WEIGHT, m_vars[SAVEVAR_WEIGHT]);

    add_prop(LIVE_I_LAST_HERB, m_vars[SAVEVARS_LAST_HERB]);
    add_prop(LIVE_I_LAST_STEAL, m_vars[SAVEVARS_LAST_STEAL]);
    add_prop(LIVE_I_LAST_PEEK, m_vars[SAVEVARS_LAST_PEEK]);

    /* Backward compatibility for use of the variable saved_props. */
    if (size > 0) add_prop(CONT_I_WEIGHT, saved_props[0]);
    if (size > 1) add_prop(CONT_I_HEIGHT, saved_props[1]);
    if (size > 2) add_prop(WIZARD_I_BUSY_LEVEL, saved_props[2]);
    if (size > 4) add_prop(CONT_I_VOLUME, saved_props[4]);
    saved_props = 0;

    /* If a person logs in, then a restriction is not present anymore. */
    m_delkey(m_vars, SAVEVAR_RESTRICT);

    /* Backward compatibility */
    add_prop(PLAYER_I_MORE_LEN, &query_option(OPT_MORE_LEN));
}

/*
 * Function name: query_aliases
 * Description  : This returns the list of aliases of a player.
 * Arguments    : mapping - the indices are the alias-names and the
 *                          values the replacements.
 */
public mapping
query_aliases()
{
    if (mappingp(m_alias_list))
        return ([ ]) + m_alias_list;
    else
        return ([ ]);
}

/*
 * Function name: add_aliases
 * Description  : Service function to allow the mudlib to add default aliases.
 * Arguments    : mapping m - a mapping with the aliases to add.
 */
static nomask void
add_aliases(mapping m)
{
    if (!mappingp(m))
        return;

    if (mappingp(m_alias_list))
        m_alias_list += m;
    else
        m_alias_list = m;
}

/*
 * Function name: query_nicks
 * Description  : This returns the list of nick(name)s of a player.
 * Arguments    : mapping - the indices are the namenames and the
 *                          values the replacements.
 */
public mapping
query_nicks()
{
    if (mappingp(m_nick_list))
        return ([ ]) + m_nick_list;
    else
        return ([ ]);
}

/*
 * Function name: set_option
 * Description  : Set a player option.
 * Arguments    : int opt - the option to set (use <options.h>).
 *                int val - the value to set the option to.
 * Returns      : int 1 - success, 0 - fail.
 */
public nomask int
set_option(int opt, int val)
{
    switch (opt)
    {
    case OPT_MORE_LEN:
        if ((val > 100) || (val < 1))
            return 0;

        options = sprintf("%3d", val) + options[3..];
	/* Backward compatibility. */
        add_prop(PLAYER_I_MORE_LEN, val);
        return 1;

    case OPT_SCREEN_WIDTH:
        if ((val != -1) && ((val > 200) || (val < 40)))
            return 0;

        options = options[..2] + sprintf("%3d", val) + options[6..];
        fixup_screen();
        return 1;

    case OPT_WHIMPY:
        if ((val > 99) || (val < 0))
            return 0;

        options = options[..5] + sprintf("%2d", val) + options[8..];
        /* Update the cache of the whimpy value, too. */
        ::set_whimpy(val);
        return 1;

    case OPT_ALWAYS_KNOWN:
        add_prop(LIVE_I_ALWAYSKNOWN, val);
	/* Intentional fallthrough. */

    case OPT_ALL_COMMUNE:
    case OPT_AUTO_PWD:
    case OPT_AUTOLINECMD:
    case OPT_AUTOWRAP:
    case OPT_BLOCK_INTIMATE:
    case OPT_BRIEF:
    case OPT_ECHO:
    case OPT_GAG_MISSES:
    case OPT_GIFT_FILTER:
    case OPT_MERCIFUL_COMBAT:
    case OPT_NO_FIGHTS:
    case OPT_SHOW_UNMET:
    case OPT_SILENT_SHIPS:
    case OPT_TABLE_INVENTORY:
    case OPT_TIMESTAMP:
    case OPT_UNARMED_OFF:
    case OPT_WEB_PERMISSION:
        if (val)
            options = efun::set_bit(options, OPT_BASE + opt);
        else
            options = efun::clear_bit(options, OPT_BASE + opt);
        return 1;

    default:
        return 0;
    }

    /* Not reached. */
    return 1;
}

/*
 * Function name: query_option
 * Description  : Return a player option.
 * Arguments    : int opt - the option (use defines in <options.h>).
 *                int setting - if true, return the unmodified value.
 * Returns      : int - the value of the option.
 */
public nomask varargs int
query_option(int opt, int setting)
{
    if (!strlen(options))
	options = OPT_DEFAULT_STRING;

    switch (opt)
    {
    case OPT_MORE_LEN:
        /* Use the live value from the client, if it exists. */
        if (cl_height && !setting)
	    return cl_height;
        return atoi(options[..2]);

    case OPT_SCREEN_WIDTH:
        /* Use the live value from the client, if it exists. */
        if (cl_width && !setting)
	    return cl_width;
        return atoi(options[3..5]);

    case OPT_WHIMPY:
        return atoi(options[6..7]);

    case OPT_ALL_COMMUNE:
    case OPT_AUTO_PWD:
    case OPT_AUTOLINECMD:
    case OPT_AUTOWRAP:
    case OPT_BLOCK_INTIMATE:
    case OPT_BRIEF:
    case OPT_ECHO:
    case OPT_GAG_MISSES:
    case OPT_GIFT_FILTER:
    case OPT_MERCIFUL_COMBAT:
    case OPT_NO_FIGHTS:
    case OPT_SHOW_UNMET:
    case OPT_SILENT_SHIPS:
    case OPT_TABLE_INVENTORY:
    case OPT_TIMESTAMP:
    case OPT_UNARMED_OFF:
    case OPT_WEB_PERMISSION:
    case OPT_ALWAYS_KNOWN:
        return efun::test_bit(options, OPT_BASE + opt);

    default:
        return 0;
    }

    /* Not reached. */
    return 0;
}

/*
 * Function name: set_restricted
 * Description  : Allow a player to self-impose a playing restriction. It also
 *                allows the administration to restrict a person.
 * Arguments    : int seconds - the time in seconds the for the restriction.
 *                int self - if true, then it is a self-imposed restriction.
 *                    Otherwise it is an administrative restriction.
 */
public nomask void
set_restricted(int seconds, int self)
{
    /* Self-imposed restriction. */
    if (self)
    {
        if (this_object() != this_interactive())
            return;

        m_vars[SAVEVAR_RESTRICT] = (time() + seconds);

#ifdef LOG_RESTRICTED
        SECURITY->log_syslog(LOG_RESTRICTED,
            sprintf("%s %-11s until %s\n", ctime(time()),
                query_cap_name(), ctime(m_vars[SAVEVAR_RESTRICT])),
		LOG_SIZE_100K);
#endif /* LOG_RESTRICTED */
    }
    /* Administrative restriction. */
    else
    {
        if (file_name(previous_object()) != WIZ_CMD_ARCH)
            return;

        m_vars[SAVEVAR_RESTRICT] = -(time() + seconds);
    }
}

/*
 * Function name: reset_restricted
 * Description  : Reset a present restriction.
 * Arguments    : int self - if true, then it is a self-imposed restriction.
 *                    Otherwise it is an administrative restriction.
 */
public nomask void
reset_restricted(int self)
{
    /* Self-imposed restriction. */
    if (self)
    {
        if (this_object() != this_interactive())
            return;

        m_delkey(m_vars, SAVEVAR_RESTRICT);
#ifdef LOG_RESTRICTED
        SECURITY->log_syslog(LOG_RESTRICTED,
            sprintf("%s %-11s lifted the restriction.\n",
            ctime(time()), query_cap_name()), LOG_SIZE_100K);
#endif /* LOG_RESTRICTED */
    }
    else
    {
        if (file_name(previous_object()) != WIZ_CMD_ARCH)
            return;

        m_delkey(m_vars, SAVEVAR_RESTRICT);
    }
}

/*
 * Function name: query_restricted
 * Description  : Get the time() value of the moment until which is restricted.
 * Returns      : int - the time() value of the moment, or 0. A negative value
 *                    indicates restriction by the administration.
 */
public nomask int
query_restricted()
{
    return m_vars[SAVEVAR_RESTRICT];
}

/*************************************************************************
 *
 * Remember handling routines.
 *
 */

/*
 * Function name:   query_remember_name
 * Description:     Gives back a mapping with all names that a player has
 *                  remembered.
 * Returns:         The mapping with all names.
 */
public nomask mapping
query_remember_name()
{
    if (mappingp(m_remember_name))
        return ([ ]) + m_remember_name;
    else
        return ([ ]);
}


/*
 * Function name:   set_remember_name
 * Description:     Sets the people who are remembered by a player.
 * Arguments:       nlist: A mapping with names of remembered players
 */
public nomask void
set_remember_name(mapping nlist)
{
    m_remember_name = ([ ]) + nlist;
}

/*
 * Function name:   query_remembered
 * Description  :   Returns true if the player has rememberd someone.
 *                  If no argument is passed the entire remembered mapping is
 *                  returned.
 * Arguments    :   The name to check for.
 * Returns      :   True if name is remembered
 */
public varargs mixed
query_remembered(mixed name)
{
    if (!mappingp(m_remember_name))
        m_remember_name = ([ ]);

    if (name)
        return m_remember_name[name];
    return ([ ]) + m_remember_name;
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
    int max;

    /* Alreayd known? */
    if (query_remembered(str))
	return 2;

    /* Not introduced? */
    if (!query_met(str) && !query_introduced(str))
	return 0;

    if (!mappingp(m_remember_name))
        m_remember_name = ([ ]);

    max = F_MAX_REMEMBERED(query_stat(SS_INT), query_stat(SS_WIS));
    if (m_sizeof(m_remember_name) >= max)
	return -1;

    remove_introduced(str);
    m_remember_name[str] = 1;

    return 1; /* Remember ok */
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
    int result;

    name = lower_case(name);
    if (query_introduced(name))
    {
        result = 1;
	remove_introduced(name);
    }

    if (query_remembered(name))
    {
        result = 1;
        m_delkey(m_remember_name, name);
    }

    return result;
}

/*
 * Function name: query_gift_accept
 * Description  : Find out whether we accept gifts from a person or find out
 *                the names of all persons we accept gifts from.
 * Arguments    : string name - the lower case name of the player, if omitted,
 *                    gives all names.
 * Returns      : int 1/0 - if true, gifts are accepted, or an array of names.
 */
public mixed
query_gift_accept(string name)
{
    if (name)
        return m_gift_accept[name];
    else
        return m_indices(m_gift_accept);
}

/*
 * Function name: add_gift_accept
 * Description  : Add a person to the list of people whose gifts to accept.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was added (0 = already added).
 */
public int
add_gift_accept(string name)
{
    name = lower_case(name);
    if (m_gift_accept[name])
        return 0;

    m_gift_accept[name] = 1;
    return 1;
}

/*
 * Function name: remove_gift_accept
 * Description  : Remove a person from the list of people whose gifts to accept.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was removed (0 = was not added).
 */
public int
remove_gift_accept(string name)
{
    name = lower_case(name);
    if (!m_gift_accept[name])
        return 0;

    m_delkey(m_gift_accept, name);
    return 1;
}

/*
 * Function name: query_gift_reject
 * Description  : Find out whether we reject gifts from a person or find out
 *                the names of all persons we reject gifts from.
 * Arguments    : string name - the lower case name of the player, if omitted,
 *                    gives all names.
 * Returns      : int 1/0 - if true, gifts are rejected, or an array of names.
 */
public mixed
query_gift_reject(string name)
{
    if (name)
        return m_gift_reject[name];
    else
        return m_indices(m_gift_reject);
}

/*
 * Function name: add_gift_reject
 * Description  : Add a person to the list of people whose gifts to reject.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was added (0 = already added).
 */
public int
add_gift_reject(string name)
{
    name = lower_case(name);
    if (m_gift_reject[name])
        return 0;

    m_gift_reject[name] = 1;
    return 1;
}

/*
 * Function name: remove_gift_reject
 * Description  : Remove a person from the list of people whose gifts to reject.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - if true, it was removed (0 = was not added).
 */
public int
remove_gift_reject(string name)
{
    name = lower_case(name);
    if (!m_gift_reject[name])
        return 0;

    m_delkey(m_gift_reject, name);
    return 1;
}

/*
 * Function name: query_friendship
 * Description  : Find out whether we name is a friend of the player, or
 *                the names of all who we consider friends.
 * Arguments    : string name - the name to check, or 0
 * Returns      : int 1/0, or an array of names.
 */
public mixed
query_friendship(string name)
{
    if (name)
        return m_friends_list[lower_case(name)];
    else
        return m_indices(m_friends_list);
}

/*
 * Function name: add_friendship
 * Description  : Add a name to the players list of friends.
 * Arguments    : string name - the name of the player to add.
 * Returns      : int 1 = added, 0 = already added.
 */
public int
add_friendship(string name)
{
    name = lower_case(name);
    if (m_friends_list[name])
        return 0;

    m_friends_list[name] = 1;
    return 1;
}

/*
 * Function name: remove_friendship
 * Description  : Remove a name from the players list of friends.
 * Arguments    : string name - the name of the player to remove.
 * Returns      : int 1 = removed, 0 = wasn't present.
 */
public int
remove_friendship(string name)
{
    name = lower_case(name);
    if (!m_friends_list[name])
        return 0;

    m_delkey(m_friends_list, name);
    return 1;
}

/*
 * Function name: set_whimpy
 * Description  : When a living gets too hurt, it might try to run from
 *                the combat it is engaged in. This will happen if the
 *                percentage of hitpoints left is lower than the whimpy
 *                level, ie: (100 * query_hp() / query_max_hp() < flag)
 *                This routine interfaces to the option as for players
 *                the whimpy status is an option.
 * Arguments    : int flag - the whimpy level. Must be in range 0-99.
 */
public void
set_whimpy(int flag)
{
    set_option(OPT_WHIMPY, flag);
}

/*
 * Function name:   set_wiz_unmet
 * Description:     Marks if the wizard wants to see all as met or unmet
 * Arguments:       flag: 1 if see as unmet, 0 if see as met, 2 see npcs as unmet.
 * Returns:         The new state
 */
public int
set_wiz_unmet(int flag)
{
    wiz_unmet = flag;
    return wiz_unmet;
}

/*
 * Function name:   query_wiz_unmet
 * Description:     Returns if the wizard wants to see all as met or unmet
 * Returns:         The current state
 */
public int
query_wiz_unmet()
{
    return wiz_unmet;
}
