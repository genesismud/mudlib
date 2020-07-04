/*
 * /lib/skill_raise.c
 *
 * This is generic routines for advancing in skills. It is used by inheriting
 * this file and configuring which skills that are teached. 
 *
 * Skill costs are calculated with the formula:
 *
 *         cost = (skilllev ^ 3 * skill_cost_factor) / 100
 */

#pragma strict_types
#pragma save_binary
#pragma strict_types

#include <files.h>
#include <language.h>
#include <macros.h>
#include <money.h>
#include <ss_types.h>
#include <state_desc.h>

static mapping sk_trains,     /* The available skills to train */
               sk_default,    /* The default basic skills */
               sk_tdesc;      /* The description printed */
static string *desc,          /* The main descriptions of skill levels. */
              *subdesc;       /* The subdescriptions of skill levels. */

/*
 * Function name: create_skill_raise
 * Description  : Initialize the package.
 */
public nomask void
create_skill_raise()
{
    sk_default = SS_SKILL_DESC;
    sk_trains = ([]);
    sk_tdesc = ([]);
    
    subdesc =
        ({
            "novice",
            "junior",
            "apprentice",
            "",
            "confident",
            "seasoned",
            "expert",
            "eminent",
            "brilliant", 
            "superior",
        });

    desc = 
        ({
            "student",
            "amateur",
            "layman",
            "acolyte",
            "journeyman",
            "craftsman",
            "professional",
            "veteran", 
            "master",
            "guru",
        });

}

/*
 * Function name: sk_query_main_levels
 * Description  : Give the array of main level descriptions of skill levels.
 * Returns      : string * - the array.
 */
public nomask string *
sk_query_main_levels()
{
    return desc + ({ });
}

/*
 * Function name: sk_query_sub_levels
 * Description  : Give the array of sublevel descriptions of skill levels.
 * Returns      : string * - the array.
 */
public nomask string *
sk_query_sub_levels()
{
    return subdesc + ({ });
}

/*
 * Function name: sk_add_train
 * Description:   Add a skill that can be trained.
 * Arguments:     skillnum: Skill number
 *                desc: Text written when raising the skill, ({ str1, str2 })
 *                              str1: what player sees
 *                              str2: what all other see
 *                      If a single textstring is given it will be used
 *                      for both what player sees and others see.
 *
 *                      Default messages will then be:
 *                        You improve your ability to <str1> ....
 *                        XXX improves <his> ability to <str2> ...
 *
 *                name: Name of skill
 *                costf: The costfactor for this skill in precentage
 *                maxskill: The maxlevel that we can teach to.
 *                stat: The limiting stat
 *                weight: The limit weight, max skill will be:
 *                              weight * stat / 100
 */
public varargs void
sk_add_train(mixed skillnum, mixed desc, string name, int costf, 
    int maxskill, int stat, int weight)
{
    int il;
    mixed skval;

    if (!sk_trains)
    {
        create_skill_raise();
    }

    if (!mappingp(sk_default))
    {
        return 0;
    }

    if (pointerp(skillnum))
    {
        for (il = 0; il < sizeof(skillnum); il++)
        {
            sk_add_train(skillnum[il], desc, name, costf, maxskill);
        }
        return;
    }

    if (!intp(skillnum))
    {
        return;
    }

    if (weight == 0)
    {
        stat = -1;
        weight = 100;
    }
    skval = sk_default[skillnum];
    
#ifdef STAT_LIMITED_SKILLS
    if (pointerp(skval))
    {
         sk_trains[skillnum] = ({ skval[0], skval[1], maxskill, 
            skval[2], skval[3] });
    }
    else
    {
        sk_trains[skillnum] = ({ name, costf, maxskill, stat, weight });
    }
#else
    if (pointerp(skval))
    {
        sk_trains[skillnum] = ({ skval[0], skval[1], maxskill });
    }
    else
    {
        sk_trains[skillnum] = ({ name, costf, maxskill});
    }
#endif STAT_LIMITED_SKILLS

    if (pointerp(desc))
    {
        if (sizeof(desc) > 1)
        {
            sk_tdesc[skillnum] = desc;
        }
        else
        {
            sk_tdesc[skillnum] = ({ desc[0], desc[0] });
        }
    }
    else
    {
        sk_tdesc[skillnum] = ({ desc, desc });
    }
}

