/*
 * /std/weapon.c
 *
 * Contains all routines relating to weapons of any kind.
 * Defined functions and variables:
 *
 * set_hit(int)         Sets the weapon "to hit" value.
 *
 * set_pen(int)         Sets the weapon penetration value.
 *
 * set_pm(int *)        Sets some modifiers for the penetration on the weapon.
 *                      only useful if weapon is of multiple damage type.
 *
 * set_wt(type)         Set the weapon type.
 *                      If not defined, set to default.
 *
 * set_dt(type)         Set the damage type.
 *                      If not defined, set to default.
 *
 * set_hands(which)     Set the hand(s) used to wield the weapon.
 *                      If not defined, set to default.
 *
 * set_ac_mod(int)      Sets the armour class modifier for the weapon.
 *
 * set_wf(obj)          Sets the name of the object that contains the function
 *                      to call for extra defined wield() and unwield()
 *                      functions.
 *
 * query_wielded()      Returns the wielder if this object is wielded
 *
 * Weapons recover by default.
 */

#pragma save_binary
#pragma strict_types

inherit "/std/object";

#include <cmdparse.h>
#include <files.h>
#include <formulas.h>
#include <language.h>
#include <macros.h>
#include <ss_types.h>
#include <stdproperties.h>
#include <wa_types.h>

static int      wep_hit,        /* Weapon to hit */
                wep_pen,        /* Weapon penetration */
                wep_wt,         /* Weapon type */
                wep_dt,         /* Damage type */
                wep_hands,      /* How many hands the weapon takes */
                wielded,        /* Wielded or not */
                wielded_in_hand,/* Wielded in which hand */
                *m_pen,         /* Modifiers for the wep_pen */
                ac_modifier,    /* Modifier for the armour class */
                hits,           /* No of hits the weapon have made */
                dull,           /* How dull the weapon has become */
                corroded,       /* Corrotion on the weapon */
                repair_dull,    /* How much dullness we have repaired */
                repair_corr,    /* How much corrosion we have repaired */
                likely_corr,    /* How likely will this weapon corrode */
                likely_break,   /* How likely will this weapon break? */
                likely_dull,    /* How likely will it be dulled by battle? */
                max_value;      /* Value of weapon at prime condition */
static object   wielder,        /* Who is holding it */
                wield_func;     /* Object that defines extra wield/unwield */
static private int      will_not_recover;       /* True if it won't recover */

/*
 * Prototypes
 */
string  wep_condition_desc();
void    update_prop_settings();
varargs void remove_broken(int silent = 0);

/*
 * Function name: create_weapon
 * Description  : Create the weapon. You must define this function to
 *                construct the weapon.
 */
public void
create_weapon()
{
}

/*
 * Function name: create
 * Description  : Create the weapon. As this function is declared nomask
 *                you must use the function create_weapon to actually
 *                construct it. This function does some basic initializing.
 */
public nomask void
create_object()
{
    wep_wt = F_WEAPON_DEFAULT_WT;
    wep_dt = F_WEAPON_DEFAULT_DT;
    likely_dull = 10;
    likely_corr = 10;
    likely_break = 10;
    wep_hands = F_WEAPON_DEFAULT_HANDS;
    set_name("weapon");
    set_pname("weapons");
    add_adj("unwielded");
    wielded = 0;
    add_prop(OBJ_I_VALUE, "@@query_value");
    add_prop(OBJ_I_VOLUME, 500);
    add_prop(OBJ_I_WEIGHT, 200);
    m_pen = ({ 0, 0, 0 });
    ac_modifier = 0;

    /* If this will be true, the weapon will not recover on reboot and is
     * lost after the reboot.
     */
    will_not_recover = (random(100) < PERCENTAGE_OF_RECOVERY_LOST);

    create_weapon();

    update_prop_settings();
}

/*
 * Function name: reset_weapon
 * Description  : This function can be used to reset the weapon at a regular
 *                interval. However, if you do so, you must also call the
 *                function enable_reset() from your create_weapon() function
 *                in order to start up the reset proces.
 */
public void
reset_weapon()
{
}

/*
 * Function name: reset_object
 * Description  : Reset the weapon. If you want to make the weapon reset
 *                at a certain interval you must use reset_weapon as this
 *                function is nomasked.
 */
public nomask void
reset_object()
{
    reset_weapon();
}

/*
 * Function name: short
 * Description  : The short description. We modify it when the weapon is
 *                broken. There is a little caveat if the wizard has not
 *                set a short description since it will double the
 *                adjective 'broken'.
 * Arguments    : object for_obj - the object that wants to know.
 * Returns      : string - the description.
 */
