/*
 * /cmd/wiz/normal/files.c
 *
 * This is a subpart of /cmd/wiz/normal.c
 * 
 * The commands in this sub-part all have to do with the manipulation of
 * files and objects.
 *
 * Commands in this file:
 * - aft
 * - clone
 * - cp
 * - destruct
 * - distrust
 * - ed
 * - load
 * - mkdir
 * - mv
 * - remake
 * - rm
 * - rmdir
 * - trust
 * - update
 */

#include <cmdparse.h>
#include <filepath.h>
#include <language.h>
#include <options.h>

/*
 * Global variable.
 */
static private object  loadmany_wizard;
static private string *loadmany_files;
static private string *loadmany_going = ({ });

static private mapping aft_tracked;
static private mapping aft_current = ([ ]);
static private string *aft_sorted = ({ });

/*
 * Prototypes.
 */
nomask int update_ob(string str);

/*
 * Maxinum number of files that can be copied/moved/removed with one command
 * Protects from errors like 'cp /std/* .'
 */
#define MAXFILES 20

/*
 * Function name: copy
 * Description  : Copy a (text) file. Limited to about 50kB files by the GD.
 *                It could be circumvented but very few files are larger
 *                than that. Maybe I should make this into an simul-efun?
 * Arguments    : string path1 - source path.
 *                string path2 - destination path (including filename).
 * Returns      : int 1/0 - success/failure.
 */
private nomask int
copy(string path1, string path2)
{
    string buf;

    /* Read the source file and test for success. */
    buf = read_file(path1);
    if (!strlen(buf))
    {
	return 0;
    }

    switch(file_size(path2))
    {
	/* Target is a directory. Impossible. */
	case -2:
	    return 0;

	/* Target file does not exist or is empty. Proceed. */
	case -1:
	case  0:
	    break;

	/* Existing target file. Try to remove it first. */
	default:
	    if (!rm(path2))
	    {
		return 0;
	    }
    }

    /* Write the buffer and return the return value of the efun. */
    return write_file(path2, buf);
}

#define MULTI_CP 0
#define MULTI_MV 1
#define MULTI_RM 2
#define MULTI_OPERATION ({ "Copied", "Moved", "Removed" })

/*
 * Function name: multi_file
 * Description  :
 * Arguments    : string str - the command line argument.
 *                int operation - the operation type.
 * Returns      : int 1/0 - success/failure.
 */
private nomask int
multi_file(string str, int operation)
{
    string *files, *parts, *source, *cont, target;
    int    index, size, done;
    
    CHECK_SO_WIZ;
    
    if (!stringp(str))
    {
	notify_fail("No argument given to " + query_verb() + ".\n");
	return 0;
    }

    /* Explode the argument and solve the tilde-path notation. */
    files = explode(str, " ") - ({ "" });
    str = this_player()->query_path();
    index = -1;
    size = sizeof(files);
    while(++index < size)
    {
	files[index] = FTPATH(str, files[index]);
    }

    /* For cp and mv we need at least two arguments, mark the target. */
    if (operation != MULTI_RM)
    {
	if (size == 1)
	{
	    notify_fail("Syntax: " + query_verb() + " <source> <target>\n");
	    return 0;
	}

	target = files[--size];
	files -= ({ target });
    }

    /* Expand the wildcards. */
    index = -1;
    source = ({ });
    while(++index < size)
    {
	parts = explode(files[index], "/");
	str = implode(parts[..(sizeof(parts) - 2)], "/") + "/";

	if (pointerp((cont = get_dir(files[index]))))
	    source += map(cont - ({ ".", ".." }), &operator(+)(str));
	else
	    return notify_fail(query_verb() + ": " + files[index] +
			       ": No such file or directory.\n");
    }

    /* If too many files are selected, we still process the first batch. */
    if ((size = sizeof(source)) == 0)
	return notify_fail(query_verb() + ": " + files[0] +
			   ": No such file or directory.\n");
    
    if (size > MAXFILES)
    {
	write("Selected " + size + " files. Only the first " + MAXFILES +
	      " will be processed.\n");
	source = source[..(MAXFILES - 1)];
	size = sizeof(source);
    }

    if (size > 1)
    {
	if ((operation != MULTI_RM) &&
	    (file_size(target) != -2))
	{
	    notify_fail("When selecting multiple files, the destination " +
			"must be a directory.\n");
	    return 0;
	}

	write("Selected " + size + " files.\n");
    }

    index = -1;
    while(++index < size)
    {
	switch(operation)
	{
	case MULTI_CP:
	    if (file_size(source[index]) == -2)
	    {
		write("Directory " + source[index] + " cannot be copied.\n");
		break;
	    }

	    str = target;
	    if (file_size(target) == -2)
	    {
		parts = explode(source[index], "/");
		str += "/" + parts[sizeof(parts) - 1];
	    }

	    if (copy(source[index], str))
	    {
		done++;
	    }
	    else
	    {
		write("Failed at " + source[index] + "\n");
	    }
	    break;

	case MULTI_MV:
	    if ((file_size(source[index]) == -2) &&
		(size > 1))
	    {
		write("When moving directory " + source[index] +
		      " the directory must be the only argument.\n");
		break;
	    }

	    str = target;
	    if (file_size(target) == -2)
	    {
		parts = explode(source[index], "/");
		str += "/" + parts[sizeof(parts) - 1];
	    }
	    
	    if (rename(source[index], str))
	    {
                SECURITY->remove_binary(str);
		done++;
	    }
	    else
	    {
		write("Failed at " + source[index] + "\n");
	    }
	    break;

	case MULTI_RM:
	    if (file_size(source[index]) == -2)
	    {
		write("Directory " + source[index] +
		      " cannot be removed using rm.\n");
		break;
	    }

	    if (rm(source[index]))
	    {
		done++;
	    }
	    else
	    {
		write("Failed at " + source[index] + "\n");
	    }
	    break;
	}
    }

    write(MULTI_OPERATION[operation] + " " + done + " file" +
	  ((done != 1) ? "s" : "") + ".\n");

    return 1;
}

