/*
 * unique.c
 *
 * Use this library to control the cloning of unique items, i.e. items that
 * should only exist in a limited quanity.
 *
 * Fixed a bug with unique_lookup_alternative function - Arman Oct 2018
 */

#pragma no_clone
#pragma strict_types

#include <std.h>
#include <macros.h>
#include <formulas.h>
#include <stdproperties.h>

#define CLONE_MASTER    ("/d/Genesis/sys/global/unique")

static string
unique_lookup_alternative(mixed alternatives, int always)
{
    if (stringp(alternatives))
        return alternatives;

    /* This is really odd, please don't mix! */
    if (pointerp(alternatives))
    {
        string *always_alt = ({ });
        int     total;

        for (int i = 0; i < sizeof(alternatives); i++)
        {
            mixed alt = alternatives[i];
            if (stringp(alt))
                alt = alternatives[i] = ({ alt, total + random(100) });

            if (!pointerp(alt) || sizeof(alt) != 2)
                continue;

            total += alternatives[i][1];
            always_alt += ({ alternatives[i][0] });
        }

        int chance = random(100);
        if (always)
            chance = random(total);

        foreach (mixed alt: alternatives)
        {
            if (pointerp(alt) && sizeof(alt) == 2)
            {
                if (chance <= alt[1])
                    return alt[0];
            }
        }
    
        if (always && sizeof(always_alt))
            return one_of_list(always_alt);
    }
}

/*
 * Function name: resolve_unique
 * Description  : Use this function to limit the number of clones of a
 *                certain object which are created over time. This
 *                function does not clone the object, but returns the
 *                path of the object that should be cloned.
 *                 
 *                See 'man clone_unique' for how this is accomplished.
 *                 
 * Arguments    : string file - The file path to the main object.
 *                int    num  - The target number of clones
 *                              Default: 1
 *                mixed alt   - May be a string path, an array of string
 *                              paths, or an array of arrays, each
 *                              containing a string chance and an int.
 *                int  always - If 'alt' is an array, always try to
 *                              to pick an element from it.
 *                              Default: 1
 *                int  chance - The chance of the main item to be cloned
 *                              and checked for.
 *
 * Notes        : For details on usage, see the associated man page
 *                for this function and examples on how to
 *                correctly use it.
 */
public varargs string
resolve_unique(string rare, int num = 1, mixed alt = 0, int always = 1, 
             int chance = F_DEFAULT_CLONE_UNIQUE_CHANCE, int not_used = 1) 
{
    string file;
    
    if (random(100) <= chance) {
        if (CLONE_MASTER->may_clone(rare, num))
            file = rare;
    }

    if (!file) {
        file = unique_lookup_alternative(alt, always);
    }

    return file;
}

/*
 * Function name: clone_unique
 * Description  : Use this function to limit the number of clones of a
 *                certain object which are created over time.
 *                 
 *                See 'man clone_unique' for how this is accomplished.
 *                 
 * Arguments    : string file - The file path to the main object.
 *                int    num  - The target number of clones
 *                              Default: 1
 *                mixed alt   - May be a string path, an array of string
 *                              paths, or an array of arrays, each
 *                              containing a string chance and an int.
 *                int  always - If 'alt' is an array, always try to
 *                              to pick an element from it.
 *                              Default: 1
 *                int  chance - The chance of the main item to be cloned
 *                              and checked for.
 *
 * Notes        : For details on usage, see the associated man page
 *                for this function and examples on how to
 *                correctly use it.
 */
public varargs object
clone_unique(string rare, int num = 1, mixed alt = 0, int always = 1, 
             int chance = F_DEFAULT_CLONE_UNIQUE_CHANCE, int not_used = 1) 
{
    string file;
    object ob;

    setuid();
    seteuid(getuid());

    file = resolve_unique(rare, num, alt, always, chance, not_used);

    if (file) {
        ob = clone_object(file);
    }

    return ob;
}

