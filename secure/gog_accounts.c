/*
 * /secure/gog_accounts.c
 *
 * This object stores the accounts the players have at the Gnomes of Genesis.
 *
 * Coded by Tintin, 1992
 *
 * Version 2.0 by Mercade, July 1995
 * - code cleanup and use of disk cache.
 *
 * Version 3.0 by Mercade, May 2006
 * - added processing from deposit object to here.
 * - introduction of gem deposits.
 * - stricter client server approach.
 */

#pragma no_clone
#pragma no_inherit
#pragma no_shadow
#pragma save_binary
#pragma strict_types

inherit "/lib/cache.c";

#include <composite.h>
#include <log.h>
#include <macros.h>
#include <mail.h>
#include <math.h>
#include <money.h>
#include <options.h>
#include <ss_types.h>
#include <std.h>

#include "/d/Genesis/sys/deposit.h"

/*
 * Global variable.
 */
static private string  current_user = 0;
static private mapping current_account = 0;
static private mapping gem_deposits = ([ ]);
static private mapping transfers = ([ ]);

/*
 * Each account is saved in a separate file. The account contains both coins
 * and gems. The coins are listed in the number of coins per type. The gem
 * deposits are identified with gems## where ## is the gem bank identifier
 * that refers to a certain gem deposit as defined by the domain. The ## is
 * defined as: "100 * get_domain_number() + Sequence". The Sequence is a
 * domain specific sequential number as listed in d/<domain>/open/SHOPS.
 *
 * ([ "coins"  : ({ (int)copper, (int)silver, (int)gold, (int) platinum })
 *    "time"   : (int) last time() modified
 *    "fee"    : (int) fees due in copper coins
 *    "gems##" : ([ (string) gem filename : (int) number of gems ])
 * ])
 *
 * The mapping gem_deposits contains a list of the registered banks and their
 * location description. This must fit in the sentence "The bank in <desc>."
 *
 * ([ "gems##" : (string) description of location ])
 *
 * The mapping transfers contains a list of the transfers of gems as
 * requested by players. The index isn't really relevant (it's a time stamp by
 * lack of anything better), but using a mapping makes it easier to save to
 * disk.
 *
 * ([ index : ([ "name" : (string) - the name of the player
 *               "from" : (int) - the bank ID to transfer from
 *               "to"   : (int) - the bank ID to transfer to
 *               (string) gem filename : (int) number of gems ]) ])
 */

/*
 * Prototype.
 */
static void remove_idle_accounts(int letter);
static void consolidate_accounts();

/*
 * Function name: create
 * Description  : This function is called when the object is created.
 *                It makes sure that all accounts are up to date and
 *                every month it cleans up old accounts.
 */
public void
create()
{
    setuid();
    seteuid(getuid());

    set_cache_size(25);

    set_alarm(10.0, 0.0, &remove_idle_accounts(0));
    
    transfers = restore_map(GEM_TRANSFERS);
    if (!mappingp(transfers))
    {
        transfers = ([ ]);
    }
    if (m_sizeof(transfers))
    {
        set_alarm(5.0, 0.0, consolidate_accounts);
    }

    gem_deposits = restore_map(GEM_BANKS_FILE);
    if (!mappingp(gem_deposits))
    {
        gem_deposits = ([ ]);
    }
}

/*
 * Function name: register_bank
 * Description  : Banks must be registered in order to accept gems.
 * Arguments    : int bank_id - the bank ID (see header for description)
 *                string desc - the location of the bank.
 *                    It must fit in the sentence "The branch in <desc>."
 */
public void
register_bank(int bank_id, string desc)
{
    string bank_name = BANK_NAME(bank_id);

    if (gem_deposits[bank_name] && (desc != gem_deposits[bank_name]))
    {
        log_file(LOG_GOG_BANKS, "Bank " + bank_name + " renamed from \"" +
            gem_deposits[bank_name] + "\" to \"" + desc + "\"\n");
    }
    if (strlen(desc))
    {
        gem_deposits[bank_name] = desc;
    }
    save_map(gem_deposits, GEM_BANKS_FILE);
}