/* **************************************************************************
 * aft - [Avernir's] file tracker
 */

/*
 * AFT uses disk-caching and the files are stored in the wizards personal
 * directory. The data type of the save-file is as follows. The information
 * of this file is stored in aft_tracked.
 *
 * ([
 *    (string) path : ({
 *                       (string) private name,
 *                       (int) last modification time,
 *                    }),
 * ])
 *
 * The current file (last tracked) for each wizard is stored in aft_current,
 * with the following data type. This structure is not saved.
 *
 * ([
 *    (string) name : (int) index of last tracked file,
 * ])
 */

#define AFT_PRIVATE_NAME (0)
#define AFT_FILE_TIME    (1)
#define AFT_FILE         ("/.aft")

/*
 * Function name: read_aft_file
 * Description  : This function reads the aft-file of a particular wizard.
 *                If such a file does not exist, the mapping is cleared.
 */
private void
read_aft_file()
{
    string name = this_interactive()->query_real_name();

    aft_tracked = read_cache(SECURITY->query_wiz_path(name) + AFT_FILE);

    /* No such file, set to defaults. */
    if (!m_sizeof(aft_tracked))
    {
	aft_tracked = ([ ]);
	aft_sorted = ({ });
    }
    else
    /* Sort the names in the mapping. */
    {
	aft_sorted = sort_array(m_indices(aft_tracked));
    }
}

/*
 * Function name: save_aft_file
 * Description  : This function saves the aft-file of a particular wizard.
 *                When empty, the save-file is deleted.
 */
private void
save_aft_file()
{
    /* No file being tracked, remove the file completely. */
    if (!m_sizeof(aft_tracked))
    {
	/* We do not need to appent the .o as that is done in rm_cache(). */
	rm_cache(SECURITY->query_wiz_path(
	    this_interactive()->query_real_name()) + AFT_FILE);
	return;
    }

    save_cache(aft_tracked, (SECURITY->query_wiz_path(
	this_interactive()->query_real_name()) + AFT_FILE));
}

/*
 * Function name: aft_find_file
 * Description  : This function will return the full path of the file the
 *                wizard wants to handle. It accepts the path (including
 *                tilde, the private name of the file and the number in the
 *                list of files.
 * Arguments    : string file - the file to find.
 * Returns      : string - path of the file found.
 */
private string
aft_find_file(string file)
{
    int index;
    mapping tmp;

    /* May be the index number in the list of files. */
    if (index = atoi(file))
    {
	if ((index > 0) &&
	    (index <= sizeof(aft_sorted)))
	{
	    return aft_sorted[index - 1];
	}

	/* No such index. Maybe it is a name, who knows ;-) */
    }

    /* May be a private name the wizard assigned to the file. This filter
     * statement will return a mapping with only one element if the private
     * name was indeed found.
     */
    tmp = filter(aft_tracked,
	&operator(==)(file, ) @ &operator([])(, AFT_PRIVATE_NAME));
    if (m_sizeof(tmp))
    {
	return m_indices(tmp)[0];
    }

    /* Could be the path itself. */
    file = FTPATH(this_interactive()->query_path(), file);
    if (pointerp(aft_tracked[file]))
    {
	return file;
    }

    /* File not found. */
    return 0;
}

/*
 * Function name: aft_catchup_file
 * Description  : Map-function called to update the time a (log) file was
 *                last accessed. For convenience we use time() here and not
 *                the actual file time. This is good enough anyway, because
 *                if the file is going to be changed, it is going to be
 *                changed after the current time().
 * Arguments    : mixed data - the array-value for a particular file-name.
 * Returns      : mixed - the same array with the time adjusted.
 */
private mixed
aft_catchup_file(mixed data)
{
    data[AFT_FILE_TIME] = time();

    return data;
}

