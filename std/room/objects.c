/*
 * /std/room/objects.c
 *
 * Module of /std/room.c
 *
 * Provides easy methods for adding objects to rooms.
 */
static mapping room_objects;

/*
 * Function Name: reset_auto_objects
 * Description  : Reset any items on the list of objects to automatically
 *                reset.
 *                It will only clone 15 items at a time, if more is needed
 *                they will be cloned after a short delay.
 */
void
reset_auto_objects()
{
    int clone_count;
    
    if (!mappingp(room_objects))
        return;
    
    foreach (string file, mixed data : room_objects)
    {
        /* This code is relying heavily on the copy-by-reference of arrays */
        int count = data[0];
        function condition = data[1];
        function init_call = data[2];
        object *clones = data[3];
        
        clones = filter(clones, objectp);
        
        if (functionp(condition))
            clones = filter(clones, condition);
        
        while (sizeof(clones) < count)
        {
            object ob;

            ob = clone_object(file);
            if (!objectp(ob))
                return;
            
            if (functionp(init_call))
                init_call(ob);

            if (living(ob))
                ob->move_living("xx", this_object(), 1, 1);
            else
                ob->move(this_object(), 1);
            
            clones += ({ ob });

            clone_count++;
            if (clone_count > 15)
            {
                set_alarm(5.0 * rnd(), 0.0, &reset_auto_objects());
                return;
            }
        }
        
        data[3] = clones;
        
    }
}


/*
 * Function Name: add_auto_object
 * Description  : Add an object to the list of objects that should
 *                be automatically reset.
 * 
 *                NOTE: In most cases you don't want this function, you wan't
 *                      the interfaces add_npc or add_object
 *                      
 * Arguments    : string file - The file to clone
 *                int count   - How many should be cloned.
 *                function condition - Whas is the condition for the cloning
 *                                     to take place.
 *                function init - Can be a set to a function to be called in
 *                                the newly cloned object.
 */
varargs void
add_auto_object(string file, int count = 1, function condition = 0,
    function init_call = 0)
{
    if (!stringp(file))
        return 0;

    if (!mappingp(room_objects))
        room_objects = ([ ]);
    
    room_objects[file] = ({ count, condition, init_call, ({ }) });

    /* Reset has to be started for the cloning to be done */
    if (!query_reset_active())
    {
        enable_reset();
    }
}


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
 * Function Name: add_object
 * Description  : Clone an object into the room, reset it when
 *                it has been removed from the room.
 * Arguments    : string file - the file to clone
 *                int count   - how many of this item.
 *                function init - (optional) If you want a function to be
 *                                called in the object add it here.
 *                                For example &->add_name("extra name")
 *                                to have add_name called.
 */
varargs void
add_object(string file, int count = 1, function init_call = 0)
{
    add_auto_object(file, count,
        &operator(==)(, this_object()) @ &environment(), init_call);
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