public varargs string
short(object for_obj)
{
    if (query_prop(OBJ_I_BROKEN))
    {
        return "broken " + ::short(for_obj);
    }

    return ::short(for_obj);
}

/*
 * Function name: plural_short
 * Description  : The plural short description. When the weapon is broken,
 *                we alter it. See 'short' for details.
 * Arguments    : object for_obj - the object that wants to know.
 * Returns      : string - the description.
 */
public varargs string
plural_short(object for_obj)
{
    string str = ::plural_short(for_obj);

    /* We make this additional check for stringp(str) because if no plural
     * short has been set, we shoudln't alter it. The plural short will
     * be generated if no plural short has been set.
     */
    if (query_prop(OBJ_I_BROKEN) && stringp(str))
    {
        return "broken " + str;
    }

    return str;
}

/*
 * Function name: long
 * Description  : The long description. We add the information about the
 *                condition to it.
 * Arguments     : string str - a possible add-item to look for.
 *                 object for_obj - the object that wants to know.
 * Returns       : string - the long description.
 */
public varargs string   
long(string str, object for_obj)
{ 
    return ::long(str, for_obj) + (str ? "" : wep_condition_desc());
}

/*
 * Function name: wield_me
 * Description  : When the player tries to wield this weapon, this function
 *                is called. If the player managed to wield the weapon,
 *                the message is printed.
 * Returns      : int 1  - success
 *                string - a fail message (nothing is printed).
 */
nomask mixed
wield_me()
{
    int wret, skill, pen;
    string penuse, wfail;

    if (wielded)
        return "You already wield " + LANG_THESHORT(this_object()) + ".\n";
    else if (this_player() != environment(this_object()))
        return "You must carry the " + short() + " to be able to wield it.\n";
    else if (query_prop(OBJ_I_BROKEN))
        return "The " + short() + " is broken and not wieldable.\n";

    wielder = this_player();

    /* 
     * Check for a hand to wield the weapon in.
     */
    wielded_in_hand = wep_hands;
    if (wep_hands != W_ANYH)
    {
        /* 
         * Anything in both hands
         */
        if (wielder->query_tool(W_BOTH) && wep_hands < W_FOOTR)
        {
            return "Your hands seem busy with other things.\n";
        }

        /* 
         * Anything in the specified hand
         */
        if (wielder->query_tool(wep_hands))
        {
            return "The " + wielder->query_tool(wep_hands)->short() +
                " is in the way.\n";
        }
    
        if ((wep_hands == W_BOTH) &&
            (wielder->query_tool(W_RIGHT) || wielder->query_tool(W_LEFT)))
        {
            return "You need both hands to wield it.\n";
        }
    }
    else if (!wielder->query_tool(W_BOTH) && 
             !wielder->query_tool(W_RIGHT))
    {
        wielded_in_hand = W_RIGHT;
    }
    else if (!wielder->query_tool(W_BOTH) && 
             !wielder->query_tool(W_LEFT))
    {
        wielded_in_hand = W_LEFT;
    }
    else
    {
        return "You have no hand left to wield that weapon.\n";
    }

    if (stringp(wfail = wielder->wield(this_object())))
    {
        return wfail;
    }

    wret = 0;

    /*
     * A wield function in another object.
     */
    if ((!wield_func) || (!(wret=wield_func->wield(this_object()))))
    {
        if (wielded_in_hand == W_BOTH)
            write("You wield " + LANG_THESHORT(this_object()) +
		" in both your hands.\n");
        else if (wielded_in_hand < W_FOOTR)
            write("You wield " + LANG_THESHORT(this_object()) + " in your " +
                (wielded_in_hand == W_RIGHT ? "right" : "left") + 
                " hand.\n");
        else
            write("You wield " + LANG_THESHORT(this_object()) + " on your " +
                (wielded_in_hand == W_FOOTR ? "right" : "left") + 
                " foot.\n");

        say(QCTNAME(this_player()) + " wields " + 
            this_player()->query_possessive() + " " +
            QSHORT(this_object()) + ".\n");
    }
    if (intp(wret) && wret >= 0)
    {
        wielded = 1;
        set_adj("wielded");
        remove_adj("unwielded");
        return 1;
    }

    if (stringp(wfail = wielder->unwield(this_object())))
    {
        /* This is serious and almost fatal, please panic! */
        wielded = 1;
        set_adj("wielded");
        remove_adj("unwielded");
        return 1;
    }


    /*
     * If the wieldfunc returned a value <0 the we can not wield
     * likewise if it returned a string, but then we use that string
     * as error message.
     */
    if (stringp(wret))
        return wret;
    else 
        return "You cannot wield " + LANG_THESHORT(this_object()) + ".\n";
}

