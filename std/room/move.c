/* 
 * /std/room/move.c
 *
 * This is a sub-part of /std/room.c
 *
 * It handles loading rooms and moving between rooms.
 */

static string room_dircmd;   /* Command given after the triggering verb */

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
	    tmp = this_player()->query_encumberance_weight();

	    /* Compute the fatigue bonus. Sneaking gives double fatigue and
	     * so does talking with 80% encumberance, while 20% or less gives
             * only half the fatigue.
	     */
	    tired = (this_player()->query_prop(LIVE_I_SNEAK) ?
		(tired * 2) : tired);
	    tired = ((tmp > 80) ? (tired * 2) :
                ((tmp < 20) ? (tired / 2) : tired));

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
    notify_fail("There is no obvious exit " + query_verb() + ".\n");
    return 0;
}
