/*
 * /std/living/tool.c
 *
 * General routines for using tool slots
 */

static mapping tool_slots = ([]);  /* The object occupying a certain slot */ 

/*
 * Function name: clear_tool_slots
 * Description:   remove all tool slots occupied by the given object
 * Arguments:     the object to look for
 * Returns:       1/0 - slots removed/no slots removed
 */
static nomask int
clear_tool_slots(object tool)
{
    int size = m_sizeof(tool_slots);
    tool_slots = filter(tool_slots, &operator(!=)(, tool));
    return (m_sizeof(tool_slots) != size);
}

/*
 * Function name: occupy_slot
 * Description:   Try to put the specified object in its slot(s)
 * Arguments:     The object to be put in the slot(s)
 * Returns:       1 - slots successfully occupied
 *                string - An error message
 */
nomask mixed
occupy_slot(object tool)
{
    int i, size, *slots;
    string name;

    slots = tool->query_slots(); 
    size = sizeof(slots);

    /*
     * Are all the slots it needs free?
     */
    i = -1;
    while (++i < size)
    {
        if (tool_slots[slots[i]])
	{
            name = tool_slots[slots[i]]->short();
            name = (strlen(name) ? "The " + name : "Something");
            return name + " is in the way.\n";
	}
    }

    i = -1;
    while(++i < size)
    {
        tool_slots[slots[i]] = tool;
    }
    
    return 1;
}

/*
 * Function name: empty_slot
 * Description:   Clear a given item's slots and move it out of its subloc
 * Arguments:     object tool - the object to look for
 * Returns:       1/0 - slots remove/slots not removed
 */
nomask int
empty_slot(object tool)
{
    if (!clear_tool_slots(tool))
    {
       return 0;
    }

    /* Move the tool from its subloc */
    if (environment(tool) == this_object())
    {
        tool->move(this_object());
    }

    return 1;
}

/*
 * Function name: query_tool
 * Description:   Get the item occupying a given slot or all items in tool slots
 * Arguments:     int slot - the slot to check or -1 for all items
 * Returns:       If a slot is given as an argument, the object occupying 
 *                the slot or 0.  If -1 is given as an argument, an array
 *                of objects.
 */
public mixed
query_tool(int slot)
{
    object *tools, *values, tool, tmp;
    int i;

    if (slot >= 0)
    {
        for (i = 1; i <= slot; i *= 2)
	{
            if (i & slot)
	    {
                tmp = tool_slots[i];
                if (!tmp || (tool && (tmp != tool)))
		{
                    return 0;
		}

                tool = tmp;
	    }
	}

        return tool;
    }

    tools = ({});
    values = m_values(tool_slots);
    for (i = 0; i < sizeof(values); i++)
    {
        tools |= ({ values[i] });
    }

    return tools;
}

/*
 * Function name: query_tool_map
 * Description:   Get the tool mapping, which maps each occupied slot to the
 *                item it contains
 * Returns:       The tool mapping
 */
public mapping
query_tool_map()
{
    return tool_slots + ([]);
}
