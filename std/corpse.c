/*
 * /std/corpse.c
 *
 * This is a decaying corpse. It is created automatically
 * when a player or monster die.
 */

#pragma save_binary
#pragma strict_types

inherit "/std/container";

#include <cmdparse.h>
#include <composite.h>
#include <files.h>
#include <language.h>
#include <macros.h>
#include <ss_types.h>
#include <stdproperties.h>
#include <wa_types.h>

/*
 * Prototypes
 */
void decay_fun();
void set_decay(int d);
string short_func();
string pshort_func();
string long_func();

#define DECAY_TIME	10
#define DECAY_LIMIT	 3
#define DECAY_UNIT	60.0

int             decay;
int             decay_id;
string          met_name, nonmet_name, state_desc, pstate_desc;
mixed		leftover_list;

#if 0
/*
 * Function name: create_corpse
 * Description  : Constructor. Define this routine to create your own corpse.
 */
void
create_corpse()
{
}
#endif

/*
 * Function name: create_container
 * Description  : Constructor. Don't mask this routine, but define
 *                create_corpse() instead.
 */
nomask void
create_container()
{
    add_prop(OBJ_I_SEARCH_TIME, 2);
    add_prop(OBJ_S_SEARCH_FUN, "search_for_leftovers");

    set_short(short_func);
    set_pshort(pshort_func);
    set_long(long_func);
    add_prop(CONT_I_WEIGHT, 50000);
    add_prop(CONT_I_VOLUME, 50000);
    state_desc = "corpse of ";
    pstate_desc = "corpses of ";

    this_object()->create_corpse();
    seteuid(getuid());

    /* If the decay time is not set yet, set the default decay time.
     * This automatically starts the decay alarm.
     */
    if (!decay)
    {
        set_decay(DECAY_TIME);
    }
}

#if 0
/*
 * Function name: reset_corpse
 * Description  : Reset routine. If you use this, also call enable_reset().
 */
void
reset_corpse()
{
}
#endif

/*
 * Function name: reset_container
 * Description  : Reset routine. Don't mask this routine, but define
 *                reset_corpse() instead.
 */
nomask void
reset_container()
{
    this_object()->reset_corpse();
}

/*
 * Function name: set_name
 * Description  : Called upon construction of this corpse to set the name
 *                of the diseased. It follows up with some additional names
 *                such as 'remains', etc.
 *                Do NOT call this routine more than once!
 *                Use add_name() for additional names.
 * Arguments    : string name - the name of the person who died.
 */
void
set_name(string name)
{
    met_name = lower_case(name);
    nonmet_name = previous_object()->query_nonmet_name();

    ::set_name("corpse");
    add_name("corpse of " + met_name);
    add_name("remains");
    add_name("remains of " + met_name);
    if (nonmet_name)
    {
	add_name("corpse of " + nonmet_name);
	add_name("remains of " + nonmet_name);
    }
}

/*
 * Function name: query_real_name
 * Description  : Returns the lower case name of this corpse.
 * Returns      : string - the lower case met name.
 */
string
query_real_name()
{
    return met_name;
}

/*
 * Function name: query_met_name
 * Description  : Returns the met-name of this corpse. (capitalized)
 * Returns      : string - the met name.
 */
string
query_met_name()
{
    return capitalize(met_name);
}

/*
 * Function name: query_nonmet_name
 * Description  : Returns the nonmet name of this corpse.
 * Returns      : string - the nonmet name.
 */
string
query_nonmet_name()
{
    return nonmet_name;
}

/*
 * Function name: set_state_desc
 * Description  : Change the state description of the corpse. Normally this
 *                is "corpse of " or "heap with decayed remains of ".
 * Arguments    : string desc - the new state description.
 */
void
set_state_desc(string desc)
{
    state_desc = desc;
}

/*
 * Function name: query_state_desc
 * Descritpion  : Returns the present state description.
 * Returns      : string - the state description.
 */
string
query_state_desc()
{
    return state_desc;
}

/*
 * Function name: set_pstate_desc
 * Description  : Change the plural state description of the corpse. Normally
 *                this is "corpses of " or "heaps with decayed remains of ".
 * Arguments    : string pdesc - the new plural state description.
 */
void
set_pstate_desc(string pdesc)
{
    pstate_desc = pdesc;
}

/*
 * Function name: query_pstate_desc
 * Descritpion  : Returns the present plural state description.
 * Returns      : string - the plural state description.
 */
