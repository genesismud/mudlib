/*
 * /secure/master/mail_admin.c
 *
 * This part of SECURITY is the mail administrator. Because the mail system
 * uses a data-structure that cannot be manipulated directly (hashing and
 * individual message files), we need a central module to control the system.
 * The arch-level "mailadmin" command can be used to access the functionality
 * in this module.
 *
 * Possible caveat: This module assumes that the names of all players with a
 * mailbox can be put in a single array.
 *
 * The following subcommands are supported:
 * - export a the mail-folder of a player to file;
 * - print statistics about the mail system;
 * - purge all mailboxes and message-files that have reference to players
 *   that do not exist any longer, then purge all message-files that are
 *   not pointed at by any mail folder.
 */

#include "/sys/composite.h"
#include "/sys/mail.h"
#include "/sys/std.h"

#define DIR_NAME_MESSAGE(dir)  (MSG_DIR + "d" + (dir))
#define MESSAGE_FILENAME(d, m) (DIR_NAME_MESSAGE(d) + "/" + (m))

#define STATISTICS_FILE   ("/syslog/log/MAILBOX_STATS")
#define MAX_MAILBOX_STATS (100)
#define MAX_MAILBOX_PURGE (100)
#define MAX_MESSAGE_PURGE ( 25)
#define LINE_LENGTH       ( 77)
#define SPACES ("                                 ")

/*
 * Global variables.
 *
 * mail_wizard  - the wizard handling the mail administrator when an alarm
 *                is running.
 * mail_system  - mapping with the names of the players with a mailbox as
 *                index and for value an array with the message dates of all
 *                messages.
 * mail_alarm   - the alarm-id if an alarm is running.
 */
static private string  mail_wizard  = 0;
static private mapping mail_system  = 0;
static private int     mail_alarm   = 0;

/*
 * Function name: restore_mail
 * Description  : Restore the mail-file of a player from disk.
 * Arguments    : string name - the name of the player.
 * Returns      : mapping     - the mail of the player.
 */
static mapping
restore_mail(string name)
{
    mapping mail;

    mail = restore_map(FILE_NAME_MAIL(name));

    if ((!mappingp(mail)) ||
	(m_sizeof(mail) != M_SIZEOF_MAIL))
    {
	return 0;
    }

    return mail;
}

/*
 * Function name: save_mail
 * Description  : Save the mail-file of a player to disk.
 * Arguments    : mapping mail - the mail of the player.
 *                string  name - the name of the player.
 */
static void
save_mail(mapping mail, string name)
{
    save_map(mail, FILE_NAME_MAIL(name));
}

/*
 * Function name: restore_message
 * Description  : Restore an individual message from disk.
 * Arguments    : int number - the time of the message.
 * Returns      : mapping  - the message restored.
 */
static varargs mapping
restore_message(int number)
{
    mapping message;

    message = restore_map(FILE_NAME_MESSAGE(number, HASH_SIZE));

    if ((!mappingp(message)) ||
	(m_sizeof(message) != M_SIZEOF_MSG))
    {
	return 0;
    }

    return message;
}

/*
 * Function name: save_message
 * Description  : Save the individual message to disk.
 * Arguments    : mapping message - the message.
 *                int     number  - the current time to save.
 */
static void
save_message(mapping message, int number)
{
    save_map(message, FILE_NAME_MESSAGE(number, HASH_SIZE));
}

/*
 * Function name: mail_tell_wizard
 * Description  : Give a message to the wizard handling the mailadmin
 *                command.
 * Arguments    : string str - the message to give to the wizard.
 */
void
mail_tell_wizard(string str)
{
    object wizard = find_player(mail_wizard);

    if (objectp(wizard))
    {
	tell_object(wizard, str);
    }
}

/*
 * Function name: wrap_text
 * Description  : This will take a text and make sure that none of the lines
 *                in the text exceed LINE_LENGTH characters. All lines longer
 *                than that will be broken. Lines will not be merged.
 * Arguments    : string - the text to be wrapped.
 * Returns      : string - the wrapped text.
 */
