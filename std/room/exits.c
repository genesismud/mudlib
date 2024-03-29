/*
 * /std/room/exits.c
 *
 * This module is a part of /std/room.c
 *
 * It contains the functions and data related to the exits of a room.
 */

#include <cmdparse.h>
#include <files.h>
#include <filter_funs.h>

/*
 * Global variables.
 */
static mixed room_exits;
static mixed tired_exits;
static mixed non_obvious_exits;
static int   room_no_obvious;
static string *default_dirs = DEFAULT_DIRECTIONS;
static mapping no_exit_messages;

/*
 * Prototype
 */
int unq_move(string str);
int unq_no_move(string str);

/*
 * Function name: ugly_update_action
 * Description  : Just fix so the verb is updated in a room when an exit
 *                is added or removed.
 * Arguments    : string cmd - the command verb.
 *                        int add    - add command if 1 else remove.
 */
static void
ugly_update_action(object player, string cmd, function fun)
{
    string name = explode(function_name(fun), ">")[1];

    /* Does the player have the command already? */
    foreach (mixed arr: commands(player)) {
        if (arr[0] == cmd && arr[2] == this_object() &&
            arr[3] == name)
            return;
    }

    object old_tp = this_player();
    set_this_player(player);
    add_action(fun, cmd);
    set_this_player(old_tp);
}

/*
 * Function name: init
 * Description  : Add direction commands to livings in the room.
 */
public void
init()
{
    ::init();

    int index = -2;
    int size = sizeof(room_exits);
    while ((index += 3) < size)
    {
        add_action(unq_move, room_exits[index]);
    }

    foreach (string dir: default_dirs)
    {
        add_action(unq_no_move, dir);
    }
}

/*
 * Function name: set_noshow_obvious
 * Description  : With this function you can set that all exits in this room
 *                are non-obvious. The fifth argument of add_exit() should
 *                only be used if there are also obvious exits. Using this
 *                is quicker when dealing with all exits.
 * Arguments    : int obv - 1/0 - if true then all exits are hidden.
 */
public void
set_noshow_obvious(int obv)
{
    room_no_obvious = obv;
}

/*
 * Function name: query_noshow_obvious
 * Description  : Return whether all exits in this room are hidden.
 * Returns      : int - if true then all exits are hidden.
 */
public int
query_noshow_obvious()
{
    return room_no_obvious;
}

/*
 * Function name: add_exit
 * Description  : Add one exit to the room. Only the first and the second
 *                argument are mandatory. The rest is optional.
 * Arguments    : string place - The filename of the room to move to. This can
 *                    be an absolute or relative path, but also just the
 *                    filename if that is in the same directory. Do not add
 *                    the trailing ".c".
 *                string cmd - The command the player has to give to move. Use
 *                    the long version of the "southwest". The short version
 *                    "sw" is automatically added.
 *                mixed efunc - The delay value. This can be VBFC. Possible
 *                    delay values are:
 *                    = 0: Player can move directly to destination.
 *                    = 1: Cannot use this exit, and do not try others.
 *                    > 1: Cannot use this exit, but try the rest.
 *                    < 0: Move is done with delay (see link_room)
 *                mixed tired - How much more tired the player should become
 *                    from walking in that direction. Default value is 1.
 *                    This can be VBFC.
 *                mixed non_obvious - When true, the player will not see this
 *                    exit, but he can use it. VBFC can be used here.
 * Returns      : int 1/0 - success/failure.
 */
public varargs int
add_exit(string place, string cmd, mixed efunc, mixed tired, mixed non_obvious)
{
    string dir;

    /* No extra exits allowed. */
    if (query_prop(ROOM_I_NO_EXTRA_EXIT))
    {
        return 0;
    }

    /* If the place can consist of only the filename. Then we should add the
     * complete path name from the filename from this room.
     */
    if (stringp(place) && place[0] != '/' && place[0] != '@')
    {
        dir = implode(explode(file_name(this_object()), "/")[..-2], "/");
        place = FPATH(dir, place);
    }

    if (pointerp(room_exits))
        room_exits = room_exits + ({ place, cmd, efunc });
    else
        room_exits = ({ place, cmd, efunc });

    /* Only create this for non-default values. Note that 1 is the default
     * value. We won't add that either, but parse 0 as 1 later.
     */
    if (!intp(tired) || (tired > 1))
    {
        if (!pointerp(tired_exits))
            tired_exits = ({ });

        tired_exits += allocate((sizeof(room_exits) / 3) - sizeof(tired_exits));
        tired_exits[sizeof(tired_exits) - 1] = tired;
    }

    /* Only create this for non-default values. */
    if (non_obvious)
    {
        if (!pointerp(non_obvious_exits))
            non_obvious_exits = ({ });

        non_obvious_exits += allocate((sizeof(room_exits) / 3) - sizeof(non_obvious_exits));
        non_obvious_exits[sizeof(non_obvious_exits) - 1] = non_obvious;
    }

    map(FILTER_LIVE(all_inventory()), &ugly_update_action(, cmd, unq_move));
    default_dirs -= ({ cmd });
    return 1;
}