public int
aft(string str)
{
    string *args;
    int    size;
    int    index = -1;
    int    flag = 0;
    int    changed;
    string name = this_interactive()->query_real_name();
    string *files;
    mapping tmp;

    CHECK_SO_WIZ;

    /* Set to default when there is no argument. */
    if (!strlen(str))
    {
	str = "lu";
    }

    args = explode(str, " ");
    size = sizeof(args);

    /* Read the wizards aft-file. */
    read_aft_file();

    /* Wizard is not tracking any files and does not want to select a file
     * to track either.
     */
    if (!m_sizeof(aft_tracked) &&
	args[0] != "s")
    {
	write("You are not tracking any files.\n");
	return 1;
    }

    switch(args[0])
    {
    case "c":
	if (size != 2)
	{
	    notify_fail("Syntax: aft c <file>\n");
	    return 0;
	}

	str = aft_find_file(args[1]);
	if (!stringp(str))
	{
	    notify_fail("You are not tracking a file \"" + args[1] + "\".\n");
	    return 0;
	}

	/* Mark the file as being up to date and make it current. */
	aft_tracked[str] = aft_catchup_file(aft_tracked[str]);
	aft_current[name] = member_array(str, aft_sorted);
	save_aft_file();

	write("Caught up on " + str + ".\n");
	return 1;

    case "C":
	if (size != 1)
	{
	    notify_fail("Syntax: aft C\n");
	    return 0;
	}

	/* Mark all files as being up to date. */
	aft_tracked = map(aft_tracked, aft_catchup_file);
	save_aft_file();

	write("Caught up on all files.\n");
	return 1;

    case "l":
	flag = 1;
	/* Continue at "lu". */

    case "lu":
	if (size != 1)
	{
	    notify_fail("Syntax: aft " + args[0] + "\n");
	    return 0;
	}

	/* Loop over all files being tracked. */
	size = sizeof(aft_sorted);
	while(++index < size)
	{
	    changed = (file_time(aft_sorted[index]) >
		aft_tracked[aft_sorted[index]][AFT_FILE_TIME]);
	    /* Only print if the file actually changed, or if the wizard
	     * signalled that he wanted all files.
	     */
	    if (flag || changed)
	    {
		write(sprintf("%2d %-8s%-1s %-1s %-50s %8d\n",
		    (index + 1),
		    aft_tracked[aft_sorted[index]][AFT_PRIVATE_NAME],
		    ((index == aft_current[name]) ? ">" : ":"),
		    (changed ? "*" : " "),
		    aft_sorted[index],
		    file_size(aft_sorted[index])));

		args[0] = "oke";
	    }
	}

	/* No output of any files. Give him a "fail" message. */
	if (args[0] != "oke")
	{
	    write("No changes in any of the tracked files.\n");
	}

	return 1;

    case "r":
	flag = 1;
	/* Continue at "rr". */

    case "rr":
	switch(size)
	{
	case 1:
	    /* user wants to see next changed file. Loop over all files,
	     * starting at the current file.
	     */
	    index = aft_current[name] - 1;
	    size = sizeof(aft_sorted);
	    while(++index < (size + aft_current[name]))
	    {
		/* If there is a change, break. */
		if (file_time(aft_sorted[index % size]) >
		    aft_tracked[aft_sorted[index % size]][AFT_FILE_TIME])
		{
		    index %= size;
		    args[0] = "oke";
		    break;
		}
	    }

	    /* No change to any files. Give him a "fail" message. */
	    if (args[0] != "oke")
	    {
		write("No changes in any of the tracked files.\n");
		return 1;
	    }

	    /* Add the found file to the list of arguments. */
	    args += ({ aft_sorted[index] });
	    break;

	case 2:
	    /* user specified a file to read. */
	    str = aft_find_file(args[1]);
	    if (!stringp(str))
	    {
		notify_fail("You are not tracking a file \"" + args[1] +
		    "\".\n");
		return 0;
	    }

	    args[1] = str;
	    break;

	default:
	    notify_fail("Syntax: aft " + args[0] + " [<file>]\n");
	    return 0;
	}

	/* Mark as read and file to current. Then save. */
	aft_current[name] = member_array(args[1], aft_sorted);
	aft_tracked[args[1]] = aft_catchup_file(aft_tracked[args[1]]);
	save_aft_file();

	write("AFT on " + args[1] + "\n");
	/* Force the wizard to use the tail command. We can force since we
	 * have his/her euid.
	 */
	return this_interactive()->command("tail " + (flag ? "" : "-r ") +
	    args[1]);
	/* not reached */

    case "s":
	switch(size)
	{
	case 2:
	    /* User does not want a private name. */
	    args += ({ "" });
	    break;

	case 3:
	    /* Specified a private name. See whether it is not a duplicate. */
	    tmp = filter(aft_tracked,
		&operator(==)(args[2], ) @ &operator([])(, AFT_PRIVATE_NAME));
	    if (m_sizeof(tmp))
	    {
		notify_fail("Name \"" + args[2] + "\" already used for " +
		    m_indices(tmp)[0] + ".\n");
		return 0;
	    }

	    break;

	default:
	    notify_fail("Syntax: aft s <path> [<name>]\n");
	    return 0;
	}

        args[1] = FTPATH(this_interactive()->query_path(), args[1]);
	if (aft_tracked[args[1]])
	{
	    notify_fail("You are already tracking " + args[1] + ".\n");
	    return 0;
	}

	if (file_size(args[1]) < 0)
	{
	    notify_fail("There is no file " + args[1] + ".\n");
	    return 0;
	}

	/* Add the file, and mark as unread. Then save. */
	aft_tracked[args[1]] = ({ args[2], 0 });
	aft_sorted = sort_array(m_indices(aft_tracked));
	save_aft_file();

	write("Started tracking on " + args[1] + ".\n");
	return 1;

    case "u":
	if (size != 2)
	{
	    notify_fail("Syntax: aft u <file>\n");
	    return 0;
	}

	str = aft_find_file(args[1]);
	if (!stringp(str))
	{
	    notify_fail("You are not tracking a file \"" + args[1] + "\".\n");
	    return 0;
	}

	if (member_array(str, aft_sorted) >= aft_current[name])
	{
	    aft_current[name] -= 1;
	}

	aft_tracked = m_delete(aft_tracked, str);
	aft_sorted -= ({ str });
	save_aft_file();

	write("Unselected file " + str + ".\n");
	return 1;

    case "U":
	aft_tracked = ([ ]);
        aft_current = m_delete(aft_current,
	    this_interactive()->query_real_name());
	aft_sorted = ({ });
	save_aft_file();

	write("Unselected all files. Stopped all tracking.\n");
	return 1;

    default:
	notify_fail("No subcommand \"" + args[0] + "\" to aft.\n");
	return 0;
    }

    write("Impossible end of aft switch. Please report to an archwizard!\n");
    return 1;
}