/*
 * Function name: query_bank_desc
 * Description  : Get the description of a bank from its ID.
 * Arguments    : string - the bank_id - the bank ID.
 * Return       : string - the description, or 0.
 */
public string
query_bank_desc(int bank_id)
{
    string bank_name = BANK_NAME(bank_id);

    return gem_deposits[bank_name];
}

/*
 * Function name: find_bank_by_desc
 * Description  : Based on the description of (the location of) a bank, find
 *                the corresponding bank ID.
 * Arguments    : string desc - the description
 * Returns      : int - the bank ID, or 0.
 */
public int
find_bank_by_desc(string desc)
{
    desc = lower_case(desc);
    foreach(string bank_name, string bank_desc: gem_deposits)
    {
        if (desc == lower_case(bank_desc))
        {
            return BANK_ID_BY_NAME(bank_name);
        }
    }
    return 0;
}

/*
 * Function name: log_transaction
 * Description  : Logs a transaction related to the current account.
 * Arguments    : int transaction - the type of transaction
 *                string text - the text to log.
 *                string name - the account in question.
 */
static varargs void
log_transaction(int transaction, string text, string name = current_user)
{
    string file;

    /* Detect tampering. */
    if (this_interactive())
    {
        if (this_interactive()->query_real_name() != name)
        {
            text += " (Action by " + capitalize(this_interactive()->query_real_name()) + ")";
        }
    }
    else
    {
        text += " (Action by: " + file_name(previous_object()) + ")";
    }

    switch(transaction)
    {
    case TRANSACTION_GEMS:
        file = LOG_GOG_GEMS;
        break;
    case TRANSACTION_COINS:
        file = LOG_GOG_COINS;
        break;
    default:
        file = LOG_GOG_ACCOUNT;
    }

    log_file(file, ctime(time()) + " " + FORMAT_NAME(name) + ": " + text + "\n", LOG_SIZE_1M);
}

/*
 * Function name: query_has_account
 * Description  : Find out whether a certain player has an account.
 * Arguments    : string name - the (lower case) name of the player.
 * Returns      : int 1/0 - if true, the account exists.
 */
public int
query_has_account(string name)
{
    name = lower_case(name);

    return (file_size(DEPOSIT_FILE(name) + ".o") > 0);
}

/*
 * Function name: convert_old_account
 * Description  : This routine will automatically convert an old GoG account
 *                into the new format.
 */
static void
convert_old_account()
{
    /* Convert the coins into an array. */
    current_account[DEPOSIT_COINS] = ({ current_account[DEPOSIT_OLD_CC],
        current_account[DEPOSIT_OLD_SC], current_account[DEPOSIT_OLD_GC],
        current_account[DEPOSIT_OLD_PC] });
         
    /* Let's be nice and wipe the fee, if any. */
    current_account[DEPOSIT_TIME] = time();
    current_account[DEPOSIT_FEE] = 0;

    /* Remove the old account identifiers. */
    m_delkey(current_account, DEPOSIT_OLD_CC);
    m_delkey(current_account, DEPOSIT_OLD_SC);
    m_delkey(current_account, DEPOSIT_OLD_GC);
    m_delkey(current_account, DEPOSIT_OLD_PC);
    m_delkey(current_account, DEPOSIT_OLD_FD);
    m_delkey(current_account, DEPOSIT_OLD_TM);
}

/*
 * Function name: load_account
 * Description  : Internal routine to make sure the current account is active
 *                before doing further processing.
 * Arguments    : string name - the (lower case) name of the account holder.
 *                int nonew - if the account does not exist, do not create one.
 * Returns      : int 1/0 - if true the account exists, or is created.
 */