/*
 * Function name: remove_exit
 * Description  : Remove one exit from the room.
 * Arguments    : string cmd - command of the exit to be removed.
 */
public int
remove_exit(string cmd)
{
    if (!pointerp(room_exits))
        return 0;
    if (query_prop(ROOM_I_NO_EXTRA_EXIT))
        return 0;

    int i;
    for (i = 1; i < sizeof(room_exits); i += 3)
    {
        if (cmd == room_exits[i])
        {
            room_exits = exclude_array(room_exits, i - 1, i + 1);
           i /= 3;
            if (i < sizeof(tired_exits))
            {
                tired_exits = exclude_array(tired_exits, i, i);
            }
            if (i < sizeof(non_obvious_exits))
            {
                non_obvious_exits = exclude_array(non_obvious_exits, i, i);
            }

            if (member_array(cmd, DEFAULT_DIRECTIONS) >= 0)
                default_dirs += ({ cmd });

            map(FILTER_LIVE(all_inventory()), &ugly_update_action(, cmd, unq_no_move));
            return 1;
        }
    }
    return 0;
}

/*
 * Function name: query_exit
 * Description:   Gives a list of the possible exits from this room.
 * Returns:       An array on the form below (example):
 *
 * ({
 *    "/room/church", "north" , "@@exitfun" ,
 *    "/room/post", "post" , "@@exitfun" ,
 * })
 *
 */
public mixed
query_exit()
{
    if (!pointerp(room_exits))
    {
        return ({ });
    }

    return ({ }) + room_exits;
}

/*
 * Function name: query_tired_exits
 * Description  : Gives a list of the tired values for each exit. The values
 *                may contain VBFC. Rather than using this function, you
 *                should better use the function query_tired_exit() for
 *                the particular exit you need.
 * Returns      : mixed - an array with one element per exit.
 */
public mixed
query_tired_exits()
{
    int *tired;
    int size;

    if (!pointerp(tired_exits))
    {
        tired = allocate(sizeof(room_exits) / 3);
    }
    else
    {
        tired = tired_exits + allocate((sizeof(room_exits) / 3) -
            sizeof(tired_exits));
    }

    size = sizeof(tired);
    while (--size >= 0)
    {
        if (tired[size] < 1)
        {
            tired[size] = 1;
        }
    }

    return tired;
}

/*
 * Function name: query_tired_exit
 * Description  : This function returns the tired value for a particular
 *                exit. It checks for VBFC and works out the virtual array
 *                used to keep the tired values. The default tired value
 *                is 1.
 * Arguments    : int index - the index-number of the exit to query.
 * Returns      : int - the tired value.
 */
public int
query_tired_exit(int index)
{
    int tired;

    if (index < sizeof(tired_exits))
    {
        tired = check_call(tired_exits[index]);
    }

    return ((tired > 0) ? tired : 1);
}

/*
 * Function name: exit_data
 * Description  : Internal helper which constructs the return value for
 *                a number of query_exit_ functions.
 * Arguments    : int offset - Which value from room_exits to return
 */
 private mixed *
 exit_data(int offset)
 {
    int size = pointerp(room_exits) ? sizeof(room_exits) / 3 : 0;
    mixed arr = allocate(size);

    for (int i = 0; i < size; i++) {
        arr[i] = room_exits[(i * 3) + offset];
    }

    return arr;
 }

/*
 * Function name: query_exit_rooms
 * Description  : Returns an array of strings containing the full path names
 *                to all rooms that are connected to this one by add_exit()'s.
 *                The values may contain VBFC.
 * Returns      : mixed - the array with exits.
 */
mixed
query_exit_rooms()
{
   return exit_data(0);
}

/*
 * Function name: query_exit_cmds
 * Description  : Returns an array of strings that the player can use as
 *                commands to move through an exit added by add_exit().
 * Returns      : string * - the array with commands.
 */