/* **************************************************************************
 * clone - clone an object
 */

/*
 * Function name: clone_message
 * Description  : This function returns the proper message to be displayed
 *                to people watching the cloning. Wizards get a message,
 *                mortal players see 'something'.
 * Arguments    : mixed cloned - the file_name with object number of the
 *                               object cloned.
 * Returns      : string - the description.
 */
public nomask string
clone_message(mixed cloned)
{
    object proj = previous_object(-1);

    if (!(proj->query_wiz_level()))
        return "something";

    if ((!stringp(cloned)) || (!strlen(cloned)))
        return "something";
    if (!objectp(cloned = find_object(cloned)))
        return "something";

    if (living(cloned))
        return (string)cloned->query_art_name(cloned);

    return (strlen(cloned->short(cloned)) ?
        LANG_ASHORT(cloned) : file_name(cloned));
}

/*
 * Function name: clone_ob
 * Description  : This function actually clones the object a wizard wants
 *                to clone.
 * Arguments    : string what - the filename of the object to clone.
 * Returns      : object - the object cloned.
 */
static nomask object
clone_ob(string what)
{
    string str, mess;
    object ob;

    str = FTPATH((string)this_interactive()->query_path(), what);
    if (!strlen(str))
    {
	notify_fail("Invalid file.\n");
	return 0;
    }

    if (file_size(str + ".c") < 0 && file_size(str) < 0)
    {
	notify_fail("No such file.\n");
	return 0;
    }
    
    ob = clone_object(str);
    if (!ob)
    {
	notify_fail("You can not clone: " + str + "\n");
	return 0;
    }
    say(QCTNAME(this_interactive()) + " fetches @@clone_message:" +
	file_name(this_object()) + "|" + file_name(ob) +
        "@@ from another dimension.\n");
    return ob;
}

nomask int
clone(string str)
{
    object ob;
    int num, argc;
    string *argv;
    string desc;

    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Clone what object ?\n");
	return 0;
    }

    argv = explode(str, " ");
    argc = sizeof(argc);

    switch (argv[0])
    {
    case "-i":
	ob = clone_ob(argv[1]);
	if (!ob)
	    return 0;
	ob->move(this_interactive(), 1);
	if (this_interactive()->query_option(OPT_ECHO))
	{
	    desc = (living(ob) ? ob->query_art_name(this_interactive()) : LANG_ASHORT(ob));
	    write("You clone " + desc + " into your inventory.\n");
	}
	else
	{
	    write("Ok.\n");
	}
	break;

    case "-e":
	ob = clone_ob(argv[1]);
	if (!ob)
	    return 0;
	ob->move(environment(this_interactive()), 1);
        /* We need to do this instead of write() because cloning a living
         * will alter this_player().
         */
	if (this_interactive()->query_option(OPT_ECHO))
	{
	    desc = (living(ob) ? ob->query_art_name(this_interactive()) : LANG_ASHORT(ob));
	    this_interactive()->catch_tell("You clone " + desc +
	        " into your environment.\n");
	}
	else
	{
	    this_interactive()->catch_tell("Ok.\n");
	}
	break;

    default:
	ob = clone_ob(argv[0]);
	if (!ob)
	    return 0;

	num = (int)ob->move(this_interactive());
	switch (num)
	{
	case 0:
	    if (this_interactive()->query_option(OPT_ECHO))
	    {
	        desc = (living(ob) ? ob->query_art_name(this_interactive()) : LANG_ASHORT(ob));
	        write("You clone " + desc + " into your inventory.\n");
	    }
	    else
	    {
	        write("Ok.\n");
	    }
	    break;
	    
	case 1:
	    write("Too heavy for destination.\n");
	    break;
	    
	case 2:
	    write("Can't be dropped.\n");
	    break;
	    
	case 3:
	    write("Can't take it out of it's container.\n");
	    break;
	    
	case 4:
	    write("The object can't be inserted into bags etc.\n");
	    break;
	    
	case 5:
	    write("The destination doesn't allow insertions of objects.\n");
	    break;
	    
	case 6:
	    write("The object can't be picked up.\n");
	    break;
	    
	case 7:
	    write("Other (Error message printed inside move() function).\n");
	    break;
	    
	case 8:
	    write("Too big volume for destination.\n");
	    break;
	    
	default:
	    write("Strange, very strange error in move: " + num + "\n");
	    break;
	}
	if (num)
	{
	    num = (int)ob->move(environment(this_interactive()));
	    if (this_interactive()->query_option(OPT_ECHO))
	    {
	        desc = (living(ob) ? ob->query_art_name(this_interactive()) : LANG_ASHORT(ob));
	        write("You clone " + desc + " into your environment.\n");
	    }
	    else
	    {
	        write("Ok.\n");
	    }
	}
	break;
    }
    return 1;
}

