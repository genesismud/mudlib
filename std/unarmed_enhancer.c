/*
 * /std/unarmed_enhancer.c
 *
 */

inherit "/std/armour";

#include <cmdparse.h>
#include <files.h>
#include <formulas.h>
#include <macros.h>
#include <stdproperties.h>
#include <wa_types.h>
#include <composite.h>

static int      enh_hit,        /* Enhancer to hit */
                enh_pen,        /* Enhancer penetration */
                enh_dt,         /* Enchancer damage type */
                *m_pen;         /* Modifiers for the enh_pen */

// Prototype
void create_unarmed_enhancer();

/*
 * Function name: create_armour
 * Description  : Set various default values
 */
public nomask void
create_armour()
{
    ::create_armour();

    enh_dt = W_BLUDGEON;
    m_pen = ({ 0, 0, 0 });

    create_unarmed_enhancer();
}

/*
 * Function name: create_unarmed_enhancer
 * Description  : In order to create an unarmed enhancer, you should
 *                redefine this function. Typically you use this function
 *                to set the name and description, the armour type, the
 *                armour class, hit and pen etcetera.
 */
void
create_unarmed_enhancer()
{
}

/*
 * Function name: query_enhanced_attack_ids
 * Description  : When worn, this returns in which attack ids the enhancer is
 *                actually being used.
 * Returns      : int array - the attack ids, or 0 if not worn.
 */
public nomask int *
query_enhanced_attack_ids()
{
    int *aids = query_slots();

    if (sizeof(aids) == 0)
    {
        return 0;
    }
    aids = map(aids, wearer->cr_convert_slot_to_attack_id);
    if (sizeof(aids) == 0)
    {
        return 0;
    }
    return aids;
}

/*
 * Function name: set_hit
 * Description  : Set the to hit value in the enhancer. This relates to how easy
 *                it is to make a hit with this enhancer.
 *                When the lock has been set, no changes are allowed anymore.
 * Arguments    : int class - the hit value.
 */
void
set_hit(int class)
{
    if (query_lock())
        return;

    enh_hit = class;
}

/*
 * Function name: query_hit
 * Description  : Query the to hit value in the enhancer.
 * Returns      : int - the hit value.
 */
int query_hit() { return enh_hit; }

/*
 * Function name: set_pen
 * Description  : Set penetration value of the enhancer. This relates to how
 *                badly it hurts when you are hit by this enhancer.
 *                When the lock has been set, no changes are allowed anymore.
 * Arguments    : int class - the pen value.
 */
void
set_pen(int class)
{
    if (query_lock())
        return;

    enh_pen = class;
}

/*
 * Function name: query_pen
 * Description  : Query penetration value of the enhancer.
 * Returns      : int - the pen value.
 */
int query_pen() { return enh_pen; }

/*
 * Function name: set_pm
 * Description  : Set the modifiers for damage types. These modify the pen
 *                value for the individual damage types.
 *                When the lock has been set, no changes are allowed anymore.
 * Arguments    : int *list - array of modifiers ({ impale, slash, bludgeon })
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
 * Description  : Query the modifiers for the damage types.
 * Returns      : int * - array of modifiers ({ impale, slash, bludgeon })
 */
int *query_pm() { return m_pen + ({}); }

/*
 * Function name: query_modified_pen
 * Description  : Query the effective (modified) pen values for the different
 *                damage types. These take corrosion and dulling into account.
 * Returns      : int * - array of modified pen values ({ impale, slash, blg })
 */
int *
query_modified_pen()
{
    int *m_pent, i, cond, pen;
    object to = this_object();

    cond = to->query_condition() - to->query_repair();
    pen = to->query_pen();

    m_pent = allocate(W_NUM_DT);

    for (i = 0; i < W_NUM_DT; i++)
    {
        if (!pointerp(m_pen))
            m_pent[i] = pen;
        else if (i >= sizeof(m_pen))
            m_pent[i] = pen + (i ? m_pen[0] : 0);
        else
            m_pent[i] = pen + m_pen[i];
    }

    return ({ m_pent[0] - 2 * cond / 3, m_pent[1] - cond,
                m_pent[2] - cond / 3 });
}

