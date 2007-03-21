/*
 * /sys/formulas.h
 *
 * This file holds all system game formulas, like that of combat.
 */

#ifndef WA_TYPES_DEF
#include "/sys/wa_types.h"
#endif

#ifndef SS_TYPES_DEF
#include "/sys/ss_types.h"
#endif

#ifndef PROP_DEF
#include "/sys/stdproperties.h"
#endif

#ifndef F_FORMULAS
#include "/config/sys/formulas2.h"
#endif

#ifndef F_FORMULAS
#define F_FORMULAS

/*
 * No need to use our own formulas.c as math.c seems logical enough to
 * use as well.
 */
#ifndef MATH_FILE
#define MATH_FILE "/sys/global/math"
#endif  MATH_FILE
 
/*
 * Stats
 *
 * F_EXP_TO_STAT - conversion from (acc) experience points to the stat.
 * F_STAT_TO_EXP - conversion from a stat to experience points.
 *
 * 0.27777777 ~= 1.0/3.6; the one point extra in F_STAT_TO_EXP is to account
 * for rounding errors.
 */
#define F_EXP_TO_STAT(xp)   (ftoi(pow((10.0 * itof(xp)), 0.27777777)))
#define F_STAT_TO_EXP(stat) (ftoi(pow(itof(stat), 3.6) / 10.0) + 1)

/*
 * Armour
 *
 * Note that these are only valid for humanoid armours.
 */
#define ARMOUR_FILE 				"/std/armour"
#define F_VALUE_ARMOUR(ac) 			(20 + (ac) * ((ac) + 21) / 3)
#define F_ARMOUR_DEFAULT_AT 			A_TORSO
#define F_LEGAL_AM(list) 			(sizeof(list) == 3)
#define F_ARMOUR_CLASS_PROC(proc_of_max) 	(proc_of_max)
#define F_LEGAL_ARMOUR_REPAIR(rep, cond)	((rep) <= (cond)  && \
							(rep) < (cond) / 2 + 3)
#define F_ARMOUR_REPAIR_COST_FACTOR             (10)
#define F_ARMOUR_VALUE_REDUCE(m_cond)		(max((100 - (m_cond) * 5), 10))
#define F_ARMOUR_BREAK(m_cond, likely)		((m_cond)>(20 - (likely) / 2 ) \
		|| (m_cond) > random(40 - (likely)))

#define F_ARMOUR_CONDITION_WORSE(hits, ac, lik)	((hits) > random(500) + \
		4 * (40 - (lik)))
 /* random(500) was 1000 Mercade */

#define F_AT_WEIGHT_FACTOR(type) \
   (((type) == A_SHIELD) ? 20 : \
    (((type) & A_BODY) ? 40 : 0) + \
    (((type) & A_LEGS) ? 30 : 0) + \
    (((type) & A_HEAD) ? 10 : 0) + \
    (((type) & A_R_FOOT) ? 10 : 0) + \
    (((type) & A_L_FOOT) ? 10 : 0) + \
    (((type) & A_R_ARM) ? 10 : 0) + \
    (((type) & A_L_ARM) ? 10 : 0) + \
    (((type) & A_R_HAND) ? 7 : 0) + \
    (((type) & A_L_HAND) ? 7 : 0) + \
    (((type) & A_ROBE) ? 20 : 0))

#define F_WEIGHT_DEFAULT_ARMOUR(ac, at) \
    (F_AT_WEIGHT_FACTOR(at) * (428 * (((ac) > 1) ? (ac) - 1 : 1) + \
     (((ac) > 14) ? 10000 : 0)) / 100)

#define F_WEIGHT_FAULT_ARMOUR(w, ac, at)\
    (F_WEIGHT_DEFAULT_ARMOUR(ac, at) * 800 / 1000 > (w))

/* 
 * Weapon
 *
 * Note also that these are only valid for humanoid weapons.
 */
#define WEAPON_FILE 				"/std/weapon"
#define F_VALUE_WEAPON(wch, wcp) 		(20 + (wch * wcp))
#define F_WEAPON_DEFAULT_WT 			W_FIRST
#define F_WEAPON_DEFAULT_DT 			W_IMPALE
#define F_WEAPON_DEFAULT_HANDS			W_ANYH
#define F_WEAPON_CLASS_PROC(proc_of_max)        (proc_of_max)
#define F_WEAPON_VALUE_REDUCE(du, co)	   (max((100 - (du)* 3 - (co)* 6), 10))
#define F_LEGAL_WEAPON_REPAIR_DULL(rep, dull)	((rep) <= (dull) && \
							(rep)< 2*(dull)/ 3 + 3)
#define F_LEGAL_WEAPON_REPAIR_CORR(rep, corr)	((rep) <= (corr) && \
							(rep) < (corr) / 2 + 1)
#define F_WEAPON_REPAIR_COST_FACTOR             (10)
#define F_WEAPON_BREAK(dull, corr, likely)	((dull) > (20 - (likely)) || \
		(corr) > (5 -(likely)/ 4) || (dull) > random(40 -(likely)) || \
		(corr) > random(10 - (likely) / 4))