/*
 * Function name: command_wield
 * Description:   Called to wear this armour.
 * Returns:       see wield_me()
 */
mixed
command_wield()
{
    return wield_me();
}

/*
 * Function name: unwield_me
 * Description:   unwield this weapon
 * Returns:       1 - success
 *                0 - weapon not wielded
 *                string - an error message (failure)
 */
public nomask mixed
unwield_me()
{
    mixed wret;

    if (!wielded || !wielder)
    {
        return 0;
    }

    wret = 0;

    if ((!wield_func) || (!(wret = wield_func->unwield(this_object()))))
    {
        if (check_seen(wielder))
        {
            wielder->catch_tell("You stop wielding " +
		LANG_THESHORT(this_object()) + ".\n");
        }
        else
        {
            wielder->catch_tell("You stop wielding something.\n");
        }

        tell_room(environment(wielder), QCTNAME(wielder) +
            " stops wielding " + wielder->query_possessive() + " " +
            QSHORT(this_object()) + ".\n", wielder);
    }

    if (intp(wret) && (wret >= 0))
    {
        wielder->unwield(this_object());
        wielded = 0;
        remove_adj("wielded");
        add_adj("unwielded");
        wielded_in_hand = wep_hands;
        return 1;
    }

    return (stringp(wret) ? wret : "");
}

/*
 * Function name: command_unwield
 * Description:   Called to wear this armour.
 * Returns:       see unwield_me()
 */
mixed
command_unwield()
{
    return unwield_me();
}

/*
 * Function name: leave_env
 * Description  : When the weapon leaves a certain inventory this function
 *                is called. If the weapon is wielded, we unwield. If you
 *                mask this function you _must_ make a call to ::leave_env
 * Arguments    : object from - the object the weapon leaves.
 *                object desc - the destination of the weapon.
 */
void
leave_env(object from, object dest)
{
    ::leave_env(from, dest);

    if (!wielded)
        return;

    if ((!wield_func ||
         !wield_func->unwield(this_object())) &&
        wielder)
    {
        tell_object(wielder, "You stop wielding " +
	    LANG_THESHORT(this_object()) + ".\n");
    }

    wielder->unwield(this_object());
    remove_adj("wielded");
    add_adj("unwielded");
    wielded = 0;
}

/*
 * Function name: set_hit
 * Description  : Set the to hit value in the weapon. This can only be done
 *                if the lock has not been set.
 * Arguments    : int class - the new weapon class.
 */
void
set_hit(int class)
{
    if (query_lock())
    {
        return;
    }

    wep_hit = class;
}

/*
 * Function name: query_hit
 * Description:   Query the to hit value in the weapon
 */
int query_hit() { return wep_hit; }

/*
 * Function name: set_pen
 * Description:   Set penetration of the weapon
 */
void
set_pen(int class)
{
    if (query_lock())
        return;                 /* All changes has been locked out */

    wep_pen = class;
}

/*
 * Function name: query_pen
 * Description:   Query penetration of the weapon
 */
int query_pen() { return wep_pen; }

/*
 * Function name: set_pm
 * Description:   Set the modifiers for damage types.
 * Arguments:     list, an array of modifiers like ({ impale, slash, bludgeon })
 *                The sum of the modifiers should be 0 and a modifier for
 *                a damage type that is not used should also be 0.
 */
void
set_pm(int *list)
{
    if (query_lock())
        return;

    if (F_LEGAL_AM(list))
        m_pen = list;
}

/*
 * Function name: query_pm
 * Description:   Query the modifiers of damage type
 */
int *query_pm() { return m_pen + ({}); }

/*
 * Function name; query_modified_pen
 * Description:   Query for pen modified for different damage types
 */
int *
query_modified_pen()
{
    int *m_pent, i, du, co, pen;

    du = dull - repair_dull;
    co = corroded - repair_corr;
    pen = this_object()->query_pen();

    m_pent = allocate(W_NO_DT);

    for (i = 0; i < W_NO_DT; i++)
    {
        if (!pointerp(m_pen))
            m_pent[i] = pen;
        else if (i >= sizeof(m_pen))
            m_pent[i] = pen + (i ? m_pen[0] : 0);
        else
            m_pent[i] = pen + m_pen[i];
    }

    return ({ m_pent[0] - 2 * (du + co) / 3, m_pent[1] - du - co,
                m_pent[2] - (du + co) / 3 });
}

/*
 * Function name: set_wt
 * Description:   Set the weapon type
 */
void
set_wt(int type)
{
    int *likely;

    if (query_lock())
        return;                 /* All changes has been locked out */

    if (type >= 0 && type < W_NO_T)
    {
        wep_wt = type;
        likely = W_DRAWBACKS[type];
        likely_dull = likely[0];
        likely_corr = likely[1];
        likely_break = likely[2];
    }
}