public varargs int
load_account(string name, int nonew = 0)
{
    name = lower_case(name);
    if (!strlen(name))
    {
    	return 0;
    }
    if (current_user == name)
    {
        return 1;
    }

    /* If the account exists, load it from disk (cache). */
    if (query_has_account(name))
    {
        current_user = name;
        current_account = read_cache(DEPOSIT_FILE(current_user));
        /* The "tm" is the only element we know for sure to be nonzero in
         * old accounts. */
        if (current_account[DEPOSIT_OLD_TM])
        {
            convert_old_account();
        }
        return 1;
    }
    /* Don't create a new account if we don't want it. */
    if (nonew)
    {
        return 0;
    }
    current_user = name;
    current_account = ([
        DEPOSIT_COINS: allocate(SIZEOF_MONEY_TYPES),
        DEPOSIT_TIME : time(),
        DEPOSIT_FEE  : 0
        ]);
    return 1;
}

/*
 * Function name: save_account
 * Description  : Internal routine to make sure the current account is stored
 *                safely to disk after processing.
 */
static void
save_account()
{
    save_cache(current_account, DEPOSIT_FILE(current_user));
}

/*
 * Function name: update_fee
 * Description  : Recalculate the fees that are due. Do not actually charge
 *                the fee. That is done in remove_fee().
 * Arguments    : string name - the (lower case) name of the player.
 */
public void
update_fee(string name)
{
    int delay;
    int index;
    int value;

    if (!load_account(name, 1)) return;

    /* Don't recalculate on successive transactions. */
    delay = time() - current_account[DEPOSIT_TIME];
    if (delay < GOG_MIN_FEE_TIME)
    {
        return;
    }
    delay = min(delay, GOG_MAX_FEE_TIME);

    /* Calculate the value of the account. Don't calculate fee over the
     * fee already due. */
    value = -current_account[DEPOSIT_FEE];
    for (index = 0; index < SIZEOF_MONEY_TYPES; index++)
    {
        value += (current_account[DEPOSIT_COINS][index] * MONEY_VALUES[index]);
    }

    current_account[DEPOSIT_TIME] = time();
    /* Multiply the total value of the account with the annual percentage
     * and then multiply with the fraction of the year. Formula ordered for
     * maximum precesion using integer calculus. */
    if (value > 0)
    {
        current_account[DEPOSIT_FEE] += ((value * GOG_YEAR_PROC_FEE * delay) / 100 / GOG_ONE_YEAR);
    }
}

/*
 * Function name: remove_fee
 * Description  : Removes the fee that is due from the active account.
 * Arguments    : string name - the (lower case) name of the player.
 * Returns      : int 1/0 - false if the account does not cover the fee.
 */
public int
remove_fee(string name)
{
    int *coins = allocate(SIZEOF_MONEY_TYPES);
    int  index;
    int  amount;

    update_fee(name);
    amount = current_account[DEPOSIT_FEE];
    if (!amount)
    {
        return 1;
    }

    for (index = 0; index < SIZEOF_MONEY_TYPES; index++)
    {
        coins[index] = MONEY_VALUES[index] * current_account[DEPOSIT_COINS][index];
    }
    /* If the account is not big enough to pay the fee, clear the account. */
    if (SUM_ARRAY(coins) < amount)
    {
    	current_account[DEPOSIT_COINS] = allocate(SIZEOF_MONEY_TYPES);
    	log_transaction(TRANSACTION_OTHER, "Account too low to pay " + amount + " copper in fees.");
    	save_account();
    	return 0;
    }

    /* Remove the fee from the denominations, smallest first. */
    for (index = 0; index < SIZEOF_MONEY_TYPES; index++)
    {
        if (amount <= coins[index])
        {
            coins[index] -= amount;
            break;
        }
        else
        {
            amount -= coins[index];
            coins[index] = 0;
        }
    }

    /* Level the denominations. Give change where needed. */
    amount = 0;
    for (index = SIZEOF_MONEY_TYPES - 1; index >= 0; index--)
    {
        coins[index] += amount;
        amount = coins[index] % MONEY_VALUES[index];
        coins[index] = coins[index] / MONEY_VALUES[index];
    }
 
    log_transaction(TRANSACTION_COINS, "Paid " + current_account[DEPOSIT_FEE] +
        " copper in fees.");
    current_account[DEPOSIT_COINS] = coins;
    current_account[DEPOSIT_FEE] = 0;
    save_account();
    return 1;
}