/* **************************************************************************
 * cp - copy multiple files
 */
nomask int
cp_cmd(string str)
{
    return multi_file(str, MULTI_CP);
}

/* **************************************************************************
 * destruct - destruct an object
 */
nomask int
destruct_ob(string str)
{
    object *oblist;
    int    dflag;

    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Destruct what?\n");
	return 0;
    }

    if (sscanf(str, "-D %s", str) == 1)
    {
	dflag = 1;
    }

    if (!parse_command(str, environment(this_interactive()), "[the] %i",
	oblist))
    {
	notify_fail("Destruct what?\n");
	return 0;
    }

    oblist = NORMAL_ACCESS(oblist, 0, 0);
    if (!sizeof(oblist))
    {
	notify_fail("Destruct what?\n");
	return 0;
    }

    if (sizeof(oblist) > 1)
    {
	notify_fail("You can destruct only one object at a time.\n");
	return 0;
    }

    if (living(oblist[0]))
    {
	say(QCTNAME(oblist[0]) + " is disintegrated by " +
	    QTNAME(this_interactive()) + ".\n");
	if (this_player()->query_option(OPT_ECHO))
	    write("Destructed " + oblist[0]->query_the_name(this_player()) +
		  " (" + RPATH(MASTER_OB(oblist[0])) + ").\n");
	else
	    write("Ok.\n");
    }
    else
    {
	say(QCTNAME(this_interactive()) + " disintegrates " +
	    LANG_ASHORT(oblist[0]) + ".\n");
	if (this_player()->query_option(OPT_ECHO))
	    write("Destructed " + LANG_THESHORT(oblist[0]) + " (" +
		  RPATH(MASTER_OB(oblist[0])) + ").\n");
	else
	    write("Ok.\n");
    }

    if (dflag)
    {
	SECURITY->do_debug("destroy", oblist[0]);
    }
    else
    {
	oblist[0]->remove_object();
    }

    return 1;
}

/* **************************************************************************
 * distrust - distrust an object
 */
nomask int
distrust(string str)
{
    object ob;

    CHECK_SO_WIZ;

    if (!str)
    {
	notify_fail("Distrust what object?\n");
	return 0;
    }

    ob = present(str, this_interactive());

    if (!ob)
	ob = present(str, environment(this_interactive()));

    if (!ob)
	ob = parse_list(str);

    if (!ob) 
    {
	notify_fail("Object not found: " + str + "\n");
	return 0;
    }

    if (geteuid(ob) != geteuid(this_object()))
    {
	notify_fail("Object not trusted by you.\n");
	return 0;
    }

    /* Remove the previous euid */
    ob->set_trusted(0);

    write("Ok.\n");
    return 1;
}

/* **************************************************************************
 * du - calculate disk usage
 */
static nomask int xdu(string p, int af);

nomask int
du(string str)
{
    int aflag;
    string p, flag, path;
    
    if (!str)
    {
	path = ".";
    }
    else
    {
	/* There is no getopt in CDlib... or? */
	if (sscanf(str, "%s %s", flag, path) == 2)
	{
	    if (flag != "-a")
	    {
		notify_fail("usage: du [-a] [path]\n");
		return 0;
	    }
	    else
		aflag = 1;
	}
	else
	{
	    if (str == "-a")
	    {
		aflag = 1;
		path = ".";
	    }
	    else
		path = str;
	}
    }
    p = FTPATH(this_interactive()->query_path(), path);
    
    if (p == "/")
	p = "";
    
    xdu(p, aflag);

	return 1;
}

