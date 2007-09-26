/*
 * /secure/login.c
 *
 * This is the object called from the GameDriver to log people into
 * the Game. This object decides which player object is to be used by
 * the player and swaps the socket to that object.
 */
#pragma no_inherit
#pragma save_binary
#pragma strict_types

#include <composite.h>
#include <const.h>
#include <files.h>
#include <language.h>
#include <log.h>
#include <login.h>
#include <macros.h>
#include <mail.h>
#include <ss_types.h>
#include <std.h>
#include <stdproperties.h>
#include <time.h>

/*
 * These are the neccessary variables stored in the save file.
 */
private string  name;            /* The real name of the player        */
private string  password;        /* The password of the player         */
private string  player_file;     /* The racefile to use for the player */
private mapping m_remember_name; /* The players we have remembered.    */
#ifdef FORCE_PASSWORD_CHANGE
private int     password_time;   /* Time the password was changed      */
#endif FORCE_PASSWORD_CHANGE
private int     restricted;      /* Are we restricted?                 */

#define ATTEMPT_LOG  "/open/attempt"
#define GUEST_LOGIN  "guest"
#define CLEANUP_TIME 120.0 /* two minutes  */
#define TIMEOUT_TIME 120.0 /* two minutes  */
#define PASS_QUEUE   600   /* ten minutes */
#define PASS_ARMAGEDDON 300 /* 5 minutes*/
#define ONE_DAY      86400 /* one day in seconds */

#define ENTER_ENTER  0 /* notify that someone logged in              */
#define ENTER_REVIVE 3 /* notify that someone revived from linkdeath */
#define ENTER_SWITCH 4 /* notify that someone switched terminals     */

/*
 * Global valiables that aren't in the save-file.
 */
static int time_out_alarm; /* The id of the alarm used for timeout.     */
static int login_flag = 0; /* True if the player passed the queue.      */
static int login_type = ENTER_ENTER; /* Login/revive LD/switch terminal */
static int password_set = 0; /* New password set or not.                */
static string old_password; /* The old password of the player.          */

/*
 * Prototypes.
 */
static void check_password(string p);
static void tell_password();
static void try_throw_out(string str);
static void queue(string str);
static void waitfun(string str);
static void get_name(string str);

/* General offensiveness check. The list can be added to as you like. Just
 * do not make it too long. Also, remember that you should use banish for
 * individual names. Please keep the list alphabetized. These strings are
 * parsed by wildmatch.
 */
#define OFFENSIVE ({ \
    "*arse*",  \
    "ass*",    \
    "*ass",    \
    "*bitch*", \
    "*clit*",  \
    "*cock*",  \
    "*cunt*",  \
    "*dick*",  \
    "*fag*",   \
    "*fart*",  \
    "*fuck*",  \
    "*hell*",  \
    "*peck*",  \
    "*penis*", \
    "*pussy*", \
    "*rape*",  \
    "*shit*",  \
    "*slut*",  \
    "*suck*" })

/*
 * Function name: clean_up
 * Description  : This function is called every two minutes and if the
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
    write_socket("Time out! Join us another time.\n");

    destruct();
}

/*
 * Function name: login
 * Description  : This function is called when a player wants to login.
 *                A lot of checks are made.
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

    write_socket("Gamedriver version:  " + SECURITY->do_debug("version") +
        "\t\tMudlib version:  " + MUDLIB_VERSION +
        "\n\nPlease enter your name: ");

    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    input_to(get_name);

    return 1;
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

    /*
     * Determine if we are in the lockout period.
     */
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
 * Function name: start_player2
 * Description  : Swapsocket to player object and if we are not already
 *                in the game enter it.
 * Arguments    : object ob - the playerobject to swap to.
 */
static void
start_player2(object ob)
{
    object dump;

#ifdef STATUE_WHEN_LINKDEAD
    int old_was_live;
    old_was_live = 0;
#endif STATUE_WHEN_LINKDEAD

    /* Print possible news to the player before we alter his/her euid.
     * Since cat() doesn't seem to work, even when setting this_player to
     * this_object, we have to use this construct to make sure the person
     * gets to read the message.
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
        /* Swap old socket to dummy player. */
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
#ifdef STATUE_WHEN_LINKDEAD
    else if (!old_was_live)
    {
        ob->revive();
        ob->fixup_screen();
    }
#endif STATUE_WHEN_LINKDEAD
    else
    {
        ob->fixup_screen();
    }

    /* Notify the wizards of the action. */
    SECURITY->notify(ob, login_type);

    ob->update_hooks();
    destruct();
}

