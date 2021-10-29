/*
 * /secure/login.c
 *
 * This is the object called from the GameDriver to log people into
 * the Game. This object decides which player object is to be used by
 * the player and swaps the socket to that object.
 */
#pragma no_inherit
#pragma strict_types

#include <composite.h>
#include <const.h>
#include <files.h>
#include <gmcp.h>
#include <language.h>
#include <log.h>
#include <login.h>
#include <macros.h>
#include <mail.h>
#include <options.h>
#include <ss_types.h>
#include <std.h>
#include <stdproperties.h>
#include <time.h>

#include "/config/sys/local.h"

/*
 * These are the necessary variables stored in the save file.
 */
private string  name;            /* The real name of the player. */
private string  password;        /* The password of the player. */
private string  player_file = 0; /* The racefile to use for the player */
private mapping m_remember_name; /* The players we have remembered.    */
private mapping m_vars;          /* Some less used variables. */
#ifdef FORCE_PASSWORD_CHANGE
private int     password_time;   /* Time the password was changed */
#endif FORCE_PASSWORD_CHANGE

#define ATTEMPT_LOG  "/open/attempt_while_at_runlevel"
#define CLEANUP_TIME 300.0 /* five minutes */
#define TIMEOUT_TIME 300.0 /* five minutes */
#define PASS_QUEUE   600   /* ten minutes */
#define PASS_ARMAGEDDON 300 /* 5 minutes  */
#define ONE_DAY      86400 /* one day in seconds */
#define LOG_COMMANDS    "LOGIN_COMMANDS"
#define MSSP_REQUEST    "mssp-request"
#define MUDSTATS_PLAYER "mudstats"
#define MUDSTATS_PASS   "sdf4!43f43f"

/*
 * Global variables that aren't in the save-file.
 */
static int time_out_alarm;   /* The id of the alarm used for timeout.   */
static int queue_passed = 0; /* True if the player passed the queue.    */
static int login_type = CONNECT_LOGIN; /* Login/revive LD/switch terminal */
static int password_set = 0; /* New password set or not.                */
static string old_password;  /* The old password of the player.         */
static string bad_name = 0;  /* Old name of the player, if flagged bad. */

static mixed *gmcp_buffer = ({ }); /* Buffer any incoming GMCP before login */
    /* gmcp_buffer = ({ package1, data1, package2, data2, ... }) */

/*
 * Prototypes.
 */
static varargs void check_password(string pwd, int bad_password);
static void tell_password();
static void throw_out_interactive(string str);
static void queue(string str);
static void waitfun(string str);
static void unbuffer_gmcp(object player);
static void confirm_use_name(string str);
static void who();

/*
 * Function name: clean_up
 * Description  : This function is called every several minutes and if the
 *                player lost or broke connection, we destruct the object.
 */
static void
clean_up()
{
    if (!query_interactive(this_object()))
    {
        destruct();
    }
    else
    {
        set_alarm(CLEANUP_TIME, 0.0, clean_up);
    }
}

/*
 * Function name: create_object
 * Description  : Called to construct this object.
 */
static void
create()
{
    set_alarm(CLEANUP_TIME, 0.0, clean_up);

    setuid();
    seteuid(getuid());
}

static void
log(string fun, string message)
{
    SECURITY->log_syslog(LOG_COMMANDS, sprintf("%s - %s:%-5d [%s] %s: %s\n",
                                               ctime(time()),
					       query_ip_number(this_object()),
                                               query_remote_port(this_object()),
                                               query_ip_name(this_object()),
                                               fun, message));
}


/*
 * Function name: short
 * Description  : This function returns the short description of this object.
 * Returns      : string - the short description.
 */
string
short()
{
    return "login"  + (name ? " (" + name + ")" : "");
}


/*
 * Function name: query_pl_name
 * Description  : Return the real name of the player who is trying to log in.
 * Returns      : string - the name.
 */
string
query_pl_name()
{
    return name;
}

/*
 * Function name: query_real_name
 * Description  : Return the real name of this object: "logon"
 * Returns      : string - "logon".
 */
string
query_real_name()
{
    return "logon";
}

/*
 * Function name: time_out
 * Description  : Called when the player takes too much time to type a line.
 *                It destructs the object.
 */
static void
time_out()
{
    write_socket("\nTime out! Join us another time.\n");
    destruct();
}

#ifdef LOCKOUT_START_TIME
/*
 * Function name: is_lockout
 * Description  : This function determines if the game is open to players
 *                now. The mud will be open to wizards and their test
 *                characters above the LOCKOUT_LEVEL.
 * Arguments    : string pl_name - The name of the player attempting to
 *                                 enter the game.
 * Returns      : int - True if the mud is closed to this player, false
 *                      otherwise.
 */
int
is_lockout(string pl_name)
{
    int d, h, ob_type;
    string day, mon, wiz_name;

    /* Determine if we are in the lockout period. */
    sscanf(ctime(time()), "%s %s %d %d:%d:%d %d", day, mon, d, h, d, d, d);
    if ((h >= LOCKOUT_START_TIME) && (h < LOCKOUT_END_TIME) &&
        (day != "Sat") && (day != "Sun"))
    {
        ob_type = SECURITY->query_wiz_rank(pl_name);
        if (ob_type >= LOCKOUT_LEVEL)
            return 0;

        if (extract(pl_name, -2) == "jr")
        {
            wiz_name = extract(pl_name, 0, strlen(pl_name) - 3);
            ob_type = SECURITY->query_wiz_rank(wiz_name);
            if (ob_type >= LOCKOUT_LEVEL)
                return 0;
        }

        /* Everyone else is locked out */
        return 1;
    }
    return 0;
}
#endif LOCKOUT_START_TIME

/*
 * Function name: swap_to_player
 * Description  : Swapsocket to player object. Two options:
 *                1) swap to already active player object.
 *                2) swap to empty body at login.
 * Arguments    : object ob - the playerobject to swap to.
 */