string
query_pstate_desc()
{
    return pstate_desc;
}

/*
 * Function name: short_func
 * Description  : Returns the short description of this object, based on
 *                recognition of the corpse.
 * Returns      : string - the short description.
 */
string
short_func()
{
    object pob;

    pob = vbfc_caller();
    if (!pob || !query_interactive(pob) || pob == this_object())
        pob = previous_object(-1);
    if (!pob || !query_interactive(pob))
	pob = this_player();
    if (pob && pob->query_real_name() == lower_case(met_name))
	return state_desc + "yourself";
    else if (pob && pob->query_met(met_name))
	return state_desc + capitalize(met_name);
    else
	return state_desc + LANG_ADDART(nonmet_name);
}

/*
 * Function name: pshort_func
 * Description  : Returns the plural short description of this object, based on
 *                recognition of the corpse.
 * Returns      : string - the plural short description.
 */
string
pshort_func()
{
    object pob;

    pob = vbfc_caller();
    if (!pob || !query_interactive(pob) || pob == this_object())
        pob = previous_object(-1);
    if (!pob || !query_interactive(pob))
	pob = this_player();
    if (pob && pob->query_real_name() == lower_case(met_name))
	return pstate_desc + "yourself";
    else if (pob && pob->query_met(met_name))
	return pstate_desc + capitalize(met_name);
    else
	return pstate_desc + LANG_PWORD(nonmet_name);
}

/*
 * Function name: long_func
 * Description  : Returns the long description of this object, based on
 *                recognition of the corpse.
 * Returns      : string - the long description.
 */
string
long_func()
{
    object pob;

    pob = vbfc_caller();
    if (!pob || !query_interactive(pob) || pob == this_object())
	pob = this_player();
    if (pob->query_real_name() == lower_case(met_name))
	return "This is your own dead body.\n";
    if (pob->query_met(met_name))
	return "This is the dead body of " + capitalize(met_name) + ".\n";
    else
	return "This is the dead body of " + nonmet_name + ".\n";
}

/*
 * Function name: remove_object
 * Description  : When the corpse is destructed, move the inventory into its
 *                environment first.
 */
void
remove_object()
{
    /* If we are destructed, move our inventory out. */
    all_inventory(this_object())->move(environment(), 0);

    ::remove_object();
}

/*
 * Function name: move_out
 * Description  : Filter function to move all inventory out of the object.
 * Arguments    : object ob - the object to move out.
 * Returns      : int 1/0 - if true, the object was moved out.
 */
static int
move_out(object ob)
{
    return !ob->move(environment(this_object()));
}

/*
 * Function name: decay_remove
 * Description  : Second stage of decay. Clone the leftovers and then destruct
 *                the corpse.
 */
void
decay_remove()
{
    int i, j, flag;
    object obj;

    for (i = 0; i < sizeof(leftover_list); i++)
    {
	if (leftover_list[i][4])
	{
	    for (j = 0 ; j < leftover_list[i][2] ; j++)
	    {
		obj = clone_object(leftover_list[i][0]);
		obj->leftover_init(leftover_list[i][1], query_prop(CORPSE_S_RACE));
		obj->move(environment(this_object()), 0);
		flag = 1;
	    }
	}
    }

    if (flag)
    {
        tell_room(environment(this_object()),
            "The " + QSHORT(this_object()) +
            " rapidly decays, leaving some sad remains behind.\n");
    }

    remove_object();
}

/*
 * Function name: decay_fun
 * Description  : First stage of decay. Decay to a heap of remains, and move
 *                all inventory into the room. Then set the alarm for the
 *                second stage.
 */
void
decay_fun()
{
    object *ob;
    string desc;
    int i;

    ob = filter(all_inventory(this_object()), move_out);
    /* fix this to get singular/plural of 'appear' */
    i = ((sizeof(ob) != 1) ? sizeof(ob) :
         ((ob[0]->query_prop(HEAP_I_IS)) ? (int)ob[0]->num_heap() : 1));
    if (strlen(desc = COMPOSITE_DEAD(ob)))
    {
	tell_room(environment(this_object()),
	    "As the " + QSHORT(this_object()) + " rapidly decays, " +
            desc + " appear" +
	    ((i == 1 || desc == "something" || desc == "nothing") ? "s" : "") +
            " from it.\n");
    }
    state_desc = "heap with decayed remains of ";
    pstate_desc = "heaps with decayed remains of ";
    add_name("heap");

    /* Set the short decay limit so it will remove itself. */
    set_decay(DECAY_LIMIT);
}

