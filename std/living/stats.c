/*
    /std/living/stats.c

    This is a subpart of living.c
    All stat and skill related routines are coded here.

    This file is included into living.c
 */

#include <files.h>
#include <macros.h>
#include <state_desc.h>
#include <ss_types.h>

static int	*delta_stat;  /* Temporary extra stats. */
static int      *stats;	      /* Stats the calculated values from acc_exp */
static int	*stat_extra;  /* Extra to add to the stats */

/*
 * Function name:   ss_reset
 * Description:     Reset stats and skills at start of character.
 */
static void
ss_reset()
{
    stats = allocate(SS_NO_STATS); 
    delta_stat = allocate(SS_NO_STATS);
    
    learn_pref = allocate(SS_NO_STATS); 
    acc_exp = allocate(SS_NO_STATS); 
    stat_extra = allocate(SS_NO_STATS);
}

/*
 * Function name:   query_skill_cost
 * Description:     Calculates the cost in experience points to raise a skill
 *                  a few levels.
 * Arguments:       oldskill: The old skill level
 *                  newskill: The new skill level
 * Returns:         The cost in experience points
 */
nomask public int
query_skill_cost(int oldskill, int newskill)
{
    return stat_to_exp(newskill) - stat_to_exp(oldskill);
}

/*
 * Function name:   set_stat_extra
 * Description:     Sets an extra number to add to the normal stats. It could be
 * 		    some object changing the livings stats while being held or
 *		    some other not time based stat bonus.
 * Arguments:       stat - What stat
 *		    val  - The extra value to modify stat with
 * Returns:	    The extra stat value
 */
public int
set_stat_extra(int stat, int val)
{
    if (stat < 0 || stat >= SS_NO_STATS)
	return 0;

    stat_extra[stat] = val;
    return val;
}

/*
 * Function name:   query_stat_extra
 * Description:     Query about the setting of the extra modifier of stats
 * Arguments:	    stat - What stat to ask about
 * Returns:	    The extra stat value
 */
public int
query_stat_extra(int stat)
{
    if (stat < 0 || stat >= SS_NO_STATS)
	return 0;

    return stat_extra[stat];
}

/*
 * Function name: set_base_stat
 * Description  : Sets the value of a specific stat to a new value. It is
 *                possible to give a deviation from the basic value to add
 *                randomness.
 * Arguments    : int stat - The stat-number as defined in <ss_types.h>.
 *                int value - the new value to set the stat to.
 *                int deviation - The deviation in percentage from the value
 *                    that you set which will be randomly applied per stat.
 *                    A stat value of 50 and a deviation of 10% leads to a
 *                    stat value in the range 45-55. Maximum deviation: 50%
 *                    Default: 0%
 * Returns      : int - the value of the stat, or 0 if failed.
 */
varargs int
set_base_stat(int stat, int value, int deviation = 0)
{
    int offset;

    if ((stat < 0) ||
        (stat >= SS_NO_STATS) ||
        (value < 1 ))
    {
        return 0;
    }

    if (deviation)
    {
        /* For value = 60, deviation = 10%, this does 60 - 6 + random(13) */
        deviation = ((deviation > 50) ? 50 : deviation);
        offset = ((value * deviation) / 50);
        value += random(offset + 1) - (offset / 2);
    }

    stats[stat] = value;
    return value;
}

/*
 * Function name:   query_base_stat
 * Description:     Gives the value of a specific stat
 * Arguments:       stat: The index in the array of stats
 * Returns:         The value of a stat, -1 on failure.
 */
public int
query_base_stat(int stat)
{
    if ((stat < 0) ||
        (stat >= SS_NO_STATS))
    {
	return -1;
    }

    return stats[stat];
}

/*
 * Function name:   query_average_stat
 * Description:     Calculate the avarage of all stats of a living
 * Returns:         The calculated avarage
 */
public int
query_average_stat()
{
    return (query_base_stat(SS_STR) + query_base_stat(SS_DEX) +
	query_base_stat(SS_CON) + query_base_stat(SS_INT) +
	query_base_stat(SS_WIS) + query_base_stat(SS_DIS)) / 6;
}

