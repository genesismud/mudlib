/*
 * /sys/global/language.c
 *
 * This file holds some standard natural language functions and routines
 * to translate a numerical value to an associated state description.
 */

#pragma save_binary
#pragma strict_types

#include <const.h>
#include <state_desc.h>
#include <ss_types.h>
#include <stdproperties.h>

/* Global variables. */
static string *nums, *numt, *numnt, *numo;
int    *stat_levels, *exp_levels;
string *exp_titles;
mixed  stat_strings;

void
create()
{
    nums = ({ "one", "two", "three", "four", "five", "six", "seven",
              "eight", "nine", "ten","eleven","twelve","thirteen",
              "fourteen", "fifteen", "sixteen", "seventeen", "eighteen",
              "nineteen"});
    numo = ({ "first", "second", "third", "fourth", "fifth", "sixth",
              "seventh", "eighth", "ninth", "tenth","eleventh","twelfth",
              "thirteenth", "fourteenth", "fifteenth", "sixteenth",
              "seventeenth", "eighteenth", "nineteenth"});
    numt = ({ "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
              "eighty", "ninety"});
    numnt = ({ "twent", "thirt", "fort", "fift", "sixt", "sevent", "eight",
	       "ninet" });

    stat_levels  = SD_STATLEVELS;
    stat_strings = ({ SD_STATLEV_STR, SD_STATLEV_DEX, SD_STATLEV_CON,
                      SD_STATLEV_INT, SD_STATLEV_WIS, SD_STATLEV_DIS, });
    exp_levels = SD_AV_LEVELS;
    exp_titles = SD_AV_TITLES;

}

#define CFUN
#ifdef CFUN
string
article(string str) = "article";
#else
string
article(string str)
{
    if (!str)
    {
	return 0;
    }

    str = lower_case(str);
    if (wildmatch("the *", str))
    {
        return "";
    }

    if (wildmatch("[aeiou]*", str))
    {
        return "an";
    }

    return "a";
}
#endif

string
add_article(string str) 
{
    string s;

    s = article(str);
    return strlen(s) ? (s + " " + str) : str;
}

string
strip_article(string str) 
{
    int len = strlen(str);

    if ((len > 2) && (str[..1] == "a ")) return str[2..];
    if ((len > 3) && (str[..2] == "an ")) return str[3..];
    return str;
}

/*
 * Function name: word_number
 * Description  : Transform a number into text: 1 -> "one"
 * Macro        : LANG_NUM2WORD(num) in <language.h>
 * Arguments    : int num - the number.
 * Returns      : string - the text representation.
 */
string
word_number(int num)
{
    int tmp;
    
    if (num < 1) return "no";
    if (num < 20) return nums[num-1];
    if (num > 99)
    {
	if (num > 999)
	{
	    if (num > 999999)
		return "many";
	    tmp = num % 1000;
	    return word_number(num / 1000) + " thousand" +
		(tmp ? " " + word_number(tmp) : "");
	}
	tmp = num % 100;
	return word_number(num / 100) + " hundred" +
	    (tmp ? " " +  word_number(tmp) : "");
    }
    tmp = num % 10;
    return numt[num / 10 - 2] + (tmp ? "-" + nums[tmp - 1] : "");
}

/*
 * Function name: word_ord_number
 * Description  : Transform an ordinal number into text: 1 -> "first"
 * Macro        : LANG_ORD2WORD(num) in <language.h>
 * Arguments    : int num - the number.
 * Returns      : string - the text representation.
 */
