/*
 * /std/room/exits.c
 *
 * This module is a part of /std/room.c
 *
 * It contains the functions and data related to the exits of a room.
 */

#include <filter_funs.h>
#include <filepath.h>

/*
 * Global variables.
 */
static mixed room_exits;
static mixed tired_exits;
static mixed non_obvious_exits;
static int   room_no_obvious;

/*
 * Prototype
 */
int unq_move(string str);
int unq_no_move(string str);

/*
 * Fix to get rid of the obnoxius 'What ?' when we try to walk in a nonexistant
 * direction. These are the default direction commands.
 */
#define DEF_DIRS ({ "north", "south", "east", "west", "up", "down", \
		    "northeast", "northwest", "southeast", "southwest" })

/*
 * Function name: ugly_update_function
 * Description  : Just fix so the verb is updated in a room when an exit
 *                is added or removed.
 * Arguments    : string cmd - the command verb.
 *		  int add    - add command if 1 else remove.
 */
public void
ugly_update_action(string cmd, int add)
{
    object *livings;
    object old_tp;
    int index;
    int size;

    old_tp = this_player();
    livings = FILTER_LIVE(all_inventory(this_object()));
    size = sizeof(livings);
    index = -1;
    while(++index < size)
    {
	set_this_player(livings[index]);
	if (add == 1)
	    add_action(unq_move, cmd);
	else
	    add_action(unq_no_move, cmd);
    }

    if (objectp(old_tp))
    {
	set_this_player(old_tp);
    }
}

/*
 * Function name: init
 * Description  : Add direction commands to livings in the room.
 */
public void
init()
{
    int index;
    int size;
    string *dd;

    ::init();

    dd = DEF_DIRS;
    index = -2;
    size = sizeof(room_exits);
    while((index += 3) < size)
    {
	add_action(unq_move, room_exits[index]);
	dd -= ({ room_exits[index] });
    }

    index = -1;
    size = sizeof(dd);
    while(++index < size)
    {
	add_action(unq_no_move, dd[index]);
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
    if (stringp(place) &&
	(place[0] != '/') &&
	(place[0] != '@'))
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

	tired_exits +=
            allocate((sizeof(room_exits) / 3) - sizeof(tired_exits));
	tired_exits[sizeof(tired_exits) - 1] = tired;
    }

    /* Only create this for non-default values. */
    if (non_obvious)
    {
	if (!pointerp(non_obvious_exits))
	    non_obvious_exits = ({ });

	non_obvious_exits +=
            allocate((sizeof(room_exits) / 3) - sizeof(non_obvious_exits));
	non_obvious_exits[sizeof(non_obvious_exits) - 1] = non_obvious;
    }

    ugly_update_action(cmd, 1);
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
    int i;

    if (!pointerp(room_exits))
	return 0;
    if (query_prop(ROOM_I_NO_EXTRA_EXIT))
	return 0;

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
	    ugly_update_action(cmd, 0);
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
    while(--size >= 0)
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
 * Function name: query_exit_rooms
 * Description  : Returns an array of strings containing the full path names
 *                to all rooms that are connected to this one by add_exit()'s.
 *                The values may contain VBFC.
 * Returns      : mixed - the array with exits.
 */
mixed
query_exit_rooms()
{
    int index;
    int size;
    mixed exits;

    if ((size = sizeof(room_exits)) < 3)
    {
	return ({ });
    }

    exits = ({ });
    index = -3;
    while((index += 3) < size)
    {
	exits += ({ room_exits[index] });
    }
    return exits;
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
    int index;
    int size;
    string *cmds;

    if ((size = sizeof(room_exits)) < 3)
    {
	return ({ });
    }

    cmds = ({ });
    index = -2;
    while((index += 3) < size)
    {
	cmds += ({ room_exits[index] });
    }
    return cmds;
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
    int index;
    int size;
    mixed delays;

    if ((size = sizeof(room_exits)) < 3)
    {
	return ({ });
    }

    delays = ({ });
    index = -1;
    while((index += 3) < size)
    {
	delays += ({ room_exits[index] });
    }
    return delays;
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
    int index;
    int size;
    int size_cmds;
    string *obvious_exits;

    /* No non-obvious exits marked, so all exits are obvious. */
    if (!pointerp(non_obvious_exits))
    {
	return query_exit_cmds();
    }

    size_cmds = sizeof(room_exits) / 3;
    if (!size_cmds)
    {
	return ({ });
    }

    obvious_exits = ({ });
    index = -1;
    size = sizeof(non_obvious_exits);
    while(++index < size_cmds)
    {
	if (index >= size ||
	    !check_call(non_obvious_exits[index]))
	{
	    obvious_exits += ({ room_exits[(index * 3) + 1] });
	}
    }

    return obvious_exits;
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
    {
	return allocate(sizeof(room_exits) / 3);
    }

    return non_obvious_exits + allocate((sizeof(room_exits) / 3) -
					sizeof(non_obvious_exits));
}
