/*
 * /lib/herb_support.c
 *
 * Support for herbs that is used for potions as well.
 */

#pragma save_binary
#pragma strict_types

#include <files.h>
#include <macros.h>
#include <stdproperties.h>
#include <ss_types.h>
#include <herb.h>
#include <composite.h>
#include <language.h>

/*
 * Variables
 */
string poison_file;
mixed *effects;
mixed *poison_damage;

/* 
 * Function name: do_tmp_stat
 * Description:   This function is called from the effects if the herb is
 *                stat-affecting. One stat is randomly increased
 *                temporarily, lasting as long as strength * 10. If strength is
 *                negative, the stat is decreased instead.
 * Arguments:     stat     - The number of the stat
 *                strength - How strong the herb is - time effect will last
 */
void
do_tmp_stat(int stat, int strength)
{
    if (strength > 0)
    {
        this_player()->add_tmp_stat(stat, random(3) + 1,
            strength / 2 + random(strength));

        switch(stat)
        {
        case SS_STR:
            write("You feel strengthened.\n");
            break;
        case SS_DEX:
            write("You feel more dexterous.\n");
            break;
        case SS_CON:
            write("You feel healthier.\n");
            break;
        case SS_INT:
            write("You feel brighter.\n");
            break;
        case SS_WIS:
            write("You feel wiser.\n");
            break;
        case SS_DIS:
            write("You feel more secure.\n");
            break;
        default:
            write("You feel more clever.\n");
        }
    }

    if (strength < 0)
    {
        this_player()->add_tmp_stat(stat, -(random(3) + 1), strength / 2 +
            random(strength));

        switch(stat)
        {
        case SS_STR:
            write("You feel weakened.\n");
            break;
        case SS_DEX:
            write("You feel slower.\n");
            break;
        case SS_CON:
            write("You feel less healthy.\n");
            break;
        case SS_INT:
            write("You feel stupider.\n");
            break;
        case SS_WIS:
            write("You feel less wise.\n");
            break;
        case SS_DIS:
            write("You feel more insecure.\n");
            break;
        default:
            write("You feel less clever.\n");
        }
    }
}

/*
 * Function name: add_resistance
 * Description:   This function is called from the herb-effects, and adds some
 *                resistance in the player. Max strength is 40.
 *                The resistance added is an additive resistance.
 *                (See /doc/man/general/spells for more info on resistance)
 * Arguments:     res_type - The resistance type
 *                strength - How strong the herb is
 */
void
add_resistance(mixed res_type, int strength)
{
    object res_obj;
    res_obj = clone_object(RESISTANCE_OBJECT);
    if (strength > 40)
    {
        strength = 40;
    }
    res_obj->set_strength(strength);
    res_obj->set_time(5 * (40 - strength / 2) + 5 * random(strength));
    res_obj->set_res_type(res_type);
    res_obj->move(this_player());
    write("You feel more resistant.\n");
}

/*
 * Function name: special_effect
 * Description:   Redefine this when you have done set_effect(HERB_SPECIAL);
 *                to do the effect of your herb.
 */
void
special_effect()
{
    write("You don't feel any effect.\n");
}

/* 
 * Function: do_herb_effects
 * Description: In this function the effect(s) of the herb are resolved.
 *              To define a standard effect, do 
 *              set_effect(herb_type, str, strength); in create_herb(),
 *              where herb_type is one of the herb-types in /sys/herb_types.h,
 *              str is a string for the affect type, and strength is an
 *              integer for the strength of the effect.
 *              Read /doc/man/general/herbs for more information.
 *              One effect per herb should be the norm, but adding one or
 *              two is ok, as long as they don't make the herb too good.
 */
