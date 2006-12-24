/*
 * player/death_sec.c
 *
 * This is a subpart of player_sec.c
 *
 * Handling of what happens when the player dies and resurrects.
 * Scars all also handled here.
 *
 * This file is included into player_sec.c
 */

#include <formulas.h>
#include <ss_types.h>
#include <std.h>

/*
 * Prototypes
 */
void make_scar();
void modify_on_death();
int query_average();

/*
 * Function name:   second_life
 * Description:     Handles all that should happen to a dying player.
 * Arguments        object killer - the object that killed us.
 * Returns:         True if the living object should get a second life
 */
public int
second_life(object killer)
{
    if (query_wiz_level())
    {
        return 0;
    }

    if (query_prop(TEMP_BACKUP_BRIEF_OPTION))
    {
        set_option(OPT_BRIEF, 0);
        remove_prop(TEMP_BACKUP_BRIEF_OPTION);
    }

    catch(SECURITY->store_predeath());

    make_scar();
    modify_on_death();

    set_m_in(F_GHOST_MSGIN);
    set_m_out(F_GHOST_MSGOUT);

    set_intoxicated(0);
    set_stuffed(0);
    set_soaked(0);
    set_mana(query_max_mana());

    stop_fight(query_enemy(-1)); /* Forget all enemies */

    tell_object(this_object(), F_DEATH_MESSAGE);

    this_object()->death_sequence();
    save_me(0); /* Save the death badge if player goes linkdead. */

    return 1;
}

/*
 * Function name:   modify_on_death
 * Description:     Modifies some values (e.g. exp, stats and hp) when a
 *                  player has died.
 */
static void 
modify_on_death()
{
    int index = (SS_STR - 1);
    int reduce_combat, reduce_general;
    float stat_factor;

    /* Remember how much experience we had. */
    update_max_exp();

    /* Note that only combat and general experience are affected by death.
     * Every stat is adjusted for the ratio between death' taking and the
     * total experience. This means that on a higher stat you will loose
     * more points. */
    reduce_combat = F_DIE_REDUCE_XP(query_exp_combat());
    reduce_general = F_DIE_REDUCE_XP(query_exp_general());
    stat_factor = 1.0 - (itof(reduce_combat + reduce_general) / itof(query_exp()));

    while(++index < SS_NO_EXP_STATS)
    {
        set_acc_exp(index, ftoi(itof(query_acc_exp(index)) * stat_factor));
    }

    /* Remove the death penalty from the combat and general experience. */
    set_exp_combat(query_exp_combat() - reduce_combat);
    set_exp_general(query_exp_general() - reduce_general);

    /* Recalculate the stats. */
    acc_exp_to_stats();

    /* We should reset our hitpoints to something above 0. */
    set_hp(F_DIE_START_HP(query_max_hp()));

    /* Update the progress indicator. */ 
    add_prop(PLAYER_I_LASTXP, query_exp());
    update_last_stats();
}

/*
 * Function name:   reincarnate
 * Description:     Manages the reincarnation of a player
 */
public void
reincarnate()
{
    set_player_file(LOGIN_NEW_PLAYER);
    LOGIN_NEW_PLAYER->reincarnate_me();
}

/*
 * Function name:   remove_ghost
 * Description:     This is normally not called except when a poor soul has
 *                  not been fetched by Death and reincarnated.
 * Arguments:       quiet : true if no messages should be printed
 * Returns:         0 if the player is not a ghost
 *                  1 otherwise.
 */
public int
remove_ghost(int quiet)
{
    if (!quiet && query_ghost())
    {
        tell_object(this_object(), "You have just been resurrected!\n");
        say(QCTNAME(this_object()) + " is suddenly resurrected!\n");
    }

    set_ghost(0);

    save_me(0);
    return 1;
}

/*
 * Function name:   make_scar
 * Description:     Add a random scar to a person.
 */
static void
make_scar()
{
    if (query_average() < 15)
        return;

    set_scar(query_scar() | (1 << random(F_MAX_SCAR)));
}