/*
 * Function name: query_relative_stat
 * Description  : Calculate the relative stat value (percentage) of a stat
 *                compared to the average stat. Default: 100
 * Arguments    : int stat - the stat to query.
 * Returns      : int - the relative stat value as percentage.
 */
public int
query_relative_stat(int stat)
{
    if ((stat < 0) ||
        (stat >= SS_NO_STATS))
    {
	return 0;
    }

    return (100 * query_base_stat(stat)) / query_average_stat();
}

/*
 * Function name: expire_tmp_stat()
 * Description  : Remove tmp_stat information as it times out.
 * Arguments    : int stat  - the stat that expires
 *                int value - the value to subtract.
 */
void
expire_tmp_stat(int stat, int value)
{
    delta_stat[stat] -= value;
}

/*
 * Function name: add_tmp_stat
 * Description:   add a temporary stat.
 * Arguments:     stat - which stat
 *                ds - change in stat
 *                dt - How many F_INTERVAL_BETWEEN_HP_HEALING intervals
 *                     to keep the change.
 * Returns:       1 - Ok, 0 - Change rejected.
 */
public int
add_tmp_stat(int stat, int ds, int dt)
{
    int tmp, i, start;
    int *end;

    tmp = query_stat(stat) - query_base_stat(stat);
    
    if ((ds + tmp > 10 + query_base_stat(stat) / 10) ||
        (dt <= 0))
    {
	return 0;
    }

    delta_stat[stat] += ds;

    dt = MIN(dt, F_TMP_STAT_MAX_TIME);
    set_alarm(itof(dt * F_INTERVAL_BETWEEN_HP_HEALING), 0.0,
        &expire_tmp_stat(stat, ds));

    return 1;
}

/*
 * Function name: query_stat
 * Description  : Get the compound value of a stat. Never less than 1.
 * Arguments    : int stat - Which stat to find.
 * Returns      : int - the stat value.
 */
public int
query_stat(int stat)
{
    int i, tmp;

    if ((stat < 0) ||
        (stat >= SS_NO_STATS))
    {
	return -1;
    }

    tmp = query_base_stat(stat);
    tmp += delta_stat[stat];
    tmp += stat_extra[stat];

    return (tmp > 0 ? tmp : 1);
}

/*
 * Function name: query_rel_stat
 * Description  : Measure a stat versus the average stat as percentage. A stat
 *                that is on average returns 100%
 * Arguments    : int stat - the stat to find.
 * Returns      : int - the percentage
 */
public int
query_rel_stat(int stat)
{
    return (100 * query_stat(stat)) / query_average_stat();
}

/*
 * Function name: exp_to_stat
 * Description  : Translates given number of exp to a stat/skill value.
 * Arguments    : int exp - the experience points to be translated.
 * Returns      : int     - the new skill/stat value
 */
nomask int
exp_to_stat(int exp)
{
    return F_EXP_TO_STAT(exp);
}

/*
 * Function name:   stat_to_exp
 * Description:     Translates given stat value to minimum number of
 *                  experience points required to get to that stat value.
 * Arguments:       exp: The number of experience points to be translated.
 * Returns:         The amount of experience
 */
nomask int
stat_to_exp(int stat)
{
    return F_STAT_TO_EXP(stat);
}

/*
 * Function name: stats_to_acc_exp
 * Description:   Translates the current base stats into acc_exp. All stats
 *                will be translated to quest experience only. This is used
 *                used only from default setup in player_sec::new_init()
 */
static void
stats_to_acc_exp()
{
    int il, sum, tmp;

    for (il = SS_STR, sum = 0; il < SS_NO_STATS; il++)
    {
	tmp = stat_to_exp(query_base_stat(il));
	if (tmp > 0)
	{
	    set_acc_exp(il, tmp);

            /* Only count the "real" stats in the total experience. */
	    if (il < SS_NO_EXP_STATS)
	    {
	        sum += tmp;
	    }
	}
	else
        {
	    set_acc_exp(il, 0);
        }
    }

    set_exp_quest(sum);
    set_exp_combat(0);
    set_exp_general(0);
}

/*
 * Function name: acc_exp_to_stats
 * Description:   Translates the current accumulated exp into stats.
 */