/*
 * Function name: query_account_fee
 * Description  : Find out how many fees are due.
 * Arguments    : string name - the (lower case) name of the player.
 * Returns      : int - the fees.
 */
public int
query_account_fee(string name)
{
    if (!load_account(name, 1)) return 0;

    return current_account[DEPOSIT_FEE];
}

/*
 * Function name: query_account_coins
 * Description  : Find out the money part of the account of a player.
 * Arguments    : string name - the (lower case) name of the player.
 * Returns      : int * - the array with the coins, or 0.
 */
public int * 
query_account_coins(string name)
{
    if (!load_account(name, 1)) return 0;

    return secure_var(current_account[DEPOSIT_COINS]);
}

/*
 * Function name: query_account_value
 * Description  : Find out the value of the money part of an account. The
 *                value is corrected for the fees that are due.
 * Arguments    : string name - the (lower case) name of the player.
 * Returns      : int - the value in cc.
 */
public int
query_account_value(string name)
{
    int *coins = query_account_coins(name);

    return (pointerp(coins) ? (MONEY_MERGE(coins) - current_account[DEPOSIT_FEE]) : 0);
}

/*
 * Function name: add_money
 * Description  : Add coins to the trust of the deposit.
 * Arguments    : string name - the (lower case) name of the player.
 *                int coinindex - the coin index of the coin type.
 *                int number - the number of coins deposited.
 * Returns      : int 1/0 - if true, the money was.
 */
public int
add_money(string name, int coinindex, int number)
{
    if ((coinindex < 0) || (coinindex >= SIZEOF_MONEY_TYPES))
    {
        return 0;
    }

    load_account(name);
    current_account[DEPOSIT_COINS][coinindex] += number;
    log_transaction(TRANSACTION_COINS, "Deposit " + number + " " +
        MONEY_TYPES[coinindex] + ".");
    save_account();
    return 1;    
}

/*
 * Function name: remove_money
 * Description  : Remove money from the care of the deposit.
 * Arguments    : string name - the (lower case) name of the player.
 *                int coinindex - the coin index of the coin type.
 *                int number - the number of coins withdrawn.
 * Returns      : int 1/0 - if true, the money was removed.
 */
public int
remove_money(string name, int coinindex, int number)
{
    if ((coinindex < 0) || (coinindex >= SIZEOF_MONEY_TYPES))
    {
        return 0;
    }

    if (!load_account(name, 1)) return 0;

    /* We don't want negative accounts. */
    current_account[DEPOSIT_COINS][coinindex] -= min(number,  current_account[DEPOSIT_COINS][coinindex]);
    log_transaction(TRANSACTION_COINS, "Withdraw " + number + " " +
        MONEY_TYPES[coinindex] + ".");
    save_account();
    return 1;
}

/*
 * Function name: add_gem
 * Description  : Add a gem into the trust of the deposit. Note that the
 *                deposit of gems is strictly tied to the deposit where they
 *                were deposited.
 * Arguments    : string name - the (lower case) name of the player.
 *                int bank_id - the bank identifier of the bank.
 *                string gem - the full path to the gem (without trailing .c)
 *                int number - the number of gems deposited.
 * Returns      : int 1/0 - if true, the gem was added.
 */
public int
add_gem(string name, int bank_id, string gem, int number)
{
    string bank_name = BANK_NAME(bank_id);

    load_account(name);

    if (!mappingp(current_account[bank_name]))
    {
        current_account[bank_name] = ([ ]);
    }

    current_account[bank_name][gem] += number;
    log_transaction(TRANSACTION_GEMS, "Deposit " + number + " " +
        ((number == 1) ? gem->query_short() : gem->query_plural_short()) +
        " at bank " + bank_id + " (" + gem + ").");
    save_account();
    return 1;
}

/*
 * Function name: remove_gem
 * Description  : Remove a gem from the care of the deposit.
 * Arguments    : string name - the (lower case) name of the player.
 *                int bank_id - the bank identifier of the bank.
 *                string gem - the full path to the gem (without trailing .c)
 *                int number - the number of gems to be removed.
 * Returns      : int 1/0 - if true, the gem was removed.
 */
