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

// Begin with general/mortal options

// More length
#define OPT_MORE_LEN		0
// Screen width
#define OPT_SCREEN_WIDTH	1
// Brief display mode
#define OPT_BRIEF		2
// Echo display mode
#define OPT_ECHO		3
// Whimpy level
#define OPT_WHIMPY		4
// See fights of others
#define OPT_NO_FIGHTS		5
// Backward compatibility.
#define OPT_BLOOD		5
// Disable unarmed combat
#define OPT_UNARMED_OFF         7
// Gag all messages of misses
#define OPT_GAG_MISSES          8
// Be merciful in combat
#define OPT_MERCIFUL_COMBAT     9
// Auto-wrap long notes
#define OPT_AUTOWRAP		10
// Web participation
#define OPT_WEBPERM		12

// Wizard options

// Automatic display of current directory on cwd changes
#define OPT_AUTO_PWD		6
// Automatically alias line commands
#define OPT_AUTOLINECMD		11
// Get a time stamp on wiz lines.
#define OPT_TIMESTAMP		13

#endif /* _options_h */
