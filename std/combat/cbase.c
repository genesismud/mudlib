/*
 /std/combat/cbase.c

 This is the externalized combat routines. 

 This object is cloned and linked to a specific individual when
 engaged in combat. The actual object resides in 'limbo'.

 Ver 2.0 JnA: 911220

   This version uses an attack and defence table. Combat no longer have
   a concept of weapons and armours. Only attacks and hitlocations.

   This file is meant to be inherited by more advanced combat systems.

 Note that this is the implementation of the combat system in general. If
 you want to make an entirely different combat system, you are recommended
 to change the 'COMBAT_FILE' define in config.h
*/

#pragma save_binary
#pragma strict_types

#include <std.h>
#include <stdproperties.h>
#include <filter_funs.h>
#include <macros.h>
#include <formulas.h>
#include <composite.h>
#include <ss_types.h>
#include <wa_types.h>
#include <math.h>
#include <comb_mag.h>
#include <options.h>
#include "/std/combat/combat.h"

/*
 * Prototypes
 */
public nomask int cb_query_panic();
static nomask int fixnorm(int offence, int defence);
static nomask void heart_beat();
public string cb_attack_desc(int aid);
public string cb_hitloc_desc(int hid);
static void stop_heart();
public mixed cb_query_weapon(int which);
public mixed * query_attack(int id);


/*
    Format of each element in the attacks array:

         ({ wchit, wcpen, dt, %use, skill })
              wchit: Weapon class to hit
              wcpen: Weapon class penetration
              dt:    Damage type
              %use:  Chance of use each turn
              skill: The skill of this attack (defaults to wcpen)
              m_pen: The modified pen used in combat

         att_id:    Specific id, for humanoids W_NONE, W_RIGHT etc

    Format of each element in the hitloc_ac array:

         ({ *ac, %hit, desc })
              ac:    The ac's for each damagetype for a given hitlocation
              %hit:  The chance that a hit will hit this location
              desc:  String describing this hitlocation, ie "head", "tail"
              m_ac:  Modified ac to use in combat
                       .......
         Note that the sum of all %hit must be 100.

         hit_id:    Specific id, for humanoids, A_TORSO, A_HEAD etc
*/

static int    *att_id = ({}),    /* Id's for attacks */
              *hit_id = ({}),    /* Id's for hitlocations */
              panic,             /* Dont panic... */
              panic_time,        /* Time panic last checked. */
              tohit_val,         /* A precalculated tohit value for someone */
              hit_heart,         /* Controls the quickness when fighting. */
              i_am_real,         /* True if the living object is interactive */
              alarm_id,          /* The id of the heart_beat alarm */
              combat_time,       /* The last time a hit was made. */
              tohit_mod;         /* Bonus/Minus to the tohit value */

static float  speed = 0.0,       /* How often I hit */
              delay = 0.0;

static mixed  *attacks = ({}),   /* Array of each attack */
              *hitloc_ac = ({}); /* The armour classes for each hitloc */

static object me,                /* The living object concerned */
              *enemies = ({}),   /* Array holding all living I hunt */
              tohit_ob,          /* Last object we calculated tohit values */
              attack_ob;         /* Object to attack == Current enemy. */

/*
 * Description: Give status information about the combat values
 *
 */
public string
cb_status()
{
    string str;
    int il, tmp, size;
    mixed ac;
    object *valid_enemies;

    enemies = filter(enemies, objectp);

    str = "Living object: " + file_name(me) + 
        " (Uid: " + getuid(me) + ", Euid: " + geteuid(me) + ")\n";

    str += "Combat object: " + file_name(this_object()) + 
        " (Uid: " + getuid(this_object()) + 
            ", Euid: " + geteuid(this_object()) + ")\n";

    if (attack_ob)
        str += "Fighting: " + attack_ob->query_name() +
            " (" + file_name(attack_ob) +")\n";

    /* if the enemies have been destroyed then it can cause problems
     * so remove the non live ones.  Left
     */
    valid_enemies = FILTER_LIVE(enemies);

    if (sizeof(valid_enemies))
       str += "Enemies:\n" + break_string(COMPOSITE_LIVE(valid_enemies), 76, 3);
    else
        str += "No enemies pending";

    str += sprintf("\nPanic: %3d, Attacks: %3d, Hitlocations: %3d\n",
                   cb_query_panic(), sizeof(att_id), sizeof(hit_id));

    il = -1;
    size = sizeof(att_id);
    while(++il < size)
    {
        if (!il)
            str += sprintf("\n%-20s %@|9s\n","  Attack",
                           ({"wchit",
                             "impale/slash/bludg ", "wcskill",
                             "   %use" }));
        ac = attacks[il][ATT_DAMT];
        ac = ({ (ac & W_IMPALE ? attacks[il][ATT_WCPEN][0] + " " : "no "),
                (ac & W_SLASH ? attacks[il][ATT_WCPEN][1] + " " : "no "), 
                (ac & W_BLUDGEON ? attacks[il][ATT_WCPEN][2] + " " : "no ") });
        ac = implode(ac,"   ");

        str += sprintf("%-20s %|9d %-17s %|9d %|9d\n", 
            this_player()->check_call(cb_attack_desc(att_id[il])) + ":",
            attacks[il][ATT_WCHIT],
            ac,
            attacks[il][ATT_SKILL],
            attacks[il][ATT_PROCU]);
    }

    il = -1;
    size = sizeof(hit_id);
    while(++il < size)
    {
        if (!il)
            str += sprintf("\n%-15s %@|9s\n","  Hit location",
                           ({"impale", "slash", "bludgeon", " %hit" }));
        str += sprintf("%-15s", hitloc_ac[il][HIT_DESC] + ":") + " ";
        ac = hitloc_ac[il][HIT_AC];
        if (!pointerp(ac))
            ac = ({ ac, ac, ac });
        else 
            ac = ac[0..2];
        str += sprintf("%@|9d %|9d\n", ac, hitloc_ac[il][HIT_PHIT]);
    }

    str += "\nParry: " + me->query_skill(SS_PARRY) + "  Defense: " + 
        me->query_skill(SS_DEFENSE) + "  Stat av: " +
        (tmp = me->query_average_stat()) + "  Dex: " + me->query_stat(SS_DEX) +
        "  Enc: " + (me->query_encumberance_weight() +
            me->query_encumberance_volume() / 2) + "\nVol: " +
        me->query_prop(CONT_I_VOLUME) + "  Speed: " +
        me->query_prop(LIVE_I_QUICKNESS) + "  Exp at kill: " +
            F_EXP_ON_KILL(tmp, tmp) +
       (me->query_npc() ? (" at " + me->query_exp_factor() + "%") : "") + "\n";

    return str;
}

/*
 * Function name: cb_data
 * Description:   More data about combat stats.
 * Returns:       A string to write
 */
