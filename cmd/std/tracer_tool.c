/*
 * /cmd/std/tracer_tool.c
 *
 * Below are the functions from the object tracer. This is a general purpose
 * tool. It can be used to find objects, list info about them, and walk up
 * and down the inventory lists.
 *
 * The following commands are supported:
 *
 * - At
 * - Call
 * - Cat
 * - Clean
 * - Dump
 * - Destruct
 * - Ed
 * - Goto
 * - More
 * - Move
 * - Reload
 * - Set
 * - Tail
 * - Top
 */

#pragma no_clone
#pragma save_binary
#pragma strict_types

inherit "/cmd/std/tracer_tool_base";

#include <filepath.h>
#include <macros.h>
#include <std.h>
#include <stdproperties.h>

/*
 * Prototype.
 */
int In(string str);
static nomask void object_items(object target);
static nomask void light_status(object target, int level);
static int profile_sort(string item, mixed a, mixed b);

#define CHECK_SO_WIZ    if (WIZ_CHECK < WIZ_NORMAL) return 0; \
                        if (this_interactive() != this_player()) return 0
#define TRACER_STORES   "_tracer_stores"
#define TRACER_VARS     "_tracer_vars"
#define SPACES          ("                                            ")

/*
 * Function name: get_soul_id
 * Description  : Returns the proper name in order to get a nice printout.
 *                On the other hand the name is simple so people can type
 *                it if they only want to know which commands this soul
 *                supports.
 * Returns      : string - the name.
 */
string
get_soul_id()
{
    return "tracer";
}

/*
 * Function name: query_tool_soul
 * Description  : Identify this as a tool soul.
 * Returns      : int 1 - always.
 */
nomask public int
query_tool_soul()
{
    return 1;
}

/*
 * Function name: query_cmdlist
 * Description  : This function returns mapping with the commands this soul
 *                supports and the functions to call. Please put new in
 *                alphabetical order.
 * Returns      : mapping - the commands and functions.
 */
mapping
query_cmdlist()
{
    return ([
             "At"       : "At",

             "Call"     : "Call",
             "Cat"      : "Cat",
             "Clean"    : "Clean",
             
             "Dump"     : "Dump",
             "Destruct" : "Destruct",
             
             "Ed"       : "Ed",
             
             "Goto"     : "Goto",
             
             "I"        : "Inventory",	/* Pointless alias, remove later */
             "Inventory": "Inventory",	/* Pointless alias, remove later */
             "In"       : "In",
	     "Items"	: "Items",	/* Pointless alias, remove later */

	     "Light"	: "Light",	/* Pointless alias, remove later */
             
             "More"     : "More",
             "Move"     : "Move",

	     "Reload"	: "Reload",
             
             "Set"      : "Set",

	     "Tail"	: "Tail",
	     "Top"	: "Top"
             ]);
}

/* **************************************************************************
 * Here follows the actual functions. Please add new functions in the 
 * same order as in the function name list.
 * **************************************************************************/

/*
 * At - do someting in someones environment.
 *
 * Syntax   : At <person> <command>
 * Arguments: <person>  - the name of the person.
 *            <command> - the command to execute.
 */
int
At(string str)
{
    string name;
    string cmd;
    object obj;

    CHECK_SO_WIZ;

    if (!stringp(str) ||
        (sscanf(str, "%s %s", name, cmd) != 2))
    {
        notify_fail("Syntax: At <name> <command>\n");
        return 0;
    }

    if (!objectp(obj = find_player(name)))
    {
        notify_fail("Player \"" + name + "\" not found.\n");
        return 0;
    }

    if (!objectp(obj = environment(obj)))
    {
        notify_fail(capitalize(name) + " does not have an environment.\n");
        return 0;
    }

    return In(file_name(obj) + " " + cmd);
}

/*
 * Call - call a function in an object.
 *
 * Syntax   : Call <object> <function> [<arg1>[%%<arg2>...]]
 * Arguments: <object>   - the object to call a function in.
 *            <function> - the function to call in the object.
 *            <arg1> ... - possible arguments to call the function with.
 */
