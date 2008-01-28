/*
 * /std/living/wield.c
 *
 * A sub-part of /std/living.c that deals with wielding weapons.
 */

#include <composite.h>
#include <stdproperties.h>

public mixed query_weapon(int which);

/*
 * Function name: wield_reset
 * Description:   initialize the wield routines
 */
static nomask void
wield_reset()
{
    add_subloc(SUBLOC_WIELD, this_object());
}

/*
 * Function name: show_wielded
 * Description:   Display the items that are worn
 * Arguments:     object for_obj - the onlooker
 * Returns:       A string describing the worn items
 */
public string
show_wielded(object for_obj)
{
    mixed *a;
    string str, p, pr;

    a = query_weapon(-1);

    if (!sizeof(a))
    {
        return "";
    }
    
    if (for_obj != this_object())
    {
        p = this_object()->query_possessive();
        pr = capitalize(this_object()->query_pronoun()) + " is";
    }
    else
    {
        p = "your";
        pr = "You are";
    }

    a = map(a, &->query_wield_desc(p)) - ({ 0 });

    /* Only use table form when displaying inventory. */
    if (for_obj->query_option(OPT_TABLE_INVENTORY) &&
        for_obj->query_prop(TEMP_SUBLOC_SHOW_ONLY_THINGS))
    {
        return HANGING_INDENT("Wielded : " + COMPOSITE_WORDS(a), 10, 0);
    }
    else
    {
        str = pr + " wielding " + COMPOSITE_WORDS(a) + ".";
        return HANGING_INDENT(str, 2, 0);
    }
}

/*
 * Function name: wield
 * Description:   Wield a weapon
 * Arguments:     The weapon to wield
 * Returns:       string - error message (weapon not wielded)
 *                1 - successs (weapon wielded)
 */
public mixed
wield(object weapon)
{
    mixed val;

    if (stringp(val = occupy_slot(weapon)))
    {
        return val;
    }

    if (weapon->move(this_object(), SUBLOC_WIELD))
    {
        clear_tool_slots(weapon);
        return "You cannot wield this weapon for some strange reason.\n";
    }

    combat_reload();

    if (stringp(val = query_combat_object()->cb_wield_weapon(weapon)))
    {
        clear_tool_slots(weapon);
        weapon->move(this_object());
        return val;
    }

    return 1;
}

/* 
 * Function name: unwield
 * Description:   Unwield a weapon
 * Arguments:     The weapon to unwield
 */
public void
unwield(object weapon)
{
    /* We do this to remove the weapon from the wielded-subloc. */
    if (environment(weapon) == this_object())
    {
        weapon->move(this_object());
    }

    empty_slot(weapon);

    /* No need for a combat reload */

    query_combat_object()->cb_unwield(weapon);
}

static int
check_weapon_object(object wep)
{
    return (wep->check_weapon());
}

/*
 * Function name: query_weapon
 * Description  : Returns the weapon held in a specified location or all the
 *                weapons the living wields when -1 is given as argument.
 * Arguments    : int which - a numeric label describing a weapon location.
 *                            On humanoids this is W_RIGHT etc. Give -1 to
 *                            list all weapons.
 * Returns      : object   - the corresponding weapon or 0.
 *                object * - all weapons in an array for which == -1.
 */
public mixed
query_weapon(int which)
{
    object weapon;

    if (which == -1)
    {
        return filter(query_tool(-1), check_weapon_object);
    }

    if ((weapon = query_tool(which)) &&
        (weapon->query_attack_id() == which) &&
        check_weapon_object(weapon))
    {
        return weapon;
    }

    return 0;
}