string
cb_data()
{
    string str;
    int i, val, tmp, t2, ac, size;
    object *arr;

    str = "Living object: " + file_name(me) +
        " (Uid: " + getuid(me) + ", Euid: " + geteuid(me) + ")\n";

    str += "Combat object: " + file_name(this_object()) +
        " (Uid: " + getuid(this_object()) +
            ", Euid: " + geteuid(this_object()) + ")\n";

    val = 2 * fixnorm(me->query_stat(SS_DEX), 50) -
        fixnorm(me->query_prop(CONT_I_VOLUME), 60000) -
        fixnorm(me->query_encumberance_weight() +
                me->query_encumberance_volume(), 60);

    tmp = 0;
    i = -1;
    size = sizeof(att_id);
    while(++i < size)
    {
        tmp += attacks[i][ATT_WCHIT] * attacks[i][ATT_PROCU];
    }
    tmp /= 100;

    val += 4 * fixnorm(2 * tmp, 50);

    str += sprintf("\n%-20s %5d\n", "Offensive tohit:", val);

    val = 0;
    i = -1;
    size = sizeof(att_id);
    while(++i < size)
    {
        ac = attacks[i][ATT_DAMT];
        if (ac & W_IMPALE)
            tmp = attacks[i][ATT_M_PEN][0];
        else if (ac & W_SLASH)
            tmp = attacks[i][ATT_M_PEN][1];
        else if (ac & W_BLUDGEON)
            tmp = attacks[i][ATT_M_PEN][2];
        val += tmp * attacks[i][ATT_PROCU];
    }
    val /= 100;

    str += sprintf("%-20s %5d\n", "Offensive pen:", val);

    val = 2 * fixnorm(50, me->query_stat(SS_DEX)) -
        fixnorm(60000, me->query_prop(CONT_I_VOLUME)) -
        fixnorm(60, me->query_encumberance_weight() +
                me->query_encumberance_volume());

    if (sizeof((object *)me->query_weapon(-1) - ({ 0 })))
    {
        tmp = me->query_skill(SS_PARRY);
    }
    else
    {
        tmp = me->query_skill(SS_UNARM_COMBAT) / 2;
    }

    tmp += me->query_skill(SS_DEFENSE);

    val += 4 * fixnorm(70, tmp);

    str += sprintf("%-20s %5d\n", "Defensive tohit:", val);

    val = 0;
    i = -1;
    size = sizeof(hit_id);
    while(++i < size)
    {
        val += hitloc_ac[i][HIT_M_AC][0] * hitloc_ac[i][HIT_PHIT];
    }
    val /= 100;

    str += sprintf("%-20s %5d\n", "Defensive ac:", val);

    str += "\nExp at kill: " + (F_KILL_GIVE_EXP(me->query_average_stat()) *
         (me->query_npc() ? me->query_exp_factor() : 100) / 100) +
         "  Speed: " + me->query_prop(LIVE_I_QUICKNESS);

    arr = all_inventory(me);
    i = -1;
    size = sizeof(arr);
    tmp = 0;
    while(++i < size)
    {
        tmp += arr[i]->query_prop(OBJ_I_VALUE);
    }
    str += "  Carried value: " + tmp + " (" + sizeof(arr) + ") objects.\n";

    return str;
}

/*
 * Function name: create
 * Description:   Reset the combat functions
 */
public nomask void
create()
{
    combat_time = 0; /* intentionally not time() */
    panic_time = time();
    if (me)
        return;
    this_object()->create_cbase();
}

/*
 * Function name: clean_up
 * Description:   This function is called when someone wants to get rid of
 *                this object, but is not sure if it is needed. Usually
 *                called from the GD, or via call_out() from remove_object()
 */
public void
clean_up()
{
    if (!objectp(me))
        destruct();
}

/*
 * Function name: remove_object
 * Description:   See if we can safely remove this object. If me exists
 *                then this object will not be removed. Use the -D flag
 *                if you really want to get rid of this object.
 */
public void
remove_object()
{
    set_alarm(1.0, 0.0, clean_up);
}

/*
 * Function name: combat_link
 * Description:   Called by the internal combat routines on startup
 */
public void 
cb_link()
{
    if (objectp(me))
        return;

    me = previous_object();
    i_am_real = !(me->query_npc());
}

/*
 * Description: Return the connected living
 */
public object qme() 
{ 
    return me; 
}

/*
 * Function name: cb_configure
 * Description:   Configure attacks and hitlocations.
 */
public void
cb_configure()
{
    att_id = ({}); 
    hit_id = ({});
    hitloc_ac = ({}); 
    attacks = ({});
}

/*
 * Function name: cb_add_panic
 * Description:   Adjust the panic level.
 * Arguments:     dpan:  The panic increase/decrease
 */
public void
cb_add_panic(int dpan)
{
    int oldpan;

    oldpan = cb_query_panic();

    panic += dpan; 
    if (panic < 0) 
        panic = 0; 

    if (!panic && oldpan)
        tell_object(me, "You feel calm again.\n");
}

/*
 * Function name: cb_query_panic
 * Description:   Give the panic level.
 */
public int
cb_query_panic() 
{ 
    int n, chk;

    if (me->query_enemy(0))
        panic_time = time(); /* So we don't heal panic while fighting */
    else 
    {
        n = (time() - panic_time) / F_INTERVAL_BETWEEN_PANIC_HEALING;
        if (n > 0 && (chk = (1 << n)) > 0)
        {
            if (panic > 0)
                panic = panic / chk;
            panic_time += n * F_INTERVAL_BETWEEN_PANIC_HEALING;
        }
    }

    return panic; 
}

/*
 * Function name: cb_may_panic
 * Description:   Check on our panic level, act accordingly.
 */
public void
cb_may_panic()
{
    int il, size, dis;
    object *tm;

    /* If you don't run away, then you shouldn't panic either. */
    if (me->query_ghost() ||
        me->query_prop(NPC_I_NO_RUN_AWAY))
    {
        return;
    }

    dis = (int)me->query_stat(SS_DIS);
    if (random(cb_query_panic()) > F_PANIC_WIMP_LEVEL(dis))
    {
        tell_object(me,"You panic!\n");
        tell_room(environment(me), QCTNAME(me) + " panics!\n", me);
        tm = (object*)me->query_team_others();
        size = sizeof(tm);
        il = -1;
        while(++il < size)
        {
            if (environment(me) == environment(tm[il]))
                tm[il]->add_panic(25);
        }
        me->run_away();
    }
}

/*
 * Function name: cb_adjust_combat_on_intox
 * Description:   Called to let intoxication affect combat. This
 *                is used to do nasty drunk type things *laugh*
 * Arguments:     pintox: %intoxicated
 */
public void
cb_adjust_combat_on_intox(int pintox)
{
    object *p;

    if (pintox < 90)
        return;

    p = all_inventory(environment(me));

    if (!sizeof(p))
    {
        /* Here we check for neat things to do */
    }
}

/*
 * Normalize offensive / defensive values
 *
 */
static nomask int
fixnorm(int offence, int defence)
{
   if (offence + defence == 0)
       return 0;

   return ((100 * offence) / (offence + defence)) - 50;
}

/*
 * Function name: cb_update_tohit_val
 * Description:   Update the tohit value for some object. Changing ones
 *                encumberance while fighting will not have any effect
 *                unless this function is called, but I think it's worth
 *                it since we save cpu.
 * Arguments:     ob - The object we shall try to hit
 *                weight - If the formula should be weighted any way.
 */
varargs void
cb_update_tohit_val(object ob, int weight)
{
    tohit_ob = ob;
    tohit_val = 2 * fixnorm(me->query_stat(SS_DEX), ob->query_stat(SS_DEX)) -
        fixnorm(me->query_prop(CONT_I_VOLUME), ob->query_prop(CONT_I_VOLUME)) -
        (((me->query_encumberance_weight() +
           me->query_encumberance_volume()) -
          (ob->query_encumberance_weight() +
           ob->query_encumberance_volume())) / 4);
    tohit_val += weight + tohit_mod;
}

/*
 * Function name: cb_set_tohit_mod
 * Description:   Apply a modifier to the base tohit value.  This
 *                differs from specifying a weight value in
 *                cb_update_tohit_val in that the mod will carry
 *                over to different opponents.
 * Arguments:     int mod - the modifier amount
 * Note:          The possibility of combat object cleanup should
 *                be taken into account when using this function.
 */
void
cb_set_tohit_mod(int mod)
{
    tohit_mod = mod;
}

/*
 * Function name: cb_query_tohit_mod
 * Description:   Returns the current tohit mod as set by
 *                cb_set_tohit_mod.
 */
int
cb_query_tohit_mod()
{
    return tohit_mod;
}

/*
 * Function name: cb_tohit
 * Description:   Decide if we hit our victim or not. This should depend
 *                on wchit and skill/stat differences me/victim
 * Arguments:     aid:   The attack id
 *                wchit: Weapon class 'to hit'
 *                vic:   The intended victim
 * Returns:       True if hit, otherwise a negative value indicating how much
 *                we failed.
 */