int
Call(string str)
{
    string *args;
    string source;
    mixed  ret;
    object obj;
    object s_obj;
    int    found;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        notify_fail("Syntax: Call <object> <function> [<arguments>]\n");
        return 0;
    }

    str = extract(implode(explode((str + " "), "\\n"), "\n"), 0, -2);
    args = explode(str, " ");
    if (sizeof(args) == 1)
    {
        notify_fail("Syntax: Call <object> <function> [<arguments>]\n");
        return 0;
    }

    if (!objectp(obj = get_assign(args[0])))
	obj = parse_list(args[0]);

    if (!objectp(obj))
    {
        write("Object '" + args[0] + "' not found.\n");
        return 1;
    }

    write("Looking for function " + args[1] + "() in '" + args[0] +
        "'.\n");
    if (strlen(source = function_exists(args[1], obj)))
    {
        write("Found in '" + obj->short() + "' (" + file_name(obj) + ")\n");
        if (source != MASTER_OB(obj))
        {
            write("Routine defined in inherited file: " + source + "\n");
        }
        found = 1;
    }

    s_obj = obj;
    while (objectp(s_obj = shadow(s_obj,0)))
    {
        if (function_exists(args[1], s_obj))
        {
            write("Function in shadow " + file_name(s_obj) + "\n");
            found = 1;
        }
    }

    if (!found)
    {
        write("Not found in '" + obj->short() + "' (" + file_name(obj) +
            ") or any of its shadows.\n");
        return 1;
    }
    write("\n");

    /* This message should be done before the actual call in order not to
     * get an error if the object gets destructed for then the VBFC will
     * fail.
     */
    say("@@call_message:" + file_name(this_object()) + "|" +
        this_player()->query_real_name() + "|" + args[1] + "|" +
        file_name(obj) + "@@");

    /* If the total number of argements is two, this means there is no
     * argument to the function, else, we compute the arguments and
     * make the call via call_otherv().
     */
    if (sizeof(args) == 2)
    {
        ret = call_other(obj, args[1]);
    }
    else
    {
        ret = call_otherv(obj, args[1], parse_arg(implode(args[2..], " ")));
    }

    print_value(ret);

    assign("ret", ret);

    return 1;
}

/*
 * Function name: call_message
 * Description  : Give a different message to wizards and mortals when
 *                you call a function in a certain object.
 * Arguments    : string name    - the person patching.
 *                string command - the function called.
 *                string patched - the patched object.
 * Returns      : string - the straight message.
 */
string
call_message(string name, string command, string patched)
{
    object caller = find_player(name);
    object pobj   = previous_object(-1);
    object in_ob  = find_object(patched);
    int    wiz    = pobj->query_wiz_level();
    string obj;
    string called;

    if (!wiz &&
        (!CAN_SEE(pobj, caller) ||
         !CAN_SEE_IN_ROOM(pobj)))
    {
        return "";
    }

    if (pobj == in_ob)
        obj = "you!";
    else if (living(in_ob))
        obj = in_ob->query_the_name(pobj) + ".";
    else if (stringp(obj = in_ob->short(pobj)))
        obj += ".";
    else if (wiz)
        obj = file_name(in_ob) + ".";
    else
        obj = "something.";

    if (wiz)
        called = " called '" + command + "' in ";
    else if (extract(command, 0, 5) == "query_")
        called = " checked the internals of ";
    else
        called = " patched the internals of ";

    return caller->query_The_name(pobj) + called + obj + "\n";
}

/*
 * Cat - cat a file.
 *
 * Syntax   : Cat [<object>]
 * Arguments: <object> - the object to cat.
 * Default  : 'here'
 */
int
Cat(string str)
{
    object ob;
    int sz;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        str = "here";
        ob = environment(this_interactive());
        write(file_name(ob) + "\n");
    }
    else
    {
	if (!objectp(ob = get_assign(str)))
	    ob = parse_list(str);
    }

    if (!objectp(ob))
    {
        notify_fail("Object '" + str + "' not found.\n");
        return 0;
    }

    str = MASTER_OB(ob) + ".c";
    if (!(sz = cat(str, 0, 100)))
    {
        notify_fail("No read access to: " + str + "\n");
        return 0;
    }
    else if (sz == 100)
	write("... Truncated after 100 lines\n");

    return 1;
}

/*
 * Clean - destruct all non-interactive objects in somethings inventory.
 *
 * Syntax   : Clean [<object>]
 * Arguments: <object> - the object to clean.
 * Default  : 'here'
 */
int
Clean(string str)
{
    object ob, *ob_list;
    string tmp;
    int    index;
    int    size;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        ob = environment(this_interactive());
        write(file_name(ob) + "\n");
    }
    else
    {
	if (!objectp(ob = get_assign(str)))
	    ob = parse_list(str);
    }

    if (!objectp(ob))
    {
        notify_fail("Clean what object?\n");
        return 0;
    }

    ob_list = all_inventory(ob);

    index = -1;
    size = sizeof(ob_list);

    if (!size)
    {
        write("Nothing in object inventory to destruct.\n");
        return 1;
    }

    while(++index < size)
    {
        if (query_interactive(ob_list[index]))
            continue;

        catch(write("Destructing: " +
            (stringp(tmp = ob_list[index]->short(this_interactive())) ?
            capitalize(tmp) : file_name(ob_list[index])) + "\n"));

        /* Try to remove it the easy way if possible. */
        catch(ob_list[index]->remove_object());

        /* Destruct if the hard way if it still exists. */
        if (objectp(ob_list[index]))
        {
            SECURITY->do_debug("destroy", ob_list[index]);
        }
    }

    say(QCTNAME(this_interactive()) +
        " cleans the room with a magical gesture.\n");
    return 1;
}