/*
 * Function name: query_wt
 * Description:   Query the weapon type
 */
int query_wt() { return wep_wt; }

/*
 * Function name: set_dt
 * Description:   Set the damage type of the weapon
 */
void
set_dt(int type)
{
    if (query_lock())
        return;                 /* All changes has been locked out */

    if (F_LEGAL_DT(type))
        wep_dt = type;
}

/*
 * Function name: query_dt
 * Description:   Query damage type of weapon
 */
int query_dt() { return wep_dt; }

/*
 * Function name: set_hands
 * Description:   Set how the weapon should be wielded
 */
void
set_hands(int which)
{
    if (query_lock())
        return;                 /* All changes has been locked out */

    if (F_LEGAL_HANDS(which))
        wep_hands = which;
}

/*
 * Description: The hands that we can use for this weapon
 */
int query_hands() { return wep_hands; }

/*
 * Description: This is the attack that it supports
 */
int query_attack_id() { return wielded_in_hand; }

/*
 * Description: This is the tool slot that the weapon occupies now
 */
int *
query_slots()
{
    int abit, *hids;

    for (hids = ({}), abit = 2; abit <= wielded_in_hand; abit *= 2)
    {
        if (wielded_in_hand & abit)
        {
            hids = hids + ({ abit });
        }
    }
    return hids;
}

/*
 * Description: The slots the weapon protects
 */
int *
query_protects()
{
    switch (wielded_in_hand)
    {
        case W_LEFT:
            return ({ A_L_ARM });
        case W_RIGHT:
            return ({ A_R_ARM });
        case W_BOTH:
            return ({ A_L_ARM, A_R_ARM });
        case W_FOOTR:
            return ({ A_R_FOOT });
        case W_FOOTL:
            return ({ A_L_FOOT });
        default:
            return ({ });
    }
}

/*
 * Sets the object to call wield/unwield in when this occurs.
 * Those functions can return:
 *              0 - No affect the weapon can be wielded / unwielded
 *              1 - It can be wielded / unwielded but no text should be printed
 *                  (it was done in the function)
 *              -1  It can not be wielded / unwielded default failmsg will be 
 *                  written
 *             string  It can not be wielded / unwielded 'string' is the 
 *                     fail message to print
 */
void
set_wf(object obj)
{
    if (query_lock())
        return;                 /* All changes has been locked out */

    wield_func = obj;
}

#if 0
/* 
 * Function name: wield
 * Description  : This function might be called when someone tries to wield
 *                this weapon. To have this function called, use the function
 *                set_wf().
 * Arguments    : object obj - the weapon someone tried to wield.
 * Returns      : int  0 - wield this weapon normally.
 *                     1 - wield the weapon, but print no messages.
 *                    -1 - do not wield the weapon, use default messages.
 *                string - do not wield the weapon, use this fail message.
 */
public mixed
wield(object obj)
{
    return 0;
}

/*
 * Function name: unwield
 * Description  : This function might be called when someone tries to unwield
 *                this weapon. To have this function called, use the function
 *                set_wf().
 * Arguments    : object obj - the weapon to stop wielding.
 * Returns      : int  0 - the weapon can be unwielded normally.
 *                     1 - unwield the weapon, but print no messages.
 *                    -1 - do not unwield the weapon, print default messages.
 *                string - do not unwield the weapon, use this fail message.
 */
public mixed
unwield(object obj)
{
    return 0;
}
#endif

/*
 * Function name: set_corroded
 * Description:   Use this to increases the corroded status on weapons.
 * Arguments:     cond - The new condition we want (can only be raised)
 * Returns:       1 if new condition accepted, 0 if no corrosion
 */
int
set_corroded(int corr)
{
    if (corr > corroded)
    {
        corroded = corr;
        if (F_WEAPON_BREAK(dull - repair_dull, corroded - repair_corr,
                        likely_break))
            set_alarm(0.1, 0.0, &remove_broken(0));
        if (wielded && wielder)
            wielder->update_weapon(this_object());
        return 1;
    }
    return 0;
}

/*
 * Function name: query_corroded
 * Description:   Returns how many times this weapon has become corroded
 * Returns:       The number of times
 */
int query_corroded()
{
    return corroded;
}

/*
 * Function name: set_likely_corr
 * Description:   Set how likely it is this weapon will corrode when in acid 
 *                or something like that. 0 means it won|t corrode at all.
 * Arguments:     i - how likely it will corrode, probably corrode if random(i)
 *                    [0, 20] recommended
 */
