/*
 * Manages cooldowns in players.
 */

mapping cooldowns;

static int cooldown_alarm_id;
static float last_cooldown_time;

#define COOLDOWN_TIME       (0)
#define COOLDOWN_CALLBACK   (1)
#define COOLDOWN_MAX    (86400000.0)  /* The max possible cooldown, if higher
                                       * than this the cooldown is real time. */

static void update_cooldown_state(int mode);

string
stat_cooldowns()
{
    update_cooldown_state(0);

    if (!cooldowns || !m_sizeof(cooldowns))
        return "";

    float current = gettimeofday();
    string str = sprintf("%-30s %s\n", "Cooldown Key", "Remaining (s)");

    foreach (string key, mixed cooldown: cooldowns)
    {
        float left = current - cooldown[COOLDOWN_TIME];

        if (cooldown[COOLDOWN_TIME] < COOLDOWN_MAX) {
            left = cooldown[COOLDOWN_TIME];
        }
        str += sprintf("%-30s %f7.2\n", key, left);
    }

    return str + "\n";
}

/*
 * Function name: trigger_cooldown
 * Description  : Triggers a "cooldown". Cooldowns are cleared after
 *                duration has passed. Active cooldowns can only be extended.
 *
 * Arguments    : string - A unique key identifying the cooldown.
 *                float  - Seconds, how long the cooldown should be active.
 *                int    - Offline, when true the cooldown will count down when
 *                         the player is offline, defaults to false.
 *                function - A callback executed when the cooldown expired.
 *                           The function might not be call if something else
 *                           refreshes the cooldown or if the player re-logs.
 *
 * Returns      : int - true if the cooldown was activated / extended
 */
int
trigger_cooldown(string key, float duration, int offline = 0, function expire = 0)
{
    if (!mappingp(cooldowns))
        cooldowns = ([ ]);

    /* Update time remaining and check if this is an extension */
    update_cooldown_state(0);

    float now =  gettimeofday();
    if (cooldowns[key])
    {
        float prev = cooldowns[key][COOLDOWN_TIME];
        float left = prev > COOLDOWN_MAX ? prev - now : prev;

        /* Is the current expiration longer? */
        if (duration < prev) {
            update_cooldown_state(1);
            return 0;
        }

        cooldowns[key] = ({ offline ? now + duration : duration, expire });
        call_hook(HOOK_COOLDOWN_REFRESH, key, duration);
    } else {
        cooldowns[key] = ({ offline ? now + duration : duration, expire });
        call_hook(HOOK_COOLDOWN_START, key, duration);
    }

    update_cooldown_state(1);
    return 1;
}

/*
 * Function name: query_cooldown
 * Arguments:     string - the cooldown
 * Returns:       int - true if the cooldown is active
 */
int
query_cooldown(string key)
{
    if (!mappingp(cooldowns))
        return 0;

    update_cooldown_state(0);
    return pointerp(cooldowns[key]);
}

/*
 * Function name: update_cooldown_state
 * Description  : Clears any cooldowns which are expired and schedules the
 *                alarm for the next cooldown.
 */
static void
update_cooldown_state(int schedule = 1)
{
    if (!mappingp(cooldowns))
        return;

    if (schedule && cooldown_alarm_id)
    {
        remove_alarm(cooldown_alarm_id);
        cooldown_alarm_id = 0;
    }

    float now = gettimeofday();
    float delta = last_cooldown_time ? now - last_cooldown_time : 0.0;
    float next = 600.0;

    foreach (string key, mixed cooldown: cooldowns)
    {
        float expiration = cooldown[COOLDOWN_TIME];
        float left;

        if (expiration > COOLDOWN_MAX)
        {
            left = expiration - now;
        } else  {
            cooldown[COOLDOWN_TIME] -= delta;
            left = cooldown[COOLDOWN_TIME];
        }

         if (left <= 0.0) {
            function callback =  cooldown[COOLDOWN_TIME];
            m_delkey(cooldowns, key);
            call_hook(HOOK_COOLDOWN_EXPIRED, key);

            if (functionp(callback)) {
                try {
                    callback();
                } catch (string err) {
                    if (query_wiz_level())
                        tell_object(this_object(), err);
                    else
                       tell_object(this_object(), "You notice a wrongness in " +
                        "the fabric of space.\n");
                }
            }

         } else if (left < next) {
            next = left;
         }
    }

    last_cooldown_time = now;

    if (schedule)
        cooldown_alarm_id = set_alarm(next, 0.0, &update_cooldown_state());
}