#define F_WEAPON_CONDITION_DULL(hits, pen, lik)	((hits) > random(500) + \
		10 * (30 - (lik)))
#define F_WEIGHT_DEFAULT_WEAPON(wpen, wtype) \
	(W_WEIGHTS_OFFSET[wtype] + W_WEIGHTS_FACTOR[wtype] * (wpen))
#define F_WEIGHT_FAULT_WEAPON(weight, wpen, wtype) \
	((weight) < (F_WEIGHT_DEFAULT_WEAPON(wpen, wtype) * 4 / 5))

#define F_LEGAL_DT(type) ((type) & (W_IMPALE | W_SLASH | W_BLUDGEON))

#define F_LEGAL_HANDS(which) ((which) == W_ANYH ||  \
			      (which) == W_LEFT ||  \
			      (which) == W_RIGHT || \
			      (which) == W_BOTH)

#define F_LEGAL_WCHIT(wc, type)      (F_LEGAL_TYPE(type)       && \
			       ((wc) <= W_MAX_HIT[type]))
#define F_LEGAL_WCPEN(wc, type)      (F_LEGAL_TYPE(type)       && \
			       ((wc) <= W_MAX_PEN[type]))

#define F_DEFAULT_CLONE_UNIQUE_CHANCE 33

/*
 * Bows.
 */
#define F_LAUNCH_W_FATIGUE_TIME(x) (20.0 + itof((x)->query_stat(SS_STR)))
#define F_LAUNCH_W_DAM_FACTOR      100
#define F_BOWSTRING_SNAP_CHANCE     20
#define F_PROJECTILE_BREAK_CHANCE   30

/*
 * Herbalism.
 */
#define F_EXP_HERBSEARCH(difficulty) \
    (250 + (25 * (((difficulty) > 10) ? 10 : (difficulty))))
#define F_HERB_INTERVAL             60

/*
 * Thievery.
 */
#define F_AWARENESS_BONUS                    5
#define F_BACKSTAB_HIT(bs, dex, aware, def) \
        (random( 80 * (bs) + 20 * (dex) - 40 * ((aware) + (def)) ) \
        - random(160) )
#define F_BACKSTAB_PEN(bs, knife, wp, str) \
        F_PENMOD((wp) * (1 + ((bs) + (str)) ) / 10, (knife))
#define F_STEAL_EXP(vic_val) (((vic_val) / 4) > 1048 ? 1048 : ((vic_val) / 4))
#define F_BACKSTAB_FATIGUE           (10)
#define F_STEAL_MIN_SKILL            (20)
#define F_BACKSTAB_MIN_SKILL         (20)

/* 
 * Living
 */
#define F_KILL_NEUTRAL_ALIGNMENT        (10)
#define F_MAX_ABS_ALIGNMENT		(1200)
#define F_KILL_ADJUST_ALIGN(k_al, v_al) \
    (int)call_other(MATH_FILE, "delta_align_on_kill", (k_al), (v_al))
#define F_QUEST_ADJUST_ALIGN(my_align, quest_align) \
    (F_KILL_ADJUST_ALIGN((my_align), -(quest_align)))
#define F_PANIC_WIMP_LEVEL(dis)		(10 + 3 * (dis))
#define F_PANIC_DEPR_PROC		(4)
#define F_PANIC_DEPR_CONST		(1)

#define F_MAX_SCAR			(10)
#define F_SCAR_DESCS ({   "left leg", "right leg", "nose", "left arm", \
			  "right arm", "left hand", "right hand",      \
			  "forehead", "left cheek", "right cheek"      \
		     })
/*
 * The following constants define how quickly a living heals.
 */
#define F_INTERVAL_BETWEEN_HP_HEALING		20  /*(in sec)*/
#define F_INTERVAL_BETWEEN_MANA_HEALING		30  /*(in sec)*/
#define F_INTERVAL_BETWEEN_FATIGUE_HEALING	60  /*(in sec)*/
#define F_INTERVAL_BETWEEN_STUFFED_HEALING	20  /*(in sec)*/
#define F_INTERVAL_BETWEEN_SOAKED_HEALING	20  /*(in sec)*/
#define F_INTERVAL_BETWEEN_INTOX_HEALING	20  /*(in sec)*/
#define F_INTERVAL_BETWEEN_HEADACHE_HEALING	20  /*(in sec)*/
#define F_INTERVAL_BETWEEN_PANIC_HEALING	20  /*(in sec)*/

#define MAX_MANA_UPDATE				4

/* Amount to heal per interval for various stats */
#define F_HEADACHE_RATE                 1
#define F_SOBER_RATE                    1
#define F_MANA_HEAL_RATE                1
#define F_UNSTUFF_RATE                  1
#define F_UNSOAK_RATE                   16
#define F_HEAL_FORMULA(con, intox) (((con) * 5 + (intox) * 2 + 100) / 20)
#define F_FATIGUE_FORMULA(stuffed, max) (5 + (stuffed) * 45 / (max))