void set_likely_corr(int i) { likely_corr = i; }

/*
 * Function name: query_likely_corr
 * Description:   Query how likely the weapon will corrode, use it to test
 *                if weapon survived your rustmonster or acid pool :)
 */
int query_likely_corr() { return likely_corr; }

/*
 * Function name: set_dull
 * Description:   Use this to increases the dull status on weapons.
 * Arguments:     cond - The new condition we want (can only be raised)
 * Returns:       1 if new condition accepted
 */
int
set_dull(int du)
{
    if (du > dull)
    {
        dull = du;
        if (F_WEAPON_BREAK(dull - repair_dull, corroded - repair_corr,
                        likely_break))
            set_alarm(0.1, 0.0, &remove_broken(0));
        if (wielded && wielder)
            wielder->update_weapon(this_object());
        return 1;
    }
    return 0;
}

/* 
 * Function name: query_dull
 * Description:   Returns how many times this weapon has become duller 
 * Returns:       The number of times
 */
int query_dull() { return dull; }

/*
 * Function name: set_likely_dull
 * Description:   Set how likely the weapon will get dull or in case of a club
 *                or mace, wear down in combat Mainly used from did_hit().
 * Arguments:     i - how likely [0, 30] recommended
 */
void set_likely_dull(int i) { likely_dull = i; }

/*
 * Function name: query_likely_dull
 * Description:   How likely it is this weapon will become duller when used
 * Returns:       How likely it is
 */
int query_likely_dull() { return likely_dull; }

/*
 * Function name: set_likely_break
 * Description:   Set how likely the weapon is to break if you use it.
 * Argument:      i - How likely, [0, 20] recommended
 */
void set_likely_break(int i) { likely_break = i; }

/*
 * Function name: query_likely_break
 * Description:   How likely is it this weapon will break with use
 * Returns:       How likely it is
 */
int query_likely_break() { return likely_break; }

/*
 * Function name: remove_broken
 * Description  : The weapon got broken so the player has to stop
 *                wielding it.
 * Arguments    : int silent - true if no message is to be genereated
 *                             about the fact that the weapon broke.
 */
varargs void
remove_broken(int silent = 0)
{
    /* If the weapon is not wielded, we only adjust the broken information
     * by adding the adjective and the property. We do this within the
     * if-statement since we do not want to screw the message that may
     * be displayed later. When the property is added, the adjective is
     * added automatically.
     */
    if (!wielded || !wielder)
    {
        add_prop(OBJ_I_BROKEN, 1);
        return;
    }

    if (objectp(wield_func))
    {
        wield_func->unwield(this_object());
    }

    /* If the wizard so chooses, these messages may be suppressed. */
    if (!silent)
    {
        tell_object(wielder, "The " + short(wielder) + " breaks!!!\n");
        tell_room(environment(wielder), "The " + QSHORT(this_object()) +
            " wielded by " + QTNAME(wielder) + " breaks!!!\n", wielder);
    }

    /* Force the player to unwield the weapon and then adjust the broken
     * information by adding the property and the adjective.
     */
    wielder->unwield(this_object());
    remove_adj("wielded");
    add_adj("unwielded");
    add_prop(OBJ_I_BROKEN, 1);
    wielded = 0;
}

/*
 * Function name: set_repair_dull
 * Description:   When trying to repair the weapon, call this function. Repairs
 *                can only increase the repair factor. (This is sharpening)
 * Arguments:     rep - The new repair number
 * Returns:       1 if new repair status accepted
 */
int
set_repair_dull(int rep)
{
    if (rep > repair_dull && F_LEGAL_WEAPON_REPAIR_DULL(rep, dull))
    {
        repair_dull = rep;
        if (wielded && wielder)
            wielder->update_weapon(this_object());
        return 1;
    }
    return 0;
}

/*
 * Function name: query_repair_dull
 * Description:   How many times has this weapon been sharpened
 * Returns:       The number of times
 */
int query_repair_dull() { return repair_dull; }

/*
 * Function name: set_repair_corr
 * Description:   When trying to repair the weapon, call this function. Repairs
 *                can only increase the repair factor. This repairs corroded.
 * Arguments:     rep - The new repair number
 * Returns:       1 if new repair status accepted
 */
int
set_repair_corr(int rep)
{
    if (rep > repair_corr && F_LEGAL_WEAPON_REPAIR_CORR(rep, corroded))
    {
        repair_corr = rep;
        if (wielded && wielder)
            wielder->update_weapon(this_object());
        return 1;
    }
    return 0;
}

/*
 * Function name: query_repair_corr
 * Description:   How many times this weapon has been repaired from corrosion
 * Returns:       How many times
 */