public int
remove_gem(string name, int bank_id, string gem, int number)
{
    string bank_name = BANK_NAME(bank_id);

    if (!load_account(name, 1)) return 0;

if (!bank_id)
{
write_file("/syslog/log/GOG_CHEAT", ctime(time()) + " " + capitalize(name) +
    " retrieves " + number + " " + ((number == 1) ? gem->query_short() : gem->query_plural_short()) +
    " in " + file_name(environment(this_player())) + " (" + gem + ").\n");
}

    /* May have gems, but not at this bank. */
    if (!mappingp(current_account[bank_name])) return 0;

    /* Must have at least the number of gems to be removed. */
    if (current_account[bank_name][gem] < number) return 0;

    /* Remove the gem(s) from the account. */
    current_account[bank_name][gem] -= number;
    /* Remove the gem type if no gems left. */
    if (current_account[bank_name][gem] < 1)
    {
        m_delkey(current_account[bank_name], gem);
    }
    /* Remove the bank if no gem types left. */
    if (!m_sizeof(current_account[bank_name]))
    {
        m_delkey(current_account, bank_name);
    }

    log_transaction(TRANSACTION_GEMS, "Retrieve " + number + " " +
        ((number == 1) ? gem->query_short() : gem->query_plural_short()) +
        " from bank " + bank_id + " (" + gem + ").");
    save_account();
    return 1;
}

/*
 * Function name: query_has_gems
 * Description  : Find out whether the player has gems stored, and if so, where.
 * Arguments    : string name - the (lower case) name of the player.
 *                bank_id - the gem bank identifier of the bank where we are.
 * Returns      : 0 - no gems, anywhere.
 *                1 - gems at the requested bank, but not elsewhere.
 *                2 - no gems at the requested bank, but gems elsewhere.
 *                3 - gems both at the requested bank and elsewhere.
 */
public int
query_has_gems(string name, int bank_id)
{
    string bank_name = BANK_NAME(bank_id);
    string *bank_names;
    int result = 0;

    if (!load_account(name, 1)) return 0;

    bank_names = m_indices(current_account);
    /* If the we have gems at the bank, bit 1 gets set. */
    if (IN_ARRAY(bank_name, bank_names))
    {
        result = 1;
        bank_names -= ({ bank_name });
    }
    /* If we (also) have other banks, bit 2 gets set. Only count the bank names,
     * that is, names starting with the DEPOSIT_GEMS prefix. */
    if (sizeof(filter(bank_names, &wildmatch(DEPOSIT_GEMS + "*", ))))
    {
        result |= 2;
    }
    
    return result;
}

/*
 * Function name: query_account_gems
 * Description  : Find out how many gems a player has at a bank, in total,
 *                or in other bank.
 * Arguments    : string name - the name of the player.
 *                bank_id - the gem bank identifier. This can have three forms:
 *                   id - list all gems for the "id" bank.
 *                    0 - list all gems
 *                  -id - list all gems, except for the "id" bank.
 * Returns      : mapping - ([ (string) gem filename : (int) number of gems ]), or 0.
 */
public mapping
query_account_gems(string name, int bank_id)
{
    /* Always get the correct bank name, regardless of signage. */
    string bank_name = BANK_NAME(abs(bank_id));
    mapping gems = ([ ]);

    if (!load_account(name, 1)) return 0;

    /* We want a specific bank. */
    if (bank_id > 0)
    {
        return secure_var(current_account[bank_name]);
    }

    foreach(string bank_index: m_indices(current_account))
    {
        /* We only want the mapppings (== banks) */
        /* If the bank_name matches, exclude it. */
        if (mappingp(current_account[bank_index]) &&
            (bank_index != bank_name))
        {
            foreach(string gem_index: m_indices(current_account[bank_index]))
            {
                gems[gem_index] += current_account[bank_index][gem_index];
            }
        }
    }

    return gems;
}

