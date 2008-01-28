/*
 * /std/living/wear.c
 *
 * A subpart of /std/living.c containing some wearable related functions.
 *
 * $Id:$
 *
 */

#include <composite.h>
#include <wa_types.h>

static private mapping m_worn = ([]);

public object *query_clothing(int slot);

/*
 * Function name: wear_reset
 * Description:   initialize the wear routines
 */
static nomask void
wear_reset()
{
    add_subloc(SUBLOC_WORNA, this_object());
}

/*
 * Function name: show_worn
 * Description:   Display the items that are worn
 * Arguments:     object for_obj - the onlooker
 * Returns:       A string describing the worn items
 */
public string
show_worn(object for_obj)
{
    object *worn = query_clothing(-1);
    string str;
    
    if (!sizeof(worn))
        return "";

    /* Only use table form when displaying inventory. */
    if (for_obj->query_option(OPT_TABLE_INVENTORY) &&
        for_obj->query_prop(TEMP_SUBLOC_SHOW_ONLY_THINGS))
    {
        return HANGING_INDENT("Worn    : " +
            FO_COMPOSITE_ALL_DEAD(worn, for_obj), 10, 0);
    }
    else
    {
        str = ((for_obj == this_object()) ? "You are" : 
            capitalize(query_pronoun()) + " is") + " wearing " +  
            FO_COMPOSITE_ALL_DEAD(worn, for_obj) + ".";
        return HANGING_INDENT(str, 2, 0);
    }
}

/*
 * Function name: occupy_clothing_slots
 * Description:   Add a piece of clothing to the slots where it is worn
 * Arguments:     The clothing being added
 * Returns:       string - error message (clothing could not be added)
 *                1 - success (clothing added)
 */
public mixed
occupy_clothing_slots(object ob)
{
    int *slots, i;

    slots = ob->query_slots();

    for (i = 0; i < sizeof(slots); i++)
        if (!pointerp(m_worn[slots[i]]))
            m_worn[slots[i]] = ({ ob });
        else
            m_worn[slots[i]] += ({ ob });

    return 1;
}

/* 
 * Function name: clear_clothing_slots
 * Description:   remove a piece of clothing from the slots where it is worn
 * Arguments:     the clothing object being removed
 */
static void
clear_clothing_slots(object ob)
{
    int *slots, i;

    slots = m_indices(m_worn);
    for (i = 0; i < sizeof(slots); i++)
        if (!pointerp(m_worn[slots[i]]) ||
            !sizeof(m_worn[slots[i]] -= ({ ob })))
            m_delkey(m_worn, slots[i]);
}

/*
 * Function name: remove_clothing
 * Description:   Remove an item that has been worn
 * Arguments:     object ob - the item we wish to remove
 */
public void
remove_clothing(object ob)
{
    clear_clothing_slots(ob);

    /* We do this to remove the item from the worn-subloc. */
    if (environment(ob) == this_object())
        ob->move(this_object());
}

/*
 * Function name: wear_clothing
 * Description:   Wear something
 * Arguments:     object ob - the item to be worn
 * Returns:       string - an error message (the item cannot be worn)
 *                1 - item successfully worn
 */
public mixed
wear_clothing(object ob)
{
    int *slots, i;

    if (ob->move(this_object(), SUBLOC_WORN))
    {
        return "You cannot wear the " + ob->short(this_object()) +
            " for some reason.\n";
    }

    return occupy_clothing_slots(ob);
}

/*
 * Function name: query_clothing
 * Description:   See what is worn on a particular location or get
 *                all items worn.
 * Arguments:     int slot - the location to check for clothing or -1
 *                           to get all clothing worn.
 * Returns:       (object *) all items worn on the specified location
 */
public object *
query_clothing(int slot)
{
    mixed *values;
    object *all_worn;
    int i;

    if (slot == -1)
    {
        all_worn = ({});
        values = m_values(m_worn);

        for (i = 0; i < sizeof(values); i++)
            all_worn |= values[i];

        return all_worn;
    }

    return (m_worn[slot] || ({}));
}

/*
 * Function name: wear_arm
 * Description:   wear an armour
 * Arguments:     The armour to be worn
 * Returns:       string - error message (armour could not be worn)
 *                1 - success (armour was worn)
 */
public mixed
wear_arm(object arm)
{
    mixed val;

    if (stringp(val = occupy_slot(arm)))
        return val;

    if (arm->move(this_object(), SUBLOC_WORNA))
    {
        clear_tool_slots(arm);
        return "You cannot wear the " + arm->short(this_object()) +
            " for some reason.\n";
    }

    if (stringp(val = occupy_clothing_slots(arm)))
    {
        arm->move(this_object());
        clear_tool_slots(arm);
        return val;
    }

    combat_reload();

    if (stringp(val = query_combat_object()->cb_wear_arm(arm)))
    {
        arm->move(this_object());
        clear_tool_slots(arm);
        clear_clothing_slots(arm);
        return val;
    }

    return 1;
}

/*
 * Function name:   remove_arm
 * Description:     Remove an armour
 * Arguments:       arm - The armour.
 */
public void
remove_arm(object arm)
{
    if (environment(arm) == this_object())
        arm->move(this_object());

    clear_clothing_slots(arm);
    clear_tool_slots(arm);

    /* No need for a combat reload */

    query_combat_object()->cb_remove_arm(arm);
}

static int
check_armour_object(object arm)
{
    return (arm->check_armour() && (arm->query_worn() == this_object()));
}

/*
 * Function name: query_armour
 * Description  : Returns the armour of a given position or lists all armours
 *                worn when -1 is given as argument.
 * Arguments    : int which - a numeric label describing an armour location.
 *                            On humanoids this is TS_HEAD etc. Give -1 to
 *                            list all.
 * Returns      : object   - the corresponding armour or 0.
 *                object * - all armours when -1 is given.
 */
varargs public mixed
query_armour(int which) 
{
    object arm;

    if (which == -1)
        return filter(query_tool(-1), check_armour_object);

    if ((arm = query_tool(which)) && check_armour_object(arm))
        return arm;

    return 0;
}