/*
 * Destruct - destruct a certain object.
 *
 * Syntax   : Destruct [-D] <object>
 * Arguments: -D       - destruct the object with force.
 *            <object> - the object to destruct.
 */
int
Destruct(string str)
{
    object ob;
    int    dflag;
    string a;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        notify_fail("Destruct what?\n");
        return 0;
    }

    if (sscanf(str, "-D %s", a) == 1)
    {
        dflag = 1;
        str = a;
    }

    if (!objectp(ob = get_assign(str)))
	ob = parse_list(str);

    if (!objectp(ob))
    {
        notify_fail("Object '" + str + "' not found.\n");
        return 0;
    }

    /* Need catch() here for bugs in short(). Baaad wiz. */
    catch(write("Trying to destruct: " +
        (strlen(str = ob->short(this_interactive())) ? str : "---") + "\n"));

    /* Clean up the binary too, so it will have to be reloaded. */
    SECURITY->remove_binary(MASTER_OB(ob));

    if (dflag)
        SECURITY->do_debug("destroy", ob);

    if (objectp(ob))
        ob->remove_object();

    if (objectp(ob))
        SECURITY->do_debug("destroy", ob);

    write("Ok.\n");
    return 1;
}

/*
 * Dump - print information on an object.
 *
 * Syntax   : Dump <object> <flag>
 * Arguments: <object> - the object to dump.
 *            <flag>   - what to dump, this can take many forms.
 */
