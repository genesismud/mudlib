/*
 * /std/potion.c
 *
 * This is the standard object used for any form of potion.
 *
 * It works much the same way as herb.c.
 * Potions are quaffed btw ;-)
 *
 *      Nota bene:
 *      Potions do define a global variable alco_amount. Quaffing a
 *      potion, however, does not intoxicate. The variable alco_amount
 *      is kept for backwards compatibility. It can be queried using
 *      function query_alco_strength().
 */
#pragma save_binary
#pragma strict_types

inherit "/std/object";
inherit "/lib/herb_support";

#include <cmdparse.h>
#include <composite.h>
#include <files.h>
#include <macros.h>
#include <ss_types.h>
#include <stdproperties.h>

int	id_diff,		/* How difficult is it to id the potion? */
        magic_resistance,	/* How strong is the resistance against magic */
        soft_amount,
        alco_amount,
        potion_value,
        identified = 0,
        quaffed_it;
object *gFail;
string	quaff_verb,
        potion_name,
        id_long,
        unid_long,
        id_smell,
        unid_smell,
        id_taste,
        unid_taste;

/*
 * Function name: do_id_check
 * Description:   This little function is called each time the potion
 *                is referred to by a player, to check if (s)he
 *                identifies it or not.
 * Arguments:	  player - The player
 * Returns: 1 for identification, else 0.
 */
varargs int
do_id_check(object player)
{
    if (!objectp(player))
        player = this_player();

    if (objectp(player) && (
        ((environment(this_object()) == player) && identified) ||
        (id_diff <= player->query_skill(SS_ALCHEMY))
        ))
        return 1;
    else
        return 0;
}

int quaff_it(string str);
int taste_it(string str);
int smell_it(string str);

/*
 * Function name: init
 * Description:   adds the quaff-action to the player
 */
void
init()
{
    ::init(); /* If this isn't added cmd items won't work. */

    add_action(quaff_it, quaff_verb);
    if (quaff_verb != "quaff")
        add_action(quaff_it, "quaff");
    if (quaff_verb != "taste")
        add_action(taste_it, "taste");
    if (quaff_verb != "smell")
        add_action(smell_it, "smell");
}

void
leave_env(object from, object to)
{
    if (from && living(from))
    {
        remove_name(potion_name);
        identified = 0;
    }

    ::leave_env(from, to);
}

void
enter_env(object dest, object old)
{
    if (dest && living(dest) && do_id_check(dest))
    {
        add_name(potion_name);
        identified = 1;
    }

    ::enter_env(dest, old);
}

/*
 * Function name: long_func
 * Description:   This is an VBFC function for the set_long in the
 *                potion, which tests if the player examining it can
 *                identify it.
 *                Make sure to set_id_long(str) and set_unid_long(str)
 *                from the create_potion() function.
 */
nomask string
long_func()
{
    if (do_id_check(this_player()))
        return id_long;
    return unid_long;
}

/*
 * Function name: set_quaff_verb
 * Description:   Set what verb the player should type to be able to
 * 		  quaff the potion. Default is "quaff"
 *                "drink" is not possible.
 * Arguments:     str - The verb
 */
void
set_quaff_verb(string str)
{
    if (str != "drink")
        quaff_verb = str;
}

/*
 * Function name: query_quaff_verb
 * Description:   What verb is required to quaff this potion?
 * Returns:	  The verb;
 */
string
query_quaff_verb() { return quaff_verb; }

/*
 * Function name: set_potion_name
 * Description:   Set the true name of the potion
 * Arguments:	  str - The name
 */
void
set_potion_name(string str) { potion_name = str; }

/*
 * Function name: query_potion_name
 * Description:   What is the true name of the potion
 * Returns:	  The name of the potion
 */
string
query_potion_name() { return potion_name; }

/*
 * Function name: set_id_long
 * Description:   Set the long description you see if you know
 *                what potion it is.
 * Arguments:     str - The description
 */
void
set_id_long(string str) { id_long = str; }

/*
 * Function name: query_id_long
 * Description:   The long description if you can id the potion
 * Returns:       The long description
 */
string
query_id_long() { return id_long; }

/*
 * Function name: set_unid_long
 * Description:   Set the long description you see if you cannot identify the 
 *		  potion.
 * Arguments:     str - The long description
 */
void
set_unid_long(string str) { unid_long = str; }

/*
 * Function name: query_unid_long
 * Description:   Query the long description you get if you cannot identify
 *		  the potion.
 * Returns:   	  The unidentified long description
 */
string
query_unid_long() { return unid_long; }

