/*
 * options.h
 *
 * This file contains all player and wizard option defines.
 */

#ifndef _options_h
#define _options_h

/* The default string of options for new players. It contains the following
 * information: more=20, screenwidth=80, whimpy=36 (very bad shape), echo on,
 * autopwd.
 */
#define OPT_DEFAULT_STRING " 20 8036\"  !"

/* Bit strings use only six bits per character in order to keep it printable.
 * The base of 48 means that 8 characters are used before the first bit.
 * [0..2] more length, [3..5] screen width and [6..7] whimpy level.
 */
#define OPT_BASE		48

/* These options are not directly linked to a bit in the array. */
#define OPT_MORE_LEN		-1 /* Number of lines with more. */
#define OPT_SCREEN_WIDTH	-2 /* Screen width in columns. */
#define OPT_WHIMPY		-3 /* Wimpy percentage. */

/* General (Mortal) bits */
#define OPT_BRIEF		 0 /* Brief description of rooms. */
#define OPT_ECHO		 1 /* Echo of commands. */
#define OPT_NO_FIGHTS		 2 /* Do not see combat of others. */
#define OPT_BLOOD		 2 /* Backward compatibility for NO_FIGHTS. */
#define OPT_UNARMED_OFF          3 /* Do not use unarmed combat. */
#define OPT_GAG_MISSES           4 /* Gag messages of combat misses. */
#define OPT_MERCIFUL_COMBAT      5 /* Be merciful in combat. */
#define OPT_WEBPERM		 6 /* Web participation. */
#define OPT_TABLE_INVENTORY	 7 /* Modern tabulated inventory. */
#define OPT_AUTOWRAP		19 /* Auto-wrap long notes. */

/* Wizard bits */
#define OPT_AUTO_PWD		18  /* Display of current dir on changes. */
#define OPT_AUTOLINECMD		20  /* Automatically alias line commands. */
#define OPT_TIMESTAMP		21  /* Get a time stamp on wiz lines. */

#endif /* _options_h */