/*
 * Function name: query_bank_gems_value
 * Description  : Internal routine to calculate the value of the gems of a
 *                certain bank.
 * Precondition : The current_account of the player must be loaded.
 * Arguments    : string bank_name - the name of the bank.
 * Returns      : int - the value (as seen by current_user).
 */
static int
query_bank_gems_value(string bank_name)
{
    object player = find_player(current_user);
    int bank_id = BANK_ID_BY_NAME(bank_name);
    int skill = (1000 / (player->query_skill(SS_APPR_VAL) + 1));
    int value = 0;

    foreach(string gem, int number: current_account[bank_name])
    {
        value += QUERY_GEM_VALUE(gem) * number;
    }

    /* Use the bank_id a seed for consistency. */
    skill = random(skill, bank_id);
    value += (((skill % 2 ? -skill % 70 : skill) * value) / 100);
    /* Round to the nearest gold coin. */
    return (((value + 71) / 144) * 144);
}

/*
 * Function name: query_gems_value
 * Description  : Find out the value of the gems a player has in storage at a
 *                bank or in total.
 * Arguments    : string name - the name of the player.
 *                bank_id - the gem bank identifier, or 0 for all banks.
 * Returns      : int - the value in coppers.
 */
public int
query_gems_value(string name, int bank_id)
{
    int value = 0;

    if (!load_account(name, 1)) return 0;

    if (bank_id > 0)
    {
        return query_bank_gems_value(BANK_NAME(bank_id));
    }

    foreach(string bank_index: m_indices(current_account))
    {
        /* We only want the mapppings (== banks) */
        if (mappingp(current_account[bank_index]))
        {
            value += query_bank_gems_value(bank_index);
        }
    }
    return value;
}

/*
 * Function name: query_accounts
 * Description  : Find out at which banks a player has a (gem) account.
 * Arguments    : string name - the name of the player.
 * Returns      : int * - the bank ID's of the banks, or 0.
 */
public int *
query_accounts(string name)
{
    int *bank_ids = ({ });

    if (!load_account(name, 1)) return 0;

    foreach(string bank_index: m_indices(current_account))
    {
        bank_ids += ({ BANK_ID_BY_NAME(bank_index) });
    }
    return bank_ids - ({ 0 });
}

/*
 * Function name: consolidate_account
 * Description  : This routine will add the gems that are in transit to
 *                the account of the player.
 * Arguments    : string code - the index into the consolidation mapping.
 */
public void
consolidate_account(string code)
{
    mapping transit = transfers[code];
    string name;
    int    bank_id;
    string bank_name;
    string bank_desc;

    if (!mappingp(transit))
    {
        return;
    }

    name = transit[DEPOSIT_NAME];
    bank_id = transit[DEPOSIT_TO];
    bank_name = BANK_NAME(bank_id);
    /* This would be very bad! We cannot find the "to" bank. */
    if (!gem_deposits[bank_name])
    {
        CREATE_MAIL("Consolidation of gems failed", "GoG", name, "",
            "The consolidation of your gems under number " + code +
            " has failed. Please bring this to the attention of a " +
            "knowledgeble archwizard.\n");
        log_transaction(TRANSACTION_GEMS, "Consolidation " + code +
            " to non-existing bank " + bank_id + ".", name);
        return;
    }

    /* Remove all non-gem elements from the transit mapping. */
    m_delkey(transit, DEPOSIT_NAME);
    m_delkey(transit, DEPOSIT_FROM);
    m_delkey(transit, DEPOSIT_TO);

    /* Move all gems to the new bank. */
    load_account(name, 0);
    if (!mappingp(current_account[bank_name]))
    {
        current_account[bank_name] = ([ ]);
    }

    /* Do the actual moving of the gems. */
    foreach(string gem, int number: transit)
    {
        current_account[bank_name][gem] += number;
        log_transaction(TRANSACTION_GEMS, "Consolidate " + number + " " +
            ((number == 1) ? gem->query_short() : gem->query_plural_short()) +
            " to bank " + bank_id + " (" + gem + ").");
    }
    m_delkey(transfers, code);
    save_map(transfers, GEM_TRANSFERS);
    save_account();

    bank_desc = (gem_deposits[bank_name] ? gem_deposits[bank_name] : "us");
    CREATE_MAIL("Gems safely reached " + bank_desc, "GoG", name, "",
        "Greetings " + capitalize(name) + ",\n\nThe Gnomes of Genesis are " +
        "pleased to inform you that your gems have safely reached " +
        bank_desc + ". They are placed in your deposit box and are " +
        "available to you during opening hours.\n\nThe Gnomes of Genesis\n");
}