public int
cb_tohit(int aid, int wchit, object vic)
{
    int tmp, whit;

    /*
     * Four factors are normalized (-50, 50) in the 'to-hit'.
     * 1 - Weapon class 'to hit' <-> Defensive skill
     * 2 - Weight
     * 3 - Volume
     * 4 - Dexterity
     * These are weighted with the factors (4, 1, 1, 2)
     */

    if (sizeof((object *)vic->query_weapon(-1) - ({ 0 })))  /* Any weapons ? */
    {
        tmp = (int) vic->query_skill(SS_PARRY);
    }
    else
    {
        /* Let the encumberance of the victim lower the effectiveness of
         * unarmed combat until 50% when fully encumbered.
         */
        tmp = max(min(vic->query_encumberance_weight(), 100), 0);
        tmp = ((200 - tmp) * vic->query_skill(SS_UNARM_COMBAT)) / 200;
    }

    tmp += vic->query_skill(SS_DEFENSE);

    /*
     * Is it dark or opponent invis? Then how well do we fight?
     */

    if (!CAN_SEE_IN_ROOM(me) ||
        (vic->query_prop(OBJ_I_INVIS) > me->query_prop(LIVE_I_SEE_INVIS)))
    {
        wchit = me->query_skill(SS_BLIND_COMBAT) * wchit / 100;
    }
    if (!CAN_SEE_IN_ROOM(vic) ||
        (me->query_prop(OBJ_I_INVIS) > vic->query_prop(LIVE_I_SEE_INVIS)))
    {
        tmp = vic->query_skill(SS_BLIND_COMBAT) * tmp / 100;
    }

    whit = 4 * fixnorm(random(wchit) + random(wchit) + 
                       random(wchit) + random(wchit), random(tmp));

    if (vic != tohit_ob)
        cb_update_tohit_val(vic);

    whit += tohit_val;

    if (whit > 0)
        return 1;
    else
        return whit - 1;
}

/*
 * Function name: cb_query_combat_time
 * Description  : Query the time value of the time when a hit was last made,
 *                either by us, or on us.
 * Returns      : int - the time value of the last hit..
 */
public int
cb_query_combat_time()
{
    return combat_time;
}

/*
 * Function name: cb_update_combat_time
 * Description  : Remember the last time a hit was made, either by us, or on us.
 */
public void
cb_update_combat_time()
{
    combat_time = time();
}

/*
 * Function name: cb_try_hit
 * Description:   Decide if we a certain attack fails because of something
 *                related to the attack itself, ie specific weapon that only
 *                works some of the time. This is supposed to be
 *                replaced by a more intelligent routine in creature and
 *                humanoid combat. (called from heart_beat)
 * Arguments:     aid:   The attack id
 * Returns:       True if hit, otherwise 0.
 */
public int
cb_try_hit(int aid) 
{ 
    return 1; 
}

/*
 * Function name: cb_got_hit
 * Description:   Tells us that we got hit. It can be used to reduce the ac
 *                for a given hitlocation for each hit. This is supposed to be
 *                replaced by a more intelligent routine in creature and
 *                humanoid combat. (called from cb_hit_me)
 * Arguments:     hid:   The hitloc id
 *                ph:    The %hurt
 *                att:   Attacker
 *                aid:   The attack id of the attacker
 *                dt:    The damagetype
 *                dam:   The number of hitpoints taken
 */
public varargs void
cb_got_hit(int hid, int ph, object att, int aid, int dt, int dam) 
{
}

/*
 * Function name: cb_attack_desc
 * Description:   Gives the description of a certain attack slot. This is
 *                supposed to be replaced by more intelligent routines in
 *                humanoid and creature combat
 * Arguments:     aid:   The attack id
 * Returns:       string holding description
 */
public string
cb_attack_desc(int aid)
{
    return "body";
}

public string
cb_hitloc_desc(int hid)
{
    int i = member_array(hid, hit_id);

    if (i < 0)
    {
        return "body";
    }

    return hitloc_ac[i][HIT_DESC];
}

/* 
 * Function name: tell_watcher
 * Description:   Send the string from the fight to people that want them
 * Arguments:     str   - The string to send
 *                enemy - Who the enemy was
 *                arr   - Array of objects never to send message
 */
varargs void
tell_watcher(string str, mixed enemy, mixed arr)
{
    object *ob;
    int i, size;

    ob = all_inventory(environment(me)) - ({ me });

    if (arr)
    {
        if (pointerp(arr))
            ob -= arr;
        else
            ob -= ({ arr });
    }

    if (!pointerp(enemy))
    {
        enemy = ({ enemy });
    }

    ob -= enemy;

    i = -1;
    size = sizeof(ob);
    while(++i < size)
    {
        if (!ob[i]->query_option(OPT_NO_FIGHTS) && CAN_SEE_IN_ROOM(ob[i]))
        {
            if (CAN_SEE(ob[i], me))
                ob[i]->catch_msg(str);
            else
            tell_object(ob[i], capitalize(FO_COMPOSITE_ALL_LIVE(enemy, ob[i])) +
                (sizeof(enemy) == 1 ? " is " : " are ") +
                "hit by someone.\n");
        }
    }
}

/* 
 * Function name: tell_watcher_miss
 * Description:   Send the string from the fight to people that want them
 *                when there is a miss.
 * Arguments:     str   - The string to send
 *                enemy - Who the enemy was
 *                arr   - Array of objects never to send message
 */
varargs void
tell_watcher_miss(string str, object enemy, mixed arr)
{
    object *ob;
    int i, size;

    ob = all_inventory(environment(me)) - ({ me, enemy });

    if (arr)
    {
        if (pointerp(arr))
            ob -= arr;
        else
            ob -= ({ arr });
    }

    i = -1;
    size = sizeof(ob);
    while(++i < size)
    {
        if (!ob[i]->query_option(OPT_NO_FIGHTS) &&
            !ob[i]->query_option(OPT_GAG_MISSES) &&
            CAN_SEE_IN_ROOM(ob[i]) &&
            CAN_SEE(ob[i], me))
        {
            ob[i]->catch_msg(str);
        }
    }
}

/*
 * Function name: cb_did_hit
 * Description:   Tells us that we hit something. Should produce combat
 *                messages to all relevant parties. This is supposed to be
 *                replaced by a more intelligent routine in creature and
 *                humanoid combat. (called from heart_beat)
 * Arguments:     aid:   The attack id
 *                hdesc: The hitlocation description
 *                hid:   The hitlocation id
 *                phurt: The %hurt made on the enemy
 *                enemy: The enemy who got hit
 *                dt:    The current damagetype
 *                phit:  The %success that we made with our weapon
 *                       If this is negative, it indicates fail
 *                dam:   Damage we did in hit points
 */
