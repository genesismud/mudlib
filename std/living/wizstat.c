/* 
 * /std/living/wizstat.c
 *
 * Contains the code to implement the various wizardly information commands.
 */

#include <filepath.h>
#include <formulas.h>

public int
base_stat(int acc_exp)
{
    return F_EXP_TO_STAT(acc_exp);
}

/*
 * Function name: stat_living
 * Description  : Give status information on this living.
 * Returns      : string - the description.
 */
public string
stat_living()
{
    string str, tmp;
    object to;
    int *stats = ({ SS_STR, SS_DEX, SS_CON, SS_INT, SS_WIS, SS_DIS,
                    SS_RACE, SS_GUILD, SS_OTHER });

    to = this_object();
    str = sprintf(
		  "Name: %-11s Rank: %-10s " +
#ifdef USE_WIZ_LEVELS
                  "(%-2d) " +
#endif
		  "Race: %-10s Gender: %-10s\n" +
	          "File: %-31s Uid: %-11s  Euid: %-11s\n"  +
		  "------------------------------------------------------" +
		  "--------------------\n" +
		  "Exp: %-8d  Quest: %-7d  Combat: %-7d General: %-7d " +
		  "Av.Stat: %d\n" +
		  "Hp: %4d(%4d) Mana: %4d(%4d)\n" +  
		  "Fatigue: %5d(%5d)  Weight: %7d(%7d)   Volume: %7d(%7d)\n\n" +
		  "Stat: %@8s\n"  +
                  "Value:%@8d\n" +
                  "Base: %@8d\n" +
                  "Exp:  %@8d\n" +
	          "Learn:%@8d\n\n" +
		  "Intox: %4d  Stuffed: %3d Soaked: %3d  Align : %d\n" +
		  "Scar : %4d  Ghost  : %3d Invis : %3d  Npc   : %3d  " +
		  "Whimpy: %3d\n",
		  capitalize(query_real_name()),
		  WIZ_RANK_NAME(SECURITY->query_wiz_rank(query_real_name())),
#ifdef USE_WIZ_LEVELS
		  SECURITY->query_wiz_level(query_real_name()),
#endif
		  to->query_race_name(),
		  to->query_gender_string(),
		  extract(RPATH(file_name(this_object())), 0, 30),
		  getuid(this_object()),
		  geteuid(this_object()),
		  to->query_exp(),
		  to->query_exp_quest(),
		  to->query_exp_combat(),
		  to->query_exp_general(),
		  to->query_average_stat(),
		  to->query_hp(),
		  to->query_max_hp(),
		  to->query_mana(),
		  to->query_max_mana(),
		  to->query_fatigue(),
		  to->query_max_fatigue(),
		  to->query_prop(OBJ_I_WEIGHT),
		  to->query_prop(CONT_I_MAX_WEIGHT),
		  to->query_prop(OBJ_I_VOLUME),
		  to->query_prop(CONT_I_MAX_VOLUME),
		  SS_STAT_DESC,
                  map(stats, &to->query_stat()),
                  map(map(stats, &to->query_acc_exp()), base_stat),
                  map(stats, &to->query_acc_exp()),
		  to->query_learn_pref(-1),
		  to->query_intoxicated(),
		  to->query_stuffed(),
		  to->query_soaked(),
		  to->query_alignment(),
		  to->query_scar(),
		  to->query_ghost(),
		  to->query_invis(),
		  to->query_npc(),
		  to->query_whimpy());

    if (strlen(tmp = to->query_prop(OBJ_S_WIZINFO)))
	str += "Wizinfo:\n" + tmp;

    return str;
}

/*
 * Function name: fix_skill_desc
 * Description  : This function will compose the string describing the
 *                individual skills the player has.
 * Arguments    : int sk_type     - the skill number.
 *                mapping sk_desc - the mapping describing the skills.
 * Returns      : string - the description for this skill.
 */
nomask static string
fix_skill_desc(int sk_type, mapping sk_desc)
{
    string desc;

    if (pointerp(sk_desc[sk_type]))
    {
        desc = sk_desc[sk_type][0];
    }
    else
    {
        if (!(desc = this_object()->query_skill_name(sk_type)))
	{
	    desc = "special";
	}
    }

    return sprintf("%s: %3d (%6d)", extract(desc, 0, 23),
		   this_object()->query_skill(sk_type), sk_type);
}

/*
 * Function name: skill_living
 * Description  : This function returns a proper string describing the
 *                skills of this living.
 * Returns      : string - the description.
 */
public string
skill_living()
{
    string *skills;
    string sk;
    int *sk_types;
    int index;
    int size;
    mapping sk_desc;

    sk_types = sort_array(query_all_skill_types());

    if (!sizeof(sk_types))
    {
	return capitalize(query_real_name()) + " has no skills.\n";
    }

    sk_desc = SS_SKILL_DESC;
    sk = "";
    skills = map(sk_types, &fix_skill_desc(, sk_desc));
    size = ((sizeof(skills) + 1) / 2);
    skills += ({ "" });
    index = -1;
    while(++index < size)
    {
	sk += sprintf("%38s %38s\n", skills[index], skills[index + size]);
    }
    
    return "Skills of " + capitalize(query_real_name()) + ":\n" + sk;
}