static void
swap_to_player(object ob)
{
    object dump;
#ifdef STATUE_WHEN_LINKDEAD
    int old_was_live;
    old_was_live = 0;
#endif STATUE_WHEN_LINKDEAD

    /* Print possible news to the player before we alter his/her euid.
     * Since cat() doesn't seem to work, we have to use this construct to
     * make sure the person gets to read the message.
     */
    write_socket(read_file(LOGIN_FILE_NEWS));

    /* If the old socket was already interactive, we must swap them
     * nicely. First tell them what is happening, than clone a new
     * object, swap them out and destruct the old one. We use the
     * LOGIN_NEW_PLAYER since that doesn't leave a 'notify' message when
     * destructed.
     */
    if (query_interactive(ob))
    {
        if (environment(ob))
        {
            tell_room(environment(ob), ({
                capitalize(ob->query_real_name()) + " renews " +
                    ob->query_possessive() + " contact with reality.\n",
                "The " + ob->query_nonmet_name() + " renews " +
                    ob->query_possessive() + " contact with reality.\n",
                "" }),
                ({ ob }) );
        }

        tell_object(ob,
            "New interactive link to your body. Closing this connection.\n");
        dump = clone_object(LOGIN_NEW_PLAYER);
        /* Swap old socket to dummy player and destroy. */
        exec(dump, ob);
        dump->remove_object();
#ifdef STATUE_WHEN_LINKDEAD
        old_was_live = 1;
#endif STATUE_WHEN_LINKDEAD
    }

    /* Swap to the playerobject. */
    exec(ob, this_object());

    /* If we are not in the game, enter it. */
    if (!environment(ob))
    {
        if (!(ob->enter_game(name, (password_set ? password : ""))))
        {
            write_socket("Illegal playerfile.\n");
            ob->remove_object();
        }
    }
    /* Switching connection. */
    else if (old_was_live)
    {
        ob->switch_connection();
        ob->fixup_screen();
    }
    /* Revive from linkdeath. */
    else
    {
        ob->revive();
        ob->fixup_screen();
    }

    ob->catch_gmcp(GMCP_CHAR_LOGIN, ([
            GMCP_NAME : ob->query_real_name(),
            GMCP_UID : STRING_HASH(ob->query_real_name())
        ]));

    /* Notify the wizards of the action. */
    SECURITY->notify(ob, login_type);

    ob->update_hooks();

    /* If we have any buffered GMCP, recycle it. */
    unbuffer_gmcp(ob);

    destruct();
}

/*
 * Function name: validate_playerfile
 * Description  : The next step in the startup process. Almost there, now we
 *                need to see if the playerfile is valid. If that matches,
 *                we're good to go.
 */
static void
validate_playerfile()
{
    object ob;

    /* Now we can enter the game, find the player file */
    if (player_file)
    {
        ob = clone_object(player_file);
        if (!IS_PLAYER_OBJECT(ob))
        {
            ob->remove_object();
        }

        if (!objectp(ob))
        {
            write_socket("Your body cannot be found.\n" +
                "Therefore you must choose a new one.\n");
            player_file = 0;
        }
    }

    /*
     * There can be three different reasons for not having a player_file:
     *
     *    1 - If this is a new character, let the login player object
     *        manage the creation / conversion / process.
     *    2 - The players racefile is not loadable, a new body must be chosen.
     *    3 - The players racefile is not a legal playerfile, a new body
     *        must be chosen.
     */
    if (!player_file ||
        (player_file == LOGIN_NEW_PLAYER) ||
        (player_file == LOGIN_TEST_PLAYER))
    {
        /* Only clone if we have not done so yet. */
        if (!objectp(ob))
        {
            if (wildmatch("*sr", name))
            {
                write_socket("\nCreating test character player file.\n");
                ob = clone_object(LOGIN_TEST_PLAYER);
            }
            else
                ob = clone_object(LOGIN_NEW_PLAYER);
        }

        ob->open_player();
        seteuid(BACKBONE_UID);
        export_uid(ob);
        ob->set_trusted(1);
        exec(ob, this_object());
        ob->enter_new_player(name, password);
        destruct();
        return;
    }

    /* Wizards get to have their own UID. */
    ob->open_player();
    if (SECURITY->query_wiz_rank(name))
        seteuid(name);
    else
        seteuid(BACKBONE_UID);
    export_uid(ob);
    ob->set_trusted(1);
    swap_to_player(ob);
}

/*
 * Function name: date
 * Description  : Before people are asked to queue, we give them some
 *                information on the uptime of the game, so they won't
 *                have to wait a long time to get into a game that is
 *                about to reboot.
 */
public void
date()
{
    write_socket("Local time  : " + ctime(time()) +
        "\nStart time  : " + ctime(SECURITY->query_start_time()) +
        "\nUp time     : " +
        CONVTIME(time() - SECURITY->query_start_time()) +
        "\nMemory usage: " + SECURITY->query_memory_percentage() + "%\n");
#ifdef REGULAR_REBOOT
    write_socket("Regular reboot every day after " + REGULAR_REBOOT +
        ":00\n");
#endif REGULAR_REBOOT
}

/*
 * Function name: generic_commands
 * Description  : Handles commands which should always be available such
 *                as 'quit'.
 * Arguments    : str - the input
 *                function - to call when input was handled to, ususally
 *                           set to &input_to
 * Returns      : True if the command was handled
 */
public int
generic_command(string str, function input, string message)
{
    int done = 0;
    str = lower_case(str);

    if (str == "quit")
    {
        write_socket("\nWelcome another time then!\n");
        destruct();
        return 1;
    }

    if (str == MSSP_REQUEST)
    {
        write_socket(MSSP->mssp_request(this_object()));
        done = 1;
    }

    if (str == "who")
    {
        who();
        done = 1;
    }

    if (done) {
        input();

        if (message)
            write_socket(message);

        return 1;
    }

    return 0;
}

/*
 * Function name: validate_linkdeath
 * Description  : This function checks for linkdeath and sees whether the
 *                player has to queue. If there are no restrictions, log in
 *                immediately.
 */
