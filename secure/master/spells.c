 /*
    spells.c

    Routines for handling the list of spells.

*/

static string	*spell_verb,	/* The activating word */
		*spell_func,	/* The function to call */
		*spell_cobj,	/* The object to call the function in */
		*spell_name;	/* The name of the spell */

/*
 * Function name: add_spell
 * Description:   Add a spell to the player's supply of spells.
 * Arguments:	  verb - The name of the spell, the name it is cast by.
 *		  func - The name of the function to call.
 *		  obj - The object to call the function in. 
 * 		  name - The name of the spell.
 * Returns:       1 - Ok, 0 - The spell already existed.
 */
int 
add_spell(string verb, string func, mixed cobj, string name)
{

    string tmp, callobj, *list;

    /* Check for already existing spells with the same name */
    if (member_array(verb, spell_verb) >= 0)
	return 0;

    if (objectp(cobj))
    {
	callobj = file_name(cobj);
	list = explode(callobj, "#");
	callobj = list[0];
    }
    else
	callobj = cobj;

    /* Kludge to fix bad initialization of types */
    if (!pointerp(spell_verb))
    {
	spell_verb = ({});
	spell_func = ({});
	spell_cobj = ({});
	spell_name = ({});
    }

    spell_verb += ({ verb });
    spell_func += ({ func });
    spell_cobj += ({ callobj });
    if (name)
	spell_name += ({ name });
    else
	spell_name += ({ verb });

    return 1;
}

/*
 * Function name: list_spells
 * Description:   List the active spells.
 */
void 
list_spells()
{
    int i;
    string space;

    space = "                                                 ";

    for (i = 0 ; i < sizeof(spell_verb) ; i++)
	write(spell_verb[i] + extract(space, 0, 17 - strlen(spell_verb[i])) +
	      spell_name[i] + extract(space, 0, 31 - strlen(spell_name[i])) +
	      spell_func[i] + extract(space, 0, 17 - strlen(spell_func[i])) +
	      spell_cobj[i] + "\n");

}

/*
 * Function name: 
 * Description:   
 * Arguments:	  
 * Returns:       
 */
