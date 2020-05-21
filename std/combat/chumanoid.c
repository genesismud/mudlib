/*
  /std/combat/chumanoid.c

  This is the externalized combat routines for humanoids. 

  This combat object predefines a set of attacks and hitlocations 
  for humanoid living objects. It also keeps track of the total percentage
  of attacks that can be made each turn and distributes those percentages
  over the attacks depending on effectiveness of weapon wielded.

  The distribution formula is:

          %use = %maxuse * (wchit*wcpen) / ( sum(wchit*wcpen) )

  This formula is used if the 'set_attuse()' function is called and the
  value set to something different than 0. Otherwise the %use is left
  unaffected.

  This object is cloned and linked to a specific individual when
  engaged in combat. The actual object resides in 'limbo'.
*/

#pragma save_binary
#pragma strict_types

inherit "/std/combat/ctool";

#include <wa_types.h>
#include "combat.h"
#include <formulas.h>
#include <ss_types.h>
#include <std.h>
#include <options.h>

static  int             attuse;      /* Total %use, 100% is 1 attack */

/*
* Function name: create_ctool
* Description:   Reset the combat functions
*/
public nomask void
create_ctool()
{
    if (me)
    {
	return;
    }

    this_object()->create_chumanoid();
}

/*
 * Function name: cb_configure
 * Description:   Configure humanoid attacks and hitlocations.
 * Returns:       True if hit, otherwise 0.
 */
public void
cb_configure()
{
    ::cb_configure();

    add_attack(0, 0, 0, 0, W_RIGHT); me->cr_reset_attack(W_RIGHT);
    add_attack(0, 0, 0, 0, W_LEFT);  me->cr_reset_attack(W_LEFT);
    add_attack(0, 0, 0, 0, W_BOTH);  me->cr_reset_attack(W_BOTH);
    add_attack(0, 0, 0, 0, W_FOOTR); me->cr_reset_attack(W_FOOTR);
    add_attack(0, 0, 0, 0, W_FOOTL); me->cr_reset_attack(W_FOOTL);
    
    add_hitloc(0, 0, 0, A_HEAD);  me->cr_reset_hitloc(A_HEAD);
    add_hitloc(0, 0, 0, A_L_ARM); me->cr_reset_hitloc(A_L_ARM);
    add_hitloc(0, 0, 0, A_R_ARM); me->cr_reset_hitloc(A_R_ARM);
    add_hitloc(0, 0, 0, A_TORSO); me->cr_reset_hitloc(A_TORSO);
    add_hitloc(0, 0, 0, A_LEGS);  me->cr_reset_hitloc(A_LEGS);
 
    map(qme()->query_weapon(-1), cb_wield_weapon);
    map(qme()->query_armour(-1), cb_wear_arm);
}

/*
 * Description: Humanoids might reallocate what attacks they use when the
 *              attacks are modified. (If maxuse is set)
 *              The distribution formula is:
 *
 *                       %use = %maxuse * (wchit*wcpen) / ( sum(wchit*wcpen) )
 */
public void
cb_modify_procuse()
{
    int il, *attid, *enabled_attacks, swc, puse, weapon_no;
    int unarmed_off;
    mixed *att;
    object tool;

    if (!attuse)
        return;

    attid = query_attack_id();
    att = allocate(sizeof(attid));
    enabled_attacks = allocate(sizeof(attid));
    weapon_no = sizeof(cb_query_weapon(-1));
    unarmed_off = me->query_option(OPT_UNARMED_OFF);

    for (swc = 0, il = 0; il < sizeof(attid); il++)
    {
        att[il] = query_attack(attid[il]);

        if (!att[il][ATT_OBJ])
	{
	    /* No weapon in this slot.  See if unarmed is off
             * and there is a weapon wielded elsewhere.
             */
            if (unarmed_off && (weapon_no > 0))
	    {
                continue;
	    }

            /* See if there is another kind of tool in the slot
             * that might prevent us from using it to attack.
             */
            if ((tool = (object)qme()->query_tool(attid[il])) &&
                tool->query_attack_blocked(attid[il]))
	    {
                continue;
	    }
        }

        enabled_attacks[il] = 1;
        swc += att[il][ATT_WCHIT] * F_PENMOD(att[il][ATT_WCHIT], 
            att[il][ATT_SKILL]);
    }

    for (il = 0; il < sizeof(attid); il++)
    {
        if (swc && enabled_attacks[il])
        {
            puse = (attuse * att[il][ATT_WCHIT] * 
                    F_PENMOD(att[il][ATT_WCHIT], att[il][ATT_SKILL])) / swc;
        }
        else
        {
            puse = 0;
        }

        ::add_attack(att[il][ATT_WCHIT], att[il][ATT_WCPEN], 
                     att[il][ATT_DAMT], puse, attid[il],
                     (att[il][ATT_SKILL] ? att[il][ATT_SKILL] : -1),
                     att[il][ATT_OBJ] );
    }
}

/*
 * Description: Set the %attacks used each turn. 100% is one attack / turn
 * Arguments:   sumproc: %attack used
 */
public void
cb_set_attackuse(int sumproc)
{
    attuse = sumproc;
    cb_modify_procuse();
}

/*
 * Description: Query the total %attacks used each turn. 100% is one attack / turn
 * Returns:     The attackuse
 */
public int
cb_query_attackuse() { return attuse; }

/*
 * Description: Add an attack, see /std/combat/cbase.c
 */
varargs int
add_attack(int wchit, mixed wcpen, int damtype, int prcuse, int id, int skill,
    object wep)
{
    int ret;

    ret = ::add_attack(wchit, wcpen, damtype, prcuse, id, skill, wep);
    cb_modify_procuse();

    return ret;
}


/*
 * Function name: cb_wield_weapon
 * Description:   Wield a weapon.
 * Arguments:	  wep - The weapon to wield.
 * Returns:       string - error message (weapon not wielded)
 *                1 - success (weapon wielded)
 */
public mixed
cb_wield_weapon(object wep)
{
    int aid, wcskill, owchit, owcpen;
    mixed *att;
    mixed str;

    if (!me->query_wiz_level() &&
        function_exists("create_object", wep) != WEAPON_OBJECT)
    {
	return "The " + wep->short() + " is not a true weapon!\n";
    }
    
    if (stringp(str = ::cb_wield_weapon(wep)))
    {
	return str;
    }

    aid = (int) wep->query_attack_id();
    if (cb_query_weapon(aid) == wep)
    {
	att = query_attack(aid);
    	/*
         * We get no more use of the weapon than our skill with it allows.
	 */
	wcskill = (int)me->query_skill(SS_WEP_FIRST + 
				       ((int)wep->query_wt() - W_FIRST));
	if (wcskill < 1)
	    wcskill = -1;
	add_attack(att[0], att[1], att[2], att[3], aid, wcskill, wep);
    }

    return 1;
}

/*
 * Function name: cb_wear_arm
 * Description:   Wear an armour
 * Arguments:	  arm - The armour.
 * Returns:       True if worn, errtext if fail
 */
public mixed
cb_wear_arm(object arm)
{
    if (!me->query_wiz_level() && !IS_ARMOUR_OBJECT(arm))
    {
	return "The " + arm->short() + " is not a true armour!\n";
    }

    return ::cb_wear_arm(arm);
}
