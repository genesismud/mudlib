/*
 * /std/room/move.c
 *
 * This is a sub-part of /std/room.c
 *
 * It handles loading rooms and moving between rooms.
 */

#include <files.h>

static string  room_dircmd;  /* Command given after the triggering verb */
static object *link_ends,    /* Links to endpoints, filenames of rooms */
              *link_starts;  /* Link to cloned rooms, obj pointers */
static string  room_mlink;   /* For clones: master room */
static int     round_fatigue_up = random(2);

/*
 * Function name: link_room
 * Description:   Create a corridor room. Set the descriptions of it.
 *                Does not add exits. This is supposed to be replaced by
 *                the actual room inheriting this standard room. It is supposed
 *                to set proper descriptions (long and short) instead of
 *                copying the endpoint description. Corridor rooms are
 *		  created when the delay value in the third parameter to
 *		  add_exit() is a negative integer. Length of corridor is
 *		  the absolute value of the delay parameter.
 * Arguments:	  lfile: Suggested filename to clone to get a corridor room.
 *                dest:  The other endpoint of the corridor.
 *		  pos:   The position away from this startpoint
 * Returns:       Objectpointer of the cloned room.
 */
varargs public object
link_room(string lfile, mixed dest, int pos)
{
    object ob = clone_object(lfile);

    ob->set_short(query_short()); /* Default same short as starting point */
    ob->set_long(query_long()); /* Default same long as starting point */
    ob->set_add_item(query_item()); /*Default add_items same as starting pt */
    ob->add_prop(ROOM_I_LIGHT, query_prop_setting(ROOM_I_LIGHT));
    ob->add_prop(ROOM_I_INSIDE, query_prop_setting(ROOM_I_INSIDE));
    ob->add_prop(ROOM_I_TYPE, query_prop_setting(ROOM_I_TYPE));

    return ob;
}

/*
 * Function name: transport_to
 * Description:   Transport player to first linked room in clone chain.
 *                Also creates the corridor and links it to the other
 *                endpoint if the corridor does not exist.
 * Arguments:	  ex:    The direction command (north, south ...)
 *                room:  The destination objectp or stringp
 *                delay: The length of the cloned corridor.
 * Returns:       True if player moved to first pos in corridor.
 *                False if corridor could not be created.
 */
public int
transport_to(string ex, mixed room, int delay)
{
    int ne, c;
    string backstr;
    object ls, or, lastr;

    /* Find or == objectp, room = file_name */
    if (stringp(room))
    {
	or = find_object(room);
	if (!or && (LOAD_ERR(room)))
	    return 0;
	or = find_object(room);
    }
    else
    {
	or = room;
	if (!sscanf(file_name(or), "%s#", room))
	    room = file_name(or);
    }

    lastr = 0;

    /* Add one more if not there in the number of linked room chains from
     * this room. */
    if (sizeof(link_ends))
    {
	ne = member_array(room, link_ends);
	if (ne >= 0)
	    lastr = link_starts[ne];
	else
	    link_ends = link_ends + ({ room });
    }
    else
	link_ends = ({ room });

    /* Make the cloned corridor if it is not already made. Build by adding
     * one room at a time from the destination position. */
    if (!lastr)
    {
	/* Next to dest */
	if (!(lastr = this_object()->link_room(ROOM_OBJECT, or, delay)))
	    return 0;
	backstr = (string) or->make_link(this_object(), lastr);
	lastr->add_exit(or, ex, 0);
	lastr->link_master(room);    /* Destination is master of corridor */

	for (c = 1; c < delay; c++)
	{
	    ls = link_room(ROOM_OBJECT, or, delay - c);
	    ls->add_exit(lastr, ex, 0);
	    lastr->add_exit(ls, backstr, 0);
	    lastr = ls;
	    lastr->link_master(room);
	}
	lastr->add_exit(this_object(), backstr, 0);
	link_starts = (sizeof(link_starts)
		       ? link_starts + ({ lastr }) : ({ lastr }));
    }

    if (this_player()->move_living(ex, lastr))
	return 0;
    return 1;
}

/*
 * Function name: make_link
 * Description:   Find or make the return link. Called from other endpoint
 *                when linking a corridor between itself and this room.
 * Arguments:	  to_room:  File name of other endpoint
 *                via_link: Objectp to nearest cloned room in corridor.
 * Returns:       command used to move to other endpoint from this room.
 */
public string
make_link(mixed to_room, object via_link)
{
    int ne;
    mixed ex;

    if (!(sscanf(file_name(to_room), "%s#", to_room)))
	to_room = file_name(to_room);

    if (sizeof(link_ends))
    {
	ne = member_array(to_room, link_ends);
	if (ne >= 0)
	    link_starts[ne] = via_link;
	else
	{
	    link_ends = link_ends + ({ to_room });
	    ne = -1;
	}
    }
    else
    {
	link_ends = ({ to_room });
	ne = -1;
    }

    if (ne < 0)
    	link_starts = (link_starts ?
		       link_starts + ({ via_link }) : ({ via_link }));

    ex = query_exit();
    ne = member_array(to_room, ex);

    return (ne >= 0 ? ex[ne + 1] : "back");
    /* We can't go back when we are at the end point, ie one way corridor */
}

/*
 * Function name: link_master
 * Description:   Set the master room for this cloned corridor
 * Arguments:	  mfile: Filename of master room
 */
void
link_master(string mfile)
{
    room_mlink = mfile;
}

/*
 * Function name: query_link_master
 * Description:   Set the master room for this cloned corridor
 * Arguments:	  mfile: Filename of master room
 */
string
query_link_master()
{
    return room_mlink;
}