nomask int
do_herb_effects()
{
    int strength, res, i, n, a;
    string type;
    object poison, tp, to, *inv;

    seteuid(getuid());
    tp = this_player();
    to = this_object();
    i = 0;
    while (i < sizeof(effects))
    {
        switch(effects[i])
        {
        case HERB_HEALING:
            type = lower_case(effects[i + 1]);
            strength = effects[i + 2];
            res = 100 - tp->query_magic_res(MAGIC_I_RES_POISON);
            if (!type || type == "hp")
            {
                if (strength < 0) 
                { 
                    tp->reduce_hit_point(res * random(-strength) / 100);
                    if (tp->query_hp() <= 0)
                    {
                        write("You die from " + query_verb() + "ing " +
                            LANG_ADDART(this_object()->short()) + "!\n");
                        tp->do_die(to);
                        this_object()->remove_object();
                        break;
                    }
                    write("You feel less healthy.\n");
                }
                else if (strength > 0)
                {
                    tp->heal_hp(strength);
                    write("You feel healthier.\n");
                }
                else
                {
                    write("You don't feel any effect.\n");
                }
            }
            else if (type == "mana")
            {
                if (strength < 0)
                {
                    tp->set_mana(tp->query_mana() - res *
                            random(-strength) / 100);
                    write("You feel mentally weaker.\n");
                }
                else if (strength > 0)
                {
                    tp->set_mana(tp->query_mana() + strength);
                    write("You feel mentally healthier.\n");
                }
                else
                {
                    write("You don't feel any effect.\n");
                }
            }
            else if (type == "fatigue")
            {
                if (strength < 0)
                {
                    tp->set_fatigue(tp->query_fatigue() - res * 
                       random(-strength) / 100);
                    write("You feel more tired.\n");
                }
                else if (strength > 0)
                {
                    write("You feel less tired.\n");
                    tp->set_fatigue(tp->query_fatigue() + strength);
                }
                else
                {
                    write("You don't feel any effect.\n");
                }
            }
            else
            {
                write("You don't feel any effect.\n");
            }
            break;
        case HERB_ENHANCING:
            type = lower_case(effects[i + 1]);
            strength = effects[i + 2];
            if (!strength || ((strength < 0) && (res > random(100))))
            {
                write("You don't feel any effect.\n");
                break;
            }
            switch(type)
            {
            case "dex":
                do_tmp_stat(SS_DEX, strength);
                break;
            case "str":
                do_tmp_stat(SS_STR, strength);
                break;
            case "con":
                do_tmp_stat(SS_CON, strength);
                break;
            case "int":
                do_tmp_stat(SS_INT, strength);
                break;
            case "wis":
                do_tmp_stat(SS_WIS, strength);
                break;
            case "dis":
                do_tmp_stat(SS_DIS, strength);
                break;
            case "acid":
                add_resistance(MAGIC_I_RES_ACID, strength);
                break;
            case "cold":
                add_resistance(MAGIC_I_RES_COLD, strength);
                break;
            case "electr":
                add_resistance(MAGIC_I_RES_ELECTRICITY, strength);
                break;
            case "fire":
                add_resistance(MAGIC_I_RES_FIRE, strength);
                break;
            case "magic":
                add_resistance(MAGIC_I_RES_MAGIC, strength);
                break;
            case "poison":
                add_resistance(MAGIC_I_RES_POISON, strength);
                break;
            default:
                write("You don't feel any effect.\n");
                break;
            }
            break;
        case HERB_POISONING:
            type = lower_case(effects[i + 1]);
            strength = effects[i + 2];
            if (poison_file)
            {
                poison = clone_object(poison_file);
                if (!poison)
                {
                    write("You don't feel any effect.\n");
                    break;
                }
                if (strength)
                    poison->set_strength(strength);
                if (type)
                    poison->set_poison_type(type);
                if (poison_damage)
                    poison->set_damage(poison_damage);
                poison->move(tp, 1);
                poison->start_poison();
            }
            else 
            {
                poison = clone_object(POISON_OBJECT);
                poison->set_strength(strength);
                poison->set_poison_type(type);
                if (poison_damage)
                    poison->set_damage(poison_damage);
                poison->move(tp, 1);
                poison->start_poison();
            }
            break;
        case HERB_CURING:
            type = lower_case(effects[i + 1]);
            strength = effects[i + 2];
            inv = all_inventory(tp);
            n = -1;
            a = 0;
            while(++n < sizeof(inv))
            {
                if (function_exists("cure_poison", inv[n]) != POISON_OBJECT)
                {
                    continue;
                }
                if (inv[n]->cure_poison( ({ type }),
                    ((strength / 2) + random(strength)) ))
                {
                    a++;
                    strength /= 2;
                }
            }
            if (a <= 0)
            {
                write("You don't feel any effect.\n");
            }
            break;
        case HERB_SPECIAL:
            special_effect();
            break;
        default:
            write("You don't feel any effect.\n");
            break;
        }
        i += 3;
    }
    return 1;
}

/*
 * Function name: set_effect
 * Description:   Give the herb or potion an effect (see herb.h)
 * Arguments:     herb_type   - What type of effect
 *                affect_type - And what exactly do we affect?
 *                strength    - The strength
 */
void
set_effect(int herb_type, string affect_type, int strength)
{
    effects = ({ herb_type, affect_type, strength });
}

/*
 * Function name: add_effect
 * Description:   Adds one more effect to a herb or potion
 * Arguments:     herb_type   - What type of effect
 *                affect_type - And what exactly do we affect?
 *                strength    - The strength
 */
void
add_effect(int herb_type, string affect_type, int strength)
{
    effects += ({ herb_type, affect_type, strength });
}

/*
 * Function name: clear_effect
 * Description:   Remove all earlier set effects
 */
void
clear_effect()
{
    effects = ({});
}

/*
 * Function name: query_effect
 * Description:   Get the effect array
 * Returns:       The array
 */
mixed *
query_effect()
{
    return effects;
}

/*
 * Function name: set_poison_file
 * Description:   Set the file name of poison to use instead of standard
 * Arguments:     str - The file name
 */
void
set_poison_file(string str)
{
    poison_file = str;
}

/*
 * Function name: query_poison_file
 * Description:   Query the poison file (if any)
 * Returns:       The file name if set
 */
string
query_poison_file()
{
    return poison_file;
}

/*
 * Function name: set_poison_damage
 * Description:   Set the array to be sent to set_damage in the poison
 * Arguments:     damage - The damage array
 */
void
set_poison_damage(mixed *damage)
{
    poison_damage = damage;
}

/*
 * Function name: query_poison_damage
 * Description:   Query the poison damage array (if any)
 * Returns:       The damage array
 */
mixed *
query_poison_damage()
{
    return poison_damage;
}