static nomask int
xdu(string path, int aflag)
{
    int sum, i;
    string *files, output;
    
    files = get_dir(path + "/*");
    
    sum = 0;
    
    for (i = 0; i < sizeof(files); i++)
    {
	if (files[i] == "." || files[i] == "..")
	    continue;
	
	if (aflag && file_size(path + "/" + files[i]) > -1)
	{
	    write(file_size(path + "/" + files[i]) / 1024 + "\t" +
		  path + "/" + files[i] + "\n");
	}

	if (file_size(path + "/" + files[i]) == -2)
	    sum += xdu(path + "/" + files[i], aflag);
	else
	    sum += file_size(path + "/" + files[i]);
    }
    
    write(sum / 1024 + "\t" + path + "\n");
    return sum;
}

/* **************************************************************************
 * ed - edit a file
 */
nomask int 
ed_file(string file)
{
    CHECK_SO_WIZ;

    if (!stringp(file))
    {
	ed();
	return 1;
    }
    file = FTPATH((string)this_interactive()->query_path(), file);
#ifdef LOG_ED_EDIT
    SECURITY->log_syslog(LOG_ED_EDIT, sprintf("%s %-11s %s\n", ctime(time()),
        capitalize(this_player()->query_real_name()), file));
#endif LOG_ED_EDIT
    ed(file);
    return 1;
}

/* **************************************************************************
 * load - load a file
 */

#define LOADMANY_MAX   (5)
#define LOADMANY_DELAY (10.0)

/*
 * Function name: load_many_delayed
 * Description  : When a wizard wants to test many files in one turn, this
 *                function is called by the alarm to prevent the system
 *                from slowing down too much and to prevent 'evaluation too
 *                long' types of errors.
 * Arguments    : object wizard - the wizard handling the object.
 *                string *files - the files to load still.
 */
static nomask void
load_many_delayed(object wizard, string *files)
{
    loadmany_wizard = wizard;

    if (!objectp(loadmany_wizard) ||
	(member_array(loadmany_wizard->query_real_name(),
    	    loadmany_going) == -1) ||
    	(!interactive(loadmany_wizard)))
    {
    	return;
    }

    loadmany_files = files;
    wizard->reopen_soul();
}

/*
 * Function name: load_many
 * Description  : When a wizard wants to test many files in one turn, this
 *                function tests only a few (LOADMANY_MAX to be exact) and
 *                then calls an alarm to test the other files.
 */
static nomask void
load_many()
{
    int    index = -1;
    int    size;
    object obj;
    string error;

    size = (sizeof(loadmany_files) > LOADMANY_MAX) ? LOADMANY_MAX :
    	sizeof(loadmany_files);
    while(++index < size)
    {
	if (objectp(find_object(loadmany_files[index])) &&
	    loadmany_wizard->query_option(OPT_ECHO))
	{
	    tell_object(loadmany_wizard, "Already loaded:  " +
	    	loadmany_files[index] + "\n");

	    /* Already loaded.. Readjust. */
	    loadmany_files = exclude_array(loadmany_files, index, index);
	    size = (sizeof(loadmany_files) > LOADMANY_MAX) ? LOADMANY_MAX :
		sizeof(loadmany_files);
	    index--;

	    continue;
	}

	if (error = catch(call_other(loadmany_files[index],
            "teleledningsanka")))
	{
	    tell_object(loadmany_wizard, "Error loading:   " +
	    	loadmany_files[index] + "\nMessage      :   " + error + "\n");
	    continue;
	}

	if (loadmany_wizard->query_option(OPT_ECHO))
	    tell_object(loadmany_wizard, "Loaded:          " +
			loadmany_files[index] + "\n");

	/* Try to remove the object from memory the easy way. */
	if (catch(call_other(loadmany_files[index], "remove_object")))
	{
	    tell_object(loadmany_wizard, "Cannot destruct: " +
	    	loadmany_files[index] + "\n");
	}

	/* Hammer hard if the object doesn't go away that easily. */
	if (objectp(obj = find_object(loadmany_files[index])))
	{
	    SECURITY->do_debug("destroy", obj);
	}
    }

    if (sizeof(loadmany_files) > size)
    {
    	set_alarm(LOADMANY_DELAY, 0.0, &load_many_delayed(loadmany_wizard,
    	    loadmany_files[size..]));
    }
    else
    {
 	tell_object(loadmany_wizard, "Loading completed.\n");
 	loadmany_going -= ({ loadmany_wizard->query_real_name() });
    }

    loadmany_files = 0;
    loadmany_wizard = 0;
}

/*
 * Function name: load_many_delayed_reloaded
 * Description  : After an alarm this object looses its euid, so we have
 *                to reopen the soul. This function could be integrated
 *                with load_many() itself, but I decided to separate them
 *                in order to make load_many() a static function.
 */
public nomask void
load_many_delayed_reloaded()
{
    if ((geteuid(previous_object()) != geteuid()) ||
    	(!interactive(previous_object())) ||
    	(calling_function() != REOPEN_SOUL))
    {
    	loadmany_files = 0;
    	loadmany_going -= ({ loadmany_wizard->query_real_name() });
    	loadmany_wizard = 0;
    	return;
    }

    load_many();
}
 