string
word_ord_number(int num)
{
    int tmp;

    if (num < 1) return "zero";
    if (num < 20) return numo[num-1];
    if (num > 99)
    {
        if (num > 999)
	{
            if (num > 9999)   /* was 999999 */
                return "many";
            if (num == 1000)
                return "thousandth";
            if (!(tmp = num % 1000))
                return word_number(num / 1000) + " thousandth";
            return word_number(num / 1000) + " thousand " +
                word_ord_number(tmp);
        }
        if (num == 100)
            return "hundredth";
   	if (!(tmp = num % 100))
            return word_number(num / 100) + " hundredth";
        return word_number(num / 100) + " hundred " +
	    word_ord_number(tmp);
    }

    if (!(tmp = num % 10))
        return numnt[num / 10 - 2] + "ieth";
    else
        return numt[num / 10 - 2] + " " + numo[tmp - 1];
}

/*
 * Function name: word_ord_ext
 * Description  : Transform an ordinal number into extension text: 1 -> "1st"
 * Macro        : LANG_ORD2EXT(num) in <language.h>
 * Arguments    : int num - the number.
 * Returns      : string - the text representation.
 */
string
word_ord_ext(int num)
{
    int test = num % 100;
    /* 11th, 12th and 13th are not subject to first, second, third. */
    if (test > 10 && test <= 13)
    {
        return num + "th";
    }

    switch(test % 10)
    {
    case 1:
        return num + "st";
    case 2:
        return num + "nd";
    case 3:
        return num + "rd";
    default:
        return num + "th";
    }
}

/*
 * Function name: number_ord_word
 * Description  : Transform an ordinal text into number: "first" -> 1
 * Macro        : LANG_WORD2ORD(str) in <language.h>
 * Arguments    : string str - the text representation of an ordinal number.
 * Returns      : int - the number.
 */
int
number_ord_word(string str)
{
    int i, j;
    string sstr, *nt;

    if (!str)
	return 0;

    if ((i = member_array(str, numo)) > -1)
        return i + 1;

    sstr = str[0..(strlen(str)-5)];
    if ((i = member_array(sstr, numnt)) > -1)
        return (i + 2) * 10;

    if (sizeof(nt = explode(str, " ")) != 2)
        return 0;

    if ((i = member_array(nt[1], numo)) == -1)
        return 0;

    if ((j = member_array(nt[0], numt)) == -1)
        return 0;

    return (j+2)*10 + (i+1); 
}

/* lpc singular to plural converter
*/
#ifdef CFUN
string
plural_word(string str) = "plural_word";
#else
string
plural_word(string str)
{
    string tmp, slask;
    int sl, ch;
    
    if (!str) return 0;

    switch (str)
    {
    case "tooth":
	return "teeth";
    case "foot":
	return "feet";
    case "man":
	return "men";
    case "woman":
	return "women";
    case "child":
	return "children";
    case "sheep":
	return "sheep";
    case "pants":
        return "pants";
    case "dwarf":
	return "dwarves";
    case "elf":
        return "elves";
    }  
    sl = strlen(str) - 1;
    if (sl < 2)
	return str;
    ch = str[sl];
    tmp = extract(str, 0, sl - 2);
    slask = extract(str, sl - 1, sl - 1);
    switch (ch)
    {
    case 's':
	return tmp + slask + "ses";
    case 'x':
	return tmp + slask + "xes";
    case 'h':
	return tmp + slask + "hes";
    case 'y':
	if (member_array(slask, ({ "a", "e", "o" })) >= 0)
	    return tmp + slask + "ys";
	else
	    return tmp + slask + "ies";
    case 'e':
	if (slask == "f")
	    return tmp + "ves";
    }
    return str + "s";
}
#endif

string
plural_sentence(string str)
{
    int  c;
    string *a;
    
    if (!str)
	return 0;
    
    a = explode(str + " ", " ");
    if ((!a) || (sizeof(a) < 1))
	return 0;
    for (c = 1; c < sizeof(a); c++)
    {
	if (lower_case(a[c]) == "of")
	{
	    a[c - 1] = plural_word(a[c - 1]);
	    return implode(a, " ");
	}
    }
    a[sizeof(a) - 1] = plural_word(a[sizeof(a) - 1]);
    return implode(a, " ");
}