/*
 * Function name: consolidate_accounts
 * Description  : In case there are transfers pending after a reboot,
 *                perform the consolidation without further delay.
 */
static void
consolidate_accounts()
{
    foreach(string code: m_indices(transfers))
    {
        set_alarm(5.0, 0.0, &consolidate_account(code));
    }
}

/*
 * Function name: consolidate_gems
 * Description  : Prepare a gem deposit for consolidation to another bank.
 * Arguments    : string name - the name of the player.
 *                int from_id - the bank ID to move the gems from.
 *                int to_id - the bank ID to move the gems to.
 *                int fee - the fee that is charged.
 */
public void
consolidate_gems(string name, int from_id, int to_id, int fee)
{
    int index;
    string code;
    string bank_name;

    load_account(name, 0);
    if (name != current_user)
    {
        return;
    }

    bank_name = BANK_NAME(from_id);
    if (!mappingp(current_account[bank_name]))
    {
        return;
    }

    /* Find the next available index code. */
    index = time();
    while(1)
    {
        code = "C" + index;
        if (!mappingp(transfers[code])) break;
        index++;
    }

    /* Copy the gems from the bank into the transit structure. */
    transfers[code] = ([ ]);
    foreach(string gem, int number: current_account[bank_name])
    {
        transfers[code][gem] = number;
    }
    transfers[code][DEPOSIT_NAME] = name;
    transfers[code][DEPOSIT_FROM] = from_id;
    transfers[code][DEPOSIT_TO] = to_id;
    save_map(transfers, GEM_TRANSFERS);

    /* Charge the fee. */
    current_account[DEPOSIT_FEE] += fee;
    remove_fee(name);

    /* Remove the gems from the bank. They are now in transit. */
    m_delkey(current_account, bank_name);
    save_account();

    log_transaction(TRANSACTION_GEMS, "Consolidation prepared from bank " +
        from_id + " to bank " + to_id + ".");

    set_alarm(itof(GOG_TRANSIT_TIME + random(GOG_TRANSIT_TIME)), 0.0,
        &consolidate_account(code));
}

/*
 * Function nane: rename_account
 * Description  : In the case of a namechange of the player, the account has
 *                to be renamed, too. It only works if the new account does
 *                not yet exist. After all, it's not called merge_account().
 * Arguments    : string oldname - the old name of the person
 *                string newname - the new name of the person.
 * Returns      : int 1/0 - success/failure.
 */
public int
rename_account(string oldname, string newname)
{
    oldname = lower_case(oldname);
    newname = lower_case(newname);
    if (!strlen(oldname) || !strlen(newname))
    {
    	return 0;
    }

    /* This is quite possible when renaming players who don't have an account
     * at the GoG yet.
     */
    if (!query_has_account(oldname))
    {
	/* No need to spam the log with renames on people with no account.
        log_transaction(TRANSACTION_OTHER, "Rename on non-existing account.", oldname);
	 */
	return 0;
    }
    /* Merging will have to be done manually. */
    if (query_has_account(newname))
    {
        log_transaction(TRANSACTION_OTHER, "Cannot rename to existing " +
	    capitalize(newname) + ".", oldname);
	return 0;
    }

    /* Remove from the cache before renaming the file. */
    remove_from_cache(DEPOSIT_FILE(oldname));
    current_user = 0;
    rename(DEPOSIT_FILE(oldname) + ".o", DEPOSIT_FILE(newname) + ".o");
    log_transaction(TRANSACTION_OTHER, "Renamed to " + capitalize(newname) + ".", oldname);
    return 0;
}