/*
 * Function name: set_dt
 * Description  : Set the damage type of the enhancer. This should naturally
 *                make a logical match with the enhancer type. The damage type
 *                is a binary combination of W_IMPALE | W_SLASH | W_BLUDGEON
 *                  - impale   = for stabbing
 *                  - slash    = for cutting
 *                  - bludgeon = for (dull) hitting with force
 *                When the lock has been set, no changes are allowed anymore.
 * Arguments    : int type - the damage type.
 */
void
set_dt(int type)
{
    if (query_lock())
        return;

    if (F_LEGAL_DT(type))
        enh_dt = type;
}

/*
 * Function name: check_unarmed_enhancer
 * Description:   Check file for security.
 */
nomask int
check_unarmed_enhancer()
{
    return 1;
}

/*
 * Function name: set_default_enhancer
 * Description  : Configures the enhancer by replacing up to six calls with
 *                just one. For details, see the respective functions.
 * Arguments    : int hit    - set_hit(hit)
 *                int pen    - set_pen(pen)
 *                int dt     - set_dt(dt)
 *                int ac     - set_ac(ac)
 *                int at     - set_at(at)
 *                int* am    - set_am(am)
 *                object af  - set_wf(af)
 */
varargs void
set_default_enhancer(int hit, int pen, int dt, int ac, int at, int *am, object af)
{
    ::set_default_armour(ac, at, am, af);

    /* Sets the enhancer "to hit" value. */
    set_hit(hit ? hit : 5);

    /* Sets the enhancer penetration value. */
    set_pen(pen ? pen : 10);

    /* Set the damage type. */
    set_dt(dt ? dt : W_BLUDGEON);
}

/*
 * Function name: query_dt
 * Description  : Query the damage type of the enhancer.
 * Returns      : int - the damage type of the enhancer.
 */
int query_dt() { return enh_dt; }

/*
 * Function name: did_hit
 * Description:   Tells us that we hit something. Should produce combat
 *                messages to all relevant parties. If the enhancer
 *                chooses not to handle combat messages then a default
 *                message is generated.
 * Arguments:     aid:   The attack id
 *                hdesc: The hitlocation description.
 *                phurt: The %hurt made on the enemy
 *                enemy: The enemy who got hit
 *                dt:    The current damagetype
 *                phit:  The %success that we made with our enhancer
 *                dam:   The actual damage caused by this enhancer in hit points
 *                hid:   The hitlocation id
 * Returns:       True if it handled combat messages, returning a 0 will let
 *                the normal routines take over
 */
public varargs int
did_hit(int aid, string hdesc, int phurt, object enemy, int dt,
                int phit, int dam, int hid)
{
    if (dam <= 0)
        return 0;

    hits++;
    if (F_WEAPON_CONDITION_DULL(hits, enh_pen, likely_cond))
    {
        hits = 0;
        set_condition(query_condition() + 1);

        tell_object(wearer, "The " + short(wearer) + " took the full " +
            one_of_list( ({ "brunt", "force", "impact" }) ) +
            " of that last attack and looks a bit more damaged than before.\n");
    }

    return 0;
}

/*
 * Function name: wearable_item_usage_desc
 * Description  : This function returns the usage of this wearable. It is
 *                usually printed from the appraise function. The string
 *                includes the location where it should be worn. For the
 *                unarmed enhancer it also generates the damage type
 *                description.
 * Returns      : string - the description.
 */
string
wearable_item_usage_desc()
{
    string *parts = map(query_slots(1), &extract(, 1) @ wear_how);
    string * damage = ({ });

    if (enh_dt & W_IMPALE)
        damage += ({ one_of_list( ({ "impale", "pierce", "stab" }) ) });
    if (enh_dt & W_SLASH)
        damage += ({ one_of_list( ({ "slash", "cut", "hack" }) ) });
    if (enh_dt & W_BLUDGEON)
        damage += ({ one_of_list( ({ "bludgeon", "beat", "batter" }) ) });

    if (!sizeof(damage))
        damage = ({ "cause damage" });

    return ("The " + this_object()->short(this_player()) +
        " is made to be worn " + COMPOSITE_WORDS(parts) + ".  " +
        "It can be used to " + COMPOSITE_WORDS(damage) + ".\n");
}