/*
 * Function name: set_decay
 * Description  : Sets the decay time in minutes. Preferably, only call this
 *                routine from within create_corpse(), not externally. It will
 *                reset the alarm time to the new delay.
 * Arguments    : int d - the decay time in minutes.
 */
void
set_decay(int d)
{
    decay = d;

    if (decay_id)
    {
        remove_alarm(decay_id);
    }
    /* If we are too far away, do some decay first. */
    if (decay > DECAY_LIMIT)
    {
        decay_id = set_alarm((itof(decay - DECAY_LIMIT) * DECAY_UNIT), 0.0, decay_fun);
    }
    else
    {
        decay_id = set_alarm((itof(decay) * DECAY_UNIT), 0.0, decay_remove);
    }
}

/*
 * Function name: query_decay
 * Description  : Returns the (original) decay time in minutes.
 * Returns      : int - the decay time in minutes.
 */
int
query_decay()
{
    return decay;
}


/*
 * Function name: query_race
 * Description  : Find out the race of this corpse. It's in the property
 *                CORPSE_S_RACE as well.
 * Returns      : string - the race.
 */
string
query_race()
{
    return query_prop(CORPSE_S_RACE);
}

/*
 * Function name: appraise_object
 * Description  : Called to appraise the object using the command thereto.
 * Arguments    : int num - optional - if given, use this number. Otherwise
 *                    use the appraise object skill of the player.
 */
void
appraise_object(int num)
{
    int skill, value, volume, weight, seed;

    if (num)
        skill = num;
    else
        skill = this_player()->query_skill(SS_APPR_OBJ);

    volume = query_prop(CONT_I_VOLUME);
    weight = query_prop(CONT_I_WEIGHT);

    sscanf(OB_NUM(this_object()), "%d", seed);
    skill = 1000 / (skill + 1);
    skill = random(skill, seed);

    weight = cut_sig_fig(weight + (skill % 2 ? -skill % 70 : skill) *
                        weight / 100);
    volume = cut_sig_fig(volume + (skill % 2 ? -skill % 70 : skill) *
                        volume / 100);

    write("\n" + this_object()->long(0, this_player()) + "\n");
    write(break_string("You appraise that the weight is " + weight +
        " grams and you guess its volume is about " + volume +
        " milliliters.\n", 75));
}

/*
 * Function name: add_leftover
 * Description:   Add leftovers (at death) to the body.
 *		  The returned list should be an array of arrays, containing
 *                the following entries:
 *		    ({ ({ objpath, organ, nitems, vbfc, hard, cut }), ... })
 * Arguments:	  mixed list - The leftover list to use.
 */
varargs public void
add_leftover(mixed list)
{
    leftover_list = list;
}

/*
 * Function name: query_leftover
 * Description:   Return the leftover list. If an organ is specified, that
 *		  actual entry is looked for, otherwise, return the entire
 *		  list.
 *		  The returned list contains the following entries:
 *		      ({ objpath, organ, nitems, vbfc, hard, cut})
 * Arguments:	  string organ - The organ to search for, or 0 to get the
 *                    whole list.
 */
varargs public mixed
query_leftover(string organ)
{
    int i;

    if (!sizeof(leftover_list))
    {
	return ({ });
    }

    if (!strlen(organ))
    {
	return leftover_list;
    }

    for (i = 0 ; i < sizeof(leftover_list) ; i++)
    {
	if (leftover_list[i][1] == organ)
	{
	    return leftover_list[i];
	}
    }
}

/*
 * Function name: remove_leftover
 * Description:   Remove a leftover entry from a body.
 * Arguments:	  string organ - Which entry to remove.
 * Returns:       1 - Ok, removed, 0 - Not found.
 */
public int
remove_leftover(string organ)
{
    int i;

    if (!sizeof(leftover_list))
    {
	return 0;
    }

    for (i = 0 ; i < sizeof(leftover_list) ; i++)
    {
	if (leftover_list[i][1] == organ)
	{
	    leftover_list[i] = 0;
	}
    }
    leftover_list = filter(leftover_list, pointerp);
}