/*
 * Function name: remove_account
 * Description  : With this function the account of a player can be removed.
 * Arguments    : string name - the name of the person.
 * Returns      : int 1/0 - success/failure.
 */
public int
remove_account(string name)
{
    name = lower_case(name);
    if (!strlen(name) || !query_has_account(name))
    {
    	return 0;
    }

    log_transaction(TRANSACTION_OTHER, "Account removed.", name);
    rm_cache(DEPOSIT_FILE(name));
    current_user = 0;
    return 1;
}

/*
 * Function name: remove_idle_accounts
 * Description  : This function will loop over all accounts and checks
 *                whether they are still owned by a real player. If not,
 *                the account is removed.
 * Arguments    : int letter - the index to the next letter to be removed.
 */
static void
remove_idle_accounts(int letter)
{
    string *files = get_dir(DEPOSIT_FILE(ALPHABET[letter..letter] + "*.o"));

    files = map(files, &extract(, 0, -3));
    foreach(string name: files)
    {
        /* Remove the account if it belongs to a wizard or to a mortal that
         * does not exist any more.
        if (SECURITY->query_wiz_level(name) ||
            !(SECURITY->exist_player(name)))
        {
            log_transaction(TRANSACTION_OTHER, "Account purged.", name);
            rm_cache(DEPOSIT_FILE(name));
        }
	 */
    }

    if (++letter < strlen(ALPHABET))
    {
        set_alarm(10.0, 0.0, &remove_idle_accounts(letter));
    }
}

/*
 * Function name: account
 * Description  : Implementation of an account command for archwizards++ to
 *                list the contents of the account of a player.
 * Arguments    : string str - the command line argument
 * Returns      : int 1/0
 */
public int
account(string str)
{
    string name;
    int show_gems;
    int  index = SIZEOF_MONEY_TYPES;
    int *coins, *totals, total;
    int *bank_ids;
    mapping gems;
    int     count, value;
    string *text;
    int     scrw = this_player()->query_option(OPT_SCREEN_WIDTH);

    if (WIZ_CHECK < WIZ_ARCH) return 0;

    if (!strlen(str))
    {
        notify_fail("Syntax: Account [-g] <name>\n");
        return 0;
    }
    show_gems = sscanf(name = str, "-g %s", name);

    if (!query_has_account(name))
    {
        notify_fail("Player " + capitalize(name) + " has no account with the GoG.\n");
        return 0;
    }

    coins = query_account_coins(name);
    totals = MONEY_SPLIT(total = MONEY_MERGE(coins));

    write("  Account Minimized  Type\n");
    while(--index >= 0)
    {
        write(sprintf("%9d %9d  %s\n", coins[index], totals[index], MONEY_TYPES[index]));
    }

    bank_ids = GOG_ACCOUNTS->query_accounts(name);
    if (!show_gems)
    {
        write((count = sizeof(bank_ids)) ? ("Gems at " + count + " bank" +
            ((count == 1) ? "" : "s") + ": " +
	    COMPOSITE_WORDS(map(bank_ids, query_bank_desc)) +
	    ". Use -g for details.\n") : "No gem accounts.\n");
        return 1;
    }
    
    foreach(int id: bank_ids)
    {
        write("\nBank " + id + " - " + query_bank_desc(id) + "\n");
        gems = query_account_gems(name, id);
	text = ({ });

        foreach(string gem: sort_array(m_indices(gems)))
        {
            count = gems[gem];
            text += ({ sprintf("%4d %s", count, 
                ((count == 1) ? gem->query_short() : gem->query_plural_short())) });
        }
        scrw = ((scrw >= 40) ? (scrw - 3) : 77);
        write(sprintf("%-*#s\n", scrw, implode(text, "\n")));
        value = GOG_ACCOUNTS->query_gems_value(name, id);
        write("Value: " + MONEY_TEXT_NUM_SPLIT(value) + "\n");
    }

    return 1;
}

/*
 * Function name: remove_object
 * Description  : Call this function to remove the object from the memory.
 * Returns      : int 1 - always.
 */
public int
remove_object()
{
    destruct();
    return 1;
}
