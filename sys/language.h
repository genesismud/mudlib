/*
 * /sys/language.h
 *
 * Some language functions.
 */

#ifndef LANG_DEF
#define LANG_DEF

#define LANG_FILE "/sys/global/language"

/*
 * LANG_PWORD    -- Get the plural form of a noun.
 *
 * LANG_SWORD    -- Get the singular form of a noun.
 *
 * LANG_PSENT    -- Get the plural form of a noun phrase.
 *
 * LANG_ART      -- Get the article of a noun.
 *
 * LANG_ADDART   -- Get the article of a noun + the noun.
 *
 * LANG_WNUM     -- Get the textform of a number.
 * LANG_NUM2WORD -- Ditto.
 *
 * LANG_NUMW     -- Get the number of a textform.
 * LANG_WORD2NUM -- Ditto.
 *
 * LANG_POSS     -- Get the possessive form of a name.
 *
 * LANG_SHORT    -- Get the short without article. Works for heaps, too.
 *
 * LANG_ASHORT   -- Get the short with article. Works for heaps, too.
 *
 * LANG_THESHORT -- Get the short with 'the' in front. Works for heaps, too.
 *
 * LANG_ORDW     -- Get the number of an ordinal textform, "first" -> 1
 * LANG_WORD2ORD -- Ditto.
 *
 * LANG_WORD     -- Get the text in ordinal from from a number, 2 -> "second"
 * LANG_ORD2WORD -- Ditto.
 */

#define LANG_PWORD(x)    ((string)LANG_FILE->plural_word(x))
#define LANG_SWORD(x)    ((string)LANG_FILE->singular_form(x))
#define LANG_PSENT(x)    ((string)LANG_FILE->plural_sentence(x))
#define LANG_ART(x)      ((string)LANG_FILE->article(x))
#define LANG_ADDART(x)   ((string)LANG_FILE->add_article(x))
#define LANG_WNUM(x)     ((string)LANG_FILE->word_number(x))
#define LANG_NUM2WORD(x) LANG_WNUM(x)
#define LANG_NUMW(x)        ((int)LANG_FILE->number_word(x))
#define LANG_WORD2NUM(x) LANG_NUMW(x)
#define LANG_POSS(x)     ((string)LANG_FILE->name_possessive(x))
#define LANG_SHORT(x)    ((string)LANG_FILE->lang_short(x))
#define LANG_ASHORT(x)   ((string)LANG_FILE->lang_a_short(x))
#define LANG_THESHORT(x) ((string)LANG_FILE->lang_the_short(x))
#define LANG_ORDW(x)        ((int)LANG_FILE->number_ord_word(x))
#define LANG_WORD2ORD(x) LANG_ORDW(x)
#define LANG_WORD(x)     ((string)LANG_FILE->word_ord_number(x))
#define LANG_ORD2WORD(x) LANG_WORD(x)

/*
 * LANG_IS_OFFENSIVE(x) -- Returns true if the term contains offensive words
 *                         or sub-words.
 */
#define LANG_IS_OFFENSIVE(x) ((string)LANG_FILE->lang_is_offensive(x))

/*
 * LANG_VOWELS - an array with all vowels.
 */
#define LANG_VOWELS ({ "a", "e", "i", "o", "u", "y" })

/* No definitions beyond this line. */
#endif LANG_DEF