#ifdef OLD_CLONE_UNIQUE
/*
 * Function name: clone_unique
 * Description	: Use this function to clone a limited number
 *		  of a certain object, and once that number
 *		  is reached, begin cloning alternate objects.
 * Arguments	: string file - The file path to the main object.
 *		  int    num  - The max number of copies of the
 *				main object allowed.
 *		  mixed alt   - May be a string path, an array of string
 *				paths, or an array of arrays, each
 *				containing a string chance and an int. 
 *		  int  always - If 'alt' is an array, always try to
 *				to pick an element from it.
 *		  int  chance - The chance of the main item to be cloned
 *				and checked for.
 *                int use_uptime - If true, distribute the occurance of
 *                              unique items over a period of time.
 *
 * Notes	: For details on usage, see the associated man page 
 *		  for this function and examples on how to
 *		  correctly use it.
 */
public varargs object
clone_unique_old(string file, int num = 1, mixed alt = 0, int always = 0,
             int chance = F_DEFAULT_CLONE_UNIQUE_CHANCE, int use_uptime = 1)
{
    object ob;
    mixed  tmp;
    int    sz, ran, ix;
    int expired_time, max_time = F_UNIQUE_DISTRIBUTION_TIME;

    if (!strlen(file))
    {
	/* You must specify a special item to check against. */
	return ob;
    }

    /* Make sure we have the correct permissions. */
    setuid(); 
    seteuid(getuid());

    /* We won't always clone the special item first. */
    if (random(100) <= chance)
    {
	/* Make sure the file we need to test can load. */
	file->load_it();

	/* Find out how many clones we have out there. */
	tmp  = object_clones(find_object(file));
	tmp -= ({ 0 });

	if (sz = sizeof(tmp))
	{
	/* Filter out broken and wizard-held objects */
	    tmp = filter(tmp, &not() @ &->query_prop(OBJ_I_BROKEN));
	    tmp = filter(tmp, &not() @ &->query_wiz_level() @ &environment());
	    sz  = sizeof(tmp);
	}

        if (use_uptime)
        {
        /* Distribute the occurance of unique items over time. */
#ifdef REGULAR_UPTIME
            /* Achieve full availability within the regular uptime. */
            max_time = min(SECURITY->query_irregular_uptime(), max_time);
#endif REGULAR_UPTIME
#ifdef REGULAR_REBOOT
            /* Regular reboot = daily reboot, so 24 hours worth of time. */
            max_time = 86400;
#endif REGULAR_REBOOT
            max_time = (F_UNIQUE_MAX_TIME_PROC * max_time) / 100;
            expired_time = min(time() - SECURITY->query_start_time(), max_time);
            num = max(1, ((num * expired_time) / max_time));
        }

	if (sz < num)
	{
	/* We are under the limit, clone the good item. */
	    return ob = clone_object(file);
	}
    }

    if (stringp(alt))
    {
    /* Alternate was a file reference, try to clone it. */
	return ob = clone_object(alt);
    }
    else if (pointerp(alt))
    {
    /* Alternate is an array, parse it. */
	if (!(sz = sizeof(alt)))
	{
	/* We parsed an empty array, lotta good that does. */
	    return ob;
	}

	/* Give a random chance out from 1-99 for failure. */
	ix  = -1;
	ran = (random(99) + 1);

	/* Begin the loop */
	while(++ix < sz)
	{
	    /* Specified a single string arg, do a random check. */
	    if (stringp(alt[ix]))
	    {
		if (random(99) > ran)
		{
		/* Beat the odds, clone and break the loop */
		    ob = clone_object(alt[ix]);
		    break;
		}
	    }
	    else if (pointerp(alt[ix]))
	    {
	    /* Specified an array arg, item + chance */
		if (sizeof(alt[ix]) != 2)
		{
		/* Not enough args in the array. */
		    continue;
		}
		else if (alt[ix][1] >= ran)
		{
		/* We have a winner, try cloning it. */
		    ob = clone_object(alt[ix][0]);
		    break;
		}
	    }
	}

	/* If you set always, we must attempt to clone something. */
	if (always && !objectp(ob))
	{
	    tmp = one_of_list(alt);
	    
	    if (stringp(tmp))
	    {
		ob = clone_object(tmp);
	    }
	    else if (pointerp(tmp))
	    {
		ob = clone_object(tmp[0]);
	    }
	}
    }

    /* Return item or 0 as appropriate. */
    return ob;
}
#endif