/*
 * Function name: sk_do_train
 * Description:   Let a player train a skill a certain number of levels
 * Arguments:     int skillnum - Skill number
 *                object pl - Player to train
 *                int to_lev: To which level to train the player in the skill
 * Returns:       True if success
 */
public int
sk_do_train(int skillnum, object pl, int to_lev)
{
    if (!mappingp(sk_default) ||
        !living(pl))
    {
        return 0;
    }

    if (!sizeof(sk_trains[skillnum]))
    {
        return 0;
    }
 
    if ((to_lev > sk_trains[skillnum][2]) ||
        (to_lev < 0))
    {
        return 0;
    }

    pl->set_skill(skillnum, to_lev);
    return 1;
}

/*
 * Function name: sk_query_max
 * Description  : Give the max skill we can teach to for a skill.
 * Arguments    : int skillnum - the skill-number to check.
 *                int silent - don't tell anything to the player if true.
 * Returns      : int - the maximum you can train the skill to.
 */
public varargs int 
sk_query_max(int skillnum, int silent)
{
    mixed skval;

    if (!mappingp(sk_default))
    {
        return 0;
    }

    skval = sk_trains[skillnum];
    if (sizeof(skval) > 2)
    {
        return skval[2];
    }

    return 0;
}

/*
 * Function name: sk_cost
 * Description:   Give the cost for raising in a specific skill
 * Arguments:     skillnum: skill
 *                fr:   From level
 *                to:   To level
 * Returns:       Cost or 0 if it can not be tought or if fr == to.
 */
public int
sk_cost(int skillnum, int fr, int to)
{
    int cost, c_old, c_new;
    mixed skval;

    if (!mappingp(sk_default))
    {
        return 0;
    }

    skval = sk_trains[skillnum];
    if (!sizeof(skval) ||
        (fr == to))
    {
        return 0;
    }

#ifdef SKILL_DOUBLE_COST_FACTOR
    if ((fr < 90) && (to > 90))
    {
        return sk_cost(snum, fr, 90) + sk_cost(snum, 90, to);
    }
#endif
    c_old = (fr * fr * fr * skval[1]) / 100;
    c_new = (to * to * to * skval[1]) / 100;

#ifdef SKILL_DOUBLE_COST_FACTOR
    if (to > 90)
    {
         c_old *= 2;
         c_new *= 2;
    }
#endif

    cost = (c_new ? c_new : 1) - (c_old ? c_old : 1);

    /* Newbie Cost Protection..  If your Avg Stat is <= 20 and you are training
     * <= 10 steps, cut the cost in half.
     * This is to make life easier on beginning characters.
     */
    if ((this_player()->query_average_stat() <= 20) &&
        ((to - fr) <= 10))
    {
        cost /= 2;
    }

    /* Allow to untrain at 1 copper a step. */
    return (cost > 0 ? cost : 1);
}

/*
 * Function name: sk_rank
 * Description:   Give the textual level of a skill
 * Arguments:     lev: The skill level
 * Returns:       The skill rank descriptions
 */
public string
sk_rank(int lev)
{
    int subl, mainl;

    if (!desc)
    {
        create_skill_raise();
    }

    if (!lev || lev < 0)
    {
        return "without skill";
    }
    if (--lev > 99)
    {
        lev = 99;
    }
    mainl = (lev / sizeof(desc));
    if (mainl >= sizeof(desc))
    {
        mainl = sizeof(desc) - 1;
    }

    subl = (100 / sizeof(desc));
    subl = sizeof(subdesc) * (lev % subl) / subl;

    return (strlen(subdesc[subl]) ? subdesc[subl] + " " : "") + desc[mainl];
}       

/*
 * Function name: sk_query_train
 * Description:   Give a list of the skills we can train here.
 */
public int *
sk_query_train()
{
    return m_indexes(sk_trains);
}

/*
 * Function name: sk_query_name
 * Description  : Give the name of the skill for a skill number.
 * Arguments    : int skill - the skill number.
 * Returns      : string - the name.
 */
public string
sk_query_name(int skill)
{
    if (!mappingp(sk_default))
    {
        return 0;
    }

    if (sizeof(sk_trains[skill]))
    {
        return sk_trains[skill][0];
    }

    return 0;
}

/*
 * Function name: sk_find_skill
 * Description:   Find the number of a skill, when we have a string
 * Arguments:     skname: The skill name
 * Returns:       Skill number or -1 if not found
 */