nomask int
load(string str)
{
    object obj;
    string *parts;
    string error;

    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Load what?\n");
	return 0;
    }

    if (str == "stop")
    {
    	if (member_array(this_player()->query_real_name(),
    	    loadmany_going) == -1)
    	{
    	    notify_fail("You are not loading multiple files at the moment.\n");
    	    return 0;
    	}

	loadmany_going -= ({ this_player()->query_real_name() });
    	write("Stopped loading multiple files.\n");
    	return 1;
    }

    str = FTPATH(this_interactive()->query_path(), str);
    if (!strlen(str))
    {
	notify_fail("Invalid file name.\n");
	return 0;
    }

    /* If wildcards are used, the wizard means to check many files. */
    if (wildmatch("*[\\*\\?]*", str))
    {
	if (member_array(this_player()->query_real_name(),
	    loadmany_going) != -1)
	{
	    notify_fail("You are already loading multiple files. You have " +
	    	"to use \"load stop\" first if you want to interrupt that " +
	    	"sequence and start a new one. Please bear in mind that " +
	    	"this operation is costly and that you should be careful " +
	    	"with executing it a lot.\n");
	    return 0;
	}

    	/* Get the files the wizard wants to load and filter only those
    	 * that are executable, ergo that end in .c
    	 */
	loadmany_files = filter(get_dir(str), &wildmatch("*.c"));

	if (!pointerp(loadmany_files) ||
	    !sizeof(loadmany_files))
	{
	    write("No files found: " + str + "\n");
	    return 1;
	}

	write("Loading " + sizeof(loadmany_files) + " file" +
	    ((sizeof(loadmany_files) == 1) ? "" : "s") + "." +
	    ((sizeof(loadmany_files) > LOADMANY_MAX) ? (" A delay of " +
		ftoi(LOADMANY_DELAY) + " seconds is used each " +
		LOADMANY_MAX + " files.") : "") + "\n");

	/* We have to add the full path to all the files to load. Then */
	parts = explode(str, "/");
	parts[sizeof(parts) - 1] = "";
	loadmany_files = map(loadmany_files,
	    &operator(+)(implode(parts, "/"), ));
	loadmany_wizard = this_interactive();
	loadmany_going += ({ this_player()->query_real_name() });

	load_many();
	return 1;
    }

    /* File does not exists. */
    if ((file_size(str + ".c") < 0) &&
    	(file_size(str) < 0))
    {
	notify_fail("No such file.\n");
	return 0;
    }

    /* If the object is already in memory, destruct it. */
    if (objectp(obj = find_object(str)))
    {
	write("Trying to update: " + str + "\n");

    	if (!update_ob(str))
    	{
    	    write("Updating failed...\n");
    	    return 0;
    	}
    }

    if (error = catch(str->teleledningsanka()))
    {
    	write("Error loading: " + str + "\n");
        write("Message: " + error + "\n");
	return 1;
    }
    
    if (this_player()->query_option(OPT_ECHO))
	write("Loaded: " + str + "\n");
    else
	write("Ok.\n");
    return 1;
}

/* **************************************************************************
 * mkdir - make a directory
 */
nomask int
makedir(string str)
{
    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Make what dir?\n");
	return 0;
    }
    if (mkdir(FTPATH((string)this_interactive()->query_path(), str)))
	write("Ok.\n");
    else
	write("Fail.\n");
    return 1;
}


/* **************************************************************************
 * mv - move multiple files or a sigle directory.
 */
nomask int
mv_cmd(string str)
{
    return multi_file(str, MULTI_MV);
}

/* **************************************************************************
 * remake - Remake an object, checks entire dependency of inherited files
 */
nomask int 
remake_object(string str)
{
    object ob;
    string *inherits, *updatem;
    int il, upflag;

    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Remake what object ?\n");
	return 0;
    }
    str = FTPATH((string)this_interactive()->query_path(), str);
    if (!strlen(str))
    {
	notify_fail("Invalid file name.\n");
	return 0;
    }
    ob = find_object(str);
    if (!ob)
    {
	notify_fail("No such object loaded.\n");
	return 0;
    }
    inherits = SECURITY->do_debug("inherit_list", ob);
    for (updatem = ({}), il = sizeof(inherits) - 1; il >= 0; il--)
    {
	ob = find_object(inherits[il]);
			  
	if (ob && (file_time(inherits[il]) > object_time(ob)))
	    updatem += ({ inherits[il] });
	else if (ob &&
		 (sizeof(updatem & SECURITY->do_debug("inherit_list", ob))))
	    updatem += ({ inherits[il] });
    }
    for (il = 0; il < sizeof(updatem); il++)
    {
	write("Updating: " + updatem[il] + "\n");
	SECURITY->do_debug("destroy", find_object(updatem[il]));
    }
    write("\n-----------\nUpdated " + il + " objects.\n");
    /* call_other(str,"teleledningsanka"); */
    return 1;
}

/* **************************************************************************
 * rm - remove multiple files
 */
nomask int
rm_cmd(string str)
{
    return multi_file(str, MULTI_RM);
}

/* **************************************************************************
 * rmdir - delete a directory
 */