public varargs void
cb_did_hit(int aid, string hdesc, int hid, int phurt, object enemy, int dt,
           int phit, int dam)
{
    string attacker_def_desc, other_def_desc,
           damage_desc, other_damage_desc,
           extra_damage_desc,
           armour_desc,
           attack_desc;
    int pdam, rndm;
    object *armours;

    if ((!objectp(enemy)) || (!objectp(me)))
    {
        return;
    }

    attack_desc = cb_attack_desc(aid);

    if (!cb_query_weapon(aid))
    {
        damage_desc = "strike at";
        other_damage_desc    = "strikes at";
    }
    else if (dt == W_IMPALE)
    {
        damage_desc = "thrust at";
        other_damage_desc    = "thrusts at";
    }
    else
    {
        damage_desc = "swing at";
        other_damage_desc    = "swings at";
    }

    if (phurt < 0)
    {
        cb_add_panic(1);

	attacker_def_desc = other_def_desc = ((phit < -50) ? "easily " : "");

	if (phurt == -1)
	{
	    attacker_def_desc += "dodge";
	    other_def_desc    += "dodges";
	}
	else if (phurt == -2)
	{
	    attacker_def_desc += "parry";
	    other_def_desc    += "parries";
	}

        if (i_am_real &&
            !me->query_option(OPT_GAG_MISSES))
        {
            me->catch_msg("You " + damage_desc + " " +
                enemy->query_the_possessive_name(me) + " " + hdesc +
                " with your " + attack_desc + ", but " + enemy->query_pronoun() +
                " " + other_def_desc + " your attack.\n");
        }
        if (interactive(enemy) &&
            !enemy->query_option(OPT_GAG_MISSES))
        {
            enemy->catch_msg(me->query_The_name(enemy) + " " +
                other_damage_desc + " your " + hdesc + " with " +
                me->query_possessive() + " " + attack_desc + ", but you " +
                attacker_def_desc + " " + me->query_possessive() +
                " attack.\n");
        }

        tell_watcher_miss(QCTNAME(me) + " " + other_damage_desc + " " +
            QTPNAME(enemy) + " " + hdesc + " with " + me->query_possessive() +
            " " + attack_desc + ", but " + QTNAME(enemy) + " " +
            other_def_desc + " the attack.\n", enemy);

        return;
    }

    cb_add_panic(-3 - (phurt / 5));

    if (dam == 0)
    {
        armours = enemy->query_combat_object()->cb_query_armour(hid);

        if (sizeof(armours) && sizeof(armours = filter(armours,
            &operator(!=)(A_MAGIC) @ &->query_at())))
        {
            armour_desc = QSHORT(armours[random(sizeof(armours))]);

            if (i_am_real)
            {
                me->catch_msg("You " + damage_desc + " " +
                    enemy->query_the_possessive_name(me) + " " + hdesc +
                    " with your " + attack_desc + ", but " +
                    enemy->query_possessive() + " " + armour_desc +
                    " protects " + enemy->query_objective() + ".\n");
            }

            if (interactive(enemy))
            {
                enemy->catch_msg(me->query_The_name(enemy) + " " +
                    other_damage_desc + " your " + hdesc + " with " +
                    me->query_possessive() + " " + attack_desc +
                    ", but your " + armour_desc + " protects you.\n");
            }

            tell_watcher(QCTNAME(me) + " " + other_damage_desc + " " +
                QTPNAME(enemy) + " " + hdesc + " with " +
                me->query_possessive() + " " + attack_desc + ", but " +
                enemy->query_possessive() + " " + armour_desc + " protects " +
                enemy->query_objective() + ".\n", enemy);
            return;
        }
    }

    extra_damage_desc = " ";
    pdam = 100 * dam / enemy->query_max_hp();

    switch (pdam)
    {
        case 0..2:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "lightly bruise";
                    other_damage_desc = "lightly bruises";
                    break;
                case W_SLASH:
                    damage_desc = "barely scrape";
                    other_damage_desc = "barely scrapes";
                    break;
                case W_IMPALE:
                    damage_desc = "just nick";
                    other_damage_desc = "just nicks";
                    break;
                default:
                    damage_desc = "barely wound";
                    other_damage_desc = "barely wounds";
                    break;
            }

            break;

        case 3..5:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "batter";
                    other_damage_desc = "batters";
                    break;
                case W_SLASH:
                    damage_desc = "scratch";
                    other_damage_desc = "scratches";
                    break;
                case W_IMPALE:
                    damage_desc = "graze";
                    other_damage_desc = "grazes";
                    break;
                default:
                    damage_desc = "slightly wound";
                    other_damage_desc = "slightly wounds";
                    break;
            }

            break;

        case 6..9:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "pound";
                    other_damage_desc = "pounds";
                    break;
                case W_SLASH:
                    damage_desc = "rake";
                    other_damage_desc = "rakes";
                    break;
                case W_IMPALE:
                    damage_desc = "jab";
                    other_damage_desc = "jabs";
                    break;
                default:
                    damage_desc = "slightly wound";
                    other_damage_desc = "slightly wounds";
                    break;
            }

            break;

        case 10..19:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "beat";
                    other_damage_desc = "beats";
                    break;
                case W_SLASH:
                    damage_desc = "cut";
                    other_damage_desc = "cuts";
                    break;
                case W_IMPALE:
                    damage_desc = "stab";
                    other_damage_desc = "stabs";
                    break;
                default:
                    damage_desc = "wound";
                    other_damage_desc = "wounds";
                    break;
            }

            break;

        case 20..39:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "seriously beat";
                    other_damage_desc = "seriously beats";
                    break;
                case W_SLASH:
                    damage_desc = "seriously cut";
                    other_damage_desc = "seriously cuts";
                    break;
                case W_IMPALE:
                    damage_desc = "seriously stab";
                    other_damage_desc = "seriously stabs";
                    break;
                default:
                    damage_desc = "wound";
                    other_damage_desc = "wounds";
                    break;
            }

            break;

        case 40..59:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "soundly beat";
                    other_damage_desc = "soundly beats";
                    break;
                case W_SLASH:
                    damage_desc = "cut deeply into";
                    other_damage_desc = "cuts deeply into";
                    break;
                case W_IMPALE:
                    damage_desc = "stab deeply into";
                    other_damage_desc = "stabs deeply into";
                    break;
                default:
                    damage_desc = "seriously wound";
                    other_damage_desc = "seriously wounds";
                    break;
            }

            break;

        case 60..90:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "hammer";
                    other_damage_desc = "hammers";
                    break;
                case W_SLASH:
                    damage_desc = "rip into";
                    other_damage_desc = "rips into";
                    break;
                case W_IMPALE:
                    damage_desc = "lance through";
                    other_damage_desc = "lances through";
                    break;
                default:
                    damage_desc = "wound";
                    other_damage_desc = "wounds";
                    extra_damage_desc = " very seriously ";
                    break;
            }

            break;

        default:
            switch (dt)
            {
                case W_BLUDGEON:
                    damage_desc = "brutally pummel";
                    other_damage_desc = "brutally pummels";
                    break;
                case W_SLASH:
                    damage_desc = "fiercely rive";
                    other_damage_desc = "fiercely rives";
                    break;
                case W_IMPALE:
                    damage_desc = "viciously impale";
                    other_damage_desc = "viciously impales";
                    break;
                default:
                    damage_desc = "critcally wound";
                    other_damage_desc = "critcally wounds";
                    break;
            }

            break;
    }

    if (i_am_real)
    {
        me->catch_msg("You " + damage_desc + " " +
            enemy->query_the_possessive_name(me) + " " + hdesc +
            extra_damage_desc + "with your " + attack_desc + ".\n");

    }

    if (interactive(enemy))
    {
        enemy->catch_msg(QCTNAME(me) + " " + other_damage_desc + " your " +
            hdesc + extra_damage_desc + "with " +  me->query_possessive() +
            " " + attack_desc + ".\n");
    }

    tell_watcher(QCTNAME(me) + " " + other_damage_desc + " " + QTPNAME(enemy) +
        " " + hdesc + extra_damage_desc + "with " + me->query_possessive() +
        " " + attack_desc + ".\n", enemy);
}

/*
 * Function name: cb_death_occured
 * Description:   Called when 'me' dies
 * Arguments:     killer: The enemy that caused our death.
 */
public void
cb_death_occured(object killer)
{
    /*
     * We forget our enemies when we die.
     */
    stop_heart();
    enemies = ({});
    attack_ob = 0;
    combat_time = 0;
}

/*
 * Function name: cb_add_enemy
 * Description:   Used to add enemies to 'me'
 * Arguments:     enemy: The enemy to be
 *                force: If true and enemy array full one other is replaced
 */
