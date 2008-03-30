/*
   sys/state_desc.h

   Holds all textual descriptions of state descriptions of livings.
   
   Note that local changes to these arrays are done in
   /config/sys/state_desc2.h
*/

#ifndef SD_DEFINED
#include "/config/sys/state_desc2.h"
#endif

#ifndef SD_DEFINED
#define SD_DEFINED

#define SD_LANG_FILE "/sys/global/language"

#ifndef SD_AV_TITLES
/*
 * SD_AV_TITLES - the mortal titles.
 * SD_AV_LEVELS - the stat level from which you get a title.
 */
#define SD_AV_TITLES    ({ "novice", "beginner", "apprentice", "master" })
#define SD_AV_LIMITS    ({ 10, 25, 50, 100 })
/*
 * SD_NEWBIE_INDEX - Index to the highest title in SD_AV_TITLES at which you
 *                   may be considered a newbie for game purposes.
 * SD_NEBWIE_TITLE - The highest title at which you may be considered a newbie
 *                   for game purposes. Hardcoded for speed reasons.
 *                   By definition: SD_AV_TITLES[SD_NEWBIE_INDEX]
 * SD_NEWBIE_LEVEL - The stat average below which you may be considered a
 *                   newbie for game purposes. AVSTAT < SD_NEWBIE_LEVEL.
 *                   Hardcoded for speed reasons.
 *                   By definition: SD_AV_LEVELS[SD_NEWBIE_INDEX + 1]
 */
#define SD_NEWBIE_INDEX 2
#define SD_NEBWIE_TITLE "beginner"
#define SD_NEWBIE_LEVEL 50
#endif SD_AV_TITLES

/* May this player be considered a newbie? */
#define SD_IS_NEWBIE(player) ((player)->query_average_stat() < SD_NEWBIE_LEVEL)

#define SD_STAT_NAMES 	({ "STR", "DEX", "CON", "INT", "WIS", "DIS" })
#define SD_STAT_DESC ({ "str", "dex", "con", "int", "wis", "dis", "race", \
    "occup", "layman" })
#define SD_LONG_STAT_DESC ({ "strength", "dexterity", "constitution", \
    "intelligence", "wisdom", "discipline", "racial", "layman", \
    "occupational" })

#define SD_STATLEV_STR ({ "puny", "feeble", "flimsy", "weak", "well built", \
                          "muscular", "hefty", "strong", "powerful", \
                          "musclebound", "ironlike", "forceful", "crushing", \
                          "mighty", "titanic" })
#define SD_STATLEV_DEX ({ "stiff", "lumbering", "clumsy", "deft", "flexible", \
                          "nimble", "lithe", "supple", "agile", "swift", \
                          "quick", "graceful", "athletic", "gymnastic", \
                          "acrobatic" })
#define SD_STATLEV_CON ({ "sickly", "fragile", "frail", "skinny", "lean", \
                          "healthy", "firm", "hearty", "hardy", "hale", \
                          "robust", "staunch", "tough", "sturdy", "vigorous" })
#define SD_STATLEV_INT ({ "moronic", "dimwitted", "simple", "dense", "slow", \
                          "limited", "keen", "clever", "quick minded", \
                          "intelligent", "sharp", "bright", "inventive", \
                          "ingenious", "brilliant" })
#define SD_STATLEV_WIS ({ "inane", "stupid", "idiotic", "foolish", "uneducated", \
                          "literate", "educated", "learned", "scholarly", \
                          "cultivated", "knowledgeable", "erudite", "wise", \
                          "sage", "enlightened" })
#define SD_STATLEV_DIS ({ "gutless", "frightened", "spineless", "fearful", \
                          "cowardly", "insecure", "timid", "sound", "bold", \
                          "brave", "courageous", "fearless", "valiant", \
                          "heroic", "lionhearted" })

#define SD_NUM_STATLEVS 15
                      /* Lowest possible level is probably 7. But just to be safe. */