nomask int
removedir(string str)
{
    CHECK_SO_WIZ;

    if (!stringp(str))
    {
	notify_fail("Remove what dir?\n");
	return 0;
    }
    if (rmdir(FTPATH((string)this_interactive()->query_path(), str)))
	write("Ok.\n");
    else
	write("Fail.\n");
    return 1;
}

/* **************************************************************************
 * trust - trust an object.
 */
nomask int
trust_ob(string str)
{
    object ob;

    CHECK_SO_WIZ;

    if (!str) 
    {
	notify_fail("Trust what object?\n");
	return 0;
    }

    ob = parse_list(str);

    if (!ob) 
    {
	notify_fail("Object not found: " + str + "\n");
	return 0;
    }

    if (geteuid(ob))
    {
	notify_fail("Object already trusted by: " + geteuid(ob) + "\n");
	return 0;
    }

    /* Install the euid of this player as uid in the object */
    export_uid(ob);
    /* Activate the object */
    ob->set_trusted(1);

    write("Beware! You have just trusted: " + str + ".\n");
    return 1;
}

/* **************************************************************************
 * update - update an object
 */
nomask int
update_ob(string str)
{
    object ob, *obs;
    int kick_master, i, error;

    CHECK_SO_WIZ;

    if (!strlen(str))
    {
	ob = environment(this_player());
	str = MASTER_OB(ob);

	if (!ob)
	{
	    notify_fail("Update what object?\n");
	    return 0;
	}

	obs = filter(all_inventory(ob), "is_player", this_object());

	error = 0;
	for (i = 0; i < sizeof(obs); i++)
	{
	    if (obs[i]->query_default_start_location() == str)
	    {
		error = 1;	
		write("Cannot update the start location of "
		    + capitalize(obs[i]->query_real_name()) + ".\n");
	    }
	}

	if (error == 1)
	{
	    notify_fail("Update failed.\n");
	    return 0;
	}

	write("Updating environment.\n");
	say(QCTNAME(this_player()) + " updates the room.\n");

	/* Move all objects out of the room */
	for (i = 0; i<sizeof(obs); i++)
	{
	    obs[i]->move(obs[i]->query_default_start_location());
	}

	ob->remove_object();
	ob = find_object(str);
	if (ob)
	    SECURITY->do_debug("destroy", ob);

	for (i = 0; i < sizeof(obs); i++)
	    obs[i]->move(str);
	return 1;
    }
    else
    {
	str = FTPATH((string)this_interactive()->query_path(), str);
	if (!strlen(str))
	{
	    notify_fail("Invalid file name.\n");
	    return 0;
	}
	ob = find_object(str);
	if (!ob)
	{
	    notify_fail("No such object.\n");
	    return 0;
	}
    }

    if (ob == find_object(SECURITY))
	kick_master = 1;

    if (ob != this_object())
    {
        /* Remove the binary file too. */
        SECURITY->remove_binary(MASTER_OB(ob));

	ob->remove_object();
	ob = find_object(str);
	if (ob)
	    SECURITY->do_debug("destroy", ob);

	/* When updating the master object it must be reloaded at once
	   and from within the GD
	 */
	if (kick_master)
	{
	    write(SECURITY + " was updated and reloaded.\n");
	    SECURITY->teleledningsanka();
	}

	else if (!ob)
	{
	    if (this_player()->query_option(OPT_ECHO))
		write(str + " will be reloaded at next reference.\n");
	    else
		write("Ok.\n");
	}
	else
	{
	    notify_fail("Object could not be updated.\n");
	    return 0;
	}
    }
    else
    {
	write(str + " will be reloaded at next reference.\n");
	write("Destructing this object.\n");

        /* Remove the binary file too. */
        SECURITY->remove_binary(MASTER);

	destruct();
	return 1;
    }
    return 1;
}

/* wildcard updating by Gresolle 1993 */
nomask int
dupd(string s)
{
    int i;
    string *dir, *args, path, fpath;
    
    /* catch 'update' without args */

    if (!s)
	return update_ob(s);
    
    /* check if user specifies the -d option */

    args = explode(s, " ");
    if (args[0] != "-d")
	return update_ob(s);

    /* remove '-d' and keep pattern */

    s = args[1];

    /* fix path */

    path = FTPATH(this_interactive()->query_path(), s);

    /* read directory */ 

    dir = get_dir(path);

    /* remove file pattern from end of path to expand path in update loop */

    args = explode(path, "/");
    args = exclude_array(args, sizeof(args) - 1, sizeof(args));
    path = implode(args, "/");
  
    /* move through all files returned by 'dir' */

    for (i = 0; i < sizeof(dir); i++)
    {
	/* get full filepath to objects */

	fpath = FTPATH(path, dir[i]);
	
	/* just update existing objects. skip anything names workroom.c to
	    avoid accidents */

	if (find_object(fpath) && dir[i] != "workroom.c")
        {
	    if (!update_ob(fpath)) /* standard update function */
		return 0;
        }
    }
    return 1;
}

nomask int
is_player(object ob)
{
    return (living(ob) &&
	    !(ob->query_npc()));
}