public varargs void
cb_add_enemy(object enemy, int force)
{
    int pos;

    enemies = filter(enemies, objectp);

    /* Make sure panic value is updated before we add enemies */
    cb_query_panic(); 
    pos = member_array(enemy, enemies);

    if (force && pos >= 0)
    {
        enemies = ({ enemy }) + exclude_array(enemies, pos, pos);
    }
    else if (force)
    {
        enemies = ({ enemy }) + enemies;
    }
    else if (pos < 0)
    {
        enemies = enemies + ({ enemy });
    }

    if (sizeof(enemies) > MAX_ENEMIES)
    {
        enemies = slice_array(enemies, 0, MAX_ENEMIES - 1);
    }
}

/*
 * Function name: cb_adjust_combat_on_move
 * Description:   Called to let movement affect the ongoing fight. This
 *                is used to print hunting messages or drag enemies along.
 * Arguments:     True if leaving else arriving
 */
public void 
cb_adjust_combat_on_move(int leave)
{
    int i, pos, size;
    object *inv, enemy, *all, *rest, *drag;

    if (environment(me))
    {
        all = all_inventory(environment(me));
        inv = all & enemies;
        if (leave)
        {
            /*
             * If the aggressors are around.
             */
            if (sizeof(inv))
            {
                drag = ({ });
                rest = ({ });
                i = -1;
                size = sizeof(inv);
                while(++i < size)
                {
                    if (inv[i]->query_prop(LIVE_O_ENEMY_CLING) == me)
                    {
                        drag += ({ inv[i] });
                        tell_object(inv[i], "As " +
                            me->query_the_name(inv[i]) + 
                            " leaves, you are dragged along.\n");
                    }
                    else
                    {
                        rest += ({ inv[i] });
                        tell_object(inv[i], "You are now hunting " +
                            me->query_the_name(inv[i]) + ".\n");
                    }
                }

                if (sizeof(drag))
                {
                    if (i_am_real)
                    {
                        me->catch_msg(
                            "As you try to leave, you can not get rid of " +
                            COMPOSITE_LIVE(drag) + ".\n");
                    }
                    me->add_prop(TEMP_DRAGGED_ENEMIES, drag);
                }

                if (sizeof(rest) && i_am_real)
                {
                    me->catch_tell("You are now hunted by " + 
                        FO_COMPOSITE_ALL_LIVE(rest, me) + ".\n");
                }

                /* Stop fighting all the enemies that don't follow us.
                   We must still fight the enemies that do, since otherwise
                   we can move so quickly that we don't update our enemies
                   to include them when they attack again, although they
                   will autofollow and attack again on entry.
                */
                this_object()->cb_stop_fight(rest);
            }
        } 
        else 
        {
            i = -1;
            size = sizeof(inv);
            while(++i < size)
            {
                if (CAN_SEE(me, inv[i]) && CAN_SEE_IN_ROOM(me) &&
                    !NPATTACK(inv[i]))
                {
                    me->attack_object(inv[i]);
                    cb_update_tohit_val(inv[i], 30); /* Give hunter bonus */
                    tell_room(environment(me), QCTNAME(me) + " attacks " +
                        QTNAME(inv[i]) + ".\n", ({ inv[i], me }));
                    tell_object(inv[i], me->query_The_name(inv[i]) +
                        " attacks you!\n");
                    tell_object(me, "You attack " +
                        inv[i]->query_the_name(me) + ".\n");
                }
            }
        }
    }
}

/*
 * Function name: cb_run_away
 * Description:   'me' runs away from the fight
 * Arguments:     dir: The first dir tried
 */
public void
cb_run_away(string dir)
{
    object          here;
    int             size, pos,
                    i,
                    j;
    mixed           *exits;
    string          *std_exits, old_mout, old_min;

    if (me->query_ghost() ||
        me->query_prop(NPC_I_NO_RUN_AWAY))
    {
        return;
    }

    here = environment(me);
    i = 0;
    std_exits = ({ "north", "south", "west", "east", "up", "down" });
    if (stringp(dir))
        me->command(dir);

    exits = here->query_exit();
    size = sizeof(exits);
    j = random(size / 3);

    while (i < size && here == environment(me))
    {
        i += 3;
        if ((pos = member_array(exits[j * 3 + 1], std_exits)) > -1)
            std_exits[pos] = "";
        old_mout = me->query_m_out();
        me->set_m_out("panics and flees");
        old_min = me->query_m_in();
        me->set_m_in("rushes in, panicky");
        catch(me->command(exits[j * 3 + 1]));
        me->set_m_out(old_mout);
        me->set_m_in(old_min);
        j++;
        if (j * 3 >= size)
            j = 0;
    }

    size = sizeof(std_exits);
    j = random(size);
    i = 0;
    while (i < size && here == environment(me))
    {
        i++;
        if (strlen(std_exits[j]))
        {
            old_mout = me->query_m_out();
            me->set_m_out("panics and flees");
            old_min = me->query_m_in();
            me->set_m_in("rushes in, panicky");
            catch(me->command(std_exits[j]));
            me->set_m_out(old_mout);
            me->set_m_in(old_min);
        }
        j++;
        if (j >= size)
            j = 0;
    }

    if (here == environment(me))
    {
        tell_room(environment(me),
            QCTNAME(me) + " tried, but failed to run away.\n", me);

        tell_object(me, "Your legs tried to run away, but failed.\n");
    }
    else
    {
        tell_object(me, "Your legs run away with you!\n");
    }
}

/*
 * Function name: cb_wield_weapon
 * Description:   Wield a weapon. 'Weapon' is here a general term for any tool
 *                used as a weapon. Only players are limited to /std/weapon
 *                weapons.
 * Arguments:     wep - The weapon to wield.
 * Returns:       True if wielded.
 */
public mixed
cb_wield_weapon(object wep) 
{ 
    return ""; 
}

/*
 * Function name: unwield
 * Description:   Unwield a weapon.
 * Arguments:     wep - The weapon to unwield.
 * Returns:       None.
 */
public void
cb_unwield(object wep) 
{
}

/*
 * Function name: cb_query_weapon
 * Description:   Returns the weapon held in a specified location.
 *                A list of all if argument -1 is given.
 * Arguments:     which: A numeric label describing a weapon
 *                       location. On humanoids this is W_RIGHT etc.
 * Returns:       The corresponding weapon(s).
 */
public mixed
cb_query_weapon(int which)
{
    int pos;

    if (which == -1)
    {
        return map(attacks, &operator([])(, ATT_OBJ)) - ({ 0 });
    }

    if ((pos = member_array(which, att_id)) < 0)
    {
        return 0;
    }

    return attacks[pos][ATT_OBJ];
}

/*
 * Function name: cb_wear_arm
 * Description:   Wear an armour
 * Arguments:     arm - The armour.
 */
public int
cb_wear_arm(object arm) 
{ 
    return 0; 
}

/*
 * Function name: cb_remove_arm
 * Description:   Remove an armour
 * Arguments:     arm - The armour.
 */
public void
cb_remove_arm(object arm) 
{
}

/*
 * Function name: cb_query_armour
 * Description:   Returns the armour(s) protecting a given position.
 *                A list of all if argument -1 is given.
 * Arguments:     which: A numeric label describing an armour
 *                       location. On humanoids this is TS_HEAD etc.
 * Returns:       The corresponding armour(s)
 */
public mixed
cb_query_armour(int which)
{
    int i;
    object *arms;

    if (which == -1)
    {
        arms = ({});
        for (i = 0; i < sizeof(hitloc_ac); i++)
        {
            if (pointerp(hitloc_ac[i][HIT_ARMOURS]))
            {
                arms |= hitloc_ac[i][HIT_ARMOURS];
            }
        }
 
        return arms;
    }

    if ((i = member_array(which , hit_id)) < 0)
    {
        return 0;
    }

    return hitloc_ac[i][HIT_ARMOURS];
}

/*
 * Function name: cb_calc_speed
 * Description:   Calculates and sets my speed.
 */