int
Dump(string str)
{
    int     i, j, sz, query_list, ret, calls, etime;
    object  ob, *ob_list;
    string  *args, flag, extra, *props, path, tmp;
    mixed   *data, *vars, *funcs, *item;

    CHECK_SO_WIZ;

    flag = extra = "";
    if (stringp(str))
    {
	args = explode(str, " ");
	path = args[0];
	if (sizeof(args) >= 2)
	{
	    flag = args[1];
	    if (sizeof(args) >= 2)
		extra = implode(args[2..], " ");
	}
    }
    else
	path = str;

    if (!objectp(ob = get_assign(path)))
	ob = parse_list(path);

    if (!objectp(ob))
    {
        notify_fail("Object '" + path + "' not found.\n");
        return 0;
    }

    write(file_name(ob) + ":\n" + ob->short(this_interactive()));
    if (living(ob))
    {
        write(" (living)");
    }

    if (interactive(ob))
    {
        write(" (interactive)" + (SECURITY->valid_query_ip(geteuid()) ?
            (" '" + query_ip_name(ob) + "'") : ""));
    }
    write("\n");

    if (stringp(tmp = creator(ob)))
    {
        write("Creator: " + tmp + "  ");
    }
    write("UID: " + getuid(ob) + "  EUID: " + geteuid(ob) + "\n");

    write("\n");

    switch (flag)
    {
    case "":
	break;

    case "info":
        write(SECURITY->do_debug("object_info", 1, ob));
        write("\n");
        break;

    case "alarms":
        data = ob->query_alarms();
        for (i = 0; i < sizeof(data);  i++)
        {
            write("Id        : " + data[i][0] + "\n"); 
            write("Function  : " + data[i][1] + "\n");
            write("Time Left : " + ftoa(data[i][2]) + "\n");
            write("Repeat    : " + ftoa(data[i][3]) + "\n");
            write("Arguments :\n");
            dump_array(data[i][4]);
            write("\n\n");
        }
	break;

    case "cpu":
        write("Object CPU usage: " + SECURITY->do_debug("object_cpu", ob));
        write("\n");
	break;

    case "flags":        
        write(SECURITY->do_debug("object_info", 0, ob));
        write("\n");
	break;

    case "functions":
        data = SECURITY->do_debug("functionlist", ob);
        for (i = 0; i < sizeof(data); i++)
            write("    " + data[i] + "\n");
        write("\n");
        break;

    case "inherits":
        dump_array(SECURITY->do_debug("inherit_list", ob));
        write("\n");
	break;

    case "items":
	object_items(ob);
	break;

    case "list":
    case "inv":
    case "inventory":
        ob_list = all_inventory(ob);
        for (i = 0; i < sizeof(ob_list); i++)
	{
	    tmp = ob_list[i]->short(this_interactive());
	    write(sprintf("%2d: %-30s %-45s\n", (i + 1),
		(stringp(tmp) ? tmp : "---"),
		file_name(ob_list[i])));
	}
        write("\n");
	break;

    case "light":
	write(sprintf("Object: %-52s : OBJ CONT ROOM\n\n",
		      RPATH(file_name(ob))));

	light_status(ob, 0);
	break;

    case "profile":
	extra = strip(extra);
	if (!strlen(extra) || 
	    (member_array(extra, ({ "time", "calls", "average", "function"})) < 0 &&
	     member_array(extra[0..0], ({ "t", "c", "a", "f" })) < 0))
	    extra = "time";

        funcs = SECURITY->do_debug("getprofile", ob);
	if (stringp(funcs)) 
	{ 
	    write(funcs + "\n"); 
	    break; 
	}

	data = ({});
	foreach (string func: funcs)
	{
	    sscanf(func, "%d:%d: %s", calls, etime, func);
	    item = allocate(4);
	    item[0] = etime;
	    item[1] = calls;
	    item[2] = calls ? itof(etime) / itof(calls) : -1.0;
	    item[3] = func;
	    data += ({ item });
	}

	funcs = sort_array(data, &profile_sort(extra,));

	write(sprintf("%16s %16s %14s   %s\n\n", "Time", "Calls", "Average", "Function"));
	foreach (mixed func: funcs)
	    write(sprintf("%16d %16d %14s : %s\n", func[0], func[1],
		       (func[2] > 0.0 ? sprintf("%10.4f", func[2]) : "-"), func[3]));

	break;

    case "profile_avg":
	extra = strip(extra);
	if (!strlen(extra) || 
	    member_array(extra[0..0], ({ "t", "c", "a", "f" })) < 0)
	    extra = "time";

	funcs = SECURITY->do_debug("getprofile_avg", ob);
	if (stringp(funcs)) 
	{ 
	    write(funcs + "\n"); 
	    break; 
	}

	data = ({});
	foreach (mixed func: funcs)
	{
	    item = allocate(4);
	    item[0] = func[1];
	    item[1] = func[2];
	    item[2] = func[2] > 0.0 ? func[1] / func[2] : -1.0;
	    item[3] = func[0];
	    data += ({ item });
	}

	funcs = sort_array(data, &profile_sort(extra,));

	write(sprintf("%16s %16s %14s   %s\n\n", "Time", "Calls", "Average", "Function"));
	foreach (mixed func: funcs)
	    write(sprintf("%16.4f %16.4f %14s : %s\n", func[0], func[1],
		       (func[2] > 0.0 ? sprintf("%10.4f", func[2]) : "-"), func[3]));
	break;

    case "props":
    case "properties":
        props = sort_array(ob->query_props());
        for (i = 0; i < sizeof(props); i++)
        {
            write(sprintf(" %-30s : ", props[i]));
            print_value(ob->query_prop_setting(props[i]));
        }
        write("\n");
	break;

    case "shadows":
        while (ob = shadow(ob,0))
            write(file_name(ob) + "\n");
        write("\n");
	break;

    case "vars":
    case "variables":
	data = SECURITY->do_debug("get_variables", ob);
	vars = m_indices(data);

        for (i = 0; i < sizeof(vars); i++)
        {
            write(sprintf(" %-30s : ", vars[i]));
            print_value(data[vars[i]]);
        }
        write("\n");
	break;

    case "wizinfo":
	if (stringp(tmp = ob->query_prop(OBJ_S_WIZINFO)))
	    write(tmp);
	else
	    write("--- no OBJ_S_WIZINFO set ---\n");
	break;

    default:
	data = SECURITY->do_debug("get_variables", ob);
	vars = m_indices(data);
    
	if (member_array(flag, vars) > 0)
	{
	    write(flag + " : ");
	    print_value(data[flag]);
	    write("\n");
	}
	else
	    return notify_fail("Unknown parameter or variable name.\n");

	break;
    }

    return 1;
}

/*
 * Ed - ed a file.
 *
 * Syntax   : Ed [<object>]
 * Arguments: <object> - the object to ed.
 * Default  : 'here'
 */
int
Ed(string str)
{
    object ob;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        str = "here";
        ob = environment(this_interactive());
        write(file_name(ob) + "\n");
    }
    else
    {
	if (!objectp(ob = get_assign(str)))
	    ob = parse_list(str);
    }

    if (!objectp(ob))
    {
        notify_fail("Object '" + str + "' not found.\n");
        return 0;
    }

    ed(MASTER_OB(ob) + ".c");
    return 1;
}