string
wrap_text(string text)
{
    string *lines;
    int    size;

    if (!strlen(text))
    {
	return "";
    }

    lines = explode(text, "\n");
    size = sizeof(lines);
    while(--size >= 0)
    {
	if (strlen(lines[size]) > LINE_LENGTH)
	{
	    lines[size] = break_string(lines[size], LINE_LENGTH);
	}
    }

    return implode(lines, "\n");
}

/*
 * Function name: export_mail
 * Description  : With this function the mail-folder of a player can be
 *                exported to a file.
 * Arguments    : string name - the lower case name of the player whose
 *                              mailbox is to be exported.
 *                string path - the name of the output file.
 * Returns      : int 1/0 - success/failure.
 */
static int
export_mail(string name, string path)
{
    mapping mail;
    mixed   messages;
    int     index;
    int     size;
    string  text;

    /* Get the output path and test its valitity by writing the header. */
    path = FTPATH(this_player()->query_path(), path);
    if (file_size(path) != -1)
    {
	notify_fail("File " + path + " already exists.\n");
	return 0;
    }

    /* This may fail if there is no such directory, for instance. */
    if (!write_file(path, "Mailbox of: " + capitalize(name) +
	"\nPrinted at: " + ctime(time()) + "\n\n"))
    {
	notify_fail("Failed to write header of " + path + "\n");
	return 0;
    }

    /* Read the mail file. */
    mail = restore_mail(name);
    if (!mappingp(mail))
    {
	notify_fail("No correct mail folder for player " + name + ".\n");
	return 0;
    }

    /* Loop over all messages. */
    messages = mail[MAIL_MAIL];
    index = -1;
    size = sizeof(messages);
    while(++index < size)
    {
	mail = restore_message(messages[index][MAIL_DATE]);
	
	text = "Message: " + (index + 1) + "\nFrom   : " +
	      messages[index][MAIL_FROM] + "\n" +
	      (messages[index][MAIL_REPLY] ? "Reply  : " : "Subject: ") +
	      messages[index][MAIL_SUBJ] + "\n";
	
	if (mail[MSG_TO] != capitalize(name))
	{
	    text += HANGING_INDENT("To     : " +
		COMPOSITE_WORDS(explode(mail[MSG_TO], ",")), 9, 1);
	}
	
	if (mail[MSG_CC] != "")
	{
	    text += HANGING_INDENT("CC     : " +
		COMPOSITE_WORDS(explode(mail["cc"], ",")), 9, 1);
	}

	/* Write the message to file and print a sequence number to the
	 * wizard. Notice that the index of the loop is also increased in
	 * this write-statement.
	 */	
	write_file(path, text + "Date   : " +
            MAKE_DATE(messages[index][MAIL_DATE]) + " " +
            DATE_YEAR(messages[index][MAIL_DATE]) +
	    "\n\n" + wrap_text(mail[MSG_BODY]) + "\n\n");
    }

    write("Mail folder written for " + capitalize(name) + ".\nFilename: " +
	path + "\n");
    return 1;
}

/*
 * Function name: purge_one_message
 * Description  : Checks this message. It checks the validity of the names
 *                of all recipients and checks whether the message is still
 *                referenced from the mailboxes.
 * Arguments    : int dir     - the directory currently checked.
 *                string file - the filename of the message to check.
 * Returns      : int 1/0 - purged/not purged.
 */
