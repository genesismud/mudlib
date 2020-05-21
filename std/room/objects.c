/*
 * /std/room/objects.c
 *
 * Module of /std/room.c
 *
 * Provides easy methods for adding objects to rooms.
 */

/*
 * Function Name: add_npc
 * Description  : Clone a npc to the room and reset it automatically
 *                when it has been killed.
 * Arguments    : string file - the file to clone
 *                int count   - how many of this item.
 *                function init - (optional) If you want a function to be
 *                                called in the npc add it here. example
 *                                &->arm_me() to have arm_me called.
 */
varargs void
add_npc(string file, int count = 1, function init_call = 0)
{
    add_auto_object(file, count, &objectp(), init_call);
}

/*
 * Function name: room_add_object
 * Description:   Clone and move an object into the room
 * Arguments:	  file - What file it is we want to clone
 *		  num  - How many clones we want to have, if not set 1 clone
 *		  mess - Message to be written when cloned
 */
varargs void
room_add_object(string file, int num, string mess)
{
    int i;
    object ob;

    if (num < 1)
	num = 1;

    seteuid(getuid());
    for (i = 0; i < num; i++)
    {
	ob = clone_object(file);
	if (stringp(mess))
	{
	    tell_room(this_object(), mess);
	    ob->move(this_object(), 1);
 	}
	else if (living(ob))
	    ob->move_living("xx", this_object());
	else
	    ob->move(this_object(), 1);
    }
}
