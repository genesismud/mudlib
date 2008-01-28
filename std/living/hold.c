/*
 * /std/living/hold.c
 *
 * Routines related to holdable objects.
 */

#include "wa_types.h"
#include <composite.h>

/*
 * Function name: hold_reset
 * Description:   initialize holding routines
 */
nomask void
hold_reset()
{
    add_subloc(SUBLOC_HELD, this_object());
}

/*
 * Function name: hold
 * Description:   Hold an item
 * Arguments:     object tool - the item to hold
 * Returns:       1 - item successfully held
 *                string - An error message (item not held)
 */
public int
hold(object tool)
{
    mixed res;

    res = occupy_slot(tool);
  
    if (!stringp(res))
    {
        query_combat_object()->cb_modify_procuse();
        tool->move(this_object(), SUBLOC_HELD);
        return 1;
    }

    return res;
}

/*
 * Function name: release
 * Description:   Release a held item
 * Arguments:     object tool - the object to release
 */
public void
release(object tool)
{
    empty_slot(tool);
    query_combat_object()->cb_modify_procuse();
}

/*
 * Function name: show_held
 * Description:   Describe the items that are currently held
 * Arguments:     object ob - the onlooker
 * Returns:       The (string) description
 */
public string
show_held(object for_obj)
{
    mixed *a;
    int il, size;
    string str, p, pr;

    a = (object *)this_object()->query_tool(-1) - 
        (object *)this_object()->query_weapon(-1) -
        (object *)this_object()->query_armour(-1);

    if (!sizeof(a = filter(a, objectp)))
    {
        return "";
    }
    
    if (for_obj != this_object())
    {
        p = query_possessive();
        pr = capitalize(query_pronoun()) + " is";
    }
    else
    {
        p = "your";
        pr = "You are";
    }

    a = filter(map(a, &->query_hold_desc(for_obj)), stringp);
    /* Only use table form when displaying inventory. */
    if (for_obj->query_option(OPT_TABLE_INVENTORY) &&
        for_obj->query_prop(TEMP_SUBLOC_SHOW_ONLY_THINGS))
    {
        return HANGING_INDENT("Held    : " + COMPOSITE_WORDS(a), 10, 0);
    }
    else
    {
        str = pr + " holding " + COMPOSITE_WORDS(a) + ".";
        return HANGING_INDENT(str, 2, 0);
    }
}