/* Formula to heal mana with respect to spellcasting, int and intox */
/* Old formula, pre 2006-05-23
#define F_MANA_HEAL_FORMULA(sc,pintox,intel) \
    (((sc) < 31) ? 2 : ((((((sc) - 30) * MAX_MANA_UPDATE * (intel)) / \
	1000 + 5) * (100 - (pintox)) / 1000) + 2))
*/
#define F_MANA_HEAL_FORMULA(sc, pintox, wis) \
    (1 + ((wis / 10) * (100 - (pintox) / 2)) * ((sc) > 30 ? (sc) : 30) \
     / 10000)

/*
 * How long can a temporary stat addition be? (In heartbeats)
 */
#define F_TMP_STAT_MAX_TIME 30

#define F_TRACK_MANA_COST	4

/*
 * F_MAX_REMEMBERED(int, wis) returns the maximum number of players a
 * person can remember. It is based on both intelligence and wisdom.
 */
#define F_MAX_REMEMBERED(int, wis) ((((int) + (wis)) / 3) + 10)

/*
 * These macros convert from seconds to heart beats and back. We still
 * need this since query_age() is in heartbeats.
 */
#define F_SECONDS_PER_BEAT 		2
// #define F_NUM_BEATS(seconds) 	((seconds) / F_SECONDS_PER_BEAT)

/*
 * Recovery limit. How long can you keep your items if you have been away
 * from the realms. 1209600 = 2 weeks.
 */
#define F_RECOVERY_LIMIT	(1209600)

/* 
 * Death
 */
#define F_DEATH_MESSAGE      "\nYou die.\n" + 				     \
			     "You have a strange feeling.\n" +               \
                             "You can see your own dead body from above.\n\n"
#define F_GHOST_MSGIN 	     "drifts around."
#define F_GHOST_MSGOUT 	     "blows"
#define F_NAME_OF_GHOST	     "some mist"

#define F_DIE_REDUCE_XP(xp) 		((xp) / 4)
#define F_DIE_KEEP_XP(xp)		((xp) - (F_DIE_REDUCE_XP(xp)))
#define F_DIE_START_HP(max_hp) 		((max_hp) / 10)
#define F_DEATH_MIN_RELATIVE_BRUTE	(0.3)
#define F_DEATH_RELATIVE_BRUTE_RANGE	(1.0 - F_DEATH_MIN_RELATIVE_BRUTE)
#define F_DEATH_MAX_EXP_PLATFORM(m)	(((m) * 9) / 10)
#define F_DEATH_MIN_EXP_PLATFORM(m)	(((m) * 6) / 10)

#define F_EXP_ON_KILL(k_av, v_av) \
    (int)call_other(MATH_FILE, "exp_on_kill", (k_av), (v_av))
#define F_KILL_GIVE_EXP(av)	        (((av) * (av) * 400) / ((av) + 50))
#define F_EXP_TEAM_BONUS(size)          (100 + ((size) * 10))

/*
 * Combat 
 */

#define F_MAX_HP(con)  (((con) < 10) ? ((con) * 10) : (((con) * 20) - 100))

#define F_PENMOD(pen, skill) ((((pen) > (skill) ? (skill) : (pen)) + 50) * \
	(((skill) > (pen) ? (pen) + ((skill) - (pen)) / 2 : (skill)) + 50) / \
	30 - 80)

#define F_AC_MOD(ac) (((ac) + 50) * ((ac) + 50) / 50 - 50)

#define F_DAMAGE(pen, dam) ((pen) - (dam))

#define F_DARE_ATTACK(ob1, ob2) \
	((ob1)->query_prop(NPC_I_NO_FEAR) || \
	 ((ob2)->query_average_stat() <= ((ob1)->query_stat(SS_DIS) * 2)))

#define F_UNARMED_HIT(skill, dex)    ((skill) / 7 + (dex) / 20)
#define F_UNARMED_PEN(skill, str)    ((skill) / 10 + (str) / 20)
#define F_UNARMED_DEFAULT	     (40)

#define F_RELAX_TIME_AFTER_COMBAT(tme)  ((tme) + 60 + (6 * ((tme) % 10)))

/*
 * Healing alco
 */
#define F_VALUE_ALCO(alco)		(10 + ((alco) * (alco) / 10))

/*
 * Magic 
 */
#define F_VALUE_MAGICOB_HEAL(hp)	(5 * (hp) + (hp) * (hp) / 4)
#define F_VALUE_MAGIC_COMP(hp)		((hp) * 20)

/*
 * Some general values
 *
 */
/*
 * wg in grams (weight it can support), l in centimeters (length of rope)
 */
#define F_VALUE_ROPE(wg, l)		(((wg) / 10000) * ((l) / 100))

/*
 * Some string defines that are only used indirectly.
 *
 * All these kinds of string constants are defined in a /sys/file_desc.h
 * Where 'file' is the original filename. The constants below are
 * referenced by default from those files for backwards compatibilty
 * reasons.
 */
#define F_ALIVE_MSGIN                   "arrives"
#define F_ALIVE_MSGOUT 			"leaves"
#define F_ALIVE_TELEIN 			"arrives in a puff of smoke."
#define F_ALIVE_TELEOUT 		"disappears in a puff of smoke."

/* No definitions beyond this line. */
#endif F_FORMULAS