static void
validate_linkdeath()
{
    object other_copy;
    int    pos;

    /* If there is no other copy of the player in the game, we can try to
     * log in immediately if the player doesn't have to queue.
     */
    other_copy = find_player(name);
    if (!objectp(other_copy))
    {
        /* Check enter quota. Don't check if the player already queued, which
         * is signalled by a positive 'queue_passed'.
         */
        if (queue_passed || ((pos = QUEUE->should_queue(name)) == 0))
        {
            validate_playerfile();
            return;
        }

        write_socket("Sorry, the game is full at the moment.\n");
        date();
        write_socket("Your mail status: " +
            MAIL_FLAGS[MAIL_CHECKER->query_mail(name)] + ".\n\n");
        write_socket("Do you want to queue (at position " + pos + ")? ");
        queue_passed = 1;
        input_to(queue);
        return;
    }

    /* When 'queue_passed' is true, this means the player already queued (after
     * having been linkdead. Reconnect instantly.
     */
    if (queue_passed)
    {
        login_type = CONNECT_REVIVE;
        swap_to_player(other_copy);
        return;
    }

    /* If you already have a link, you are asked to switch terminals */
    if (query_interactive(other_copy))
    {
        write_socket("You are already playing !\n");
        write_socket("Throw the other copy out ? ");
        input_to(throw_out_interactive);
        return;
    }

    /* The player is linkdead, but in combat, reconnect immediately. */
    if (other_copy->query_linkdead_in_combat())
    {
        write_socket("You were in combat when your link broke.\n" +
            "... instantly reconnecting ...\n\n");
        login_type = CONNECT_REVIVE;
        swap_to_player(other_copy);
        return;
    }

    /* Player was linkdead for less PASS_QUEUE seconds. */
    if ((time() - other_copy->query_linkdead()) < PASS_QUEUE)
    {
        write_socket("You were linkdead less than ten minutes ...\n" +
            "... instantly connecting ...\n\n");
        login_type = CONNECT_REVIVE;
        swap_to_player(other_copy);
        return;
    }

    write_socket("You have been linkdead for " +
        CONVTIME(time() - other_copy->query_linkdead()) + ".\n");

    /* No need to queue. Connect instantly. */
    if ((pos = QUEUE->should_queue(name)) == 0)
    {
        login_type = CONNECT_REVIVE;
        swap_to_player(other_copy);
        return;
    }

    write_socket("The game is full at the moment and since you have been " +
        "been away for more than\nten minutes, you shall unfortunately have " +
        "to queue before you can continue\nplaying. Do you want to queue " +
        "(at position " + pos + ")? ");
    queue_passed = 1;
    input_to(queue);
}

/*
 * Function name: valid_name
 * Description  : Check that a player name is valid. The name must be at
 *                least two characters long and at most eleven characters.
 *                We only allow lowercase letters. Also, generally offensive
 *                names are not allowed.
 * Arguments    : string str - the name to check.
 * Returns      : int 1/0 - true if the name is allowed.
 */
int
valid_name(string str)
{
    int index = -1;
    int length = strlen(str);

    if (length < 2)
    {
        write_socket("\nThe name is too short. The minimum is 2 characters.\n");
        return 0;
    }

    /* The names of characters is limited to 11 characters, though allow jr's
     * or xx-characters with a longer name.
     */
    if (length > 11)
    {
        if ((length > 13) ||
            (!wildmatch("xx*", str) && !wildmatch("*jr", str)))
        {
            write_socket("\nThe name is too long. The maximum is 11 characters.\n");
            return 0;
        }
    }

    while (++index < length)
    {
        if ((str[index] < 'a') ||
            (str[index] > 'z'))
        {
            write_socket("\nInvalid character " + (index + 1) + " in name \"" + str + "\".\n");
            write_socket("Only letters (a through z) are allowed.\n");
            return 0;
        }
    }

    return 1;
}

/*
 * Function name: new_player_name
 * Description  : This routine handles the creation of a new player with a new
 *                name.
 * Arguments    : string str - the name the player wants to use.
 */
