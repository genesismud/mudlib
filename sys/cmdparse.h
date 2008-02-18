/*
 * cmdparse.h
 *
 * This holds some very handy macros for command parsing.
 */

#ifndef CMDPARSE_DEF
#define CMDPARSE_DEF

#ifndef FILES_DEFINED
#include "/sys/files.h"
#endif  FILES_DEFINED

/*
 * CMDPARSE_STD
 *
 * This defines the object holding much of the cmdparse.h code.
 */
#define CMDPARSE_STD "/sys/global/cmdparse"

/*
 * CMDPARSE_ONE_ITEM
 *
 * Parse and execute a trivial command of the type <verb> <item>
 *
 * Arguments:
 *            c     Command string after verb. ( == <item> )
 *                  <item> can be for example:
 *                      "the red apple", "all green objects" etc.
 *
 *            dofun Function called to do what ever is to be done
 *                  to each object included in <item>
 *
 *            afun  [optional] Function called for each object in <item>
 *                  to confirm inclusion in <item>. If afun == 0 then
 *                  those of the objects that are in the players inventory
 *                  or the players environment are included.
 *
 * Both dofun and afun are needed because afun most be called first for all
 * objects to get <item> descs like "the second apple" to work right.
 *
 * Returns: 
 *            An array holding all objects for which 'dofun' returned 1.
 */
#define CMDPARSE_ONE_ITEM(c, dofun, afun) \
    ((object *)CMDPARSE_STD->do_verb_1obj(c, dofun, afun, this_object()))

/*
 * CMDPARSE_IN_ITEM
 *
 * Parse and execute a command of the type <verb> <item1> "in_prep" <item2>
 *
 * It finds those of <item1> is located inside <item2>
 *
 * Arguments:
 *            c     Command string after verb. 
 *                  <itemX> can be for example:
 *                      "the red two apples", "all blue ones" etc.
 *
 *            pfun  Function called to confirm "in_prep" as correct.
 *
 *            dofun Function called to do what ever is to be done
 *                  to each object included in <item1>
 *
 *            afun  [optional] Function called for each object in <item2>
 *                  to confirm inclusion in <item2>. If afun == 0 then
 *                  those of the objects that are in the players inventory
 *                  or the players environment are included.
 *
 * Both dofun and afun are needed because afun most be called first for all
 * objects to get <item> descs like "the second apple" to work right.
 *
 * Returns: 
 *            An array holding all objects for which 'dofun' returned 1.
 */
#define CMDPARSE_IN_ITEM(c, pfun, dofun, afun) \
    ((object *)CMDPARSE_STD->do_verb_inside(c, pfun, dofun, afun, this_object()))

/*
 * CMDPARSE_WITH_ITEM
 *
 * Parse and execute a command of the type <verb> <item1> "prep" <item2>
 *
 * Arguments:
 *            c     Command string after verb. 
 *                  <itemX> can be for example:
 *                      "the red two apples", "all blue ones" etc.
 *
 *            chfun Function called to confirm "prep" as correct.
 *
 *            dofun Function called to do what ever is to be done
 *                  to each object included in <item1>
 *
 *            afun1 [optional] Function called for each object in <item1>
 *                  to confirm inclusion in <item1>. If afun1 == 0 then
 *                  those of the objects that are in the players inventory
 *                  or the players environment are included.
 *
 *            afun2 [optional] Function called for each object in <item2>
 *                  to confirm inclusion in <item2>. If afun2 == 0 then
 *                  those of the objects that are in the players inventory
 *                  or the players environment are included.
 *
 * Returns: 
 *            An array holding all objects for which 'dofun' returned 1.
 */
#define CMDPARSE_WITH_ITEM(c, chfun, dofun, afun1, afun2) \
    ((object *)CMDPARSE_STD->do_verb_with(c, chfun, dofun, afun1, afun2, this_object()))