public void
cb_calc_speed()
{
    int i;
    if (me) i = me->query_prop(LIVE_I_QUICKNESS);
    speed = 5.0 - (itof(i) / 50.0);
    speed = MAX(2.0, speed);
}

/*
 * Function name: cb_query_speed
 * Description:   Returns the combat speed (seconds between combat rounds)
 */
public float
cb_query_speed()
{
    return speed;
}

/*
 * Function name: cb_update_speed
 * Description:   Makes sure the correct speed is used.
 */
public nomask void
cb_update_speed()
{
    mixed alarm;
    float oldspeed = speed;

    cb_calc_speed();
    if ((speed != oldspeed) &&
        alarm_id &&
        (alarm = get_alarm(alarm_id)))
    {
        remove_alarm(alarm_id);
        alarm_id = set_alarm(alarm[2], speed, heart_beat);
    }
}

/***********************************************************
 * The non redefinable functions follows below
 */

static void
restart_heart()
{
    /* Mark this moment as being in combat. */
    cb_update_combat_time();

    if (!alarm_id || !get_alarm(alarm_id))
    {
        cb_update_speed();
        alarm_id = set_alarm(speed, speed, heart_beat);
    }
}

/*
 * Function name: stop_heart
 * Description  : Called to stop the heartbeat. It will remove the running
 *                alarm.
 */
static void
stop_heart()
{
    me->remove_prop(LIVE_I_ATTACK_DELAY);
    remove_alarm(alarm_id);
    alarm_id = 0;

    if (!sizeof(enemies))
    {
        combat_time = 0;
    }
}

/*
 * Function name: heart_beat
 * Description:   Do 1 round of fighting with the choosen enemy. This is
 *                done as long as both me and enemy is alive and in the
 *                same place.
 */
static nomask void
heart_beat()
{
    int             il, dt, hitsuc, tmp, size, crit, ftg;
    string logtext;
    mixed           *hitresult, *dbits, pen, fail;
    object          *new, ob;

    if (!objectp(me) || me->query_ghost())
    {
        attack_ob = 0;
        stop_heart();
        return;
    }

    /*
     * Do something when the enemy is somehow lost
     */
    if (!attack_ob || attack_ob->query_ghost() ||
        environment(attack_ob) != environment(me))
    {
        me->notify_enemy_gone(attack_ob);

        /* To cling to an enemy we must fight it. */
        me->remove_prop(LIVE_O_ENEMY_CLING);

        /* Switch enemy if we have an alternate. */
        enemies = filter(enemies, objectp);
        new = (all_inventory(environment(me)) & enemies) - ({ attack_ob });

        if (sizeof(new))
            attack_ob = new[0];
        else {
            if (attack_ob && attack_ob->query_ghost())
            {
                me->remove_prop(LIVE_I_ATTACK_DELAY);
                me->remove_prop(LIVE_I_STUNNED);
            }
            attack_ob = 0;
        }

        /* We attack another enemy when old enemy left. */
        if (attack_ob)
        {
            tell_object(me, "You turn to attack " +
                attack_ob->query_the_name(me) + ".\n");
            heart_beat();
            return;
        }
        else
        {
            stop_heart();
            return;
        }
    }

    /* First do some check if we actually attack. */
    if (pointerp(fail = me->query_prop(LIVE_AS_ATTACK_FUMBLE)) &&
        sizeof(fail))
    {
        if (i_am_real)
        {
            me->catch_msg(fail[0]);
        }
        return;
    }

    if ((tmp = me->query_prop(LIVE_I_ATTACK_DELAY)))
    {
        if ((tmp -= ftoi(speed)) > 0)
        {
            me->add_prop(LIVE_I_ATTACK_DELAY, tmp);
            return;
        }
        else
            me->remove_prop(LIVE_I_ATTACK_DELAY);
    }

#if 0
    if (((hit_heart * 100) + me->query_prop(LIVE_I_QUICKNESS)) < random(100))
        return 1; /* Wait one round more then. */
    hit_heart = -1;
#endif

    if (me->query_prop(LIVE_I_STUNNED))
    {
        return;
    }

    /* This is a hook for NPC's so that they can do spells or any
     * special actions when in combat. See /std/mobile.c
     */
    if (me->query_npc() && me->special_attack(attack_ob))
    {
        return;
    }

    /* This is the hook for single special attacks, normally spells,
     * that is done instead of the normal attacks, one turn.
     */
    if (objectp(ob = me->query_prop(LIVE_O_SPELL_ATTACK)))
    {
        ob->spell_attack(me, attack_ob);
        me->remove_prop(LIVE_O_SPELL_ATTACK);
        return;
    }

    if (me->query_prop(LIVE_I_CONCENTRATE))
    {
        return;
    }

    /* Mark this moment as being in combat. */
    cb_update_combat_time();

    il = -1;
    size = sizeof(attacks);
    while(++il < size)
    {
        /*
         * Will we use this attack this round? (random(100) < %use)
         */
        if (random(100) < attacks[il][ATT_PROCU])
        {
            /*
             * The attack has a chance of failing. If for example the attack
             * comes from a wielded weapon, the weapon can force a fail or
             * if the wchit is to low for this opponent.
             */
            hitsuc = cb_try_hit(att_id[il]);
            if (hitsuc <= 0)
            {
                continue;
            }

            /* 
             * The intended victim can also force a fail. like in the weapon
             * case, if fail, the cause must produce explanatory text himself.
             */
            hitsuc = attack_ob->query_not_attack_me(me, att_id[il]);
            if (hitsuc > 0)
            {
                continue;
            }

            hitsuc = cb_tohit(att_id[il], attacks[il][ATT_WCHIT], attack_ob);

            if (hitsuc > 0)
            {
                /* Choose one damage type */
                if (crit = (!random(10000)))
                {
                    // Critical hit!
                    pen = attacks[il][ATT_M_PEN];
                    if (sizeof(pen))
		    {
                        pen = pen[0];
		    }
 
//                    if (random(10))
		    {
                        pen *= 5;
		    }
//                    else
//		    {
//                        pen *= 10;
//                        pen += attack_ob->query_max_hp();
//		    }
                }
                else
                {
                    pen = attacks[il][ATT_M_PEN];

                    if (sizeof(pen))
                    {
                        tmp = MATH_FILE->quick_find_exp(dt);
                        if((tmp < sizeof(pen)))
                            pen = pen[tmp];
                        else
                            pen = pen[0];
                    }
                }

                dt = attacks[il][ATT_DAMT];
                dbits = ({dt & W_IMPALE, dt & W_SLASH, dt & W_BLUDGEON }) - ({0});
                dt = sizeof(dbits) ? one_of_list(dbits) : W_NO_DT;

                hitresult = (mixed*)attack_ob->hit_me(pen, dt, me, att_id[il]);

                if (crit)
		{
                   log_file("CRITICAL", sprintf("%s: %-11s on %-11s " +
                       "(dam = %4d(%4d))\n\t%s on %s\n",
                       ctime(time()), me->query_real_name(),
                       attack_ob->query_real_name(), hitresult[3], pen,
                       file_name(me), file_name(attack_ob)), -1);
		}
            }
            else
            {
                hitresult = 
                    (mixed*)attack_ob->hit_me(hitsuc, 0, me, att_id[il]);
            }

            /*
             * Generate combat message, arguments Attack id, hitloc description
             * proc_hurt, Defender
             */
            if (hitsuc > 0)
            {
                hitsuc = attacks[il][ATT_WCPEN][tmp];
                if (hitsuc > 0)
                {
                    hitsuc = 100 * hitresult[2] / hitsuc;
                }
                else
                {
                    hitsuc = 0;
                }
            }
            if (hitresult[1])
            {
                cb_did_hit(att_id[il], hitresult[1], hitresult[4], hitresult[0], 
                       attack_ob, dt, hitsuc, hitresult[3]);
            }
            else
            {
                break; /* Ghost, linkdeath, immortals etc */
            }

            /* Oops, Lifeform turned into a deadform. Reward the killer. */
            if ((int)attack_ob->query_hp() <= 0)
            {
                enemies = enemies - ({ attack_ob });
                attack_ob->do_die(me);
                break;
            }
        }
    }

    /*
     * Fighting is quite tiresome you know
     */
    ftg = random(3) + 1;
    if (me->query_fatigue() >= ftg)
    {
        me->add_fatigue(-ftg);
    }
    else
    {
        tell_object(me,
                "You are so tired that every move drains your health.\n");
        me->set_fatigue(0);
        me->reduce_hit_point(ftg);
    }

    /*
     * Fighting is frightening, we might panic!
     */
    cb_may_panic();

    if (attack_ob && !attack_ob->query_ghost())
    {
        return;
    }
    else
    {
        new = (all_inventory(environment(me)) & enemies) - ({ attack_ob });
        if (sizeof(new))
        {
            attack_ob = new[0];
            if (attack_ob)
                tell_object(attack_ob, "You turn to attack " +
                    attack_ob->query_the_name(me) + ".\n");
        }
        else
        {
            attack_ob = 0;
            stop_heart();
            return;
        }
    }
    return;
}