int query_repair_corr() { return repair_corr; }

/*
 * Function name: set_weapon_hits
 * Description:   By setting the hits counter you will have influence over how
 *                likely the weapon is to get in a worse condition. The hits
 *                variable keeps track of how many times this piece of weapon
 *                has hit something.
 * Argument:      new_hits - integer
 */
public void
set_weapon_hits(int new_hits) { hits = new_hits; }

/*
 * Function name: query_weapon_hits
 * Description:   hits variable keeps track of how many times this weapon has
 *                hit something.
 * Returns:       The number of times
 */
public int
query_weapon_hits() { return hits; }

/*
 * Function name: add_prop_obj_i_value
 * Description:   Someone is adding the value prop to this object.
 * Arguments:     val - The new value (mixed)
 * Returns:       1 if not to let the val variable through to the prop
 */
int
add_prop_obj_i_value(mixed val)
{
    if (!max_value)
    {
        max_value = 10;
        return 0;
    }

    if (intp(val) && val)
        max_value = val;

    return 1;
}

/*
 * Function name: query_value
 * Description:   Qhat's the value of this armour
 */
int
query_value()
{
    if (query_prop(OBJ_I_BROKEN))
        return 0;

    return max_value *
        F_WEAPON_VALUE_REDUCE(dull - repair_dull, corroded - repair_corr) / 100;
}

/*
 * Function Name: query_repair_cost_dull
 * Description  : Returns the cost to repair this weapon one dull step.
 * Returns      : int - the cost in cc
 */
int
query_repair_cost_dull()
{
    return max(max_value, F_VALUE_WEAPON(query_hit(), query_pen())) *
        F_WEAPON_REPAIR_COST_FACTOR / 100;
}

/*
 * Function Name: query_repair_cost_corr
 * Description  : Returns the cost to repair this weapon from one level of
 *                corrosion
 * Returns      : int - the cost in cc
 */ 
int
query_repair_cost_corr()
{
    return max(max_value, F_VALUE_WEAPON(query_hit(), query_pen())) *
        F_WEAPON_REPAIR_COST_FACTOR / 100;
}
    
/*
 * Function name: query_wf
 * Description:   Query if/what object defines wield/unwield functions
 */
object query_wf() { return wield_func; }

/*
 * Function name: try_hit
 * Description:   Called from living when weapon used.
 * Arguments:     target - Who I intend to hit.
 * Returns:       False if weapon miss. If true it might hit.
 */
int try_hit(object target) { return 1; }

/*
 * Function name: did_hit
 * Description:   Tells us that we hit something. Should produce combat
 *                messages to all relevant parties. If the weapon
 *                chooses not to handle combat messages then a default
 *                message is generated.
 * Arguments:     aid:   The attack id
 *                hdesc: The hitlocation description.
 *                phurt: The %hurt made on the enemy
 *                enemy: The enemy who got hit
 *                dt:    The current damagetype
 *                phit:  The %success that we made with our weapon
 *                dam:   The actual damage caused by this weapon in hit points
 *                hid:   The hitlocation id
 * Returns:       True if it handled combat messages, returning a 0 will let
 *                the normal routines take over
 */
public varargs int
did_hit(int aid, string hdesc, int phurt, object enemy, int dt,
                int phit, int dam, int hid)
{
    hits++;

    if (F_WEAPON_CONDITION_DULL(hits, wep_pen, likely_dull))
    {
        hits = 0;
        set_dull(query_dull() + 1);
    }

    return 0;
}

/*
 * Function name: check_weapon
 * Description  : Check file for security.
 * Returns      : int 1 - always.
 */
nomask int
check_weapon()
{
    return 1;
}

/*
 * Function name: set_default_weapon
 * Description:   Configures the weapon
 * Arguments:
 * Returns:
 */
varargs void
set_default_weapon(int hit, int pen, int wt, int dt, int hands, object obj)
{
    /* Sets the weapon "to hit" value.
    */
    if (hit) set_hit(hit);
    else set_hit(5);

    /* Sets the weapon penetration value.
    */
    if (pen) set_pen(pen);
    else set_pen(10);

    /* Set the weapon type.
    */
    if (hit) set_wt(wt);
    else set_wt(W_FIRST);

    /* Set the damage type.
    */
    if (hit) set_dt(dt);
    else set_dt(W_IMPALE | W_SLASH);

    /* Set the hand(s) used to wield the weapon.
    */
    if (hands) set_hands(hands);
    else set_hands(W_NONE);
    
    /* Sets the name of the object that contains the function
       to call for extra defined wield() and unwield()
       functions.
    */
    if (obj) set_wf(obj);
}


