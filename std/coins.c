/* 
 * /std/coins.c
 *
 * This is the heap object for coins.
 */

#pragma save_binary
#pragma strict_types

inherit "/std/heap";

#include <macros.h>
#include <money.h>
#include <language.h>
#include <stdproperties.h>
#include <ss_types.h>
#include <std.h>

/*
 * Global variable. It holds the coin type.
 */
static string coin_type;

/*
 * Prototype.
 */
void set_coin_type(string str);

/*
 * Function name: create_coins
 * Description  : Called at creation of the coins. To create your own coins
 *                you must define this function.
 */
public void
create_coins()
{
    set_name("coin");
    set_pname("coins");
    set_heap_size(1);
    set_coin_type(MONEY_TYPES[0]);
}

/*
 * Function name: create_heap
 * Description  : Constructor. This will create the heap and set some stuff
 *                that we want. You may not mask this function. You have to
 *                use create_coins() to create your own coins.
 */
public nomask void
create_heap()
{
    add_prop(OBJ_M_NO_SELL, 1);

    create_coins();
    
    if (!query_prop(HEAP_S_UNIQUE_ID))
    {
	set_coin_type(MONEY_TYPES[0]);
    }
}

/*
 * Function name: reset_coins
 * Description  : In order to have some code executed when this heap of
 *                coins resets, mask this function. Notice that in order
 *                to make them reset, call enable_reset() from the function
 *                create_coins().
 */
public void
reset_coins()
{
}

/*
 * Function name: reset_heap
 * Description  : Called to make this heap reset. You may not mask this
 *                function, so use reset_coins() instead.
 */
public nomask void
reset_heap()
{
    reset_coins();
}

/*
 * Function name: query_auto_load
 * Description  : Coins are autoloading. This function is called to find
 *                out whether they are. It returns the coin type and the
 *                number of coins in this heap.
 * Returns      : string - the auto-load string.
 */
public string
query_auto_load()
{
    return (MASTER + ":" + num_heap() + "," + coin_type);
}

/* 
 * Function name: init_arg
 * Description  : Called when autoloading. It will set the type of coins
 *                and the number of coins in the heap.
 * Arguments    : string arg - the auto-load argument.
 */
void
init_arg(string arg)
{
    int sum;
    string ct;

    if (sscanf(arg, "%d,%s", sum, ct) == 2)
    {
	set_heap_size(sum);
	set_coin_type(ct);
    }
}

/*
 * Function name: short
 * Description  : This function is called to get the short description of
 *                these coins. We make it dependant on the intelligence of
 *                the onlooker and have special cases for different numbers
 *                of coins.
 * Arguments    : object for_object - the object that wants to know.
 * Returns      : string - the short string.
 */
public varargs string
short(object for_object)
{
    string str = (strlen(coin_type) ? (coin_type + " ") : "") + "coin";

    /* No elements in the heap == no show. */
    if (num_heap() < 1)
    {
	return 0;
    }

    /* No identifier: BAD coins. Remove them. */
    if (!strlen(query_prop(HEAP_S_UNIQUE_ID)))
    {
	set_alarm(0.0, 0.0, remove_object);
	return "ghost coins";
    }

    /* One coin, singular, not really a heap. */
    if (num_heap() < 2)
    {
	return LANG_ADDART(str);
    }

    /* Less than a dozen, we see the number as a word. */
    if (num_heap() < 12)
    {
	return LANG_WNUM(num_heap()) + " " + str + "s";
    }
    if (num_heap() == 12)
    {
        return "a dozen " + str + "s";
    }

    /* No onlooker, default to this_player(). */
    if (!objectp(for_object))
    {
	for_object = this_player();
    }

    /* If we are smart enough, we can see the number of coins. */
    if (for_object->query_stat(SS_INT) / 2 > num_heap())
    {
	return num_heap() + " " + str + "s";
    }

    /* Else, default to 'many' or to a 'huge heap'. */
    return (num_heap() < 1000 ? "many " : "a huge heap of ") + str + "s";
}