/*
 * Goto - go to another location.
 *
 * Syntax   : Goto <object>
 * Arguments: <object> - the object to go to.
 */
int
Goto(string str)
{
    object mark;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        notify_fail("Goto which object?\n");
        return 0;
    }

    if (!objectp(mark = get_assign(str)))
	mark = parse_list(str);
    if (!objectp(mark))
    {
        notify_fail("Object '" + str+ "' not found.\n");
        return 0;
    }

    if (this_interactive()->move_living("X", mark))
    {
        write("Failed to Goto " + mark->short(this_interactive()) + ".\n");
        write("Maybe you wanted to go to its environment?\n");
    }

    return 1;
}

/*
 * In - perform a command in another location.
 *
 * Syntax   : In <object> <command>
 * Arguments: <object>  - the object to perform the command in.
 *            <command> - the command to perform in the object.
 */
int
In(string str)
{
    string path;
    string cmd;
    object ob;
    object old_ob;

    CHECK_SO_WIZ;

    if (!strlen(str) ||
        (sscanf(str, "%s %s", path, cmd) != 2))
    {
        notify_fail("What do you want to do where?\n");
        return 0;
    }

    if (!objectp(ob = get_assign(path)))
	ob = parse_list(path);
    if (!objectp(ob))
    {
        notify_fail("Object '" + path + "' not found.\n");
        return 0;
    }

    old_ob = environment(this_interactive());

    /* Only bother to command if we succeed to move. */
    if (!(this_interactive()->move(ob, 1)))
    {
        catch(this_interactive()->command(cmd));
        this_interactive()->move(old_ob, 1);
    }
    else
    {
        write("Failed to move you to that location.\n");
    }

    return 1;
}

/*
 * Inventory - list the inventory of an object
 * I (short for Inventory) 
 *
 * Just an undocumented wrapper for Dump, 
 * for old dogs with seating problems.
 *
 */
int
Inventory(string str)
{
    CHECK_SO_WIZ;

    return Dump((strlen(str) ? str : "me") + " inv");
}

/*
 * Items - list the pseudo items of an object
 *
 * Just an undocumented wrapper for Dump, 
 * for old dogs with seating problems.
 *
 */
int
Items(string str)
{
    CHECK_SO_WIZ;

    return Dump((strlen(str) ? str : "here") + " items");
}

/*
 * Light - list the light status of an object
 *
 * Just an undocumented wrapper for Dump, 
 * for old dogs with seating problems.
 *
 */
int
Light(string str)
{
    CHECK_SO_WIZ;

    return Dump((strlen(str) ? str : "here") + " light");
}

static nomask void
object_items(object target)
{
    int     index;
    string *items = ({ });
    string *commands = ({ });
    mixed   tmp;

    write("Items of: " +
        ((stringp(tmp = target->short(this_interactive()))) ? tmp : "---") +
        "\n");

    tmp = target->query_item();
    if (!sizeof(tmp))
    {
        write("--- no items listed ---\n");
    }
    else
    {
        for (index = 0; index < sizeof(tmp); index++)
            items += tmp[index][0];

        write(sprintf("%-*#s\n", 74, implode(sort_array(items), "\n")));
    }

    tmp = target->query_cmd_item();
    if (!sizeof(tmp))
    {
        write("--- no command items listed ---\n");
    }
    else
    {
        items = ({ });

        for(index = 0; index < sizeof(tmp); index++)
        {
            items += tmp[index][0];
            items[sizeof(items) - 1] += " (" + (index + 1) + ")";
            commands += tmp[index][1];
            commands[sizeof(commands) - 1] += " (" + (index + 1) + ")";
        }

        write(sprintf("Commands:\n%-*#s\n", 74, implode(commands, "\n")));
        write(sprintf("Command items:\n%-*#s\n", 74, implode(items, "\n")));
    }
}

/*
 * Function name: light_status
 * Description  : Print the light status of one object and recurse over
 *                its inventory.
 * Arguments    : object target - the object to process.
 *                int    level  - the how deep the object is.
 */
static nomask void
light_status(object target, int level)
{
    object *inv    = all_inventory(target);
    int    c_light = target->query_prop(CONT_I_LIGHT);
    int    o_light = target->query_prop(OBJ_I_LIGHT);
    int    r_light = target->query_prop(ROOM_I_LIGHT);
    int    index   = -1;
    int    size    = sizeof(inv);
    string desc;

    if ((level == 0) || living(target) || size ||
        c_light || o_light || r_light)
    {
        if (!strlen(desc = target->short(this_player())))
        {
            desc = RPATH(file_name(target));
        }

        if (level)
        {
            write(extract(SPACES, 0, ((level * 3) - 1)));
        }

        write(sprintf("%-*s : %3d ", (60 - (level * 3)), desc,
            o_light));

        if (size || c_light || r_light)
        {
            if (c_light && r_light)
            {
                write(sprintf("%4+d %4+d", c_light, r_light));
            }
            else if (c_light)
            {
                write(sprintf("%4+d", c_light));
            }
            else if (r_light)
            {
                write(sprintf("     %4+d", r_light));
            }
            else if (o_light)
            {
                write("(transp.)");
            }
        }

        write("\n");
    }

    while(++index < size)
    {
        light_status(inv[index], (level + 1));
    }
}