/* Verb in present tence (not ready yet)
*/
string
verb_present(string str)
{
  return str;
}

/* Name in possessive form
*/
string
name_possessive(string str)
{
    if (!strlen(str))
	return 0;

    switch (str)
    {
    case "it": return "its";
    case "It": return "Its";
    case "he": return "his";
    case "He": return "His";
    case "she": return "her";
    case "She": return "Her";
    }
    if (extract(str, strlen(str) - 1) == "s")
	return (str + "'");
    return (str + "'s");
}

/*
 * Function name: number_word
 * Description  : Transform a text number number into integer: "one" -> 1
 * Macro        : LANG_WORD2NUM(str) in <language.h>
 * Arguments    : string str - the text representation of the number.
 * Returns      : int - the number.
 */
int
number_word(string str)
{
    string *ex;
    int value, pos;

    if (sscanf(str, "%d", value))
	return value;

    value = 0;

    ex = explode(str, "y");

    if (!sizeof(ex))
        return 0;

    if (sizeof(ex) > 1)
    {
        if ((pos = member_array(ex[0], numnt)) == -1)
	    return 0;
	else
	    value = (pos + 2) * 10;
    }

    if (value)
    {
	if (ex[1][0] == ' ' || ex[1][0] == '-')
	    ex[1] = extract(ex[1], 1, strlen(ex[1]) - 1);
	if ((pos = member_array(ex[1], nums)) == -1)
	    return 0;
	else
	    value += pos + 1;
    }

    if (!value)
	if ((pos = member_array(ex[0], nums)) == -1)
	    {
		if ((pos = member_array(ex[0], numnt)) == -1)
		    return 0;
		else
		    value = (pos + 2) * 10;
	    }
	else
	    value = pos + 1;

    return value;
}

string
singular_form(string str)
{
    string singular, one, two, three;
    int last;

    switch (str)
    {
    case "teeth":
	return "tooth";
    case "feet":
	return "foot";
    case "men":
	return "man";
    case "women":
	return "woman";
    case "children":
	return "child";
    case "sheep":
	return "sheep";
    case "elves":
	return "elf";
    case "dwarves":
	return "dwarf";
    }  

    last = strlen(str);
    one  = extract(str, last - 1, last - 1);
    two  = extract(str, last - 2, last - 2);
    three = extract(str, last - 3, last - 3);

    if (one != "s")
    {
	return str;
    }
    if (two != "e" || three == "c" || three == "g")
    {
        return extract(str, 0, last - 2);
    }
    if (three == "i")
    {
        return extract(str, 0, last - 4) + "y";
    }
    if (three == "v")
    {
        return extract(str, 0, last - 4) + "fe";
    }
    return extract(str, 0, last - 3);
}

/*
 * Function name: lang_short
 * Description  : Returns the short description without article. This routine
 *                considers heaps and regular objects.
 * Arguments    : object ob - the object to get the short for.
 * Returns      : string - the short description _without_ article.
 */
string
lang_short(object ob)
{
    int heapsize;

    if (ob->query_prop(HEAP_I_IS))
    {
        heapsize = ob->num_heap();
        if (heapsize == 1)
        {
            return ob->singular_short();
        }
        else if (heapsize >= 1000)
        {
            return "huge heap of " + ob->plural_short();
        }
        return ob->short();
    }
    return ob->short();
}

/*
 * Function name: lang_a_short
 * Description  : Returns the short description with article. This routine
 *                considers heaps and regular objects.
 * Arguments    : object ob - the object to get the short for.
 * Returns      : string - the short description _with_ article.
 */
string
lang_a_short(object ob)
{
    if (ob->query_prop(HEAP_I_IS))
    {
        return ob->short();
    }
    return add_article(ob->short());
}

/*
 * Function name: lang_the_short
 * Description  : Gets the short description of an object with "the" prepended
 *                to it. This function properly handles heaps, too.
 * Arguments    : object ob - the object to get the description with "the" of.
 * Returns      : string - the proper short description.
 */