/*
 * Function name: long
 * Description  : This function will slip the short description into the
 *                long description. Money will always look like good
 *                money, but don't try to fool the shopkeepers with wooden
 *                coins ;-)
 * Returns      : string - the long description.
 */
varargs public mixed
long()
{
    if ((num_heap() < 2) ||
	(num_heap() >= 1000))
    {
	return "It is " + short() + "; it looks like good money.\n";
    }
    else
    {
	return "There are " + short() + "; they look like good money.\n";
    }
}

/*
 * Function name: set_coin_type
 * Description  : Set the type of coins we have here. Update all necessary
 *                properties with respect to the coins.
 * Arguments    : string str - the coin type to set.
 */
public void  
set_coin_type(string str)
{
    int ix;

    /* If this is one of the default coin types, set the weight, volume
     * and value correctly.
     */
    ix = member_array(str, MONEY_TYPES);
    if (ix >= 0)
    {
	mark_state();
	add_prop(HEAP_I_UNIT_VALUE, MONEY_VALUES[ix]);
	add_prop(HEAP_I_UNIT_WEIGHT, MONEY_WEIGHT[ix]);
	add_prop(HEAP_I_UNIT_VOLUME, MONEY_VOLUME[ix]);
	update_state();
    }

    /* If there is a coin-type, remove that coin type as adjective. */
    if (coin_type)
    {
	remove_adj(coin_type);
	remove_name(coin_type + " coin");
	remove_pname(coin_type);
    }

    /* Set the new coin type and set it as an adjective. Also, we update
     * our identifier.
     */
    coin_type = str;
    add_prop(HEAP_S_UNIQUE_ID, MONEY_UNIQUE_NAME(coin_type));
    set_adj(coin_type);
    add_name(coin_type + " coin");
    add_pname(coin_type);
}

/*
 * Function name: query_coin_type
 * Description  : Return what type of coins we have.
 * Returns      : string - the coin type.
 */
public string
query_coin_type()
{
    return coin_type;
}

/*
 * Function name: config_split
 * Description  : When a part of this heap is split, we make sure the new
 *                heap is made into the correct type of coins as well by
 *                setting the coin type to the coin type of the heap we are
 *                being split from.
 * Arguments    : int new_num - the number of coins in this new heap.
 *                object orig - the heap we are split from.
 */
public void
config_split(int new_num, object orig)
{
    ::config_split(new_num, orig);

    set_coin_type(orig->query_coin_type());
}

/*
 * Function name: stat_object
 * Description  : When a wizard stats this heap of coins, we add the coin
 *                type to the information.
 * Returns      : string - the stat-description.
 */
public string
stat_object()
{
    return ::stat_object() + "Coin type: " + coin_type + "\n";
}

/*
 * Function name: move
 * Description  : Make sure moving of money is logged if the amount is
 *                larger than a certain amount.
 * Arguments    : see move in /std/object.c
 * Returns      : see move in /std/object.c
 */
varargs public int
move(mixed dest, mixed subloc)
{
    string str;
    object env = environment();
    int rval = ::move(dest, subloc);

    /* If there was an error moving or if the limit is not high enough, do
     * not log.
     */
    if (rval ||
    	(num_heap() < MONEY_LOG_LIMIT[coin_type]))
    {
	return rval;
    }

    if (stringp(dest))
    {
	dest = find_object(dest);
    }

    /* No destination means coins are destructed. That is not interesting.
     * If the coins enter a wizard that is not interesting either.
     */
    if (!objectp(dest) ||
	dest->query_wiz_level())
    {
	return 0;
    }

    str = sprintf("%s: %4d %-8s ", ctime(time()), num_heap(), coin_type);
    str += (objectp(env) ? (interactive(env) ?
	capitalize(env->query_real_name()) : file_name(env)) : "void");
    str += " -> ";
    str += (interactive(dest) ? capitalize(dest->query_real_name()) :
	file_name(dest));

    /* Log the transation. */
    SECURITY->log_syslog("MONEY_LOG", (str + "\n"), 100000);

    return 0;
}