public int
sk_find_skill(string skname)
{
    mixed *skarr, *skix;
    int il;

    if (!mappingp(sk_default))
    {
        return -1;
    }

    skix = m_indexes(sk_trains);
    for (il = 0; il < sizeof(skix); il++)
    {
        skarr = sk_trains[skix[il]];
        if (sizeof(skarr) && (skarr[0] == skname))
        {
            return skix[il];
        }
    }
    return -1;
}
    
/*
 * Function name: sk_hook_allow_train_skill
 * Description:   Checks to see if a specific person can learn a
 *                a specific skill ( to a specific level ).
 *                ( Default is:  yes )
 * Arguments:     object - The player trying to learn
 *                skill  - The skill trying to train
 *                level  - The level trying to learn to
 * Returns:       1 - yes (default) / 0 - no
 */
public varargs int
sk_hook_allow_train_skill(object who, string skill, int level)
{
    return 1;
}

/*
 * Function name: sk_fix_cost
 * Description:   Fix each line in the learn list
 * Arguments:     skillnum  - The skill to check
 *                steps - How many steps player wants to raise
 * Returns:       A formatted string
 */
varargs string
sk_fix_cost(int skillnum, int steps = 1)
{
    int this_level, next_level, maximum;
    string next_rank, cost;

    this_level = this_player()->query_base_skill(skillnum);
    maximum = sk_query_max(skillnum, 1);
    next_level = min(this_level + steps, maximum);

    if (this_level >= next_level)
    {
        cost = "---     ";
        next_rank = "---";
    }
    else
    {
        cost = MONEY_MCOL_TEXT(MONEY_SPLIT(sk_cost(skillnum, this_level, next_level)), 2, 1);
        next_rank = sk_rank(next_level);
    }

    if (!sk_hook_allow_train_skill(this_player(), sk_trains[skillnum][0], next_level))
    {
        return "";
    }

    return sprintf("%-16s %19s  %-20s %s\n", capitalize(sk_trains[skillnum][0]),
        cost, next_rank, sk_rank(maximum));
}

/*
 * Function name: sk_hook_unknown_skill
 * Description  : Player tries to learn an unknown skill.
 * Arguments    : string skill - The skill he sought for.
 *                string verb  - 'learn'
 * Returns      : int 0 - as always with notify_fail.
 */
int
sk_hook_unknown_skill(string skill, string verb)
{
    notify_fail("There is no skill named '" + skill + "' to learn.\n");
    return 0;
}

/*
 * Function name: sk_hook_cant_train
 * Description  : Player can't train that skill that high for some reason.
 * Arguments    : string skill - the skill the player tries to learn.
 *                int to_lev - the level to wich the player wanted training.
 * Returns      : int 0 - as always with notify_fail.
 */
int
sk_hook_cant_train(string skill, int to_lev)
{
    notify_fail("You fail to adopt what the guildmaster teaches you.\n");
    return 0;
}

/*
 * Function name: sk_hook_stat_limited
 * Description  : Player lacks the stat(s) to learn the skill.
 * Arguments    : int skillnum - the skill the player tries to learn.
 * Returns      : int 0 - as always with notify_fail.
 */
int
sk_hook_stat_limited(int skillnum)
{
    notify_fail("You need more " + SD_LONG_STAT_DESC[sk_trains[skillnum][3]] +
        " to improve your ability to " + sk_tdesc[skillnum][0] + ".\n");
    return 0;
}

/*
 * Function name: sk_hook_cant_pay
 * Description  : Player cannot pay for session. (Kill him?)
 * Arguments    : string skill  - the skill the player tries to learn.
 *                int to_lev - the level to wich the player wanted training.
 *                int cost - the price that is required, in coppers.
 * Returns      : int 0 - as always with notify_fail.
 */
int
sk_hook_cant_pay(string skill, int to_lev, int cost)
{
    notify_fail("You do not have enough money with you.\n");
    return 0;
}

/*
 * Function name: sk_hook_raise_rank
 * Description  : The player trains and pays, write something.
 * Arguments    : int skillnum  - the number of the skill trained.
 *                int to_lev - the level reached.
 *                int cost - the price paid, in coppers.
 */