void
acc_exp_to_stats()
{
    int il, tmp;

    for (il = SS_STR; il < SS_NO_STATS; il++)
    {
	if (query_base_stat(il) >= 0)
	{
	    tmp = exp_to_stat(query_acc_exp(il));
	    set_base_stat(il, tmp);
	}
    }
}

/*
 * Function name: update_stat
 * Description:   Convert exp to stat for a single stat. This usually used
 *		  by a guild that wants its stat to behave like the normal.
 * Arguments:     stat - Which stat to update.
 */
public void
update_stat(int stat)
{
    set_base_stat(stat, exp_to_stat(query_acc_exp(stat)));
}

/*
 * Function name: update_last_stats
 * Description  : Copies the current stats into the PLAYER_AI_LAST_STATS
 *                property for later reference.
 */
public void
update_last_stats()
{
    int index;
    int *last_stats = allocate(SS_NO_EXP_STATS + 1);
    
    for (index = 0; index < SS_NO_EXP_STATS; index++)
    {
        last_stats[index] = query_base_stat(index);
    }
    last_stats[SS_NO_EXP_STATS] = query_average_stat();
    
    add_prop(PLAYER_AI_LAST_STATS, last_stats);
}

/*
 * Function name: check_last_stats
 * Description  : After experience has been added, check whether the stats
 *                of the player have changed. If so, inform the player of
 *                his stat increase.
 */
public void
check_last_stats()
{
    int index, changed = 0;
    int *last_stats = query_prop(PLAYER_AI_LAST_STATS);
    int *new_stats = allocate(SS_NO_EXP_STATS + 1);
    string olddesc, newdesc;
    
    if (sizeof(last_stats) != (SS_NO_EXP_STATS + 1))
    {
        update_last_stats();
        return;
    }
    
    for (index = 0; index < SS_NO_EXP_STATS; index++)
    {
        new_stats[index] = query_base_stat(index);
        if (new_stats[index] != last_stats[index])
        {
            changed = 1;
            if (new_stats[index] >= SD_STATLEVEL_SUP)
            {
                if (last_stats[index] < SD_STATLEVEL_SUP)
                {
                    tell_object(this_object(), "You have reached supreme " +
                        SD_LONG_STAT_DESC[index] + ".\n");
                }
            }
            else if (new_stats[index] >= SD_STATLEVEL_IMM)
            {
                if (last_stats[index] < SD_STATLEVEL_IMM)
                {
                    tell_object(this_object(), "You have reached the " +
                        SD_LONG_STAT_DESC[index] + " of an immortal.\n");
                }
            }
            if (new_stats[index] >= SD_STATLEVEL_EPIC)
            {
                if (last_stats[index] < SD_STATLEVEL_EPIC)
                {
                    tell_object(this_object(), "You have reached epic " +
                        SD_LONG_STAT_DESC[index] + ".\n");
                }
            }
            else
            {
                olddesc = GET_STAT_LEVEL_DESC(index, last_stats[index]);
                newdesc = GET_STAT_LEVEL_DESC(index, new_stats[index]);
                if (olddesc != newdesc)
                {
                    tell_object(this_object(), "Your " +
                        SD_LONG_STAT_DESC[index] + " increases from " +
                        olddesc + " to " + newdesc + ".\n");
                }
            }
        }
    }
  
    index = SS_NO_EXP_STATS;
    new_stats[index] = query_average_stat();
    if (new_stats[index] != last_stats[index])
    {
        changed = 1;
        olddesc = GET_EXP_LEVEL_DESC(last_stats[index]);
        newdesc = GET_EXP_LEVEL_DESC(new_stats[index]);
        if (olddesc != newdesc)
        {
            tell_object(this_object(), "Congratulations. Your are now " +
                "sufficiently experienced to call yourself " + newdesc + ".\n");
        }
    }
    
    if (changed)
    {
        add_prop(PLAYER_AI_LAST_STATS, new_stats);
    }
}

/*
 * Function name: update_acc_exp
 * Description  : After experience had been added to the total, this function
 *                spreads it over the acc_exp for each stat. An increase in
 *                experience is spreaded according to the learn preferences,
 *                a reduction of experience is done only in the real stats.
 * Arguments    : int exp     - the experience added or removed.
 *                int taxfree - 1/0 - If true, then the player receives 100%
 *                    of the experience in his normal stats, as well as the
 *                    due tax in the guild stats.
 */