/*
 * PARSE_COMMAND
 *
 * Performs the gamedriver parse_command() and follows it up with a standard
 * call to NORMAL_ACCESS() on the result.
 *
 * Arguments:
 *            string str - the (command-line) text to parse.
 *            mixed  env - if a singular object, it will take env + the deep
 *                         inventory of env. If an array of objects, it will
 *                         match only against those objects.
 *            string pattern - the pattern to parse against. For more info,
 *                         see "man parse_command".
 * Returns:
 *            object * - an array with matching objects, ({ }) or 0.
 */
#define PARSE_COMMAND(str, env, pattern) \
    ((object *)CMDPARSE_STD->parse_command_access((str), (env), (pattern)))

/*
 * PARSE_COMMAND_ONE
 *
 * Same as PARSE_COMMAND, but then returns the object if the player selected
 * exactly one object. Note that this returns 0 if the player selected more
 * than one item. If this fails, use PARSE_COMMAND_SIZE to find out how many
 * items were found (if you care whether it was 0 or > 1).
 */
#define PARSE_COMMAND_ONE(str, env, pattern) \
    ((object)CMDPARSE_STD->parse_command_one((str), (env), (pattern)))

/*
 * PARSE_COMMAND_SIZE
 *
 * When PARSE_COMMAND_ONE is used and fails, use this macro to find out how
 * many items were found. It could be 0, but could also be > 1.
 */
#define PARSE_COMMAND_SIZE ((int)CMDPARSE_STD->parse_command_size())

/*
 * NORMAL_ACCESS
 *
 * test for access to object
 *
 * Arguments:
 *            arr     array from parse_command to test (arr[0] gives numeric or
 *		      order info).
 *
 *            acsfunc function to use in filter to filter objects in arr
 *
 *            acsobj  object use to call acsfunc
 *
 * Returns: 
 *            An array holding all objects satisfying arr[0] and acsfunc. 
 */
#define NORMAL_ACCESS(arr, acsfunc, acsobj) \
    ((object *)CMDPARSE_STD->normal_access(arr, acsfunc, acsobj))

/*
 * VISIBLE_ACCESS
 *
 * test for access to object visible to a player, only include this_player()
 * if it is the only object.
 *
 * Arguments:
 *            arr     array from parse_command to test (arr[0] gives numeric or
 *		      order info).
 *
 *            acsfunc function to use in filter to filter objects in arr
 *
 *            acsobj  object use to call acsfunc
 *
 * Returns: 
 *            An array holding all objects satisfying arr[0] and acsfunc. 
 */
#define VISIBLE_ACCESS(arr, acsfunc, acsobj) \
    ((object *)CMDPARSE_STD->visible_access(arr, acsfunc, acsobj, 0))

/*
 * FIND_STR_IN_OBJECT
 *
 * Find the corresponding object array in a player or room.
 * Locates both 'second sword' as well as 'sword 2' or 'two swords'
 *
 * Always returns an array with objects, or sometimes an empty array.
 */
#define FIND_STR_IN_OBJECT(str, obj) \
    ((object *)CMDPARSE_STD->find_str_in_object(str, obj))

/*
 * FIND_STR_IN_ARR
 *
 * Find the corresponding object array from a given array.
 * Locates both 'second sword' as well as 'sword 2' or 'two swords'
 *
 * Always returns an array with objects, or sometimes an empty array.
 */
#define FIND_STR_IN_ARR(str, arr) \
    ((object *)CMDPARSE_STD->find_str_in_arr(str, arr))

/*
 * CMDPARSE_ITEMLIST
 *
 * Parses a string on the form:
 *
 *    <prep> <item> <prep> <item> <prep> <item> ....
 *
 * item can be a subitem, sublocation or a normal object.
 *
 * Returns an array with four elements:
 *
 *
 * ret[0]		 The prepositions 
 * ret[1]		 The items, a normal parse_command %i return value 
 * ret[2]		 True if last was not a normal object 
 * ret[3]		 True if no normal objects 
 *
 */
#define CMDPARSE_ITEMLIST(str) CMDPARSE_STD->parse_itemlist(str)

