/*
 * unique.c
 *
 * Use this library to control the cloning of unique items, i.e. items that
 * should only exist in a limited quanity.
 */

#pragma no_clone
#pragma strict_types

#include <formulas.h>
#include <stdproperties.h>

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
 *
 * Notes	: For details on usage, see the associated man page 
 *		  for this function and examples on how to
 *		  correctly use it.
 */
public varargs object
clone_unique(string file, int num = 1, mixed alt = 0, 
		int always = 0, int chance = F_DEFAULT_CLONE_UNIQUE_CHANCE)
{
    object ob;
    mixed  tmp;
    int    sz, ran, ix;

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
	    tmp = filter(tmp, &not() @ &->query_wiz_level() @ 
			&environment());
	    sz  = sizeof(tmp);
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