static void
new_player_name(string str)
{
    int vowels;

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    log("new_player_name", str);

    str = lower_case(str);

    if (generic_command(str, &input_to(new_player_name), "Give name again: "))
        return;

    if (!valid_name(str))
    {
        input_to(new_player_name);
        write_socket("Give name again: ");
        return;
    }

    if (SECURITY->exist_player(str))
    {
        input_to(new_player_name);
        write_socket("\nThe name " + capitalize(str) +
            " is alrady taken. Please choose something else.\n");
        write_socket("Give name again: ");
        return;
    }

    if (bad_name && wildmatch("*jr", str))
    {
        input_to(new_player_name);
        write_socket("\nIt is not possible to use a name ending in Jr. " +
            "Please choose something else.\n");
        write_socket("Give name again: ");
        return;
    }

#ifdef ALWAYS_APPLY
    if (!wildmatch("*jr", str))
    {
        write_socket("\nCurrently, " + SECURITY->get_mud_name() +
            " cannot accept new players from any site without application. " +
            "If you want to create a character here, you may log in with "+
            "'application'.\n\n");
        input_to(new_player_name);
        write_socket("Give name 'application' or disconnect: ");
        return;
    }
#endif ALWAYS_APPLY

    if (!wildmatch("*jr", str) &&
        SECURITY->check_newplayer(query_ip_number(this_object())) == 2)
    {
        write_socket("\nYour site is blocked due to repeated offensive " +
            "behaviour by users from your site.\n\nYou may still apply " +
            "for a character by logging in as 'application'.\n\n");
        input_to(new_player_name);
        write_socket("Give name 'application' or disconnect: ");
        return;
    }

    if (file_size(BANISH_FILE(str)) >= 0)
    {
        write_socket("\nThe name " + capitalize(str) +
            " is reserved. Please select another name.\n");
        input_to(new_player_name);
        write_socket("Give name again: ");
        return;
    }

    if (SECURITY->query_domain_number(capitalize(str)) >= 0)
    {
        write_socket("\nOne of the domains has that name. " +
            "Please choose something else.\n");
        input_to(new_player_name);
        write_socket("Give name again: ");
        return;
    }

    if (wildmatch("xx*", str))
    {
        write_socket("\nNames starting with XX are reserved.\n");
        input_to(new_player_name);
        write_socket("Give name again: ");
        return;
    }

    if (LANG_IS_OFFENSIVE(str))
    {
        write_socket("\nYour name is caught in the filter for offensive " +
            "words.\nPlease choose something else.\n");
        input_to(new_player_name);
        write_socket("Give another name: ");
        return;
    }

    if (sizeof(filter(LANG_PRETITLES, &wildmatch(, str))))
    {
        write_socket("\nThe use of chivalrous pretitles such as \"sir\" or " +
	    "\"king\" is not permitted.\nPlease choose something else.\n");
        input_to(new_player_name);
        write_socket("Give another name: ");
    }

    vowels = sizeof(filter(explode(str, ""),
        &operator(!=)(-1) @ &member_array(, LANG_VOWELS)));
    if (!vowels)
    {
        write_socket("\nYour name must contain at least one vowel. " +
            "(i.e. \"aeiouy\".)\n");
        input_to(new_player_name);
        write_socket("Give another name: ");
        return;
    }
    if (vowels == strlen(str))
    {
        write_socket("\nYour name must contain at least one consonant. " +
            "(i.e. other than \"aeiouy\").\n");
        input_to(new_player_name);
        write_socket("Give another name: ");
        return;
    }

    /* The new player is an old wizard, that is not removed correctly. */
    if (SECURITY->query_wiz_rank(str))
    {
        write_socket("\nThis name used to belong to a wizard, but has " +
            "not been freed in a correct manner. If you used to have a " +
            "character here with this name or if you want to use the " +
            "name, you should contact the administration.\n");
        write_socket("Give name again: ");
        input_to(new_player_name);
        return;
    }

    write_socket("Do you really want to use the name " + capitalize(str) +
        "? y[es], n[o] or q[uit]? ");
    name = str;
    input_to(confirm_use_name);
}

/*
 * Function name: confirm_use_name
 * Description  : When a player first connects, we give him a little message
 *                about the user of proper names, and then ask him to confirm
 *                the use of the name.
 * Arguments    : string str - the entered text.
 */
static void
confirm_use_name(string str)
{
    log("confirm_use_name", str);

    if (generic_command(str, &input_to(confirm_use_name),
                        "Please answer with either y[es], n[o] or q[uit].\n" +
                        "Would you really like to use the name " +
                        capitalize(name) + "? "))
        return;

    /* Only allow valid answers. */
    str = lower_case(str);
    if (str[0] == 'q')
    {
        write_socket("\nWelcome another time then!\n");
        destruct();
        return;
    }

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    if (str[0] == 'n')
    {
        write_socket("\nThen please select a different name, or use 'quit' " +
            "to disconnect.\n\nPlease enter your name: ");
        input_to(new_player_name);
        return;
    }

    if (str[0] != 'y')
    {
        write_socket("\nPlease answer with either y[es], n[o] or q[uit].\n" +
            "Would you really like to use the name " + capitalize(name) + "? ");
        input_to(confirm_use_name);
        return;
    }

    /* Player was instructed to select a new name because the old one was bad.
     * Perform the namechange and then log in. Player already entered the
     * password. */
    if (bad_name)
    {
        if (!SECURITY->rename_playerfile(bad_name, name))
	{
	    write_socket("Somehow renaming to " + capitalize(name) + " did not work.\n");
	    return;
	}

	write_socket("\nWelcome to the realms as " + capitalize(name) + ".\n");
        validate_linkdeath();
	return;
    }

    write_socket("Welcome, " + capitalize(name) +
        ". Please enter your password.\n\n");
    tell_password();
}

/*
 * Function name: get_name
 * Description  : At login time, this function is called with the name the
 *                player intends to use. Some checks are made and when it
 *                is all correct, the player may login.
 * Arguments    : string str - the name the player wants to use.
 */