/*
 * Function name: set_id_taste
 * Description:   Set the message when you taste the identified potion
 * Arguments:     str - The message
 */
void
set_id_taste(string str) { id_taste = str; }

/*
 * Function name: query_id_taste
 * Description:   Query the message you get if you taste and identify
 *                the potion.
 * Returns:   	  The taste of the identified potion
 */
string
query_id_taste() { return id_taste; }

/*
 * Function name: set_id_smell
 * Description:   Set the message when you smell the identified potion
 * Arguments:     str - The message
 */
void
set_id_smell(string str) { id_smell = str; }

/*
 * Function name: query_id_smell
 * Description:   Query the message you get if you smell and identify
 *                the potion.
 * Returns:   	  The smell of the identified potion
 */
string
query_id_smell() { return id_smell; }

/*
 * Function name: set_unid_taste
 * Description:   Set the message when you taste the unidentified potion
 * Arguments:     str - The message
 */
void
set_unid_taste(string str) { unid_taste = str; }

/*
 * Function name: query_unid_taste
 * Description:   Query the message you get if you taste and do not
 *                identify the potion.
 * Returns:   	  The taste of the unidentified potion
 */
string
query_unid_taste() { return unid_taste; }

/*
 * Function name: set_unid_smell
 * Description:   Set the message when you smell the unidentified potion
 * Arguments:     str - The message
 */
void
set_unid_smell(string str) { unid_smell = str; }

/*
 * Function name: query_unid_smell
 * Description:   Query the message you get if you smell and do not
 *                identify the potion.
 * Returns:   	  The smell of the unidentified potion
 */
string
query_unid_smell() { return unid_smell; }

/*
 * Function name: set_id_diff
 * Description:   Set how hard it is to identify a potion
 * Arguments:     i - The skill needed to know the potion
 */
void
set_id_diff(int i) { id_diff = i; }

/*
 * Function name: query_id_diff
 * Description:   How hard is it to identify this potion
 * Returns:  	  The difficulty
 */
int
query_id_diff() { return id_diff; }

/*
/*
 * Function name: set_soft_amount
 * Description:   Set the soft amount of the potion
 * Argument:      int - the soft amount
 */
void 
set_soft_amount(int i)
{
    if (i > 10)
        soft_amount = i;
    else
        soft_amount = 10;

    add_prop(OBJ_I_VOLUME, soft_amount);
    add_prop(OBJ_I_WEIGHT, soft_amount);
}

/*
 * Function name: query_soft_amount
 * Description:   What is the soft amount of the potion
 * Returns:       The soft amount
 */
int 
query_soft_amount(int i) { return soft_amount; }

/*
 * Function name: set_alco_amount
 * Description:   Set the alco amount of the potion
 * Argument:      int - the alco amount
 */
void 
set_alco_amount(int i) { alco_amount = i; }

/*
 * Function name: query_alco_amount
 * Description:   Quaffing potions does not intoxicate.
 *                This is achieved by making this function return 0.
 *                To query the alcohol amount in the potion use
 *                query_alco_strength().
 * Returns:       0
 */
nomask int 
query_alco_amount(int i) { return 0; }

/*
 * Function name: query_alco_strength
 * Description:   What is the alco amount of the potion?
 * Returns:       The alco amount
 */
int 
query_alco_strength(int i) { return alco_amount; }

/*
 * Function name: set_potion_value
 * Description:   Set the value of the potion when dealing with
 *                a potion specialist
 * Arguments:     i - The value
 */
void
set_potion_value(int i) { potion_value = i; }

/*
 * Function name: query_potion_value
 * Description:   The value of the potion when dealing with a specialist
 * Returns:	  The value
 */
int
query_potion_value() { return potion_value; }

/*
 * Function name:       set_quaffed_it
 * Description:         This is called if the potion is quaffed and the 
 *                      quaff_verb != "quaff
 */
void
set_quaffed_it() { quaffed_it = 1; }

/*
 * Function name: quaffed_non_quaff_potion
 * Description:   This function is called instead of do_herb_effects
 *                if you quaff a potion that isn't supposed to be
 *                quaffed.
 */
void
quaffed_non_quaff_potion()
{
    write("You don't feel any effect.\n");
}

/*
 * Function name: query_identified
 * Description:   Did you identify the potion?
 * Returns:       True if identified
 */
int
query_identified() { return identified; }

varargs void
set_identified(int i = 1) { identified = i; }

public void
create_potion()
{
    ::create_object();
    set_name("potion");
}