/*
 * More - more a file.
 *
 * Syntax   : More [<object>]
 * Arguments: <object> - the object to display with more.
 * Default  : 'here'
 */
int
More(string str)
{
    object ob;
    string text;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        str = "here";
        ob = environment(this_interactive());
        write(file_name(ob) + "\n");
    }
    else
    {
	if (!objectp(ob = get_assign(str)))
	    ob = parse_list(str);
    }

    if (!objectp(ob))
    {
        notify_fail("Object '" + str + "' not found.\n");
        return 0;
    }

    str = MASTER_OB(ob) + ".c";
    if (!(SECURITY->valid_read(str, geteuid(), 0)))
    {
        notify_fail("No read access to: " + str + "\n");
        return 0;
    }

    this_player()->more(str, 1);
    return 1;
}

/*
 * Move - move an object somewhere.
 *
 * Syntax: Move [-f] <object> [<destination>]
 * Arguments: -f            - move with force.
 *            <object>      - the object to move.
 *            <destination> - the destination to move the object to.
 * Default  : 'here' (destination)
 */
int
Move(string str)
{
    object ob;
    object to;
    string str_ob;
    string str_to;
    int    force = 0;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        notify_fail("Syntax: Move [-f] <object> [<destination>]\n");
        return 0;
    }

    if (force = wildmatch("-f *", str))
    {
        str = extract(str, 3);
    }

    if (sscanf(str, "%s %s", str_ob, str_to) == 2)
    {
	if (!objectp(to = get_assign(str_to)))
	    to = parse_list(str_to);
    }
    else
    {
        str_ob = str;
        str_to = "here";
        to = environment(this_player());

        if (objectp(to))
        {
            write(file_name(to) + "\n");
        }
    }

    if (!objectp(to))
    {
        notify_fail("Destination '" + str_to + "' not found.\n");
        return 0;
    }

    ob = parse_list(str_ob);
    if (!objectp(ob))
    {
        notify_fail("Object '" + str_ob + "' not found.\n");
        return 0;
    }

    if (force)
    {
        if (force = ob->move(to, 1))
        {
            write("Error '" + force + "' moving " + ob->short() + " to " +
                to->short() + " with force.\n");
        }
        else
        {
            write("Forcefully moved " + ob->short() + " to " + to->short() +
                ".\n");
        }

        return 1;
    }

    if (living(ob))
    {
        if (force = ob->move_living("X", to, 1))
        {
            write("Error '" + force + "' moving " + ob->short() + " to " +
                to->short() + " as living.\n");
        }
        else
        {
            write("Moved " + ob->short() + " to " + to->short() +
                " as living.\n");
            return 1;
        }
    }

    if (force = ob->move(to, 0))
    {
        write("Error '" + force + "' moving " + ob->short() + " to " +
            to->short() + ".\n");
    }
    else
    {
        write("Moved " + ob->short() + " to " + to->short() + ".\n");
    }

    return 1;
}

/*
 * Reload - Reload an object
 *
 * Syntax   : Reload <object>
 * Arguments: <object> - The object you wish to reload.
 */
int
Reload(string str)
{
    object ob, old, dest;
    string file;
    function m;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        return notify_fail("Reload what?\n");
    }

    if (!objectp(old = get_assign(str)))
    {
	if (!objectp(old = parse_list(str)))
	{
	    return notify_fail("Object '" + str + "' not found.\n");
	}
    }

    if (interactive(old))
    {
        write("This command my not be used on players.\n");
        return 1;
    }

    file = MASTER_OB(old);
    if (!this_interactive()->command("$load " + file))
    {
        write("Failed to update, aborting...\n");
        return 1;
    }

    if (!objectp(ob = clone_object(file)))
    {
        write("Failed to clone new instance, aborting...\n");
        return 1;
    }
    else
    {
        write("New instance cloned, attempting to move.\n");
    }

    if (!objectp(dest = environment(old)))
	dest = environment(this_interactive());

    if (living(ob))
        m = &ob->move_living("M",,1,1);
    else
        m = &ob->move(,0);

    if (m(dest))
    {
        write("Error with move, attempting to force... ");

        if (ob->move(dest, 1))
        {
            write("Failed, aborting.\n");
            ob->remove_object();
            if (objectp(ob))
                SECURITY->do_debug("destroy", ob);
        }
        else
        {
            write("Success, continuing.\n");
        }
    }
    else
    {
        write("Object inserted, removing old instance.\n");
        old->remove_object();
        if (objectp(old))
            SECURITY->do_debug("destroy", old);
    }
    write("Reloading complete.\n");
    return 1;
}