/* 
 * PARSE_THIS
 *
 * This define gives access to the function parse_this in the basic soul
 * COMMAND_DRIVER so that you do not have to copy it everywhere.
 *
 * s - the string to parse
 * p - the pattern to parse with
 *
 * for more information do see "man parse_this" in the chapter "soul"
 */
#define PARSE_THIS(s, p) (object *)COMMAND_DRIVER->parse_this(s, p)

/* ACTION_OTHER - An unclassified action */
#define ACTION_OTHER     0

/* ACTION_CONTACT - This action requires physical contact with the
 * target.
 */
#define ACTION_CONTACT   1

/* ACTION_CONTACT - This action requires the actor to be physically
 * near the target.
 */
#define ACTION_PROXIMATE 2

/* ACTION_AURAL - This action can be perceived with the sense of hearing */
#define ACTION_AURAL     4

/* ACTION_VISUAL - This action can be perceived with the sense of vision */
#define ACTION_VISUAL    8

/* ACTION_OLFACTORY - This action can be perceived with the sense of smell */
#define ACTION_OLFACTORY 16
/*
/* ACTION_LACTIVITY - This action requires a low level of activity.
 * These actions can typically be performed while the actor is restrained,
 * though not while completely unable to move.
 */
#define ACTION_LACTIVITY 32

/* ACTION_MACTIVITY - This action requires a moderate level of activity.
 * These actions can typically be performed while under minor restraint,
 * though not while more heavily restrained.
 */
#define ACTION_MACTIVITY 64

/* ACTION_HACTIVITY - This action requires a high level of activity.
 * These actions require a full range of activity or perhaps can be
 * performed under minimal restraint.
 */
#define ACTION_HACTIVITY 128

/* ACTION_OFFENSIVE - This action is intentionally rude or offensive */
#define ACTION_OFFENSIVE 256

/* ACTION_INGRATIATORY - This action is intentionally ingratiatory,
 * flattering, pleasing.
 */
#define ACTION_INGRATIATORY 512

/* ACTION_INTIMATE - This action is considered intimate.  This includes
 * sexual actions.
 */
#define ACTION_INTIMATE 1024

/* ACTION_THREATENING - This action is considered threatening.  It is
 * intended to cause fear in the target.
 */
#define ACTION_THREATENING 2048

/* ACTION_BLIND - This action is not displayed towards those that
 * cannot see the actor.
 */
#define ACTION_BLIND 4096

/*
 * CMDPARSE_PARALYZE_CMD_IS_ALLOWED(cmd)
 *
 * Find out whether a certain command 'cmd' is allowed to be executed while
 * the player is paralyzed. Returns true if allowed.
 */
#define CMDPARSE_PARALYZE_CMD_IS_ALLOWED(cmd) \
    ((int)CMDPARSE_STD->paralyze_cmd_is_allowed(cmd))

/*
 * CMDPARSE_PARALYZE_ALLOW_CMDS(cmds)
 *
 * Register one or more commands 'cmds' as being allowed to be executed while
 * the player is paralyzed. This must be information commands only, and may
 * not consititute any action, activity or communication. The 'cmds' can be
 * a single string or array of string with commands to allow.
 */
#define CMDPARSE_PARALYZE_ALLOW_CMDS(cmds) \
    (void)CMDPARSE_STD->paralyze_allow_cmds(cmds)

/* CMDPARSE_PARALYZE_ALLOWED
 *
 * These commands should always be allowed, even if the person is paralyzed
 * or otherwise incapacitated.
 */
#define CMDPARSE_PARALYZE_ALLOWED \
    ({ "adverbs", "bug", "commune", "date", "email", "forget", "help", \
       "idea", "last", "levels", "look", "mwho", "options", "praise", \
       "quit", "remember", "remembered", "reply", "save", "skills", \
       "stats", "sysbug", "sysidea", "syspraise", "systypo", "team", \
       "typo", "vitals", "who" })

/* No definitions beyond this line. */
#endif CMDPARSE_DEF