/*
 * Function name: load_room
 * Description  : Finds a room object for a given array index.
 * Arguments    : int index - of the file name entry in the room_exits array.
 * Returns      : object - pointer to the room corresponding to the argument
 *                         or 0 if not found.
 */
object
load_room(int index)
{
    mixed droom;
    string err;
    object ob;

    droom = check_call(room_exits[index]);
    if (objectp(droom))
    {
	return droom;
    }

    /* Handle linkrooms that get destructed, bad wizard... baa-aad wizard. */
    if (!stringp(droom))
    {
	remove_exit(room_exits[index + 1]);
	this_player()->move_living("X", query_link_master());
	return 0;
    }

    ob = find_object(droom);
    if (objectp(ob))
    {
	return ob;
    }

    if (err = LOAD_ERR(droom))
    {
	SECURITY->log_loaderr(droom, environment(this_object()),
			      room_exits[index + 1], this_object(), err);
	write("Err in load:" + err + " <" + droom +
	    ">\nPlease make a bugreport about this.\n");
	return 0;
    }
    return find_object(droom);
}

/*
 * Function name: query_dircmd
 * Description:   Gives the rest of the command given after move verb.
 *                This can be used in blocking functions (third arg add_exit)
 * Returns:       The movecommand as given.
 */
public string
query_dircmd()
{
    return room_dircmd;
}

/*
 * Function name: set_dircmd
 * Description:   Set the rest of the command given after move verb
 *		  This is mainly to be used from own move_living() commands
 *		  where you want a team to be able to follow their leader.
 * Arguments:     rest - The rest of the move command
 */
public void
set_dircmd(string rest)
{
    room_dircmd = rest;
}

/*
 * Function name: exit_move
 * Description:   Invoke the move routine which sends the actor through the
 *                exit into the next room. Basically it is a relay allowing
 *                coders to intercept the actual person->move_living() call.
 * Arguments:     string exit_cmd - the exit command, as provided to add_exit()
 *                object dest_room - the destination room
 * Returns:       int - the result of the move_living() call.
 */
public int
exit_move(string exit_cmd, object dest_room)
{
    return this_player()->move_living(exit_cmd, dest_room);
}

/*
 * Function name: unq_move
 * Description  : This function is called when a player wants to move through
 *                a certain exit. VBFC is applied to the first and third
 *                argument to add_exit(). Observe that if you have a direction
 *                command that uses its trailing test as in 'enter portal'
 *                and fails if the second word is not 'portal', your block
 *                function should return 2 as you want the rest of the dir
 *                commands to be tested.
 * Arguments    : string str - the command line argument.
 * Returns      : int 1/0.
 */
public int
unq_move(string str)
{
    int index;
    int size;
    int wd;
    int tired = 0;
    int tmp;
    object room;

    room_dircmd = str;
    index = -3;
    size = sizeof(room_exits);
    while((index += 3) < size)
    {
	/* This is not the verb we were looking for. */
	if (query_verb() != room_exits[index + 1])
	{
	    continue;
	}

	/* Players younger than 4 hours don't get tired from walking around in
         * the world.
	 */
	if (this_player()->query_age() > 14400)
	{
        tired = query_tired_exit(index / 3);
	    int enc = this_player()->query_encumberance_weight();

        /* Compute the fatigue bonus. Sneaking gives double fatigue and
         * so does moving with 80% encumberance, while 20% or less gives
         * only half the fatigue.
         */
        if (this_player()->query_prop(LIVE_I_SNEAK))
            tired *= 2;

        if (enc > 80)
            tired *= 2;
        if (enc < 20) {
            /* round_fatigue_up is 1 50% of the time */
            if (tired % 2)
                tired += round_fatigue_up;

            tired /= 2;
        }

	    /* Player is too tired to move. */
	    if (this_player()->query_fatigue() < tired)
	    {
		notify_fail("You are too tired to move in that direction.\n");
		return 0;
	    }
	}

	/* Check whether the player is allowed to use the exit. */
	if ((wd = check_call(room_exits[index + 2])) > 0)
	{
	    if (wd > 1)
		continue;
	    else
		return 1;
	}

	/* Room could not be loaded, error message is printed. */
	if (!objectp(room = load_room(index)))
	{
	    return 1;
	}

        /* Remove the fatigue after the exit has been checked. */
        if (tired)
        {
            this_player()->add_fatigue(-tired);
        }

	if ((wd == 0) ||
	    (!transport_to(room_exits[index + 1], room, -wd)))
	{
	    /* Move the player into the other room. */
            exit_move(room_exits[index + 1], room);
	}
	return 1;
    }

    /* We get here if a 'block' function stopped a move. The block function
     * should have printed a fail message.
     */
    return 0;
}

/*
 * Function name: unq_no_move
 * Description  : This function here so that people who try to walk into a
 *                'normal', but nonexistant direction get a proper fail
 *                message rather than the obnoxious "What?". Here, 'normal'
 *                exits are north, southeast, down, excetera.
 * Arguments    : string str - the command line argument.
 * Returns      : int 0 - always.
 */
public int
unq_no_move(string str)
{
    string verb = query_verb();

    if (mappingp(no_exit_messages) && no_exit_messages[verb])
    {
        notify_fail(check_call(no_exit_messages[verb]));
        return 0;
    }
    notify_fail("There is no obvious exit " + verb + ".\n");
    return 0;
}

#if 0
/*
 * Function name: query_room_env
 * Description  : Define this function if your room is nested in another
 *                room to indicate where we are. For instance a ship or
 *                carriage that moves through another location.
 * Returns      : object room - the room we are (supposedly) in.
 */
public object
query_room_env()
{
     return 0;
}
#endif 0