/*
 * Function name: get_leftover
 * Description:   Get leftovers from the body.
 * Arguments:     string arg - command-line argument.
 * Returns:       int 1/0 - success/failure.
 */
public int
get_leftover(string arg)
{
    mixed	corpses, leftovers;
    object 	*found, *weapons, theorgan;
    int		i, slash;
    string	organ, vb, fail;

    if (this_player()->query_prop(TEMP_STDCORPSE_CHECKED))
	return 0;

    vb = query_verb(); 
    
    notify_fail(capitalize(vb) + " what from what?\n");  /* access failure */
    if (!arg)
	return 0;

    if (!parse_command(arg, environment(this_player()), "%s 'from' %i",
		organ, corpses))
	return 0;

    found = VISIBLE_ACCESS(corpses, "find_corpse", this_object());
    
    if (sizeof(found) != 1)
    {
        set_alarm(0.5, 0.0, &(this_player())->remove_prop(TEMP_STDCORPSE_CHECKED));
        this_player()->add_prop(TEMP_STDCORPSE_CHECKED, 1);
        if (sizeof(found))
	    notify_fail("Please try to be more specific with what corpses you choose.\n");
        else
	    notify_fail("There is no such corpse lying around here.\n");
        return 0;
    }
    if (vb == "cut")
    {
	weapons = this_player()->query_weapon(-1);
	for (i = 0 ; i < sizeof(weapons) ; i++)
	{
	    if (weapons[i]->query_dt() & W_SLASH)
	    {
		slash = 1;
		break;
	    }
	}
	if (!slash)
	{
	    notify_fail("Better find something sharper than that to cut with.\n");
	    return 0;
	}
    }

    leftovers = query_leftover(organ);
    if (!sizeof(leftovers) || leftovers[2] == 0)
    {
        notify_fail("There's no such thing to get from the corpse.\n");
        return 0;
    }

    if (leftovers[5] && !slash)
    {
        notify_fail("You can't just tear this loose, use a knife or " +
    	    "something...\n");
	return 0;
    }

    if (strlen(leftovers[3]))
    {
	fail = check_call(leftovers[3]);
        if (strlen(fail))
        {
            notify_fail(fail + ".\n");
            return 0;
        }
    }

    if(leftovers[2]-- == 0)
    {
        remove_leftover(leftovers[1]);
    }

    theorgan = clone_object(leftovers[0]);
    theorgan->leftover_init(leftovers[1], query_prop(CORPSE_S_RACE));
    theorgan->move(this_player(), 0);

    say(QCTNAME(this_player()) + " " + vb + "s " + LANG_ADDART(organ) +
        " from " + QSHORT(found[0]) + ".\n");
    write("You " + vb +" " + LANG_ADDART(organ) + " from a corpse.\n");
    return 1;
}

/*
 * Function name: init
 * Description  : Called to link the tear/cut commands to the player.
 */
void
init()
{
    ::init();

    add_action(get_leftover, "tear");
    add_action(get_leftover, "cut");
}

/*
 * Function name: find_corpse
 * Description  : Support function for tear/cut command.
 * Arguments    : object ob - the object to check.
 * Returns      : int 1/0 - if true, it's a suitable corpse.
 */
public int
find_corpse(object ob)
{
    if (IS_CORPSE_OBJECT(ob) &&
	((environment(ob) == this_player()) ||
	 (environment(ob) == environment(this_player()))))
    {
	return 1;
    }

    return 0;
}

/*
 * Function name: search_for_leftovers
 * Description:   This function is called when someone searches the corpse
 *		  as set up in create_container()
 * Arguments:	  player - The player searching
 *		  str    - If the player specifically said what to search for
 * Returns:       A string describing what, if anything the player found.
 */
string
search_for_leftovers(object player, string str)
{
    mixed left;
    string *found;
    int i;

    left = query_leftover();
    found = ({});

    for (i = 0; i < sizeof(left); i++)
    {
	if (left[i][2] == 1)
	{
	    found += ({ LANG_ADDART(left[i][1]) });
	}
	else if (left[i][2] > 1)
	{
	    found += ({ LANG_WNUM(left[i][2]) + " " + LANG_PWORD(left[i][1]) });
	}
    }

    if (sizeof(found) > 0)
    {
	return "After searching the corpse you believe you " +
	    "can tear out or cut off " + COMPOSITE_WORDS(found) + ".\n";
    }

    return "";
}