/*
 * Set - set a variable to an object.
 *
 * Syntax   : Set <variable> <object>
 * Arguments: <variable> - the name of the variable.
 *            <object>   - the object to assign.
 */
int
Set(string str)
{
    object ob;
    string item, var;
    int i, j;
    mixed *vars, *stores;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
	vars = this_interactive()->query_prop(TRACER_VARS);
	stores = this_interactive()->query_prop(TRACER_STORES);

        write("All variables:\n");
        while (i < sizeof(vars))
        {
            if (vars[i])
            {
		if ((objectp(stores[i]) || pointerp(stores[i])) &&
		    vars[i] != "ret")
		{
		    write(vars[i] + ":\t");
		    if (objectp(stores[i]))
			write(file_name(stores[i]));
		    else if (pointerp(stores[i]))
			write("({ " + implode(stores[i], ", ") + " })");
		    write("\n");
		}
            }
            i += 1;
        }
        return 1;
    }

    if (sscanf(str, "%s %s", var, item) != 2 ||
	var[0] != '$')
    {
        notify_fail("Set variable to which object?\n");
        return 0;
    }

    ob = parse_list(item);
    if (!objectp(ob))
    {
        notify_fail("Object '" + item + "' not found.\n");
        return 0;
    }

    assign(var, ob);
    return 1;
}

/*
 * Tail - tail a file.
 *
 * Syntax   : Tail [<object>]
 * Arguments: <object> - the object to tail.
 * Default  : 'here'
 */
int
Tail(string str)
{
    object ob;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
        str = "here";
        ob = environment(this_interactive());
        write(file_name(ob) + "\n");
    }
    else
    {
	if (!objectp(ob = get_assign(str)))
	    ob = parse_list(str);
    }

    if (!objectp(ob))
    {
        notify_fail("Object '" + str + "' not found.\n");
        return 0;
    }

    str = MASTER_OB(ob) + ".c";
    if (!tail(str))
    {
        notify_fail("No read access to: " + str + "\n");
        return 0;
    }

    return 1;
}

/*
 * Top - show top ten cpu
 *
 * Syntax: Top <average> <opt num>
 */
static string *units = ({ "Y", "Z", "E", "P", "T", "G", "M", "k", "", "m", "u", "n", "p", "f", "a", "z", "y" });
static float *unit_factor = ({ 1.0e-24, 1.0e-21, 1.0e-18, 1.0e-15, 1.0e-12, 1.0e-9, 1.0e-6, 1.0e-3, 1.0,
                               1.0e3, 1.0e6, 1.0e9, 1.0e12, 1.0e15, 1.0e18, 1.0e21, 1.0e24 });