public nomask void
create_object()
{
    set_name("potion");
    set_short("unknown potion");
    set_id_long("This potion has not been described by the wizard.\n");
    set_unid_long("This is an undescribed, unknown potion.\n");
    set_unid_smell("The potion seems to be without smell.\n");
    set_id_smell("The potion is without smell.\n");
    set_unid_taste("The potion seems to be tasteless.\n");
    set_id_taste("The potion is tasteless.\n");
    set_potion_name("xortion");
    set_soft_amount(50);
    set_alco_amount(0);
    set_id_diff(20);
    set_potion_value(10);
    set_quaff_verb("quaff");

    add_prop(OBJ_I_VALUE, 0);

    create_potion();
    set_long("@@long_func");
}

public nomask void
reset_object() { this_object()->reset_potion(); }

void
consume_text(object *arr, string vb)
{
    string str;

    write("You " + vb + " " + (str = COMPOSITE_DEAD(arr)) + ".\n");
    say(QCTNAME(this_player()) + " " + vb + "s " + str + ".\n");
}

/*
 * Function name:	quaff_it
 * Description:		Quaffs the objects described as parameter to the
 *                      quaff_verb. It uses command parsing to find which
 *                      objects to quaff.
 * Arguments:		str: The trailing command after 'quaff_verb ...'
 * Returns:		True if command successful
 */
public int
quaff_it(string str)
{
    int	    il;
    object *a,
           *potions;
    string  str2,
            vb;

    if (this_player()->query_prop(TEMP_STDPOTION_CHECKED) ||
	query_prop(TEMP_OBJ_ABOUT_TO_DESTRUCT))
    {
        return 0;
    }

    gFail = ({ });
    vb = query_verb(); 

    notify_fail(capitalize(vb) + " what?\n", 0);
    if (!stringp(str))
        return 0;

/* It's impossible to quaff all potions at once.
 */
    if (str == "all")
    {
        notify_fail("You must specify which potion to " + vb + ".\n");
        return 0;
    }

    a = CMDPARSE_ONE_ITEM(str, "quaff_one_thing", "quaff_access");
    if (sizeof(a) > 0)
    {
        consume_text(a, vb);
        for (il = 0; il < sizeof(a); il++)
            a[il]->destruct_object();
        return 1;
    }
    else
    {
	set_alarm(1.0, 0.0,
	    &(this_player())->remove_prop(TEMP_STDPOTION_CHECKED));
        this_player()->add_prop(TEMP_STDPOTION_CHECKED, 1);
        if (sizeof(gFail))
            notify_fail("@@quaff_fail:" + file_name(this_object()));
        return 0;
    }
}

string
quaff_fail()
{
    string str;

    str = "You try to " + quaff_verb + " " + COMPOSITE_DEAD(gFail) +
        " but fail.\n";
    say(QCTNAME(this_player()) + " tries to " + quaff_verb + " " +
        QCOMPDEAD + " but fails.\n");
    this_player()->remove_prop(TEMP_STDPOTION_CHECKED);
    return str;
}

int
quaff_access(object ob)
{ 
    string vb = query_verb();

    if ((environment(ob) == this_player()) &&
        IS_POTION_OBJECT(ob) &&
        (vb == ob->query_quaff_verb() || vb == "quaff") &&
        !ob->query_prop(TEMP_OBJ_ABOUT_TO_DESTRUCT))
        return 1;
    else
        return 0;
}

int
quaff_one_thing(object ob)
{
    int     soft = ob->query_soft_amount(),
            alco = ob->query_alco_amount();
    string  vb = query_verb();

    if (!(this_player()->drink_soft(soft)))
    {
        write(capitalize(LANG_ADDART(ob->short())) +
            " is too much for you.\n");
        gFail += ({ ob });
        return 0;
    }
    if (!(this_player()->drink_alco(alco)))
    {
        this_player()->drink_soft(-soft);
        write(capitalize(LANG_ADDART(ob->short())) +
            " is too strong for you.\n");
        gFail += ({ ob });
        return 0;
    }
    /* Test if you quaffed a non quaff potion */
    if ((vb == "quaff") && (vb != ob->query_quaff_verb()))
            ob->set_quaffed_it();
    
    return 1;
}

/*
 * Function name: destruct_object
 * Description:   Call do_herb_effects or quaffed_non_quaff_potion
 *                Clone an empty vial and remove potion
 */