static int
purge_one_message(int dir, string file)
{
    mapping message;
    int     date;
    string *names;
    int    size;
    int    index = -1;
    int    touched;

    /* Extract the date from the filename. */
    if (sscanf(file, "m%d.o", date) != 1)
    {
        rm(MESSAGE_FILENAME(dir, file));
        return 1;
    }

    /* Restore the message. */
    catch(message = restore_message(date));
    if (!mappingp(message))
    {
        rm(MESSAGE_FILENAME(dir, file));
        return 1;
    }

    /* Check the people still registering to the message for validity. This
     * means that the player must have a mailbox and have the message
     * referenced from that mailbox.
     */
    names = explode(lower_case(message[MSG_ADDRESS]), ",");
    size = sizeof(names);
    while(++index < size)
    {
        /* Recipient does not have a mailbox anymore. */
        if (!pointerp(mail_system[names[index]]))
        {
            names[index] = "";
            touched = 1;
            continue;
        }

        /* Recipient does not have this message referenced. */
        if (member_array(date, mail_system[names[index]]) == -1)
        {
            names[index] = "";
            touched = 1;
            continue;
        }
    }

    /* No names are left, purge the message. */
    names -= ({ "" });
    if (!sizeof(names))
    {
        rm(MESSAGE_FILENAME(dir, file));
        return 1;
    }

    /* Some, but not all recipients were removed. Save the message. */
    if (touched)
    {
        message[MSG_ADDRESS] = implode(map(names, capitalize), ",");
        save_message(message, date);
    }

    /* Message survived. */
    return 0;
}

/*
 * Function name: purge_check_messages
 * Description  : This will loop over a batch of message files and then
 *                continues with the next directory.
 * Arguments    : int dir       - the directory number we are working on.
 *                int messages  - amount of messages found so far.
 *                int purged    - amount of messages purged so far.
 *                string *files - the names of the files left in this dir.
 */
static void
purge_check_messages(int dir, int messages, int purged, string *files)
{
    int     index = -1;
    int     size = (MIN(sizeof(files), MAX_MESSAGE_PURGE));

    /* Loop over this batch. */
    while(++index < size)
    {
        if (purge_one_message(dir, files[index]))
        {
            purged++;
        }
    }

    /* Continue with the next batch. */
    if (sizeof(files) > size)
    {
        files = files[size..];
        mail_alarm = set_alarm(5.0, 0.0,
            &purge_check_messages(dir, messages, purged, files));
        return;
    }

    /* Continue with the next message directory. */
    dir++;
    if (dir < HASH_SIZE)
    {
        mail_tell_wizard("MAIL ADMINISTRATOR ->> Purged message directory d" +
            dir + ".\n");
        files = get_dir(DIR_NAME_MESSAGE(dir) + "/m*.o");
        messages += sizeof(files);
        mail_alarm = set_alarm(5.0, 0.0,
            &purge_check_messages(dir, messages, purged, files));
        return;
    }

    mail_tell_wizard("MAIL ADMINISTRATOR ->> Message purging completed.\nPurged " +
        purged + " out of " + messages+ " message files.\n");

    mail_system = 0;
    mail_wizard = 0;
    mail_alarm = 0;    
}

/*
 * Function name: purge_collect_mailboxes
 * Description  : This function will slowly loop over all mailboxes to collect
 *                the message times of all messages in the game.
 * Arguments    : string *files - the names of all players with a mailbox.
 */
static void
purge_collect_mailboxes(string *files)
{
    int     index = -1;
    int     size = (MIN(sizeof(files), MAX_MAILBOX_PURGE));
    int     index2;
    mapping mail;

    /* Fetch the message dates from the mailfolders. */
    while(++index < size)
    {
	catch(mail = restore_mail(files[index]));
	if (!mappingp(mail))
	{
	    continue;
	}

        /* Initialise the array for this person. */
        mail_system[files[index]] = ({ });

        /* Add the message dates to the array. */
        index2 = sizeof(mail[MAIL_MAIL]);
        while(--index2 >= 0)
        {
            mail_system[files[index]] +=
                ({ mail[MAIL_MAIL][index2][MAIL_DATE] });
        }
    }

    /* Continue with the next batch. */
    if (sizeof(files) > size)
    {
        mail_tell_wizard("MAIL ADMINISTRATOR ->> Last mailbox: " +
            capitalize(files[size - 1]) + ".\n");
        files = files[size..];
        mail_alarm = set_alarm(10.0, 0.0, &purge_collect_mailboxes(files));
        return;
    }

    mail_tell_wizard("MAIL ADMINISTRATOR ->> Collected all mailboxes.\n");

    /* Fetch the first directory of messages. */
    files = get_dir(DIR_NAME_MESSAGE(0) + "/m*.o");

    mail_alarm = set_alarm(10.0, 0.0,
        &purge_check_messages(0, sizeof(files), 0, files));
}

