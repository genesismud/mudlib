/*
 * This library can be inherited in an object which wishes to provide
 * other objects with the possibility to listen in on various events
 * without resorting to shadows.
 * 
 */
static mapping hooks = ([ ]);

/* 
 * Function name: add_hook
 * Description  : Add a callback to the callback list
 *                The callback function will be called with different arguments
 *                depending on which hook its attached to.
 * 
 * Arguments    : string name - The hook name, usually defined in /sys/hooks.h
 *                function callback - The callback function. 
 */
void
add_hook(string name, function callback)
{
    if (!pointerp(hooks[name]))
        hooks[name]  = ({ });
    hooks[name] |= ({ callback });
}

/* 
 * Function name: remove_hook
 * Description  : Remove a hook from the provided hook name.
 *                If an object is provided all functions which reside in that 
 *                object will be removed. This is often the safest way to do it
 *                as comparison of function pointers is unreliable.
 *
 * Argument     : string - the hook name
 *                object / function   
 *                                    
 */
void
remove_hook(string name, mixed callback)
{
    if (!hooks[name])
        return;

    if (functionp(callback))
        hooks[name] -= ({ 0, callback });

    if (objectp(callback)) 
    {
        function *remove = ({ 0 });
        foreach (function f: hooks[name])
        {
            if (!functionp(f))
                continue;

            if (function_object(f) == callback) {
                remove += ({ f });
            }
        }
        hooks[name] -= remove;
    }
}

/*
 * Function name: call_hook
 * Description  : Calls all the function which have registered themselves
 *                as listening to a specific hook.
 * Example      : call_hook(HOOK_PLAYER_MOVED, source, dest);
 * 
 * Arguments    : string  - The hook name
 *                varargs - Any extra arguments to be included.
 *                          These should be documented.
 */
void
call_hook(string name, ...) 
{
    if (!hooks[name])
        return;

    foreach (function callback: hooks[name]) 
    {
        if (!functionp(callback)) 
        {
            hooks[name] -= ({ callback });
            continue;
        }

        applyv(callback, argv); 
    }
}