varargs static void
update_acc_exp(int exp, int taxfree)
{
    int   index;
    int   total;
    float factor;

    /* Negative experience. Adjust only the 'real' stats. */
    if (exp < 0)
    {
        /* Reduce all stats relative to their weight in the total experience.
         * Since exp < 0 the factor will be less than 1.0 (100%). We need to
         * divide by the old total experience, so subtract the negative delta
         * to add it to the new total.
         */
        factor = 1.0 + (itof(exp) / itof(query_exp() - exp));
        index = -1;
        while(++index < SS_NO_EXP_STATS)
        {
            set_acc_exp(index, ftoi(factor * itof(query_acc_exp(index))));
        }

        /* Recalculate the stats. */
        acc_exp_to_stats();
        return;
    }

    /* Calculate the new guild stats based on the tax. */
    index = SS_NO_EXP_STATS - 1;
    while(++index < SS_NO_STATS)
    {
        set_acc_exp(index, query_acc_exp(index) +
            ((query_learn_pref(index) * exp) / 100));
    }

    /* For tax tree experience, we divide all experience over the real stats.
     * To do this, we do not divide the learn pref by 100, but we split it
     * over the total learn prefs. This way, the total of experience added to
     * the real stats adds up to the total experience gathered.
     */
    total = (taxfree ? query_stat_pref_total() : 100);

    /* Update the acc_exp values for the real stats. */
    index = -1;
    while(++index < SS_NO_EXP_STATS)
    {
        set_acc_exp(index, query_acc_exp(index) +
            ((query_learn_pref(index) * exp) / total));
    }

    /* Recalculate the stats. */
    acc_exp_to_stats();
}

/*
 * Function name: check_acc_exp
 * Description  : This support function is called at login to make sure
 *                the accumulated experience of the player is equal to
 *                the experience in the stats. If not, those stats are
 *                updated.
 */
static void
check_acc_exp()
{
    int index = -1;
    int sum = query_exp();

    /* Make sure the array of of experience has the right size. */
    if (sizeof(acc_exp) < SS_NO_STATS)
    {
        acc_exp += allocate(SS_NO_STATS - sizeof(acc_exp));
    }

    while(++index < SS_NO_EXP_STATS)
    {
        sum -= query_acc_exp(index);
    }

    /* Don't bother about a difference smaller than 1000 points. */
    if (ABS(sum) < 1000)
    {
        return;
    }

    /* The stats don't match the total experience. Update the stats. */
    update_acc_exp(sum);
}

/*
 * Function name: set_guild_stat
 * Description  : This function allows the guildmaster to alter the
 *                experience a player has gathered in the guild. It can
 *                be set to any value.
 * Arguments    : int stat - the stat to set (guild stats only!)
 *                int exp  - the experience value to set the stat to.
 * Returns      : int      - 1/0 - set/unset.
 */
public nomask int
set_guild_stat(int stat, int exp)
{
    int change  = exp - query_acc_exp(stat);
    int stat_value;

    /* Not a guild stat... Naughty wizard.
     * or non-positive exp... Impossible.
     */
    if ((stat < SS_NO_EXP_STATS) ||
	(stat >= SS_NO_STATS) ||
	(exp < 1))
    {
	return 0;
    }

    stat_value = query_base_stat(stat);

    /* We set the new stat to to the guild stat, both in experience and
     * in stat value.
     */
    set_acc_exp(stat, exp);
    set_base_stat(stat, exp_to_stat(exp));

    if (interactive(this_object()))
    {
	SECURITY->log_syslog("CHANGE_STAT", ctime(time()) + " " +
	    capitalize(this_object()->query_real_name()) + " " +
	    SS_STAT_DESC[stat] + " " + query_base_stat(stat) +
	    " (was: " + stat_value + ")\n");
    }

    return 1;
}

/*
 * Function name: clear_guild_stat
 * Description  : This function allows guilds to clear the accumulated
 *                experience when a player leaves the guild. It is only
 *                a front for set_guild_stat.
 * Arguments    : int stat - the stat to clear (guild stat only!)
 * Returns      : int      - 1/0 - cleared/not cleared
 */
public nomask int
clear_guild_stat(int stat)
{
    return set_guild_stat(stat, 1);
}