/*
 * Function name: purge_check_mailboxes
 * Description  : This function will check all mailboxes against the names of
 *                the players and deletes mailboxes that do not have an owner.
 */
static void
purge_check_mailboxes()
{
    int    index = -1;
    int    index2;
    int    size;
    int    purged;
    string *files = ({ });
    string *players;
    string *purge_files;

    /* Loop over the alphabet. */
    while(++index < 26)
    {
        /* Get all files from the mailbox directory and subtract all filenames
         * of playerfiles starting with the same letter.
         */
        players = get_dir(PLAYER_FILE(ALPHABET[index..index] + "*.o"));
        purge_files = get_dir(FILE_NAME_MAIL(ALPHABET[index..index] + "*.o"));
        files += purge_files;
        purge_files -= players;

        /* If there are any mailboxes without player, delete the mailboxes. */
        size = sizeof(purge_files);
        if (size > 0)
        {
            purged += size;
            index2 = -1;
            while(++index2 < size)
            {
                rm(FILE_NAME_MAIL(purge_files[index2]));
            }
        }
    }

    /* Strip the .o suffix. */
    files = map(files, &extract(, 0, -3));

    mail_tell_wizard("MAIL ADMINISTRATOR ->> Checked " + sizeof(files) +
        " and purged " + purged +
        " mailboxes.\n");
//        " mailboxes.\nNext phase: collecting mailboxes.\n");

//    mail_alarm = set_alarm(10.0, 0.0, &purge_collect_mailboxes(files));
}

/*
 * Function name: purge_mail
 * Description  : This function will purge the mail system. It will do
 *                the following:
 *                - remove all mailboxes from players that do no longer exist;
 *                - check all message files and remove the names of all
 *                  recipients who no longer exist;
 *                - cross check all message files with all mailboxes and
 *                  remove those message files that are no longer connected
 *                  to any mailbox.
 * Returns      : int 1/0 - success/failure.
 */
static int
purge_mail()
{
    write("Purge started.\n");
    write("Starting with removing mailboxes of non-existant players.\n");

    /* Initialise the mail system for the message dates and the other mapping
     * for the player names.
     */
    mail_system = ([ ]);

    mail_wizard = this_player()->query_real_name();
    mail_alarm = set_alarm(5.0, 0.0, purge_check_mailboxes);

    return 1;
}

/*
 * Function name: report_statistics
 * Description  : This function will do the actual reporting about the
 *                mail system.
 * Arguments    : int boxes    - the number of mailboxes found.
 *                int messages - the number of messages found in the boxes.
 *                int files    - the number of individual message files.
 */
static void
report_statistics(int boxes, int messages, int files)
{
    mail_tell_wizard("MAIL ADMINISTRATOR ->> Done gathering statistics.\n" +
	"A total of " + boxes + " mailboxes was found, containing a total " +
	"of " + messages + " mail messages. These messages are stored in " +
	files + " individual message files. On average, " +
	(files / HASH_SIZE) + " message files are stored in each of the " +
	HASH_SIZE + " directories.\n");

    mail_wizard = 0;
    mail_alarm = 0;
}

/*
 * Function name: count_messages
 * Description  : This function loops over all message directories and counts
 *                the number of messages found in them. Note that this
 *                function is not protected against eval-cost.
 * Arguments    : int boxes    - the number of mailboxes found.
 *                int messages - the number of messages found in the boxes.
 */
static void
count_messages(int boxes, int messages)
{
    int index = -1;
    int message_files = 0;

    mail_tell_wizard("MAIL ADMINISTRATOR ->> Counting message files.\n");

    while (++index < HASH_SIZE)
    {
	message_files += sizeof(get_dir(DIR_NAME_MESSAGE(index) + "/m*.o"));
    }

    mail_alarm = set_alarm(5.0, 0.0,
	&report_statistics(boxes, messages, message_files));
}

/*
 * Function name: count_mailboxes
 * Description  : This function will loop over the mail directories and
 *                count all the mailboxes and all messages in it. Note that
 *                this function is not protected against eval-cost.
 * Arguments    : int boxes     - the number of mailboxes found.
 *                int messages  - the number of messages found in the boxes.
 *                string *files - the mailboxes yet to check.
 */