string *
query_exit_cmds()
{
    return exit_data(1);
}

/*
 * Function name: query_exit_functions
 * Description  : Returns an array of the functions called when a player goes
 *                through an exit added by add_exit(). This may be VBFC.
 * Returns      : mixed - the array of delays.
 */
mixed
query_exit_functions()
{
    return exit_data(2);
}

/*
 * Function name: query_exit_order
 * Description  : Returns a static array of the exits in the prefered sort
 *                order.
 * Returns:       string *
 */
string *
query_exit_order()
{
    return ({ "north", "northeast", "east", "southeast", "south", "southwest",
        "west", "northwest", "up", "down" });
}

/*
 * Function name: query_obvious_exits
 * Description  : Gives a list of the obvious exits from this room. When
 *                exits carry VBFC, this_player() is used to see whether he
 *                is able to see the exit.
 * Returns      : string * - An array (possibly empty) of the obvious exits.
 */
public string *
query_obvious_exits()
{
    int exit_count = sizeof(room_exits) / 3;
    if (!exit_count)
        return ({ });

    string *exit_order = query_exit_order();

    /* No non-obvious exits marked, so all exits are obvious. */
    if (!pointerp(non_obvious_exits))
    {
        string *obvious_exits = query_exit_cmds();
        string *sorted_exits = obvious_exits & exit_order;
        return sorted_exits + (obvious_exits - sorted_exits);
    }

    string *obvious_exits = ({ });
    int index = -1;
    int size = sizeof(non_obvious_exits);
    while (++index < exit_count)
    {
        if (index >= size || !check_call(non_obvious_exits[index]))
        {
            obvious_exits += ({ room_exits[(index * 3) + 1] });
        }
    }

    string *sorted_exits = obvious_exits & exit_order;
    return sorted_exits + (obvious_exits - sorted_exits);
}

/*
 * Function name: query_non_obvious_exits
 * Description  : Returns an array containing the non-obvious-exits flags.
 * Returns      : mixed - the non-obvious-exits array.
 */
public mixed
query_non_obvious_exits()
{
    if (!pointerp(non_obvious_exits))
        return allocate(sizeof(room_exits) / 3);

    return non_obvious_exits + allocate((sizeof(room_exits) / 3) -
                                        sizeof(non_obvious_exits));
}

/*
 * Function name: set_no_exit_msg
 * Description  : set the custom no-exit msg for direction(s). So instead of
 *                "There is no obvious exit west.", you can tell player
 *                "You wander west among the trees for a bit, then return to the road."
 * Arguments    : mixed exits - either a string or an array of strings with the
 *                    direction(s) for which this room does not have an exit.
 *                mixed msg - the message for these directions; supports VBFC.
 */
public void
set_no_exit_msg(mixed exits, mixed msg)
{
    /* No extra exits allowed. */
    if (query_prop(ROOM_I_NO_EXTRA_EXIT))
    {
        return;
    }

    if (!mappingp(no_exit_messages))
        no_exit_messages = ([ ]);

    if (pointerp(exits))
    {
        foreach(string exit: exits)
        {
            no_exit_messages[exit] = msg;
        }
    }
    else if (stringp(exits) && strlen(exits))
    {
        no_exit_messages[exits] = msg;
    }
}

/*
 * Function name: query_no_exit_msgs
 * Description  : Find out the exits that have a no-exit message.
 * Returns      : string * - list of exits with a no-exit message.
 */
public mixed
query_no_exit_msgs()
{
    return m_indices(no_exit_messages);
}

/*
 * Function name: query_no_exit_msg
 * Description  : Find out the specific no-exit messages per exit.
 * Arguments    : string exit - the exit for which to get the text for.
 * Returns      : mixed - the no-exit message (may be VFBC).
 */
public mixed
query_no_exit_msg(string exit)
{
    if (!mappingp(no_exit_messages))
        return 0;

    return no_exit_messages[exit];
}

/*
 * Function name: query_doors
 * Description  : Finds all door objects in this room.
 * Returns      : object * - the door objects.
 */
public object *
query_doors()
{
    return FILTER_DOOR_OBJECTS(all_inventory());
}

/*
 * Function name: query_door_cmds
 * Description  : Returns an array of strings that the player can use as
 *                commands to move through a doors in the room.
 * Returns      : string * - the array with commands.
 */
public string *
query_door_cmds()
{
    string *exits = ({ });

    foreach (object door: query_doors())
    {
        exits += door->query_pass_command();
    }

    return exits;
}