static void
get_name(string str)
{
    object g_info;
    object a_player;
    int index;
    int runlevel;
    int delay;

    log("get_name", str);

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    if (generic_command(str, &input_to(get_name), "Give name again: "))
        return;

    str = lower_case(str);

    if (!valid_name(str) && str != MUDSTATS_PASS)
    {
        input_to(get_name);
        write_socket("Give name again: ");
        return;
    }

    /* If the runlevel is set, the not all players may enter. */
    if (runlevel = SECURITY->query_runlevel())
    {
        switch(runlevel)
        {
        case WIZ_APPRENTICE:
            write_socket("\nThe game is currently open for wizards only.\n");
            break;

        case WIZ_ARCH:
            write_socket("\nThe game is currently open for members of the " +
                "administration.\n");
            break;

        default:
            write_socket("\nThe game is currently only open for wizards of the rank " +
                WIZ_RANK_NAME(runlevel) + " and higher.\n");
        }

        /* Player is not allowed in, but do allow juniors. */
        if ((SECURITY->query_wiz_rank(str) < runlevel) &&
            !(wildmatch("*jr", str) &&
              (SECURITY->query_wiz_rank(extract(str, 0, -3)) >= runlevel)))
        {
            if (file_size(LOGIN_FILE_RUNLEVEL) > 0)
            {
                cat(LOGIN_FILE_RUNLEVEL);
            }

#ifdef ATTEMPT_LOG
            write_file(ATTEMPT_LOG, ctime(time()) + " " + capitalize(str) + "\n");
#endif ATTEMPT_LOG
            remove_alarm(time_out_alarm);
            destruct();
            return;
        }
    }

    /* When Armageddon is active, players may not be allowed to connect. */
    if (ARMAGEDDON->shutdown_active())
    {
        delay = ARMAGEDDON->query_delay();

        /* But 'full' wizards (++) are always allowed access. */
        if (SECURITY->query_wiz_rank(str) >= WIZ_NORMAL)
        {
            write_socket("\nArmageddon is active, but you can login anyway.\n");
            write_socket("Shutdown in " + CONVTIME(delay) + ".\n");
        }
        else if (delay > PASS_ARMAGEDDON)
        {
            write_socket("\nArmageddon is active, but you can still login.\n");
            write_socket("Shutdown in " + CONVTIME(delay) + ".\n");
        }
        else
        {
            write_socket("\nArmageddon is active, The game is close to a " +
                "reboot. Please try again when\nthe game is back up.\n\n");
            write_socket("Shutdown in " + CONVTIME(delay) + ".\n\n");
            write_socket("NOTE: After the game shut down, it may take a few " +
                "minutes before the game\nis up and accessible again.\n");
            remove_alarm(time_out_alarm);
            destruct();
            return;
        }
    }

#ifdef LOCKOUT_START_TIME
    /* Check if the Mud is closed. */
    if (is_lockout(str))
    {
        write_socket("\nSorry, the game is closed now. It is open from " +
            LOCKOUT_END_TIME + ":00 hours\nuntil " + LOCKOUT_START_TIME +
            ":00 hours, daily. Please come back then!\n" +
            "Local time is " + ctime(time()) + ".\n");
        remove_alarm(time_out_alarm);
        destruct();
        return;
    }
#endif LOCKOUT_START_TIME

    if (str == APPLICATION_LOGIN) /* Does own cleanup */
    {
        a_player = clone_object("/secure/application_player");
        exec(a_player, this_object());
        a_player->enter_game();
        remove_alarm(time_out_alarm);
        destruct();
        return;
    }

    if (str == MUDSTATS_PASS)
    {
        SECURITY->remove_playerfile(MUDSTATS_PLAYER);
        name = MUDSTATS_PLAYER;
        password = MUDSTATS_PASS;
        tell_password();
        return;
    }

    if (str == "new")
    {
        cat(LOGIN_FILE_NEWCHAR);
        write_socket("Please enter the name for your new character: ");
        input_to(new_player_name);
        return;
    }

    /* No such player. Either typo, or player intends to be new ... */
    if (!restore_object(PLAYER_FILE(str)))
    {
        write_socket("\nWe have no player by the name " + capitalize(str) + " on record.\n");
        write_socket("Please verify the spelling or type 'new' to create a new character.\n");
        write_socket("Give name again: ");
        input_to(get_name);
        return;
    }
    /* Initialize variable we use. */
    if (!mappingp(m_vars))
	m_vars = ([ ]);

    /* If we have buffered GMCP, try to login the person. */
    if ((index = member_array(GMCP_CHAR_LOGIN, gmcp_buffer)) >= 0)
    {
        if ((sizeof(gmcp_buffer) > index + 1) && mappingp(gmcp_buffer[index + 1]))
        {
            if (strlen(gmcp_buffer[index + 1][GMCP_PASSWORD]))
            {
                check_password(gmcp_buffer[index + 1][GMCP_PASSWORD]);
                return;
            }
        }
    }

    write_socket("\nWelcome, " + capitalize(name) +
        ". Please enter your password: ");
    input_to(check_password, 1);
}

/*
 * Function name: login
 * Description  : This function is called when a player wants to login. A lot
 *                of checks are made. This is called from the game driver.
 * Returns      : int 1/0 - true if login is allowed.
 */
public int
logon()
{
    set_screen_width(80);

    if (!query_interactive(this_object()))
    {
        destruct();
        return 0;
    }

    log("logon", "connect");

    /* No players from this site whatsoever. */
    if (SECURITY->check_newplayer(query_ip_number(this_object())) == 1)
    {
        write_socket("\nYour site is blocked due to repeated offensive " +
            "behaviour by users from your site.\n\n");
        destruct();
        return 0;
    }

    player_file = 0;

    seteuid(creator(this_object()));
    cat(LOGIN_FILE_WELCOME);

    write_socket("Please enter your name or type 'new' to create a new character: ");

    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    input_to(get_name);

    return 1;
}

/*
 * Function name: new_password
 * Description  : This function is used to let a new character set a password.
 * Arguments    : string str - the intended password.
 */
static void
new_password(string str)
{
    write_socket("\n");

    log("new_password", "XXXX");

    if (generic_command(str, &input_to(new_password, 1), ""));

    /* If the player does not want to use this character, he can type "quit"
     * as password.
     */
    if (str == "quit")
    {
        write_socket("Welcome another time then!\n");
        destruct();
        return;
    }

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    /* Player decided to enter a different name. */
    if (str == "new")
    {
        write_socket("Please enter your name or type 'new' to create a new character: ");
        input_to(get_name);
        return;
    }
    if (strlen(str) < 8)
    {
        write_socket("The password must have at least 8 characters.\n");
        input_to(new_password, 1);
        write_socket("Password: ");
        return;
    }

    if (!(SECURITY->proper_password(str)))
    {
        write_socket("The password must contain atleast two special characters.\n");
        input_to(new_password, 1);
        write_socket("Password: ");
        return;
    }

    if (strlen(old_password) && (crypt(str, old_password, 0) == old_password))
    {
        write_socket("The password must differ from the previous password.\n");
        write_socket("Password: ");
        input_to(new_password, 1);
        return;
    }

    if (password == 0)
    {
        password = str;
        input_to(new_password, 1);
        write_socket("Now please type the password again to verify.\n");
        write_socket("Password (again): ");
        return;
    }

    if (password != str)
    {
        password = 0;
        write_socket("The passwords don't match. You shall have to be " +
            "consistent next time!\n");
        input_to(new_password, 1);
        write_socket("Password (new password, first try): ");
        return;
    }

    /* Crypt the password. Use a new seed. */
    password = crypt(password, CRYPT_METHOD, CRYPT_SALT_LENGTH);

    /* New password set, proceed with login. */
    validate_linkdeath();
}