/*
 * Function name: start_player1
 * Description  : The next step in the startup process.
 */
static void
start_player1()
{
    object ob;

    /* Now we can enter the game, find the player file */
    if (player_file)
    {
        ob = clone_object(player_file);
        if (function_exists("enter_game", ob) != PLAYER_SEC_OBJECT)
        {
            ob->remove_object();
        }

        if (!objectp(ob))
        {
            write_socket("Your body cannot be found.\n" +
                "Therefore you must choose a new.\n");
            player_file = 0;
        }
    }

    /* 
     * There can be three different reasons for not having a player_file:
     * 
     *    1 - If this is a new character, let the login player object
     *        manage the creation / conversion / process.
     *    2 - The players racefile is not loadable, a new body must be
     *        choosen.
     *    3 - The players racefile is not a legal playerfile, a new body
     *        must be choosen.
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

    ob->open_player(); 

    if (SECURITY->query_wiz_rank(name))
        seteuid(name);
    else
        seteuid(BACKBONE_UID);

    export_uid(ob); 
    ob->set_trusted(1); 
    start_player2(ob);
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
 * Function name: start_player
 * Description  : This function checks for linkdeath and sees whether the
 *                player has to queue. If there are no restrictions, log in
 *                immediately.
 */
static void
start_player()
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
         * is signalled by a positive 'login_flag'.
         */
        if (login_flag || ((pos = QUEUE->should_queue(name)) == 0))
        {
            start_player1();
            return;
        }

        write_socket("Sorry, the game is full at the moment.\n");
        date();
        write_socket("Your mail status: " +
            MAIL_FLAGS[MAIL_CHECKER->query_mail(name)] + ".\n\n");
        write_socket("Do you want to queue (at position " + pos + ")? ");
        login_flag = 1;
        input_to(queue);
        return;
    }

    /* When 'login_flag' is true, this means the player already queued (after
     * having been linkdead. Reconnect instantly.
     */
    if (login_flag)
    {
        login_type = ENTER_REVIVE;
        start_player2(other_copy);
        return;
    }

    /* If you already have a link, you are asked to switch terminals */
    if (query_interactive(other_copy))
    {
        write_socket("You are already playing !\n");
        write_socket("Throw the other copy out ? ");
        input_to(try_throw_out);
        return;
    }

    /* The player is linkdead, but in combat, reconnect immediately. */
    if (other_copy->query_linkdead_in_combat())
    {
        write_socket("You were in combat when your link broke.\n" +
            "... instantly reconnecting ...\n\n");
        login_type = ENTER_REVIVE;
        start_player2(other_copy);
        return;
    }

    /* Player was linkdead for less PASS_QUEUE seconds. */
    if ((time() - other_copy->query_linkdead()) < PASS_QUEUE)
    {
        write_socket("You were linkdead less than ten minutes ...\n" +
            "... instantly connecting ...\n\n");
        login_type = ENTER_REVIVE;
        start_player2(other_copy);
        return;
    }

    write_socket("You have been linkdead for " +
        CONVTIME(time() - other_copy->query_linkdead()) + ".\n");

    /* No need to queue. Connect instantly. */
    if ((pos = QUEUE->should_queue(name)) == 0)
    {
        login_type = ENTER_REVIVE;
        start_player2(other_copy);
        return;
    }

    write_socket("The game is full at the moment and since you have been " +
        "been away for more than\nten minutes, you shall unfortunately have " +
        "to queue before you can continue\nplaying. Do you want to queue " +
        "(at position " + pos + ")? ");
    login_flag = 1;
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
            write_socket("\nInvalid characters in name \"" + str + "\".\n");
            write_socket("Only letters (a through z) are allowed.\n");
            write_socket("Character number was " + (index + 1) + ".\n");
            return 0;
        }
    }

    return 1;
}

/*
 * Function name: offensive_name
 * Description  : Check whether the name is offensive or not. Note that
 *                this function makes a check for generally offensive parts
 *                only and that you have to use the banish command for
 *                special cases.
 * Arguments    : string str - the name to check.
 * Returns      : int 1/0 - true if the name is offensive.
 */
