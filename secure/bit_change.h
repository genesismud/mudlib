/*
 * bit_change.h
 *
 * This header file contains the actual bits to modify.
 */

/*
 * DOMAIN is the name of the domain of which the bits are manipulated.
 * When bits are moved, SOURCE can be the domain where the bits are moved
 * from. When it is undefined, the bits are moved within DOMAIN.
 * LOGFILE is the file in which to log the changes.
 */
#define SOURCE  ("Emerald")
#define DOMAIN  ("Emerald")
#define LOGFILE ("/d/Emerald/log/BIT_CHANGE.171098_2")

/*
 * Due to the trickyness of manipulating bits only one person can do this
 * at one time. ALLOWED contains his/her name.
 */
#define ALLOWED ("shiva")

/*
 * CHANGE contains the actual changes that have to be made to the bits of
 * the players. The format is an array or arrays as follows:
 *
 * CHANGE ({ ({ "remove", a, b }),
 *           ({ "moveall", c, d, e, f }),
 *           ({ "moveset", c, d, e, f }),
 *           ({ "copyset", c, d, e, f }) })
 *
 * "remove" removes bit 'b' from group 'a'.
 * "moveall" moves bit 'd' in group 'c' to bit 'f' in group 'e', including
 *     empty bits, ergo delete the target if the source is not set.
 * "moveset" moves bit 'd' in group 'c' to bit 'f' in group 'e', but only
 *     if the source is set. An empty source bit doesn't clean the target.
 * "copyset" copies bit 'd' in group 'c' to bit 'f' in group 'e', but only
 *     if the source is set. The source bit is not cleared.
 */
#define CHANGE  ({ \
                   ({ "remove", 0,  0 }), \
                   ({ "remove", 0,  1 }), \
                   ({ "remove", 0,  2 }), \
                   ({ "remove", 0,  3 }), \
                   ({ "remove", 0,  4 }), \
                   ({ "remove", 0,  5 }), \
                   ({ "remove", 0,  6 }), \
                   ({ "remove", 0,  7 }), \
                   ({ "remove", 0,  8 }), \
                   ({ "remove", 0,  9 }), \
                   ({ "remove", 0,  10 }), \
                   ({ "remove", 0,  11 }), \
                   ({ "remove", 0,  12 }), \
                   ({ "remove", 0,  13 }), \
                   ({ "remove", 0,  14 }), \
                   ({ "remove", 0,  15 }), \
                   ({ "remove", 0,  16 }), \
                   ({ "remove", 0,  17 }), \
                   ({ "remove", 0,  18 }), \
                   ({ "remove", 0,  19 }), \
                   ({ "remove", 1,  0 }), \
                   ({ "remove", 1,  1 }), \
                   ({ "remove", 1,  2 }), \
                   ({ "remove", 1,  3 }), \
                   ({ "remove", 1,  4 }), \
                   ({ "remove", 1,  5 }), \
                   ({ "remove", 1,  6 }), \
                   ({ "remove", 1,  7 }), \
                   ({ "remove", 1,  8 }), \
                   ({ "remove", 1,  9 }), \
                   ({ "remove", 1,  10 }), \
                   ({ "remove", 1,  11 }), \
                   ({ "remove", 1,  12 }), \
                   ({ "remove", 1,  13 }), \
                   ({ "remove", 1,  14 }), \
                   ({ "remove", 1,  15 }), \
                   ({ "remove", 1,  16 }), \
                   ({ "remove", 1,  17 }), \
                   ({ "remove", 1,  18 }), \
                   ({ "remove", 1,  19 }), \
                   ({ "remove", 2,  1 }), \
                   ({ "remove", 2,  2 }), \
                   ({ "remove", 2,  3 }), \
                   ({ "remove", 2,  4 }), \
                   ({ "remove", 2,  5 }), \
                   ({ "remove", 2,  6 }), \
                   ({ "remove", 2,  7 }), \
                   ({ "remove", 2,  8 }), \
                   ({ "remove", 2,  9 }), \
                   ({ "remove", 2,  10 }), \
                   ({ "remove", 2,  11 }), \
                   ({ "remove", 2,  12 }), \
                   ({ "remove", 2,  13 }), \
                   ({ "remove", 2,  14 }), \
                   ({ "remove", 2,  15 }), \
                   ({ "remove", 2,  16 }), \
                   ({ "remove", 2,  17 }), \
                   ({ "remove", 2,  18 }), \
                   ({ "remove", 2,  19 }), \
                   ({ "remove", 3,  0 }), \
                   ({ "remove", 3,  2 }), \
                   ({ "remove", 3,  3 }), \
                   ({ "remove", 3,  4 }), \
                   ({ "remove", 3,  5 }), \
                   ({ "remove", 3,  6 }), \
                   ({ "remove", 3,  7 }), \
                   ({ "remove", 3,  8 }), \
                   ({ "remove", 3,  9 }), \
                   ({ "remove", 3,  10 }), \
                   ({ "remove", 3,  11 }), \
                   ({ "remove", 3,  12 }), \
                   ({ "remove", 3,  13 }), \
                   ({ "remove", 3,  14 }), \
                   ({ "remove", 3,  15 }), \
                   ({ "remove", 3,  16 }), \
                   ({ "remove", 3,  17 }), \
                   ({ "remove", 3,  18 }), \
                   ({ "remove", 3,  19 }), \
    })

/*
 * These are the indices to the CHANGE definition array.
 */
#define INDEX_COMMAND      (0)
#define INDEX_SOURCE_GROUP (1)
#define INDEX_SOURCE_BIT   (2)
#define INDEX_DEST_GROUP   (3)
#define INDEX_DEST_BIT     (4)