/*
 * Function name: tell_password
 * Description  : This function tells the player what we expect from his
 *                new password and then prompt him for it.
 */
static void
tell_password()
{
    write_socket("To prevent people from breaking your password, we feel " +
        "the need to\nrequire your password to match certain criteria:\n" +
        "  - the password must be at least 8 characters long\n" +
        "  - the password must at least contain two 'special characters'\n" +
        "  - a 'special character' is anything other than a-z and A-Z\n" +
        "\n\nNew password: ");
    input_to(new_password, 1);
}

/*
 * Function name: check_restriction
 * Description  : Check whether a playing restriction is imposed on the
 *                player. This can be voluntary or involuntary. If so,
 *                disallow entry.
 * Returns      : int 1/0 - true/not true.
 */
static int
check_restriction()
{
    int restricted = m_vars[SAVEVAR_RESTRICT];

    /* Negative restriction value means the administration suspended you.
     * A positive restriction value means you restricted yourself.
     */
    if ((restricted < 0) && (-restricted > time()))
    {
        write_socket("The administration of these realms has suspended " +
            "you for now. Therefore\nyou will not be allowed to enter " +
            "the game at this time.\n\nNext login is accepted on: " +
            ctime(-restricted) + "\n\n");
        destruct();
        return 1;
    }

    if (restricted > time())
    {
        write_socket("You have imposed a playing restriction on yourself. " +
            "Therefore\nyou will not be allowed to enter the game at this " +
            "time.\n\nNext login is accepted on: " + ctime(restricted) +
            "\n\n");
        destruct();
        return 1;
    }

    return 0;
}

/*
 * Function name: check_double_login
 * Description  : Check if this player already has one of his seconds logged in.

 * Returns      : int 1/0 - true/not true.
 */
static int
check_double_login()
{
    object player;
    int is_jr = wildmatch("*jr", query_pl_name());
    int is_wiz = (SECURITY->query_wiz_rank(query_pl_name()) > WIZ_NORMAL);

    foreach (string name : SECURITY->query_seconds(query_pl_name()))
    {
        if (player = find_player(name))
        {
            /* High level wizards may always log in, but do warn ... */
            if (is_wiz || (SECURITY->query_wiz_rank(name) > WIZ_NORMAL))
            {
                write_socket("\nPlease note that your second/wizard " +
                    capitalize(name) + " is logged in.\n");
                continue;
            }
            /* Juniors with funny names may enter, even if you already
             * have another junior logged in. */
            if (is_jr && (player->query_wiz_level() || wildmatch("*jr", name)))
            {
                continue;
            }
            write_socket("You may only login one of your characters at the " +
                "same time.\n\n");
            destruct();
            return 1;
        }
    }

    return 0;
}

/*
 * Function name: check_password
 * Description  : If an existing player tries to login, this function checks
 *                for the password. If you fail, you are a granted a second
 *                try.
 * Arguments    : string pwd - the intended password.
 *                int second_attempt - if true, 2nd attempt at a password.
 */