/*
 * Function name: cb_hit_me
 * Description:   Called to decide damage for a certain hit on 'me'.
 * Arguments:     wcpen:         Weapon class penetration
 *                dt:            Damage type, MAGIC_DT if no ac helps
 *                attacker: 
 *                attack_id:     -1 if a special attack
 *                target_hitloc: The hit location to damage.  If left
 *                               unspecified or an invalid hitloc is
 *                               given, a random hitlocation will be
 *                               used.
 * Returns:       Result of hit: ({ proc_hurt, hitloc desc, phit, dam, hitloc id })
 */
varargs public nomask mixed
cb_hit_me(int wcpen, int dt, object attacker, int attack_id, int target_hitloc = -1)
{
    object      *my_weapons, my_weapon, attacker_weapon;
    int         proc_hurt, hp,
                tmp, dam, phit,
                hloc,
                j, size;
    string      msg;
    mixed       ac, attack;

    if (!objectp(me))
    {
        cb_link();
    }

    /* You can not hurt the dead. */
    if (me->query_ghost())
    {
        tell_object(attacker, me->query_The_name(attacker) +
            " is a already dead, quite dead.\n");
        tell_room(environment(me),
            QCTNAME(attacker) + " is trying to kill the already dead.\n",
            ({ me, attacker }) );
        tell_object(me, attacker->query_The_name(me) +
            " tries futily to attack you.\n");
        me->stop_fight(attacker);
        attacker->stop_fight(me);
        return ({ 0, 0, 0, 0, 0 });
    }

    /* Update the list of aggressors. If we hit ourselves: no update. */
    cb_add_enemy(attacker);
    if (!attack_ob && attacker != me) 
    {
        attack_ob = attacker;
    }
    restart_heart();

    /* Mark that we were hit. */
    cb_update_combat_time();

    /* Choose a hit location, and compute damage if wcpen > 0 */
    if ((target_hitloc == -1) || ((hloc = member_array(target_hitloc, hit_id)) < 0))
    {
        tmp = random(100);
        j = 0;
        hloc = -1;
        size = sizeof(hitloc_ac);
        while(++hloc < size)
        {
            j += hitloc_ac[hloc][HIT_PHIT];
            if (j >= tmp)
            {
                break;
            }
        }

        if (hloc >= sizeof(hitloc_ac))
        {
            hloc = sizeof(hitloc_ac) - 1;
            log_file("BAD_HITLOC", me->query_real_name() + " (" + file_name(me) +
                "): " + file_name(this_object()) + "\n");
        }
    }

    ac = hitloc_ac[hloc][HIT_M_AC];

    if (wcpen > 0)
    {
        if (dt == MAGIC_DT)
	{
            ac = 0;

            /* MAGIC_DT damage has a base damage value of wcpen / 4 */
            phit = wcpen / 4;
            phit += random(phit) + random(phit) + random(phit);
	}
        else
        {
            tmp = (int)MATH_FILE->quick_find_exp(dt);

            if (sizeof(ac) && (tmp < sizeof(ac)))
            {
                ac = ac[tmp];
            }
            else if (sizeof(ac))
            {
                ac = ac[0];
            }
            else if (!intp(ac))
            {
                ac = 0;
            }

            phit = wcpen / 4;
            phit = random(phit) + random(phit) + random(phit) + random(phit);

            ac = random(ac);
        }

        dam = max(0, F_DAMAGE(phit, ac));
    }
    else
    {
        dam = 0;
        phit = (wcpen < 0 ? wcpen : -1);
    }

    hp = me->query_hp();

    /*
     * Wizards are immortal. (immorale ??)
     */
    if ((int)me->query_wiz_level() && dam >= hp)
    {
        tell_object(me, "Your wizardhood protects you from death.\n");
        tell_room(environment(me),
            QCTNAME(me) + " is immortal and fails to die!\n", me);
        return ({ 0, 0, 0, 0, 0 });
    }

    /*
     * Ok, hurt me.
     */
    if (dam > 0 && hp)
    {
        proc_hurt = (100 * dam) / hp;
        if (dam && !proc_hurt)
            proc_hurt = 1;     /* Less than 1% damage */
    }
    else if (dam > 0)
        proc_hurt = 100;
    else if (wcpen >= 0)
        proc_hurt = 0;
    else
    {
	attack = attacker->query_combat_object()->query_attack(attack_id);
	my_weapons = me->query_weapon(-1);
	
        if (!sizeof(my_weapons))
        {
            proc_hurt = -1;   /* we dodged */          
        }
	else
	{    
            tmp = random(me->query_skill(SS_PARRY) + 
			 me->query_skill(SS_DEFENSE));

	    if (sizeof(attack) && objectp(attack[6]))
	    {
		attacker_weapon = attack[6];
	    }
		
	    if (tmp < me->query_skill(SS_PARRY) &&
		attacker_weapon->query_wt() != W_MISSILE)
            {
	        proc_hurt = -2;   /* we parried */
		my_weapon = my_weapons[random(sizeof(my_weapons))];
		my_weapon->did_parry(attacker, attack_id, dt);
	    }
	    else
	    {
	        proc_hurt = -1;   /* we dodged */
	    }
	}
    }

    if (dam > 0)
    {
        me->heal_hp(-dam);
    }

    /* Adjust our panic level. */
    if (proc_hurt >= 0)
    {
        cb_add_panic(2 + proc_hurt / 5);
    }

    /* Tell us where we were attacked and by which damagetype. */
    cb_got_hit(hit_id[hloc], proc_hurt, attacker, attack_id, dt, dam);

    /* Reward attacker for hurting me. */
    if (dam)
    {
#ifdef CB_HIT_REWARD
        me->combat_reward(attacker, dam, 0);
#endif
        if (random(dam) > random(me->query_stat(SS_DIS)))
        {
            me->cmdhooks_break_spell();
        }
    }

    return ({ proc_hurt, hitloc_ac[hloc][HIT_DESC], phit, dam, hit_id[hloc] });
}

/*
 * Function name: cb_attack
 * Description:   Called by the internal combat routines to attack.
 * Arguments:     victim: The object of the attack
 */
public nomask void
cb_attack(object victim)
{
    if (!me)
    {
        return;
    }

    restart_heart();

    if (victim == me || victim == attack_ob || victim->query_ghost())
    {
        return;
    }

    me->reveal_me(1);
    victim->reveal_me(1);
    /* Swap attack. */
    cb_add_enemy(victim, 1);
    attack_ob = victim;

    victim->attacked_by(me);

    /* No autosneak in combat. */
    me->remove_prop(LIVE_I_SNEAK);
}

/*
 * Function name:  cb_attacked_by
 * Description:    This routine is called when we are attacked or when 
 *                 someone we are hunting appears in our location.
 * Arguments:      ob: The attacker
 */