void
destruct_object()
{
    object  vial;

    if (quaffed_it && quaff_verb != "quaff")
    {
        quaffed_non_quaff_potion();
    }
    else
    {
        do_herb_effects();
    }

    seteuid(getuid());
    vial = clone_object("/std/container");
    vial->set_name("vial");
    vial->add_name(({"_std_potion_vial"}));
    vial->set_adj("empty");
    vial->set_long("An empty vial. You could fill a potion into it.\n");
    vial->add_prop(CONT_I_MAX_VOLUME, 1100);
    vial->add_prop(CONT_I_MAX_WEIGHT, 1250);
    vial->add_prop(CONT_I_VOLUME, 100);
    vial->add_prop(CONT_I_WEIGHT, 250);
    if (vial->move(environment(this_object())))
        vial->move(environment(this_object()), 1);

    add_prop(TEMP_OBJ_ABOUT_TO_DESTRUCT, 1);
    set_alarm(1.0, 0.0, remove_object);
}

/*
 * Function name: set_magic_res
 * Description:   Set how resistance this potion is agains magic / how easy it
 *		  is to dispel it.
 * Arguments:     resistance - the resistance
 */
void set_magic_res(int resistance) { magic_resistance = resistance; }

/*
 * Function name: query_magic_resistance
 * Description:   Query the magic resistance
 * Returns:       How resistive the potion is
 */
int
query_magic_res(string prop)
{ 
    if (prop == MAGIC_I_RES_MAGIC)
        return magic_resistance;
    else
        return 0;
}

/*
 * Function name: dispel_magic
 * Description:   Function called by a dispel spell
 * Argument:	  magic - How strong the dispel is
 * Returns:	  0 - No dispelling, 1 - Object dispelled
 */
int
dispel_magic(int magic)
{
    object env = environment(this_object());

    if (magic < query_magic_res(MAGIC_I_RES_MAGIC))
        return 0;

    if (living(env))
    {
        tell_room(environment(env), "The " + QSHORT(this_object()) +
            " held by " + QTNAME(env) + " glows white and explodes!\n", env);
        env->catch_msg("The " + QSHORT(this_object()) + " that you hold " +
            " glows white and explodes!\n");
    }
    else if (env->query_prop(ROOM_I_IS))
        tell_room(env, "The " + QSHORT(this_object()) +
            " glows white and explodes!\n");

    return 1;
}

int
smell_access(object ob)
{ 
    string vb = query_verb();

    if ((environment(ob) == this_player()) && IS_POTION_OBJECT(ob) &&
        (vb == "taste" || vb == "smell"))
        return 1;
    else
        return 0;
}

int
smell_one_thing(object ob)
{
    object  pl = this_player();
    string  vb = query_verb();

    if (ob->query_identified() ||
        (ob->query_id_diff() <= (pl->query_skill(SS_ALCHEMY) * 150 +
                     pl->query_skill(SS_HERBALISM) * 50) / 100) )
    {
        ob->add_name(potion_name);
        ob->set_identified(1);
        if (vb == "taste")
            write(ob->query_id_taste());
        else if (vb == "smell")
            write(ob->query_id_smell());
    }
    else
    {
        if (vb == "taste")
            write(ob->query_unid_taste());
        else if (vb == "smell")
            write(ob->query_unid_smell());
    }
    
    return 1;
}

/*
 * Function name: smell_it
 * Description:   Smell the potion, but do not quaff it
 *                If smelling should call do_herb_effects set
 *                quaff_verb to "smell"
 * Arguments:     str - the argument of the command
 * Returns:       True is successful
 */
int
smell_it(string str)
{
    int     il;
    object *a;
    string  vb;

    if (this_player()->query_prop(TEMP_STDPOTION_CHECKED) ||
	query_prop(TEMP_OBJ_ABOUT_TO_DESTRUCT))
    {
        return 0;
    }

    vb = query_verb();

    /* It's impossible to smell all potions at once. */
    
    if (str == "all")
    {
        notify_fail("You must specify which potion to " + vb + ".\n");
        return 0;
    }

    notify_fail(capitalize(vb) + " what?\n");  /* access failure */
    if (!stringp(str))
	return 0;

    a = CMDPARSE_ONE_ITEM(str, "smell_one_thing", "smell_access");
    if (sizeof(a) > 0)
    {
        say(QCTNAME(this_player()) + " " + vb + "s " +
            COMPOSITE_DEAD(a) + ".\n");
        return 1;
    }
    else
    {
	set_alarm(1.0, 0.0,
	    &(this_player())->remove_prop(TEMP_STDPOTION_CHECKED));
        this_player()->add_prop(TEMP_STDPOTION_CHECKED, 1);
        return 0;
    }
}

/*
 * Function name: taste_it
 * Description:   Taste the potion, but do not quaff it
 *                If tasting should call do_herb_effects set
 *                quaff_verb to "taste"
 * Arguments:     str - the argument of the command
 * Returns:       True is successful
 */
int
taste_it(string str)
{
    return smell_it(str);
}