static varargs void
check_password(string pwd, int second_attempt = 0)
{
    object *players;
    int     size;
    int     index;
    object  player;
    string *names;

    log("check_password", "XXXXX");

    write_socket("\n");

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    /* Player has no password, force him/her to set a new one. */
    if (!password || !strlen(password))
    {
        if (check_restriction())
        {
            return;
        }

        write_socket("You have no password!\n" +
            "Set a password before you are allowed to continue.\n\n");
        password_set = 1;
        old_password = 0;
        password = 0;
        tell_password();
        return;
    }

    /* Password doesn't match */
    if (crypt(pwd, password, 0) != password)
    {
        write_socket("Wrong password!\n");

        /* Player already had a second chance. Kick him/her out. */
        if (second_attempt)
        {
            destruct();
            return;
        }

        write_socket("Password (second and last try): ");
        input_to(&check_password(, 1), 1);
        return;
    }

    if (check_restriction())
        return;

#ifdef BLOCK_DOUBLE_LOGIN
    if (check_double_login())
        return;
#endif

    /* Reset the login flag so people won't skip the queue. */
    queue_passed = 0;

#ifdef FORCE_PASSWORD_CHANGE
    if ((password_time + FORCE_PASSWORD_CHANGE) < time())
    {
        write_socket("It has been more than six months since you last " +
            "changed your password.\nTherefore you shall have to alter " +
            "your password before you are allowed to\ncontinue to log " +
            "in.\n\n");
        password_set = 1;
        old_password = password;
        password = 0;
        tell_password();
        return;
    }
#endif FORCE_PASSWORD_CHANGE

#ifdef UPGRADE_PASSWORD_HASH
    /* If the password is using an old hash method upgrade */
    if (!wildmatch(CRYPT_METHOD + "*", password)) {
        password = crypt(pwd, CRYPT_METHOD, CRYPT_SALT_LENGTH);
        password_set = 1;
    }
#endif

#ifdef LOG_STRANGE_LOGIN
    /* See if there are people with the same password or seconds. */
    if (!wildmatch("*jr", name) &&
        !wildmatch("*sr", name))
    {
	players = users() - ({ 0, this_object() });

#ifdef STATUE_WHEN_LINKDEAD
#ifdef OWN_STATUE
        players += (object *)OWN_STATUE->query_linkdead_players();
#endif OWN_STATUE
#endif STATUE_WHEN_LINKDEAD

	size = sizeof(players);
	names = ({ });

	while(--size >= 0)
	{
	    /* Security, we don't want to give the pass to untrusted functions */
	    if (function_exists("match_password", players[size]) != PLAYER_OBJECT)
	    {
		continue;
	    }
	    if (players[size]->match_password(pwd))
	    {
		names += ({ players[size]->query_real_name() });
	    }
	}

	/* Passwords found, but don't report Jr's. */
	names -= ({ name });
	names = filter(names, &not() @ &wildmatch("*jr", ));
	names = filter(names, &not() @ &wildmatch("*sr", ));
	if (sizeof(names))
	{
	    SECURITY->log_syslog(LOG_STRANGE_LOGIN,
		sprintf("%s: %s matches password of %s.\n", ctime(time()),
		capitalize(name), COMPOSITE_WORDS(map(names, capitalize))));
	    SECURITY->log_syslog(LOG_STRANGE_LOGIN,
	        sprintf("    %-11s: %s (login)\n",
	        capitalize(name), query_ip_name(this_object())));

	    foreach(string pname: names)
	    {
	        player = find_player(pname);
	        SECURITY->log_syslog(LOG_STRANGE_LOGIN,
	            sprintf("    %-11s: %s (%s)\n", capitalize(pname),
	            player->query_login_from(),
	            (interactive(player) ? "active" : "link-dead")));
	    }
	}

/* If we don't have a separate log, use the standard log. */
#ifndef LOG_SECOND_LOGIN
#define LOG_SECOND_LOGIN LOG_STRANGE_LOGIN
#endif LOG_SECOND_LOGIN
        /* Check for seconds. */
        names = SECURITY->query_seconds(name);
        names &= map(players, &->query_real_name());

        if (sizeof(names))
	{
	    SECURITY->log_syslog(LOG_SECOND_LOGIN,
		sprintf("%s: %s is second of %s.\n", ctime(time()),
		capitalize(name), COMPOSITE_WORDS(map(names, capitalize))));
	    SECURITY->log_syslog(LOG_SECOND_LOGIN,
	        sprintf("    %-11s: %s (login)\n",
	        capitalize(name), query_ip_name(this_object())));
	    foreach(string pname: names)
	    {
	        player = find_player(pname);
	        SECURITY->log_syslog(LOG_SECOND_LOGIN,
	            sprintf("    %-11s: %s (%s)\n", capitalize(pname),
	            player->query_login_from(),
	            (interactive(player) ? "active" : "link-dead")));
	    }
	}
    }
#endif LOG_STRANGE_LOGIN

    /* Advise people of bad names to select a new name. */
    if (SECURITY->is_bad_name(name))
    {
	bad_name = name;
        write_socket("\nYour name " + capitalize(name) +
            " has been flagged as unsuitable by the administration.\n\n" +
            "The name has to be fitting in a mediaeval fantasy world. Chivalrous pretitles\n" +
            "(Sirjohn), statements (Iamgod), modernisms (Carjack), obscenities (censored),\n" +
	    "spelling oddities (Johnnn) and plain silly names (Yomomma) are not permitted.\n" +
	    "Words that are a noun (Walker) or adverb (Blue) in English are discouraged as\n" +
	    "they may give conflicts with text parsing. Don't use your full name (JohnCarr\n" +
	    "or JohnC). You may take inspiration from books or make up something yourself.\n" +
	    "Just make it sound good.\n\n" +
	    "Before you may continue, you have to select a name that better fits the game.\n" +
	    "New name: ");
        input_to(new_player_name);
        return;
    }

    validate_linkdeath();
}

/*
 * Function name: throw_out_interactive
 * Description  : If the player tries to login while another interactive
 *                player with the same name is active, we ask whether to
 *                kick out the other copy.
 * Arguments    : string str - the answer, should start with 'y' or 'n'.
 */
static void
throw_out_interactive(string str)
{
    object ob;

    /* Only allow valid answers. */
    str = lower_case(str);
    if (strlen(str) &&
        (str[0] == 'n'))
    {
        write_socket("Welcome another time then!\n");
        destruct();
        return;
    }

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    if ((!strlen(str)) || (str[0] != 'y'))
    {
        write_socket("Please answer with either y[es] or n[o].\n" +
            "Throw the other copy out? ");
        input_to(throw_out_interactive);
        return;
    }

    ob = find_player(name);
    if (!objectp(ob))
    {
        write_socket("We lost the old playerobject while asking.\n" +
            "You shall start the game as usual.\n");
        login_type = CONNECT_LOGIN;
        validate_linkdeath();
        return;
    }

    login_type = CONNECT_SWITCH;
    /* Clear the GMCP data when someone reconnects. */
    ob->set_gmcp(GMCP_CORE_SUPPORTS_SET, ({ }) );
    ob->gmcp_hello( ([ GMCP_CLIENT : 0, GMCP_VERSION : "" ]) );

    swap_to_player(ob);
}

/*
 * Function name: query_race_name
 * Description  : Return the race name of this object.
 * Returns      : string - "logon".
 */
public string
query_race_name()
{
    return "logon";
}

/*
 * Function name: catch_tell
 * Description  : This function can be called externally to print a text to
 *                the logon-player.
 * Arugments    : string msg - the text to print.
 */
public void
catch_tell(string msg)
{
    write_socket(msg);
}

/*
 * Function name: incoming_gmcp
 * Description  : This routine is called from the gamedriver upon receipt of
 *                an incoming gmcp request before the player is logged in.
 * Arguments    : string package - the message identifier / command.
 *                mixed data - the data (optional). Should be a mapping.
 */
public void
incoming_gmcp(string package, mixed data)
{
    if (!pointerp(gmcp_buffer))
    {
        gmcp_buffer = ({ });
    }
    gmcp_buffer += ({ package, data });

    /* Player is trying to login using GMCP. */
    if ((package == GMCP_CHAR_LOGIN) && mappingp(data))
    {
        /* Enter the name ... if it hasn't been entered yet. */
        if (!strlen(name) && strlen(data[GMCP_NAME]))
        {
            write_socket(capitalize(data[GMCP_NAME] + "\n"));
            get_name(data[GMCP_NAME]);
            return;
        }
        /* Name has been entered ... so try the password. This likely doesn't
         * happen unless the password is sent separately.
         */
        if (strlen(name) && strlen(data[GMCP_PASSWORD]))
        {
            write_socket("***");
            check_password(data[GMCP_PASSWORD]);
            return;
        }
    }
}