void
sk_hook_raise_rank(int skillnum, int to_lev, int cost)
{
    string rank;

    rank = sk_rank(to_lev);

    this_player()->catch_msg("You improve your ability to " +
        sk_tdesc[skillnum][0] + " and achieve the rank of " + rank + ".\n");
    say(QCTNAME(this_player()) + " improves " +
        this_player()->query_possessive() + " ability to " +
        sk_tdesc[skillnum][1] + " and receives the" + " rank of " + rank + ".\n");

    ACHIEVEMENTS->trigger_event_skill_train(this_player(), skillnum, to_lev);
}

/*
 * Function name: sk_hook_write_header
 * Description  : Write the header to the learn list.
 * Arguments    : int steps - the number of steps to train.
 */
varargs void
sk_hook_write_header(int steps = 1)
{
    write("These are the skills you are able to learn here.\n");
    write("Skill:                 Cost:          Next level:          Maximum level:\n"+
          "---------------------------------------------------------------------------\n");
}

/*
 * Function name: sk_hook_skillisting
 * Description  : Display the header when someone is listing the skills
 *                to see which training he can receive.
 */
void
sk_hook_skillisting()
{
    write("Here follows all skills we teach, and your next level in " +
          "those skills:\n");
    write("Skill:                 Cost:          Next level:          Maximum level:\n"+
          "---------------------------------------------------------------------------\n");
}

/*
 * Function name: sk_hook_no_list_learn
 * Description  : This hook is called when there are no more skills the
 *                player can learn here.
 * Returns      : int 1 - as always after a command succeeds.
 */
int
sk_hook_no_list_learn()
{
    write("There is nothing for you to learn here. You have to seek " +
        "knowledge elsewhere.\n");
    return 1;
}

/*
 * Function name: sk_hook_improved_max
 * Description:   Player can't improve this skill any higer
 * Arguments:     skill - The skill player wanted to improve
 * Returns:       0
 */
int
sk_hook_improved_max(string skill)
{
    notify_fail("You cannot raise this skill any further here, you have " +
        "to seek knowledge elsewhere.\n");
    return 0;
}

/*
 * Function name: sk_hook_allow_train
 * Description:   Function called when player tries to do the learn command
 *                Will determine if a player can train skills here.
 *                ( Default is:  yes )
 * Arguments:     object - The player trying to learn
 *                string - The string from sk_improve
 *                verb   - improve or learn typically
 * Returns:       1 - yes (default) / 0 - no
 */
public varargs int
sk_hook_allow_train(object who, string str, string verb)
{
    return 1;
}

/*
 * Function name: sk_hook_not_allow_train
 * Description:   Message to print if you are not allowed to train
 * Returns:       string -- fail message
 */
public int
sk_hook_not_allow_train()
{
    notify_fail("You are not allowed to train here.\n");
    return 0;
}

/*
 * Function name: sk_hook_not_allow_train_skill
 * Description:   Message to print if you are not allowed to train
 *                a particular skill
 * Arguments:     skill  -- the skill we can't train (optional)
 * Returns:       int 1/0 (write/notify_fail)
 */
public varargs int
sk_hook_not_allow_train_skill(string skill)
{
    write("You are not allowed to train " + (skill ?
        "the skill "+ skill : "that skill" ) +" here.\n" );

    return 1;
}

/*
 * Function name: sk_train_sufficient_stat
 * Description  : Checks if the player has enough stat to learn the skill.
 * Arguments    : int skill - the skill
 *                int to_level - the new level.
 * Returns      : int 1/0 - if true, the player has enough stat.
 */
public int
sk_train_sufficient_stat(int skill, int to_lev)
{
    /* Always allow low levels to be taught. */
    if (to_lev <= STAT_LIM_MIN_SKILL)
    {
	return 1;
    }

#ifdef STAT_LIMITED_SKILLS
    if ((sk_trains[skill][3] >= 0) &&
        (sk_trains[skill][4] > 0) && 
        ((sk_trains[skill][4] * this_player()->query_stat(sk_trains[skill][3])) < (to_lev * 100)))
    {
        return 0;
    }
#endif STAT_LIMITED_SKILLS
    return 1;
}

/*
 * Function name: sk_filter_learn
 * Description  : Filter out what skills this player can learn
 * Arguments    : int sk    - The skill
 *                int steps - The number of steps play wants to learn
 * Returns      : int 1 if play can learn skill
 */