string
lang_the_short(object ob)
{
    int heapsize;

    if (ob->query_prop(HEAP_I_IS))
    {
        heapsize = ob->num_heap();
        if (heapsize == 1)
        {
            return "the " + ob->singular_short();
        }
        else if (heapsize >= 1000)
        {
            return "the huge heap of " + ob->plural_short();
        }
        return ob->short();
    }
    return "the " + ob->short();
}

/*
 * Function name: get_num_desc
 * Description  : Spreads a value over a set of descriptions consisting of
 *                of main and (optionally) sub descriptions.
 * Arguments    : int value - the value, must be in range 0-maximum.
 *                int maximum - the maximum the value must adhere to.
 *                string* maindescs - array of main descriptions.
 *                string* subdescs - array of sub descriptions, or 0.
 *                    Individual sub descriptions must end with a space.
 *                int turnpoint - if nonzero, then every value in the maindesc
 *                    array with an index LOWER than turnpoint will have the
 *                    selections of the subdesc reversed. This to make it so
 *                    that someone can be "extremely calm" as highest form of
 *                    calm but also "extremely panicky" as worst form.
 * Returns      : string - the associated (sub +) main description.
 */
varargs string
get_num_desc(int value, int maximum, string *maindescs, string *subdescs = 0, int turnindex = 0)
{
    int mainindex, subindex;

    if (!maindescs)
    {
        return ">unknown<";
    }

    /* Maximum must be positive. */
    maximum = ((maximum > 0) ? maximum : 1);
    /* Value must be in valid range. */
    value = minmax(value, 0, maximum-1);

    /* No subdescs, return the main description only. */
    if (!subdescs)
    {
        mainindex = (value * sizeof(maindescs)) / maximum;
        return maindescs[mainindex];
    }
    
    /* Distribute the value of the range of main and sub-descriptions. */
    value = (value * sizeof(maindescs) * sizeof(subdescs)) / maximum;
        
    /* Extract the main and sub-indices. */
    mainindex = value / sizeof(subdescs);
    subindex = value % sizeof(subdescs);

    /* For the low end of the spectrum, we may reverse the sub-descriptions. */
    if (mainindex < turnindex)
    {
        subindex = sizeof(subdescs) - 1 - subindex;
    }

    return subdescs[subindex] + maindescs[mainindex];
}

/* 
 * Function name: get_num_desc_centered
 * Description  : A wrapper around get_num_desc which distributes the descriptions
 *                around a center value. 
 * Arguments    : value    - The value
 *                maximum  - The max, the minimum will be calculated from this
 *                center   - The value which correspons to the center desc. 
 *                center_width - The range for the center:
 *                               [center - center_width <-> center + center_width]
 *                *descs   - The descriptions, must be odd sized.
 */
string 
get_num_desc_centered(int value, int maximum, int center, int center_width, string *descs) 
{
    int center_desc = sizeof(descs) / 2;

    if (value > (center + center_width))
    {
        return get_num_desc(value - (center + center_width), maximum - (center + center_width), descs[(center_desc + 1)..]);
    }
    
    if (value < (center - center_width))
    {
        /* Remap values to 0-max, reverse descs */
        value = abs(value - (center - center_width));
        string *reversed = allocate(sizeof(descs) / 2 - 1);
        for (int i = 0; i < sizeof(reversed); i++) {
            reversed[i] = descs[center_desc - (1 + i)]; 
        }
        return get_num_desc(value - (center + center_width), maximum - (center + center_width), reversed);
    }

    return descs[center_desc];
}

/*
 * Function name: get_num_desc_combos
 * Description  : Finds all possible combinations of main and sub descriptions.
 * Arguments    : string* maindescs - array of main descriptions.
 *                string* subdescs - array of sub descriptions, or 0.
 *                    Individual sub descriptions must end with a space.
 *                int turnpoint - if nonzero, then every value in the maindesc
 *                    array with an index LOWER than turnpoint will have the
 *                    selections of the subdesc reversed. This to make it so
 *                    that someone can be "extremely calm" as highest form of
 *                    calm but also "extremely panicky" as worst form.
 * Returns      : string * - the list of sub and main descriptions.
 */