#define SD_STATLEVELS ({ 1, 14, 22, 30, 39, 48, 58, 68, 79, 90, 102, 114, 126, 138, 150 })
#define SD_STATLEVEL_EPIC 165 
#define SD_STATLEVEL_IMM  185 
#define SD_STATLEVEL_SUP  210 

#define SD_NUM_GUILDS       4
#define SD_GUILD_EXTENSIONS ({ "race", "occ", "lay", "craft" })
#define SD_GUILD_FULL_NAMES ({ "racial", "occupational", "layman", "craftsman" })

#define SD_SIZEOF_ADVANCE_DESCS   5
#define SD_ADVANCE_DESCS    ({ "very far from", "far from", "halfway to", \
    "close to", "very close to" })

#define GET_STAT_LEVEL_DESC(stat, level) ((string)SD_LANG_FILE->get_stat_level_desc((stat), (level)))
#define GET_STAT_INDEX_DESC(stat, index) ((string)SD_LANG_FILE->get_stat_index_desc((stat), (index)))
#define GET_EXP_LEVEL_DESC(level)        ((string)SD_LANG_FILE->get_exp_level_desc(level))

/* Observe that the denominators below are reversed for the first two
   of the statlev descs above.
*/
#define SD_STAT_DENOM	({ "slightly ", "somewhat ", "", 		\
			   "very ", "extremely " })

#define SD_BRUTE_FACT   ({ "pacifistic", "meek", "touchy", "brutal", 	\
			   "violent"})

#define SD_HEALTH 	({ "at death's door", "barely alive", 		\
                	   "terribly hurt", "in a very bad shape", 	\
			   "in a bad shape", "very hurt",	 	\
			   "feeling rather hurt", "hurt", 		\
                	   "somewhat hurt", "slightly hurt", 		\
			   "feeling very well" })
    
#define SD_MANA		({ "in a vegetable state", "exhausted",		  \
			   "worn down", "indisposed", "in a bad shape",   \
			   "very degraded", "rather degraded",\
			   "degraded", "somewhat degraded",		  \
			   "slightly degraded", "in full vigour" })

#define SD_COMPARE_STR  ({ "about the same strength as", 		  \
			   "a bit stronger than", "stronger than", 	  \
			   "much stronger than"})
#define SD_COMPARE_DEX  ({ "about as agile as", 			  \
			   "a bit better coordinated than", 		  \
			   "more agile than", "much more agile than" })
#define SD_COMPARE_CON  ({ "about as healthy as", "a bit healthier than", \
			   "healthier than", "much healthier than"})
#define SD_COMPARE_INT  ({ "about as smart as", "a bit smarter than",     \
			   "smarter than", "much smarter than"})
#define SD_COMPARE_WIS  ({ "about as wise as", "a bit wiser than",        \
			   "wiser than", "much wiser than"})
#define SD_COMPARE_DIS  ({ "about as brave as", "a bit braver than",      \
			   "braver than", "much braver than"})
#define SD_COMPARE_AC   ({ "about the same protection as",                \
                           "a bit more protection than",                  \
                           "more protection than",                        \
                           "a whole lot more protection than" })
#define SD_COMPARE_HIT  ({ "about as difficult as", "a bit easier than",  \
                           "easier than", "a lot easier than" })
#define SD_COMPARE_PEN  ({ "largely comparable to", "a bit more than",    \
                           "somewhat more than", "a lot more than" })

#define SD_BEAUTY_FEMALE ({ "like the image of perfection", "gorgeous",   \
			    "fine", "attractive", "beautiful", "pretty",  \
			    "nice", "plain", "more than usually plain",   \
			    "repulsive", "ugly", "hideous" })

#define SD_BEAUTY_MALE  ({ "like the image of perfection", "wonderful",   \
			   "handsome", "attractive", "fine", "pleasant",  \
			   "nice", "coarse", "unpleasant", "repulsive",   \
			   "ugly", "hideous" })

#define SD_PANIC  	({ "secure", "calm", "uneasy", "worried", "panicky"})
#define SD_FATIGUE	({ "alert", "weary", "tired", "exhausted" })

#define SD_SOAK  	({ "drink quite a lot more", "drink a lot more",  \
		    	   "drink some more", "drink a little more",	  \
		    	   "barely drink more" })