static void
count_mailboxes(int boxes, int messages, string *files)
{
    int     index = -1;
    int     size = (MIN(sizeof(files), MAX_MAILBOX_STATS));
    mapping mail;

    /* Count the messages in these mailfolders. */
    while(++index < size)
    {
	catch(mail = restore_mail(files[index]));
	if (!mappingp(mail))
	{
	    boxes--;
	    continue;
	}

	messages += sizeof(mail[MAIL_MAIL]);

        write_file(STATISTICS_FILE,
            sprintf("%-11s %4d\n", files[index], sizeof(mail[MAIL_MAIL])));
    }

    /* Continue with the next batch. */
    if (sizeof(files) > size)
    {
        mail_tell_wizard("MAIL ADMINISTRATOR ->> Last mailbox: " +
            capitalize(files[size - 1]) + ".\n");
        files = files[size..];
        mail_alarm = set_alarm(10.0, 0.0,
            &count_mailboxes(boxes, messages, files));
        return;
    }

    /* Continue with counting the message files. */
    mail_alarm = set_alarm(10.0, 0.0, &count_messages(boxes, messages));
}

/*
 * Function mame: mail_statistics
 * Description  : This function will print various statistics about the
 *                mail system.
 * Returns      : int 1/0 - success/failure.
 */
static int
mail_statistics()
{
    int    index = -1;
    int    boxes;
    string *files = ({ });

    /* Loop over all letters of the alphabet. */
    while(++index < 26)
    {
        files += get_dir(FILE_NAME_MAIL(ALPHABET[index..index] + "*.o"));
    }

    /* Strip the .o suffix. */
    files = map(files, &extract(, 0, -3));

    /* Start the gathering of statistics. */
    mail_wizard = this_player()->query_real_name();
    mail_alarm = set_alarm(5.0, 0.0,
        &count_mailboxes(sizeof(files), 0, files));

    write("Statictics gathering started.\n");
    return 1;
}

/*
 * Function name: mailadmin
 * Description  : This contains the implementation of the "mailadmin" command
 *                in the arch-soul.
 * Arguments    : string str - the subcommand requested.
 * Returns      : int 1/0 - success/failure.
 */
int
mailadmin(string str)
{
    string *args;

    if (!CALL_BY(WIZ_CMD_ARCH))
    {
	notify_fail("This function may only be called from the arch-soul.\n");
	return 0;
    }

    if (strlen(mail_wizard) &&
	(str != "reset"))
    {
        write("The \"mailadmin\" command is busy for " + 
	    capitalize(mail_wizard) + ".\n");
	return 1;
    }

    if (!strlen(str))
    {
	notify_fail("Syntax: mailmaster <subcommand> [<arguments>]\n");
	return 0;
    }

    set_auth(this_object(), "root:root");

    args = explode(str, " ");
    switch(args[0])
    {
    case "export":
	if (sizeof(args) != 3)
	{
	    notify_fail("Syntax: mailadmin export <player name> <filename>\n");
	    return 0;
	}

	return export_mail(lower_case(args[1]), args[2]);
	/* notreached */

    case "purge":
	if (sizeof(args) != 1)
	{
	    notify_fail("Syntax: mailadmin purge\n");
	    return 0;
	}

	return purge_mail();
	/* notreached */

    case "reset":
	/* Set all variables to 0. */
	mail_wizard = 0;
	mail_system = 0;
	remove_alarm(mail_alarm);
	mail_alarm = 0;

	write("Reset the \"mailadmin\" command.\n");
	return 1;
	/* notreached */

    case "stats":
	if (sizeof(args) != 1)
	{
	    notify_fail("Syntax: mailadmin stats\n");
	    return 0;
	}

	return mail_statistics();
	/* notreached */

    default:
	notify_fail("Unknown subcommand to \"mailadmin\".\n");
	return 0;
    }

    write("Impossible end of \"mailadmin\". Please report this.\n");
    return 1;
}