string *
get_num_desc_combos(string *maindescs, string *subdescs, int turnindex = 0)
{
    string *result = ({ });

    int mainindex, subindex;

    for (mainindex = 0; mainindex < sizeof(maindescs); mainindex++)
    {
        if (mainindex < turnindex)
	{
	    for (subindex = sizeof(subdescs) -1; subindex >= 0; subindex--)
	    {
		result += ({ subdescs[subindex] + maindescs[mainindex] });
	    }
	}
	else
	{
	    for (subindex = 0; subindex < sizeof(subdescs); subindex++)
	    {
		result += ({ subdescs[subindex] + maindescs[mainindex] });
	    }
	}
    }

    return result;
}


/*
 * Function name: get_num_level_index
 * Description  : Find the index of a level in an array based on levels, rather
 *                than spreading the levels uniformly over the range.
 *                Simply put: value >= levels[i] returns i.
 * Arguments    : int value - the value to transform to an index. If it
 *                    is below the lowest level, the first index is returned.
 *                int *levels - the levels in ascending order.
 * Returns      : int - the index associated with the level.
 */
int
get_num_level_index(int value, int *levels)
{
    int index = sizeof(levels);

    while(--index >= 0)
    {
        if (value >= levels[index])
        {
            return index;
        }
    }

    return 0;
}

/*
 * Function name: get_num_level_desc
 * Description  : Find a description in an array based on levels, rather than
 *                spreading the descriptions uniformly over the range.
 *                Simply put: value >= levels[i] returns descs[i].
 * Arguments    : int value - the value to transform to a description. If it
 *                    is below the lowest level, the first description is
 *                    returned.
 *                int *levels - the levels in ascending order, associated
 *                    with the descriptions.
 *                string *descs - the descriptions, must be as many as there
 *                    are levels.
 * Returns      : string - the description associated with the level.
 */
string
get_num_level_desc(int value, int *levels, string *descs)
{
    return descs[get_num_level_index(value, levels)];
}

/*
 * Function name: get_stat_level_desc
 * Description  : Get the description of a stat level based on the value.
 * Arguments    : int stat - the stat to describe.
 *                int level - the level to describe.
 * Returns      : string - the associated description.
 */
string
get_stat_level_desc(int stat, int level)
{
    if (stat < 0 || stat >= SS_NO_EXP_STATS)
    {
        return "";
    }

    return get_num_level_desc(level, stat_levels, stat_strings[stat]);
}

/*
 * Function name: get_stat_index_desc
 * Description  : Get the description of a stat level based on the index.
 * Arguments    : int stat - the stat to describe.
 *                int index - the index in the array.
 * Returns      : string - the associated description.
 */
string
get_stat_index_desc(int stat, int index)
{
    if (stat < 0 || stat >= SS_NO_EXP_STATS ||
        index < 0 || index >= SD_NUM_STATLEVS)
    {
        return "";
    }

    return stat_strings[stat][index];
}

/*
 * Function name: get_exp_level_desc
 * Description  : Get the description of a mortal level based on the average
 *                stat. Basically put here just to use a single variable array
 *                declaration.
 * Arguments    : int level - the level to describe.
 *                int gender - the gender of the player.
 * Returns      : string - the associated description.
 */
string
get_exp_level_desc(int level, int gender)
{
    string title = get_num_level_desc(level, exp_levels, exp_titles);

    /* Bit of a cludge to transform hero into heroine. */
    if ((gender == G_FEMALE) && (title[-4..] == "hero"))
    {
        title += "ine";
    }

    return title;
}

string
april_fools_2009(string str)
{
    return str;
}