public nomask void
cb_attacked_by(object ob)
{
    cb_add_enemy(ob);

    if (!attack_ob || (!query_ip_number(attack_ob) && query_ip_number(ob)))
    {
        attack_ob = ob;
    }

    restart_heart();

    /* No autosneak in combat. */
    me->remove_prop(LIVE_I_SNEAK);

#if 0
    if (me)
    {
        me->cr_attacked_by(ob);
    }
#endif
}

/*
 * Function name: cb_stop_fight
 * Description:   Stop fighting certain enemies
 */
public nomask void
cb_stop_fight(mixed elist)
{
    object *local;

    if (objectp(elist))
    {
        elist = ({ elist });
    }
    else if (!pointerp(elist))
    {
        elist = ({});
    }

    if (member_array(attack_ob, elist) >= 0)
    {
        attack_ob = 0;
    }

    if (pointerp(enemies) && pointerp(elist))
    {
        enemies = enemies - (object *)elist;
    }

    if (sizeof(enemies))
    {
        local = enemies & all_inventory(environment(me));
        if (sizeof (local))
        {
            attack_ob = local[0];
        }
    }
    else
    {
        combat_time = 0;
    }
}

/*
 * Function name: cb_query_enemy
 * Description:   Gives our current enemy
 * Arguments:     arg: Enemy number, (-1 == all enemies)
 * Returns:       Object pointer to the enemy
 */
public nomask mixed
cb_query_enemy(int arg)
{
    enemies = filter(enemies, objectp);

    if (arg == -1)
    {
        return enemies + ({});
    }
    else if (arg < sizeof(enemies))
    {
        return enemies[arg];
    }

    return 0;
}

/*
 * Function name: cb_query_attack
 * Description:   Gives the enemy we are fighting, if we are fighting...
 * Returns:       Object pointer to the enemy
 */
public nomask mixed
cb_query_attack() 
{ 
    return attack_ob; 
}

/*
 * Function name:  cb_heal
 * Description:    Heals the living object. Adds hp, mana and fatigue, panic
 * Arguments:      delay: Number of heart_beats since last heal
 * Returns:        0 if we healed 'me'
 */
public nomask int
cb_heal(int delay)
{
    return 0;
}

/**********************************************************
 * 
 * Below is internal functions, only used by the inheritor of
 * this standard combat object.
 */

/*
 * Function name: add_attack
 * Description:   Add an attack to the attack array.
 * Arguments:
 *             wchit: Weapon class to hit
 *             wcpen: Weapon class penetration
 *             dt:    Damage type
 *             %use:  Chance of use each turn
 *             id:    Specific id, for humanoids W_NONE, W_RIGHT etc
 *             skill: Optional skill with this attack
 *             wep:   The weapon, if there is one
 *
 * Returns:       True if added.
 */
varargs int
add_attack(int wchit, mixed wcpen, int damtype, int prcuse, int id, int skill,
    object wep)
{
    int pos, *pen, *m_pen;

    if (sizeof(attacks) >= MAX_ATTACK)
    {
        return 0;
    }

    pen = allocate(W_NO_DT);
    m_pen = allocate(W_NO_DT);

    if (skill == 0)
    {
        skill = wchit;
    }
    else if (skill < 1)
    {
        skill = 0;
    }

    pos = -1;
    while(++pos < W_NO_DT)
    {
        if (!pointerp(wcpen))
        {
            m_pen[pos] = F_PENMOD(wcpen, skill);
            pen[pos] = wcpen;
        }
        else if (pos >= sizeof(wcpen))
        {
            m_pen[pos] = (pos ? m_pen[0] : 0);
            pen[pos] = (pos ? pen[0] : 0);
        }
        else
        {
            m_pen[pos] = F_PENMOD(wcpen[pos], skill);
            pen[pos] = wcpen[pos];
        }
    }

    if ((pos = member_array(id, att_id)) < 0)
    {
        att_id += ({ id });
        attacks += ({ ({ wchit, pen, damtype, prcuse, skill, m_pen, wep }) });
        return 1;
    }
    else
    {
        attacks[pos] = ({ wchit, pen, damtype, prcuse, skill, m_pen, wep });
    }

    return 1;
}

/*
 * Function name: remove_attack
 * Description:   Removes a specific attack
 * Arguments:     id: The attack id
 * Returns:       True if removed
 */
static int
remove_attack(int id) 
{
    int pos;

    if ((pos = member_array(id, att_id)) >= 0)
    {
        attacks = exclude_array(attacks, pos, pos);
        att_id = exclude_array(att_id, pos, pos);
        return 1;
    }

    return 0;
}

/*
 * Function name: query_attack_id
 * Description:   Give all attack id's
 * Returns:       Array with elements as described in add_attack
 */
public int *
query_attack_id() 
{ 
    return att_id + ({}); 
}

/*
 * Function name: query_attack
 * Description:   Give the attack for a certain id
 * Arguments:     id: The id to return attack array for
 * Returns:       Array with elements as described in add_attack
 */
public mixed *
query_attack(int id) 
{
    int pos;

    if ((pos = member_array(id, att_id)) >= 0)
    {
        return attacks[pos];
    }

    return 0;
}

/*
 * Function name: add_hitloc
 * Description:   Add a hitlocation to the hitloc array
 * Arguments:
 *            ac:    The ac's for a given hitlocation, can be an int
 *            %hit:  The chance that a hit will hit this location
 *            desc:  String describing this hitlocation, ie "head", "tail"
 *            id:    Specific id, for humanoids A_TORSO, A_HEAD etc
 *
 * Returns:       True if added.
 */
static varargs int
add_hitloc(mixed ac, int prchit, string desc, int id, object *armours)
{
    int pos, *act, *m_act;

    if (sizeof(hitloc_ac) >= MAX_HITLOC)
    {
        return 0;
    }

    act = allocate(W_NO_DT);
    m_act = allocate(W_NO_DT);

    pos = -1;
    while(++pos < W_NO_DT)
    {
        if (!pointerp(ac))
        {
            m_act[pos] = F_AC_MOD(ac);
            act[pos] = ac;
        }
        else if (pos >= sizeof(ac))
        {
            act[pos] = (pos ? act[0] : 0);
            m_act[pos] = (pos ? F_AC_MOD(act[0]) : 0);
        }
        else
        {
            m_act[pos] = F_AC_MOD(ac[pos]);
            act[pos] = ac[pos];
        }
    }
    if ((pos = member_array(id, hit_id)) < 0)
    {
        hit_id += ({ id });
        hitloc_ac += ({ ({ act, prchit, desc, m_act, armours }) });
    }
    else
    {
        hitloc_ac[pos] = ({ act, prchit, desc, m_act, armours });
    }

    return 1;
}

/*
 * Function name: remove_hitloc
 * Description:   Removes a specific hit location
 * Arguments:     id: The hitloc id
 * Returns:       True if removed
 */
static int
remove_hitloc(int id) 
{
    int pos;

    if ((pos = member_array(id, hit_id)) >= 0)
    {
        hitloc_ac = exclude_array(hitloc_ac, pos, pos);
        hit_id = exclude_array(hit_id, pos, pos);
        return 1;
    }

    return 0;
}

/*
 * Function name: query_hitloc_id
 * Description:   Give all hitloc id's
 * Returns:       Array with elements as described in add_attack
 */
public int *
query_hitloc_id() 
{ 
    return hit_id + ({}); 
}

/*
 * Function name: query_hitloc
 * Description:   Give the hitloc for a certain id
 * Arguments:     id: The id to return hitloc array for
 * Returns:       Array with elements as described in add_hitloc
 */
public nomask mixed *
query_hitloc(int id) 
{
    int pos;

    if ((pos = member_array(id, hit_id)) >= 0)
    {
        return hitloc_ac[pos];
    }

    return 0;
}