int
Top(string arg)
{
    int show = 10, criteria = -1, c;
    int total, latest, per_call, calls;
    if (arg)
        foreach (string param : explode(arg, " ")) {
            int paramlen = strlen(param) - 1;
            if (paramlen < 0)
                continue;
            if (param == "cumulative"[..paramlen])
                total = 1;
            else if (param == "intrinsic"[..paramlen])
                total = 0;
            else if (param == "latest"[..paramlen])
                latest = 1;
            else if (param == "absolute"[..paramlen])
                latest = 0;
            else if (param == "percall"[..paramlen]) {
                per_call = 1;
                calls = 0;
            } else if (param == "numcalls"[..paramlen]) {
                calls = 1;
                per_call = 0;
            } else if (param == "time"[..paramlen])
                calls = per_call = 0;
            else if (sscanf(param, "C%i", c) >= 1 && c >= 0 && c <= 9) {
                latest = total = per_call = calls = 0;
                if (c % 2)
                    latest = 1;
                c = c / 2;
                if (c == 2)
                    calls = 1;
                 else {
                     if (c >= 3) {
                         per_call = 1;
                         c -= 3;
                     }
                     if (c % 2)
                         total = 1;
                 }
            } else if (sscanf(param, "%i", show) < 1) {
                if (param != "?" && param != "help")
                    write(sprintf("Unknown parameter %O to top.\n", param));
                write("Allowed parameters are 'intrinsic', 'cumulative', 'absolute', 'latest', 'time', 'numcalls', 'percall' and <number>.\n");
                write("What is sorted on is controlled by:\n");
                write("        time: sorts on time spent in the function.\n");
                write("    numcalls: sorts on number of calls.\n");
                write("     percall: sorts on average time spent in function per call.\n");
                write("What is measured as time spent in the function is controlled by:\n");
                write("   intrinsic: measure the time spent in the function itself.\n");
                write("  cumulative: measure the time spent in the function and in functions it calls.\n");
                write("    absolute: measure the time spent since the object was loaded.\n");
                write("      latest: measure the time spent using an exponential average.\n");
                write("How many functions to show is controled by <number>.\n");
                write("\nThe parameters can be abriviated to one character.\nThe default parameters are 'time instrinsic absolute 10'.\n");
                return 1;
            }
        }

    if (calls)
        criteria = 4 + latest; 
    else 
        criteria = 6 * per_call + 2 * total + latest;

    show = min(max(0, show), 1000);
  
    if (show) {
    mixed *avg = SECURITY->do_debug("top_functions", show, criteria);
    if (!pointerp(avg))
        return 0;
    write("\n");
    if (latest) {
        float timebase = SECURITY->do_debug("profile_timebase");
        if (intp(timebase))
            timebase = -1.0;
        write(sprintf("%8.3f seconds average\n", timebase));
    }
    int time_offset = 3 + 2 * total + latest;
    int call_offset = 7 + latest;
    float max_percall = -1.0;
    float max_time = -1.0;
    float max_calls = -1.0;
    foreach (mixed *func : avg) {
        float calls = func[call_offset];
        if (calls > max_calls)
            max_calls = calls;
        float time = func[time_offset];
        if (time > max_time)
            max_time = time;
        if (calls > 1.0e-30) {
            float percall = time / calls;
            if (percall > max_percall)
                max_percall = percall;
        }
    }
    string timeunit, callunit, percallunit;
    float timefactor, callfactor, percallfactor;

    int factor = 0, maxfactor = sizeof(units) - 1;
    while (max_time  * unit_factor[factor] < 100.0 && factor < maxfactor)
        factor++;
    timefactor = unit_factor[factor];
    timeunit = sprintf("%ss%s", units[factor], latest ? "/s" : "");

    factor = 0;
    if (latest)
        while (max_calls  * unit_factor[factor] < 100.0 && factor < maxfactor)
            factor++;
    else
        while (max_calls  * unit_factor[factor] < 100.0 && unit_factor[factor] < 1.0)
            factor++;
    callfactor = unit_factor[factor];
    callunit = sprintf("%scalls%s", units[factor], latest ? "/s" : "");

    factor = 0;
    while (max_percall * unit_factor[factor] < 100.0 && factor < maxfactor)
        factor++;
    percallfactor = unit_factor[factor];
    percallunit = sprintf("%ss/call", units[factor]);

    write(sprintf("    %10s   %10s   %10s\n", timeunit, callunit, percallunit));
    int i = 1;
    foreach (mixed *func : avg) {
        float time = func[time_offset];
        float num_calls = func[call_offset];

        string time_per_call;

        if (num_calls > 1.0e-30)
            time_per_call =  sprintf("%10.4f", time / num_calls * percallfactor);
        else
            time_per_call = "-";
        if (!latest && callfactor == 1.0)
            write(sprintf("%3d %10.4f / %10i = %10s : %s() in %s\n", i, time * timefactor,
                          ftoi(num_calls), time_per_call, func[1], func[0]));
        else if (!latest && callfactor > 1.0e-4)
            write(sprintf("%3d %10.4f / %10.3f = %10s : %s() in %s\n", i, time * timefactor,
                          num_calls * callfactor, time_per_call, func[1], func[0]));
        else
            write(sprintf("%3d %10.4f / %10.4f = %10s : %s() in %s\n", i, time * timefactor,
                          num_calls * callfactor, time_per_call, func[1], func[0]));
        i++;
    }
    }
    write("\n");
    foreach (string line : explode(SECURITY->do_debug("load_average"), ", ")) {
         string *a = explode(line, " ");
         string one, five, fifteen;
         string name = implode(a[1..], " ");
         sscanf(a[0], "%f/%f/%f", one, five, fifteen);
         write(sprintf("%9.2f %9.2f %9.2f %s\n", 
               one, five, fifteen, name));
    }
    return 1;
}

static int
profile_sort(string item, mixed a, mixed b)
{
    int p = member_array(item[0..0], ({ "t", "c", "a", "f" }));

    if(a[p] < b[p])
	return -1;
    
    if (a[p] > b[p])
	return 1;

    return 0;
}