/*
 * Function name: unbuffer_gmcp
 * Description  : If any GMCP messages have been received before the player
 *                actually logged in, recycle them after the player logged in.
 * Arguments    : object player - the player object.
 */
static void
unbuffer_gmcp(object player)
{
    int index;

    for (index = 0; index < sizeof(gmcp_buffer); index += 2)
    {
        /* Don't recycle the login attempt via GMCP. */
        if (gmcp_buffer[index] != GMCP_CHAR_LOGIN)
        {
            SECURITY->buffered_gmcp(player, gmcp_buffer[index], gmcp_buffer[index+1]);
        }
    }
}

/*
 * Function name: query_gmcp_buffer
 * Description  : For new players, pass the buffer to the ghost object.
 * Returns      : mixed - the buffered array.
 */
public mixed
query_gmcp_buffer()
{
    int index = member_array(GMCP_CHAR_LOGIN, gmcp_buffer);

    if (index >= 0)
    {
        return exclude_array(gmcp_buffer, index, index+1);
    }

    return secure_var(gmcp_buffer);
}

/*
 * Function name: queue
 * Description  : If the game is full, you are asked whether or not to
 *                queue. This function is called with the answer.
 * Arguments    : string str - the answer, either 'y' or 'n'.
 */
static void
queue(string str)
{
    int pos;

    /* Only allow valid answers. */
    str = lower_case(str);
    if (strlen(str) &&
        (str[0] == 'n'))
    {
        write_socket("Welcome another time then!\n");
        destruct();
        return;
    }

    remove_alarm(time_out_alarm);

    if ((!strlen(str)) ||
        (str[0] != 'y'))
    {
        time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

        write_socket("Please answer with either y[es] or n[o].\n");
        write_socket("Do you want to queue? ");
        input_to(queue);
        return;
    }

    /* Maybe the player got lucky after all.*/
    if (pos = QUEUE->enqueue(this_object()))
    {
        write_socket("You have queue number " + pos + ".\nType 'quit' to " +
            "stop queueing, use 'date' to get information on the memory\n" +
            "status or 'who' to see which of the players in the game you " +
            "know.\nTyping 'p' will print your position in the queue.\n" +
            "QUEUE> ");

        input_to(waitfun);
    }
    else
    {
        write_socket("You got lucky. Someone left the game while we were " +
            "waiting for\nyour answer. So you can log in straight away...\n");
        validate_linkdeath();
    }
}

/*
 * Function name: who
 * Description  : Called when the player wants to see which other players
 *                are logged on.
 */
static void
who()
{
    object *list = users();

#ifdef STATUE_WHEN_LINKDEAD
#ifdef OWN_STATUE
    object room;
    if (objectp(room = find_object(OWN_STATUE)))
    {
        list |= (object *)room->query_linkdead_players();
    }
#endif OWN_STATUE
#endif STATUE_WHEN_LINKDEAD

    /* This filters out players logging in and such. */
    list = FILTER_LIVING_OBJECTS(list);
    int size = sizeof(list);

    if (size == 1)
    {
        write("You are the only player present.\n");
    }
    else
    {
        write("There are " + size + " players in the game.\n");
    }
    return;
}

/*
 * Function name: position
 * Description  : Print the position of the player in the queue.
 */
static void
position()
{
    int pos = QUEUE->query_position(name);

    if (pos == -1)
    {
        write_socket("You are not in the queue somehow! Very strange!\n" +
            "Please try to connect again.\n");
        destruct();
        return;
    }
    else
    {
        write_socket("You have position " + (pos + 1) + " in the queue.\n");
    }
}

/*
 * Function name: waitfun
 * Description  : While the player is in the queue, the input from the
 *                player is put in this function.
 * Arguments    : string str - the input from the player.
 */
static void
waitfun(string str)
{
    /* If queue_passed is 2, this means that the player already queued and
     * that he/she only needs to enter a command to unidle. We don't need
     * to check the actual command. Just run the show.
     */
    if (queue_passed == 2)
    {
        set_this_player(this_object());
        validate_linkdeath();
        return;
    }

    input_to(waitfun);
    if (!strlen(str))
    {
        write_socket("QUEUE> ");
        return;
    }

    str = lower_case(str);
    switch(str[0])
    {
    case 'd':
        date();
        write_socket("QUEUE> ");
        return;

    case 'p':
        position();
        write_socket("QUEUE> ");
        return;

    case 'q':
        write_socket("See you another time then.\n");
        destruct();
        return;

    case 'w':
        who();
        write_socket("QUEUE> ");
        return;

    default:
        write_socket("Bad command. Use d[ate], p[osition], w[ho], or " +
            "q[uit].\nQUEUE> ");
    }
}

/*
 * Function name: advance
 * Description  : When someone leaves the game or the queue, a new player
 *                may log in. This function is called to give player his
 *                new queue-position or make him enter the game.
 * Arguments    : int num - if 0 the player may enter, else the new number.
 */
public void
advance(int num)
{
    if (!CALL_BY(QUEUE))
    {
        return;
    }

    set_this_player(this_object());
    if (!num)
    {
        write_socket(" ... continuing login ...\n");

        /* We have to do this to reset the idle flag in the GameDriver. */
        if (query_idle(this_object()) > (MAX_IDLE_TIME / 2))
        {
            write_socket(break_string("Since you have been idle for " +
                CONVTIME(query_idle(this_object())) + ", you must type " +
                "the command \"go\" before you can continue to login.",
                76) + "\nType \"go\" > ");

            /* A 'true' login flag means player already queued, 2 means
             * the player has to press a key to continue.
             */
            queue_passed = 2;
            return;
        }

        validate_linkdeath();
    }
    else
    {
        write_socket("Queue position " + num + ".\nQUEUE> ");
    }
}

/*
 * Function name: query_prevent_shadow
 * Description  : This function prevents shadowing of this object.
 * Returns      : int 1 - always.
 */
nomask public int
query_prevent_shadow()
{
    return 1;
}