/*
 * Function name: query_wield_desc
 * Description:   Describe this weapon as wielded by a something.
 * Argumensts:    p: Possessive description of wielder
 * Returns:       Description string.
 */
public nomask string 
query_wield_desc(string p)
{
    string str;

    /* Allow masking of the short. */
    if (check_seen(this_player()))
        str = LANG_ADDART(this_object()->short(this_player()));
    else
        str = "something";

    switch(wielded_in_hand)
    {
    case W_RIGHT:return str + " in " + p + " right hand";
    case W_LEFT: return str + " in " + p + " left hand";
    case W_BOTH: return str + " in both hands";
    case W_FOOTR:return str + " on " + p + " right foot";
    case W_FOOTL:return str + " on " + p + " left foot";
    }
    return str;
}

/*
 * Function name: update_prop_settings
 * Description:   Will uppdate weight and value of this object to be legal
 */
nomask void
update_prop_settings()
{
    if (query_prop(OBJ_I_VALUE) < F_VALUE_WEAPON(wep_hit, wep_pen) &&
            !query_prop(OBJ_I_IS_MAGIC_WEAPON))
        add_prop(OBJ_I_VALUE, F_VALUE_WEAPON(wep_hit, wep_pen));
 
    if (F_WEIGHT_FAULT_WEAPON(query_prop(OBJ_I_WEIGHT), wep_pen, wep_wt) &&
            !query_prop(OBJ_I_IS_MAGIC_WEAPON))
        add_prop(OBJ_I_WEIGHT, F_WEIGHT_DEFAULT_WEAPON(wep_pen, wep_wt));
}

/*
 * Function name: query_wielded
 * Description:   If this object is wielded or not
 * Returns:       The object who wields this object if this object is wielded
 */
object
query_wielded()
{
    if (wielded) return wielder;
}

/*
 * Function name: query_am
 * Description:   Called when wielding the weapon, to check for the parry.
 * Returns:       The armour modifier
 */
public nomask int *
query_am()
{
    return ({ -3, 2, 1 });
}

/*
 * Function name: query_ac
 * Description  : Called when wielding the weapon, to check for parry.
 * Returns      : int - the AC value for the weapon.
 */
public nomask int
query_ac()
{
    int ac = (wep_hit / 2) + ac_modifier;

    if (wielder)
    {
        ac += ((wielder->query_skill(SS_PARRY) * wep_hit) / 500);
    }

    return ac;
}

/*
 * Function name: set_ac_modifier
 * Description  : A modifier on the armour class for the weapon. A weapon by
 *                default has an AC related to the weapon to-hit value. When
 *                setting this value, you must document in the code why you
 *                change it, especially if you set it to a positive value.
 *                The modifier is added to the calculated AC value.
 * Arguments    : int ac_mod - the modifier, in range -10 .. 5
 */
public nomask void
set_ac_modifier(int ac_mod)
{
    if ((ac_mod <= 5) || (ac_mod >= -10))
    {
        ac_modifier = ac_mod;
    }
}

/*
 * Function name: query_ac_modifier
 * Description  : Returns the AC modifier for the weapon. See set-routine.
 * Returns      : int - the modifier for the AC value of the weapon.
 */
public nomask int
query_ac_modifier()
{
    return ac_modifier;
}

/*
 * Function name: stat_object
 * Description:   This function is called when a wizard wants to get more
 *                information about an object.
 * Returns:       str - The string to write..
 */
string
stat_object()
{
    string str;

    str = ::stat_object();

    str += "Hit: " + wep_hit + "\t\tPen: " + wep_pen + "\n";

    str += "Type: " + wep_wt + "\n";
    str += "Hands: " + wep_hands + "\n";

    return str;
}

/*
 * Function name: wep_condition_desc
 * Description:   Returns the description of the condition of the weapon
 */
string
wep_condition_desc()
{
    string hand, hand2;

    if (query_prop(OBJ_I_BROKEN))
        return "It is broken.\n";

    switch (corroded - repair_corr)
    {
        case 0:
            hand = ""; break;
        case 1:
            hand = "You find some rust marks on it!\n"; break;
        case 2:
            hand = "You can spot some rust on it!\n"; break;
        case 3:
            hand = "The weapon is full of rust!\n"; break;
        case 4:
        case 5:
            hand = "It looks like it have been bathing in acid!!\n"; break;
        default:
            hand = "It looks like it is falling apart from corrosion.\n"; break;
    }

    switch (dull - repair_dull)
    {
        case 0:
            hand2 = "in prime condition"; break;
        case 1:
            hand2 = "in a fine condition"; break;
        case 2:
            hand2 = "a little touched by battle"; break;
        case 3:
            hand2 = "scarred by battle"; break;
        case 4:
        case 5:
            hand2 = "very scarred by battle"; break;
        case 6:
        case 7:
        case 8:
            hand2 = "in big need of a smith"; break;
        default:
            hand2 = "going to break any second"; break;
    }

    return hand + "It looks like it is " + hand2 + ".\n";
}