public int
offensive_name(string str)
{
    int index = -1;
    int size  = sizeof(OFFENSIVE);

    while(++index < size)
    {
        if (wildmatch(OFFENSIVE[index], str))
        {
            return 1;
        }
    }

    return 0;
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
        input_to(get_name);
        return;
    }

    if (str[0] != 'y')
    {
        write_socket("\nPlease answer with either y[es], n[o] or q[uit].\n" +
            "Would you really like to use the name " + capitalize(name) + "? ");
        input_to(confirm_use_name);
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
    int i;
    int runlevel;
    int delay;
    int vowels;

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    str = lower_case(str);
    if (str == "quit")
    {
        write_socket("\nWelcome another time then!\n");
        destruct();
        return;
    }

    if (!valid_name(str))
    {
        input_to(get_name);
        write_socket("Give name again: ");
        return;
    }

    /* If the runlevel is set, the not all players may enter. */
    if (runlevel = SECURITY->query_runlevel())
    {
#ifdef ATTEMPT_LOG
        write_file(ATTEMPT_LOG, ctime(time()) + " " + capitalize(str) + "\n");
#endif ATTEMPT_LOG

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

    if (str == GAMEINFO_LOGIN) /* Does own cleanup */
    {
        g_info = clone_object("/secure/gameinfo_player");
        exec(g_info, this_object());
        g_info->enter_game();
        remove_alarm(time_out_alarm);
        destruct();
        return;
    }

    if (str == APPLICATION_LOGIN) /* Does own cleanup */
    {
        a_player = clone_object("/secure/application_player");
        exec(a_player, this_object());
        a_player->enter_game();
        remove_alarm(time_out_alarm);
        destruct();
        return;
    }

    /* Restore the player. If that fails, we make some additional checks
     * for we must be dealing with a new player.
     */
    if (!restore_object("/players/" + extract(str, 0, 0) + "/" + str))
    {
#ifdef ALWAYS_APPLY
        if (!wildmatch("*jr", str))
        {
            write_socket("\nCurrently, " + SECURITY->get_mud_name() + 
                " cannot accept new players from any site without " +
                "application. If you want to create a character here, " +
                "you may log in with 'application'.\n\n");
            input_to(get_name);
            write_socket("Give name 'application' or disconnect: ");
            return;
        }
#endif ALWAYS_APPLY

        if (!wildmatch("*jr", str) && 
            SECURITY->check_newplayer(query_ip_number(this_object())) == 2)
        {
            write_socket("\nYour site is blocked due to repeated offensive " +
                "behaviour by users from your site.\n\n" +
                "You may still apply for a character by logging " +
                "in as 'application'.\n\n");
            input_to(get_name);
            write_socket("Give name 'application' or disconnect: ");
            return;
        }

        if (file_size(BANISH_FILE(str)) >= 0)
        {
            write_socket("\nThe name " + capitalize(str) +
                " is reserved. Please select another name.\n");
            input_to(get_name);
            write_socket("Give name again: ");
            return;
        }

        if (SECURITY->query_domain_number(capitalize(str)) >= 0)
        {
            write_socket("\nOne of the domains has that name. " +
                "Please choose something else.\n");
            input_to(get_name);
            write_socket("Give name again: ");
            return;
        }

        if (wildmatch("xx*", str))
        {
            write_socket("\nNames starting with XX are reserved.\n");
            input_to(get_name);
            write_socket("Give name again: ");
            return;
        }

        if (offensive_name(str))
        {
            write_socket("\nYour name is caught in the filter for offensive " +
                "names. Please come up with something better or find " +
                "yourself another mud.\n");
            input_to(get_name);
            write_socket("Give another name: ");
            return;
        }

        vowels = sizeof(filter(explode(str, ""),
            &operator(!=)(-1) @ &member_array(, LANG_VOWELS)));
        if (!vowels)
        {
            write_socket("\nYour name must contain at least one vowel. " +
                "(i.e. \"aeiouy\".)\n");
            input_to(get_name);
            write_socket("Give another name: ");
            return;
        }
        if (vowels == strlen(str))
        {
            write_socket("\nYour name must contain at least one consonant. " +
                "(i.e. other than \"aeiouy\").\n");
            input_to(get_name);
            write_socket("Give another name: ");
            return;
        }


        /* The new player is an old wizard, that is not removed correctly. */
        if (SECURITY->query_wiz_rank(name))
        {
            write_socket("\nThis name used to belong to a wizard, but has " +
                "not been freed in a correct manner. If you used to have a " +
                "character here with this name or if you want to use the " +
                "name, you should contact the administration.\n");
#ifndef NO_GUEST_LOGIN
            write_socket("You can use guest-login to contact the " +
                "administration.\n");
#endif NO_GUEST_LOGIN
            write_socket("Give name again: ");
            input_to(get_name);
            return;
        }

        write_socket("\nNew character.\n");
        cat(LOGIN_FILE_NEWCHAR);
        write_socket("Do you really want to use the name " + capitalize(str) +
            "? y[es], n[o] or q[uit]? ");
        player_file = 0;
        name = str;
        input_to(confirm_use_name);
        return;
    }

    if (name == GUEST_LOGIN)
    {
#ifdef NO_GUEST_LOGIN
        write_socket("\nCurrently, " + SECURITY->get_mud_name() + " cannot " +
            "accept login from the 'guest' character. You may choose to " +
            "create real character to play this mud.\n\n");
        remove_alarm(time_out_alarm);
        destruct();
        return;
#endif NO_GUEST_LOGIN

        write_socket("\nWelcome, guest. You do not need a password....\n" +
            "... connecting ...\n");

        start_player();
        return;
    }
    
    if (player_file)
    {
        write_socket("\nWelcome, " + capitalize(name) +
            ". Please enter your password: ");
        input_to(check_password, 1);
    }
    else
    {
        write_socket("\nWelcome, " + capitalize(name) +
            ". Please enter your password.\n");
        tell_password();
    }
}

/*
 * Function name: new_password
 * Description  : This function is used to let a new character set his
 *                password.
 * Arguments    : string p - the intended password.
 */
static void
new_password(string p)
{
    write_socket("\n");
    remove_alarm(time_out_alarm);

    /* If the player does not want to use this character, he can type "quit"
     * as password.
     */
    if (p == "quit")
    {
        write_socket("Very well. Until another time, perhaps.\n");
        destruct();
        return;
    }

    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    /* Player decided to enter a different name. */
    if (p == "new")
    {
        write_socket("Please enter your name: ");
        input_to(get_name);
        return;
    }
    if (strlen(p) < 6)
    {
        write_socket("The password must have at least 6 characters.\n");
        input_to(new_password, 1);
        write_socket("Password: ");
        return;
    }

    if (!(SECURITY->proper_password(p)))
    {
        write_socket("The password does not match the basic security " +
            "standards we have set.\n");
        input_to(new_password, 1);
        write_socket("Password: ");
        return;
    }

    if (strlen(old_password) &&
        (crypt(p, old_password) == old_password))
    {
        write_socket("The password must differ from the previous password.\n");
        write_socket("Password: ");
        input_to(new_password, 1);
        return;
    }

    if (password == 0)
    {
        password = p;
        input_to(new_password, 1);
        write_socket("Now please type the password again to verify.\n");
        write_socket("Password (again): ");
        return;
    }

    if (password != p)
    {
        password = 0;
        write_socket("The passwords don't match. You shall have to be " +
            "consistent next time!\n");
        input_to(new_password, 1);
        write_socket("Password (new password, first try): ");
        return;
    }

    /* Crypt the password. Use a new seed. */
    password = crypt(password, 0);

    if (password_set)
    {
        start_player();
    }
    else
    {
        start_player1();
    }
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
        "- the password must be at least 6 characters long;\n- the password " +
        "must at least contain one 'special character';\n- a 'special " +
        "character' is anything other than a-z and A-Z;\n- the 'special " +
        "character' may not be the first or the last\n  letter in the " +
        "password, that is somewhere before and after a\n  'special " +
        "character' there must be a normal letter.\n\nNew password: ");
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
    /* Negative restriction value means the administration suspended you.
     * A positive restriction value means you restricted yourself.
     */
    if (restricted < 0)
    {
        if (-restricted > time())
        {
            write_socket("The administration of these realms has suspended " +
                "you for now. Therefore\nyou will not be allowed to enter " +
                "the game at this time.\n\nNext login is accepted on: " +
                ctime(-restricted) + "\n\n");
            destruct();
            return 1;
        }
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
    if (SECURITY->query_wiz_rank(query_pl_name()) > WIZ_NORMAL)
        return 0;
    
    foreach (string name : SECURITY->query_seconds(query_pl_name()))
    {
        if (find_player(name))
        {
            write_socket("You may only login one of your characthers at the same " +
                "time.\n\n");
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
 * Arguments    : string p - the intended password.
 */
static void
check_password(string p)
{
    object *players;
    int     size;
    int     index;
    object  player;
    string *names;

    write_socket("\n");

    remove_alarm(time_out_alarm);
    time_out_alarm = set_alarm(TIMEOUT_TIME, 0.0, time_out);

    /* Player has no password, force him/her to set a new one. */
    if (password == 0)
    {
        if (check_restriction())
        {
            return;
        }

        write_socket("You have no password!\n" +
            "Set a password before you are allowed to continue.\n\n");
        password_set = 1;
        old_password = password;
        password = 0;
        tell_password();
        return;
    }

    /* Password doesn't match */
    if (crypt(p, password) != password)
    {
        write_socket("Wrong password!\n");

        /* Player already had a second chance. Kick him/her out. */
        if (login_flag)
        {
            destruct();
            return;
        }

        login_flag = 1;
        write_socket("Password (second and last try): ");
        input_to(check_password, 1);
        return;
    }

    if (check_restriction())
        return;

#ifdef BLOCK_DOUBLE_LOGIN
    if (check_double_login())
        return;
#endif
    
    /* Reset the login flag so people won't skip the queue. */
    login_flag = 0;

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
	    if (function_exists("match_password", players[size]) != PLAYER_SEC_OBJECT)
	    {
		continue;
	    }
	    
	    if (players[size]->match_password(p))
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
	    for (index = 0; index < sizeof(names); index++)
	    {
	        player = find_player(names[index]);
	        SECURITY->log_syslog(LOG_STRANGE_LOGIN,
	            sprintf("    %-11s: %s (%s)\n", capitalize(names[index]),
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
	    for (index = 0; index < sizeof(names); index++)
	    {
	        player = find_player(names[index]);
	        SECURITY->log_syslog(LOG_SECOND_LOGIN,
	            sprintf("    %-11s: %s (%s)\n", capitalize(names[index]),
	            player->query_login_from(),
	            (interactive(player) ? "active" : "link-dead")));
	    }
	}
    }
#endif LOG_STRANGE_LOGIN

    start_player();
    return;
}

/*
 * Function name: try_throw_out
 * Description  : If the player tries to login while another interactive
 *                player with the same name is active, we ask whether to
 *                kick out the other copy.
 * Arguments    : string str - the answer, should start with 'y' or 'n'.
 */
static void
try_throw_out(string str)
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

    if ((!strlen(str)) ||
        (str[0] != 'y'))
    {
        write_socket("Please answer with either y[es] or n[o].\n" +
            "Throw the other copy out? ");
        input_to(try_throw_out);
        return;
    }

    ob = find_player(name);
    if (!objectp(ob))
    {
        write_socket("We lost the old playerobject while asking.\n" +
            "You shall start the game as usual.\n");
        login_type = ENTER_ENTER;
        start_player();
        return;
    }

    login_type = ENTER_SWITCH;
    start_player2(ob);
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
        start_player();
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
    object  *players;
    int     index;
    int     size;

    if (!mappingp(m_remember_name) ||
        !m_sizeof(m_remember_name))
    {
        write_socket("You have not remembered anyone yet.\n");
        return;
    }

    players = users() - ({ 0 }) - (object *)QUEUE->queue_list(0);
    size = sizeof(players);
    index = -1;

    if (!SECURITY->query_wiz_rank(name))
    {
        while(++index < size)
        {
            if (((!m_remember_name[players[index]->query_real_name()]) &&
                 (!players[index]->query_prop(LIVE_I_ALWAYSKNOWN))) ||
                (players[index]->query_prop(OBJ_I_INVIS)))
            {
                players[index] = 0;
            }
        }
    }

    players -= ({ 0 });
    if (!sizeof(players))
    {
        write_socket("You know none of the people in the game.\n");
    }
    else
    {
        write_socket("You know the following people in the game:\n" +
            sprintf("%-75#s\n",
            implode(sort_array(players->query_real_name()), "\n")));
    }
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
    /* If login_flag is 2, this means that the player already queued and
     * that he/she only needs to enter a command to unidle. We don't need
     * to check the actual command. Just run the show.
     */
    if (login_flag == 2)
    {
        set_this_player(this_object());

        start_player();

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
            login_flag = 2;
            return;
        }

        start_player();
    }
    else
    {
        write_socket("Queue position " + num + ".\nQUEUE> ");
    }
}

/*
 * Function name: query_login_flag
 * Description  : Returns the current login flag.
 * Returns      : int - the login flag.
 */
public int
query_login_flag()
{
    return login_flag;
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