int
sk_filter_learn(int sk, int steps)
{
    return (this_player()->query_base_skill(sk) < sk_query_max(sk, 1));
}

/*
 * Function name: sk_list
 * Description:   Someone wants a list of skills
 * Arguments:     steps - How many steps the player wants to raise
 * Returns:       1
 */
int
sk_list(int steps)
{
    int *all_sk, *guild_sk, learn;

    all_sk = sk_query_train();
    if (!steps)
    {
        steps = 1;
    }
    else if (steps < 0)
    {
        sk_hook_skillisting();
        guild_sk = all_sk;
        steps = 1;
    }
    else
    {
        guild_sk = filter(all_sk, &sk_filter_learn(, steps));
	if (!sizeof(guild_sk))
	{
	    if (this_object()->sk_hook_no_list_improve())
	    {
	    	return 1;
	    }
	    else
	    {
            	return sk_hook_no_list_learn();
 	    }
	}
        sk_hook_write_header(steps);
    }

    foreach(int skill: guild_sk)
    {
        write(sk_fix_cost(skill, steps));
    }

    return 1;
}

/*
 * Function name: sk_improve
 * Description:   Function called when player tries to do the learn command
 * Arguments:     str - The rest of the command player made
 * Returns:       1/0
 */
int
sk_improve(string str)
{
    int steps = 1, *known_sk, skillnum, level, cost, money, maxlevel;
    string skill, verb, *tmp;

    if (!sk_hook_allow_train(this_player(), str, query_verb()))
    {
        return sk_hook_not_allow_train();
    }

    /* Simple way to let Max mean maxing out the skill. */
    if (str == "max")
    {
	str = MAX_SKILL_LEVEL + "";
    }

    if (!str || sscanf(str, "%d", steps))
    {
        return sk_list(steps);
    }

    tmp = explode(lower_case(str), " ");
    if (tmp[-1] == "max")
    {
        skill = implode(tmp[0..-2], " ");
        steps = 0;
        cost = -1;
    }
    else if (sscanf(tmp[-1], "%d", steps) == 1)
    {
        skill = implode(tmp[0..-2], " ");
    }
    else
    {
        skill = str;
    }

    if (skill == "all" || skill == "list")
    {
        return sk_list(-1);
    }

    known_sk = this_player()->query_all_skill_types();
    if (!known_sk)
    {
        known_sk = ({ });
    }

    verb = query_verb();
    if ((skillnum = sk_find_skill(skill)) < 0)
    {
        return sk_hook_unknown_skill(skill, verb);
    }

    level = this_player()->query_base_skill(skillnum);

    /* Player wants to improve to the maximum of his money. */
    if (cost == -1)
    {
        money = TOTAL_MONEY(this_player());
        maxlevel = sk_query_max(skillnum, 1);
        while((cost < money) && ((level + steps + 1) <= maxlevel))
        {
            steps++;
            cost = sk_cost(skillnum, level, level + steps + 1);
        }
    }

    if (!sk_hook_allow_train_skill(this_player(), skill, level + steps))
    {
        return sk_hook_not_allow_train_skill(skill);
    }
    if (level + steps > (maxlevel = sk_query_max(skillnum, 1)))
    {
        steps = maxlevel - level;
        if (steps <= 0)
        {
            return sk_hook_improved_max(skill);
        }
    }
#ifdef STAT_LIMITED_SKILLS
    if (!sk_train_sufficient_stat(skillnum, level + steps))
    {
	return sk_hook_stat_limited(skillnum);
    }
#endif STAT_LIMITED_SKILLS
    if (!sk_do_train(skillnum, this_player(), level + steps))
    {
        return sk_hook_cant_train(skill, level + steps);
    }

    cost = sk_cost(skillnum, level, level + steps);
    if (!MONEY_ADD(this_player(), -cost))
    {
        /* Set the skill back to what it was before. */
        this_player()->set_skill(skillnum, level);
        return sk_hook_cant_pay(skill, level + steps, cost);
    }

    sk_hook_raise_rank(skillnum, level + steps, cost);
    return 1;
}

/*
 * Function name: init_skill_raise
 * Description  : Call this to add standard skill raising commands. They now
 *                give exactly the same result.
 */
public void
init_skill_raise()
{
    add_action(sk_improve, "improve");
    add_action(sk_improve, "learn");
}