/*
 * Function name: weapon_type
 * Description:   This function shuold return the type of the weapon in
 *                text.
 * Returns:       The type
 */
string
weapon_type()
{
    if (wep_wt >= W_NO_T)
    {
        return "weapon";
    }

    return W_NAMES[wep_wt];
}

/*
 * Function name: wep_usage_desc
 * Description  : This function returns the usage of this weapon. It is
 *                usually printed from the appraise function. The string
 *                includes the type of the weapon and the location where it
 *                should be wielded.
 * Returns      : string - the description.
 */
string
wep_usage_desc()
{
    string hand;

    switch (wep_hands)
    {
        case W_RIGHT:
            hand = "in the right hand"; break;
        case W_LEFT:
            hand = "in the left hand"; break;
        case W_BOTH:
            hand = "in both hands"; break;
        case W_FOOTR:
            hand = "on the right foot"; break;
        case W_FOOTL:
            hand = "on the left foot"; break;
        case W_ANYH:
            hand = "in any hand"; break;
        default:
            hand = "by some strange creature"; break;
    }

    return ("The " + weapon_type() + " is made to be wielded " + hand + ".\n");
}

/*
 * Function name: appraise_object
 * Description  : Someone tries to appraise the object. We add information
 *                about the way you should use this weapon.
 */
void
appraise_object(int num)
{
    ::appraise_object(num);

    write(wep_usage_desc());
}

/*
 * Function name: did_parry
 * Description:   Tells us that this weapon was used to parry an attack. It can
 *                be used to wear down a weapon. Note that his method is called
 *                before the combat messages are printed out.
 * Arguments:     att:   Attacker
 *                aid:   The attack id
 *                dt:    The damagetype
 */
public varargs void
did_parry(object att, int aid, int dt)
{
}

/*
 * Function name: may_not_recover
 * Description  : This function will be true if the weapon may not recover.
 * Returns      : 1 - no recovery, 0 - recovery.
 */
nomask int
may_not_recover()
{
    return will_not_recover;
}

/*
 * Function name: may_recover
 * Description  : In some situations it is undesirable to have a weapon
 *                not recover. This function may then be used to force the
 *                weapon to be recoverable.
 *
 *                This function may _only_ be called when a craftsman sells
 *                a weapon he created to a player! It may expressly not be
 *                called in weapons that are to be looted from NPC's!
 */
nomask void
may_recover()
{
    will_not_recover = 0;
}

/*
 * Function name: set_may_not_recover
 * Description  : Call this to force the weapon to be non-recoverable.
 */
nomask void
set_may_not_recover()
{
    will_not_recover = 1;
}

/*
 * Function name: query_wep_recover
 * Description:   Return the recover strings for changing weapon variables.
 * Returns:       A recover string
 */
string
query_wep_recover()
{
    return ("#WEP#" + hits + "#" + dull + "#" + corroded + "#" +
        repair_dull + "#" + repair_corr + "#" + query_prop(OBJ_I_BROKEN) +
        "#");
}

/*
 * Function name: init_wep_recover
 * Description:   Initialize the weapon variables at recover.
 * Arguments:     arg - String with variables to recover
 */
void
init_wep_recover(string arg)
{
    string foobar;
    int    broken;

    sscanf(arg, "%s#WEP#%d#%d#%d#%d#%d#%d#%s", foobar,
        hits, dull, corroded, repair_dull, repair_corr, broken, foobar);

    if (broken != 0)
    {
        add_prop(OBJ_I_BROKEN, broken);
    }
}

/*
 * Function name: query_recover
 * Description  : Called to check whether this weapon is recoverable.
 *                If you have variables you want to recover yourself,
 *                you have to redefine this function, keeping at least
 *                the filename and the weapon recovery variables, like
 *                they are queried below.
 *                If, for some reason, you do not want your weapon to
 *                recover, you should define the function and return 0.
 * Returns      : string - the default recovery string.
 */
public string
query_recover()
{
    return MASTER + ":" + query_wep_recover();
}

/*
 * Function name: init_recover
 * Description  : When the object recovers, this function is called
 *                to set the necessary variables. If you redefine the
 *                function, you must add a call to init_wep_recover
 *                with the string that you got after querying
 *                query_wep_recover.
 * Arguments    : string argument - the arguments to parse
 */
public void
init_recover(string arg)
{
    init_wep_recover(arg);
}

