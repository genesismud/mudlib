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
show_held(object ob)
{
    mixed *a;
    int il, size;
    string str, p, pr;

    a = (object *)this_object()->query_tool(-1) - 
        (object *)this_object()->query_weapon(-1) -
        (object *)this_object()->query_armour(-1) - ({ 0 });

    if (!sizeof(a))
    {
        return "";
    }
    
    if (ob != this_object())
    {
        p = query_possessive();
        pr = capitalize(query_pronoun()) + " is";
    }
    else
    {
        p = "your";
        pr = "You are";
    }

    a = map(a, &->query_hold_desc(ob)) - ({ 0 });
    str = pr + " holding " + COMPOSITE_WORDS(a) + ".";

    return HANGING_INDENT(str, 2, 0);
}
