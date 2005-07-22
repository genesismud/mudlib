/* 
 * /sys/const.h
 *
 * Useful constants.
 */

#ifndef CONST_DEF
#define CONST_DEF

/*
 * These definitions should be used with set_gender() and query_gender().
 *
 * G_MALE   - the living is male.
 * G_FEMALE - the living is female.
 * G_NEUTER - the living is neuter.
 */
#define G_MALE   0
#define G_FEMALE 1
#define G_NEUTER 2

/*
 * These definitions can be used to define the guild types. They will
 * add as binary digits as they can be stacked.
 *
 * G_RACE         - racial guild.
 * G_LAYMAN       - layman guild.
 * G_OCCUPATIONAL - occupational guild.
 * G_CRAFT        - craft guild.
 */
#define G_RACE         1
#define G_LAYMAN       2
#define G_OCCUPATIONAL 4
#define G_CRAFT        8

/*
 * MAX_ARRAY_SIZE is the maximum size arrays are allowed to have.
 */
#define MAX_ARRAY_SIZE 900

/* The QUEST_FACTOR is the factor by which quest experience is to be
 * multiplied. A factor of 10 = normal, 9 = 0.9 * normal and
 * 100 = 10 * normal
 */
#define QUEST_FACTOR 50

/* These flags are used in the call to the armageddon() routine in domain_link
 * to indicate the armageddon status.
 *
 * ARMAGEDDON_ANNOUNCE - Armageddon first wakes up to give his warning.
 * ARMAGEDDON_CANCEL   - Armageddon doesn't shut down after all.
 * ARMAGEDDON_SHUTDOWN - Shutdown time has arrived.
 */
#define ARMAGEDDON_ANNOUNCE 1
#define ARMAGEDDON_CANCEL   2
#define ARMAGEDDON_SHUTDOWN 3

/* No definitions beyond this line. */
#endif CONST_DEF