#define SD_STUFF    	({ "eat quite a lot more", "eat a lot more",	  \
		     	   "eat some more", "eat a little more",	  \
		     	   "barely eat more" })
    
#define SD_INTOX	({ "tipsy", "intoxicated", "drunk", "tanked", 	  \
			   "blitzed", "wasted", "toasted", "pissed",	  \
			   "stoned", "obliviated" })
    
#define SD_HEADACHE	({ "mild", "uncomfortable", "bad", "serious",     \
			   "intense", "pounding", "burning", "extreme" })

#define SD_IMPROVE_MIN  ( 75000)
#define SD_IMPROVE_MAX  (750000)
#define SD_IMPROVE	({ "insignificant", "minimal", "slight", "low",     \
			   "a little", "some", "modest", "average", "nice", \
			   "good", "very good", "great", "extremely good",  \
			   "immense", "fantastic" })
	    
#define SD_GOOD_ALIGN	({ "neutral", "agreeable", "trustworthy",	  \
			   "sympathetic", "nice", "sweet", "good",	  \
			   "devout", "blessed", "saintly", "holy" })

#define SD_EVIL_ALIGN	({ "neutral", "disagreeable", "untrustworthy",    \
			   "unsympathetic", "sinister", "wicked", "nasty",\
			   "foul", "evil", "malevolent", "beastly",	  \
			   "demonic", "damned" })

#define SD_ARMOUR	({ "in prime condition", "a little worn down", 	 \
                           "in a very bad shape", "in urgent need of repair", \
			   "going to break any second" })

#define SD_WEAPON_RUST	({ "no comment", "find some rust marks",	\
			   "spot some rust", "full of rust", 		\
			   "bathing in acid", "falling apart from corrosion" })

#define SD_WEAPON_COND	({ "in prime condition", "in a fine condition",	\
			   "touched by battle", "scarred by battle", 	\
			   "very scarred by battle",                    \
			   "in big need of a smith", \
			   "going to break any second" })

#define SD_ENC_WEIGHT	({ "unencumbered", "lightly burdened", "burdened", \
			   "encumbered", "heavily loaded", "over taxed",   \
			   "collapsing under your load" })

#define SD_LEVEL_MAP 	([						\
			  "goodalign" : SD_GOOD_ALIGN,			\
			  "evilalign" : SD_EVIL_ALIGN,			\
			  "statimprove" : SD_IMPROVE,			\
			  "headache" : SD_HEADACHE,			\
			  "intox" : SD_INTOX,				\
			  "stuffed" : SD_STUFF,				\
			  "soaked" : SD_SOAK,				\
			  "fatigue" : SD_FATIGUE,			\
			  "panic" : SD_PANIC,				\
			  "malebeauty" : SD_BEAUTY_MALE,		\
			  "femalebeauty" : SD_BEAUTY_FEMALE,		\
			  "mana" : SD_MANA,				\
			  "health" : SD_HEALTH,				\
			  "brute" : SD_BRUTE_FACT,			\
			  "compstr" : SD_COMPARE_STR,			\
			  "compdex" : SD_COMPARE_DEX,			\
			  "compcon" : SD_COMPARE_CON,			\
			  "compint" : SD_COMPARE_INT,			\
			  "compwis" : SD_COMPARE_WIS,			\
			  "compdis" : SD_COMPARE_DIS,			\
			  "statstr" : SD_STATLEV_STR,			\
			  "statdex" : SD_STATLEV_DEX,			\
			  "statcon" : SD_STATLEV_CON,			\
			  "statint" : SD_STATLEV_INT,			\
			  "statwis" : SD_STATLEV_WIS,			\
			  "statdis" : SD_STATLEV_DIS,			\
			  "mortal" : SD_AV_TITLES,		     	\
			  "armour" : SD_ARMOUR,				\
			  "weaponrust" : SD_WEAPON_RUST,		\
			  "weaponcond" : SD_WEAPON_COND,		\
			  "encumbrance": SD_ENC_WEIGHT,			\
		       ])
#endif
