/*
 * container.c
 *
 * Contains all routines relating to objects that can hold other objects
 *
 */

#pragma save_binary
#pragma strict_types

inherit "/std/object";  

/*
   Defined functions and variables:

   set_room(string/object)        Set the room that is the internal of this
                                  object. If this is set there is no objects
                                 in this object.

   update_light()                 This is usually not called except when an
                                  object CHANGES light status inside this
                                  container.

   There are query_xx() functions for all of the above set_xx() functions.

   These usual functions are also defined:
   reset(), long(), short(), id()
*/

#include <macros.h>
#include <stdproperties.h>
#include <composite.h>
#include <subloc.h>
#include <ss_types.h>

/*
 * All variables in this file must be declared static. This is to ensure
 * that the player object higher in the inherit chain can use save and
 * restore object without saving variables from this file. 
 */
static  mixed     cont_linkroom;  /* The room connected to this container */

static  int       cont_cur_light, /* The current light status in cont. */
                  cont_cur_weight,
                  cont_cur_volume,
                  cont_block_prop;

static  mapping   cont_sublocs,    /* Map of sublocations and the object res-
                                      ponsible for the subloc, in container */
                  cont_subloc_ids; /* Map of sublocation ids to sublocation */

static private int *NotifyProps;   /* Properties to notify about changes of */

/*
 * Prototypes
 */
void create_container();
void reset_container();
void update_internal(int l, int w, int v);
public int light();
public nomask int weight();
public nomask int volume();
nomask void fix_ob_subloc_when_remove(object ob, string sloc);
nomask string *map_subloc_id(string *sloc, string rm_sloc);
nomask int filter_subloc_id(string *sloc, string rm_sloc);
nomask int subloc_filter(object ob, mixed sloc);
public varargs string show_sublocs(object for_obj, mixed *slocs);

/*
 * Function name: create_object
 * Description:   Create the container (constructor)
 */
public nomask void
create_object()
{
    cont_block_prop = 0;
    add_prop(CONT_I_IN, 1);         /* Can have things inside it */
    add_prop(OBJ_I_LIGHT, light);   /* The total light of container */
    add_prop(OBJ_I_WEIGHT, weight); /* The total weight of container */
    add_prop(OBJ_I_VOLUME, volume); /* The total volume of container */
    add_prop(CONT_I_REDUCE_WEIGHT, 100); /* No reduction */
    add_prop(CONT_I_REDUCE_VOLUME, 100); /* No reduction */
    cont_block_prop = 1;
    cont_sublocs = ([]);
    cont_subloc_ids = ([]);
    create_container();

    NotifyProps = ({ CONT_I_ATTACH, CONT_I_TRANSP, CONT_I_CLOSED,
			 CONT_I_LIGHT, OBJ_I_LIGHT, CONT_I_WEIGHT,
			 OBJ_I_WEIGHT, CONT_I_VOLUME, OBJ_I_VOLUME });
}

/*
 * Block functions disabling setting of calculated properties
 */
nomask int add_prop_obj_i_light() { return cont_block_prop; }
nomask int add_prop_obj_i_weight() { return cont_block_prop; }
nomask int add_prop_obj_i_volume() { return cont_block_prop; }
nomask int remove_prop_obj_i_light() { return 1; }
nomask int remove_prop_obj_i_weight() { return 1; }
nomask int remove_prop_obj_i_volume() { return 1; }

/*
 * Function name: create_container
 * Description:   Reset the container (standard)
 */
public void
create_container()
{
/* 
  This is nonsense! If we do clone_object() and configure the
  container, then OBJ_I_WEIGHT and OBJ_I_VOLUME are set to a number.
  The vbfc is removed, and the whole stuff won't work!

    ::create_object();
*/

    ::set_name("container");

    add_prop(CONT_I_WEIGHT, 1000);      /* 1 Kg is weight of empty container */
    add_prop(CONT_I_VOLUME, 1000);      /* 1 liter is volume of empty cont. */

    add_prop(CONT_I_MAX_WEIGHT, 1000); /* 1 Kg is max total weight */
    add_prop(CONT_I_MAX_VOLUME, 1000); /* 1 liter is max total volume */
}

/*
 * Function name: reset_object
 * Description:   Reset the container 
 */
public nomask void
reset_object() { reset_container(); }

/*
 * Function name: reset_container
 * Description:   Reset the container (standard)
 */
public void
reset_container() { /* this does not exist any more. ::reset_object(); */ }


/*
 * Function name: volume_left()
 * Description:   Returns the volume left to fill.
 * Returns:       non_negative integer
 */
public int
volume_left()
{
    if (query_prop(CONT_I_RIGID))
        return query_prop(CONT_I_MAX_VOLUME) - query_prop(CONT_I_VOLUME) - 
            cont_cur_volume;
    else
        return query_prop(CONT_I_MAX_VOLUME) - query_prop(OBJ_I_VOLUME);
}

/*
 * Function name: query_internal_light
 * Description:   Returns the lightvalue of object internal to this container
 * Returns:       Lightvalue
 */
public int
query_internal_light() { return cont_cur_light; }

/*
 * Function name: light
 * Description:   Returns the light status in this container
 *                This function is called from query_prop() only.
 * Returns:       Light value
 */
public int
light()
{
    int li;

    li = query_prop(CONT_I_LIGHT);
    if (query_prop(CONT_I_TRANSP) || 
        query_prop(CONT_I_ATTACH) ||
        !query_prop(CONT_I_CLOSED))
    {
        if (cont_linkroom)
            return cont_linkroom->query_prop(OBJ_I_LIGHT) + li;
        else
            return cont_cur_light + li;
    }
    else
        return li;
}


/*
 * Function name: weight
 * Description:   Returns the accumulated weight of the container.
 *                This function is called from query_prop() only.
 * Returns:       Weight value
 */
public nomask int
weight()
{
    int wi;

    wi = query_prop(CONT_I_WEIGHT);
    if (cont_linkroom)
        return cont_linkroom->query_prop(OBJ_I_WEIGHT) + wi;
    else
        return cont_cur_weight + wi;
}

/*
 * Function name: volume
 * Description:   Returns the volume of the container. How much volume it
 *                currently takes up, a nonrigid sack accumulates this.
 *                This function is called from query_prop() only.
 * Returns:       Volume value
 */
public nomask int
volume()
{
    int vo;

    if (query_prop(CONT_I_RIGID))
        return query_prop(CONT_I_MAX_VOLUME);
    vo = query_prop(CONT_I_VOLUME);
    if (cont_linkroom)
        return cont_linkroom->query_prop(OBJ_I_VOLUME) + vo;
    else
        return cont_cur_volume + vo;
}


/*
 * Function name: set_room
 * Description:   Connects a room to the internals of the container.
 * Arguments:     room: The room object or a filename
 */
public void
set_room(mixed room)
{
    if (query_lock())
        return;   /* All changes has been locked out */

    cont_linkroom = room;
}

/*
 * Function name: query_room
 * Description:   Ask what room is connected to the internal of this object
 */
public mixed
query_room() { return cont_linkroom; }

/*
 * Function name: prevent_enter
 * Description:   Called when an object is trying to enter this container to
 *                see if we will allow it in.
 * Arguments:     object ob - the object that is trying to enter.
 * Returns:       1 - The object is not allowed to enter
 *                0 - The object is allowed to enter
 */
public int
prevent_enter(object ob)
{
    return 0;
}

/* 
 * Function name: enter_inv
 * Description:   Called when objects enter this container or when an
 *                object has just changed its weight/volume/light status.
 * Arguments:     ob: The object that just entered this inventory
 *                from: The object from which it came.
 */
public void
enter_inv(object ob, object from)
{
    int l, w, v;

    if (cont_linkroom)
    {
        ob->move(cont_linkroom, 1);
    }

    l = ob->query_prop(OBJ_I_LIGHT);
    w = ob->query_prop(OBJ_I_WEIGHT);
    v = ob->query_prop(OBJ_I_VOLUME);

    update_internal(l, w, v);
}


/*
 * Function name: prevent_leave
 * Description:   Called when an object is trying to leave this container
 *                to see if we allow it to leave.
 * Arguments:     object ob - the object trying to leave
 * Returns:       1 - The object is not allowed to leave
 *                0 - The object is allowed to leave
 */
public int
prevent_leave(object ob)
{
    return 0;
}

/* 
 * Function name: leave_inv
 * Description:   Called when objects leave this container or when an
 *                object is about to change its weight/volume/light status.
 * Arguments:     ob: The object that just left this inventory.
 *                to: Where it went.
 */
public void
leave_inv(object ob, object to)
{
    int l, w, v;

    if (cont_linkroom)
        return; 

    l = ob->query_prop(OBJ_I_LIGHT);
    w = ob->query_prop(OBJ_I_WEIGHT);
    v = ob->query_prop(OBJ_I_VOLUME);

    update_internal(-l, -w, -v);
}

/*
 * Function name: enter_env
 * Description:   The container enters a new environment
 * Arguments:     dest - The destination
 *                old  - From what object
 */
void
enter_env(object dest, object old)
{
    int weight, vol;

    if (dest)
    {
        weight = -cont_cur_weight + cont_cur_weight * 100 /
                dest->query_prop(CONT_I_REDUCE_WEIGHT);

        if (query_prop(CONT_I_RIGID))
            vol = 0;
        else
            vol = -cont_cur_volume + cont_cur_volume * 100 /
                dest->query_prop(CONT_I_REDUCE_VOLUME);

        dest->update_internal(0, weight, vol);
    }
}

/*
 * Function name: leave_env
 * Description:   The container leaves a new environment
 * Arguments:     from - From what object
 *                to   - To what object
 */
void
leave_env(object from, object to)
{
    int weight, vol;

    if (from)
    {
        weight = cont_cur_weight - cont_cur_weight * 100 /
                from->query_prop(CONT_I_REDUCE_WEIGHT);

        if (query_prop(CONT_I_RIGID))
            vol = 0;
        else
            vol = cont_cur_volume - cont_cur_volume * 100 /
                from->query_prop(CONT_I_REDUCE_VOLUME);

        from->update_internal(0, weight, vol);
    }
}

/* 
 * Function name: update_internal
 * Description:   Updates the light, weight and volume of things inside
 *                also updates a possible environment.
 * Arguments:     l: Light diff.
 *                w: Weight diff.
 *                v: Volume diff.
 */
public void
update_internal(int l, int w, int v)
{
    object ob, env;

    cont_cur_light += l;
    cont_cur_weight += w;
    cont_cur_volume += v;

    if (!(env = environment()))
        return;

    /* 
     * There are some containers that does not distribute internal light
     *
     * Transparent containers always do.
     * Containers that has its inventory attached on the outside do.
     * Closed containers dont if none of the above applies.
     */
    if (!query_prop(CONT_I_TRANSP) && 
        !query_prop(CONT_I_ATTACH) &&
        query_prop(CONT_I_CLOSED))
        l = 0;

    /* Rigid containers do not change in size
    */
    if (query_prop(CONT_I_RIGID))
        v = 0;

    if (l || w || v)
        env->update_internal(l,
                w * 100 / env->query_prop(CONT_I_REDUCE_WEIGHT),
                v * 100 / env->query_prop(CONT_I_REDUCE_VOLUME));
}

/*
 * Function name: update_light
 * Description:   Reevalueate the lightvalue of the container.
 */
public void
update_light(int recursive)
{
    int i;
    object *ob_list;
    
    ob_list = all_inventory(this_object());
    cont_cur_light = 0;
    if (!sizeof(ob_list))
        return;
    for (i = 0; i < sizeof(ob_list); i++)
    {
        if (recursive)
            ob_list[i]->update_light(recursive);
        cont_cur_light += ob_list[i]->query_prop(OBJ_I_LIGHT);
    }
}

/*
 * Function name: notify_change_prop
 * Description:   This function is called when a property in an object
 *                in the inventory has been changed.
 * Arguments:     prop - The property that has been changed.
 *                val  - The new value.
 *                old  - The old value.
 */
public void
notify_change_prop(string prop, mixed val, mixed old)
{
    object pobj;
    int n, o, ld;

    if (old == val)
        return;
    if (member_array(prop, NotifyProps) < 0)
        return;
    pobj = previous_object();

    switch(prop)
    {
    case CONT_I_LIGHT:
    case OBJ_I_LIGHT:
        update_internal(val - old, 0, 0);
        if (living(this_object()))
            this_object()->reveal_me(1);
        return;

    case CONT_I_WEIGHT:
    case OBJ_I_WEIGHT:
        update_internal(0, val - old, 0);
        return;

    case CONT_I_VOLUME:
    case OBJ_I_VOLUME:
        update_internal(0, 0, val - old);
        return;
    }

    n = pobj->query_internal_light();
    if (!n)
        return;

    ld = -1;  /* No change */

    /*
     * The rest is for light distribution. These are the rules:
     *
     * if ATTACH means always distribute.
     * if TRANSP means always distribute
     * if !CLOSED means distribute
     *
     * If a change in one prop causes a change in light distribution
     * from the container in this container, depends on the others.
     *
     * NOTE
     *   It is not _this_ container that has changed it is one of
     *   the containers within this container.
     */
    if (prop == CONT_I_ATTACH &&
        !pobj->query_prop(CONT_I_TRANSP) &&
        pobj->query_prop(CONT_I_CLOSED))
        ld = (val != 0);                /* 0 -> turn off, 1 -> turn on */

    else if (prop == CONT_I_TRANSP &&
        !pobj->query_prop(CONT_I_ATTACH) &&
        pobj->query_prop(CONT_I_CLOSED))
        ld = (val != 0);                /* 0 -> turn off, 1 -> turn on */

    else if (prop == CONT_I_CLOSED &&
        !pobj->query_prop(CONT_I_ATTACH) &&
        !pobj->query_prop(CONT_I_TRANSP))
        ld = (val != 0);                /* 0 -> turn off, 1 -> turn on */

    if (ld < 0)
        return;
        
    /* 0 -> turn off, 1 -> turn on 
     */
  /*
 * Changed the following line from 'if (ld == 1)', as
 * it appeared to be having the reverse effect from which
 * it was intended to. I.e. brightening the object when a 
 * container was being closed, darkening when it was being
 * opened (with a light source in the container).
 *
 * Khail - Dec 5/96
 */
    if (ld == 0)
        update_internal(n, 0, 0);
    else
        update_internal(-n, 0, 0);
    
    return;
}

/*
 * Function name: visible
 * Description:   Determine if an item is visible within a container
 * Arguments:     object ob - The item to test
 * Returns:       1/0 - item is visibie/not visible
 */
public int
visible(object ob)
{
    object env;

    if (!ob)
        return 0;

    if ((env = (object)this_object()->query_room()) &&
        (this_object()->query_prop(CONT_I_TRANSP) ||
        !this_object()->query_prop(CONT_I_CLOSED)))
    {
        return ((env->query_prop(OBJ_I_LIGHT) >
            -(this_player()->query_prop(LIVE_I_SEE_DARK))) && 
            CAN_SEE(this_player(), ob));
    }
        
    env = environment(ob);
    if (env == this_player() || (env == environment(this_player())))
        return CAN_SEE(this_player(), ob);

    while (objectp(env) && !living(env) && (env->query_prop(CONT_I_TRANSP) ||
        !env->query_prop(CONT_I_CLOSED)))
    {
        env = environment(env);
        if (env == this_player() || env == environment(this_player()))
            return CAN_SEE(this_player(), ob);
    }

    return 0;
}

/*
 * Function name: describe_contents
 * Description:   Give a description of items in this container
 * Arguments:     object for_obj - To whom to give the description
 *                object *obarr  - The items to describe
 */
public void
describe_contents(object for_obj, object *obarr)
{
    for_obj->catch_tell(show_sublocs(for_obj));

    if (this_object()->query_prop(CONT_I_ATTACH))
    {
        if (sizeof(obarr) > 0)
            for_obj->catch_tell(capitalize(COMPOSITE_DEAD(obarr)) + 
				(sizeof(obarr) > 1 ? " are" : " is") + 
				" on the " +
				this_object()->short() + ".\n");
        else
            for_obj->catch_tell("There is nothing on the " + 
				this_object()->short() + ".\n");
    }
    else if (sizeof(obarr) > 0)
        for_obj->catch_tell("The " + this_object()->short() + " contains " + 
			    COMPOSITE_DEAD(obarr) + ".\n");

    else
        for_obj->catch_tell("  " + "The " + this_object()->short() +
            " is empty.\n");
}

/*
 * Function name: show_visible_contents
 * Description:   Show the visible contents of this container
 * Arguments:     object for_obj - To whom to show the contents
 */
public void
show_visible_contents(object for_obj)
{
    object *obarr, linked;
    string str;

    if (linked = this_object()->query_room())
        obarr = all_inventory(linked);
    else
        obarr = all_inventory(this_object());
    obarr = filter(obarr, visible);
    describe_contents(for_obj, obarr);
}


/************************************************************
 * 
 * Sublocation routines. These routines manages sublocations within
 * and around containers. All containers start out with the default
 * sublocation 'inside'. Sublocation are given as second argument
 * to move()
 * 
 * Sublocations are given specific names when added to a container
 *
 * Each sublocation has a responsiple object. On this object the following
 * routines can be called:
 *
 *              show_subloc(string subloc, object on_obj, object for_obj)
 *                      - Print a description of the sublocation 'subloc'
 *                        on object 'ob_obj' for object 'for_obj'.
 *              
 */

/* 
 * Function name: add_subloc
 * Description:   Add a named sublocation to this container.
 * Arguments:     sloc: Name of sub location
 *                resp: Object responsible for the sublocation (or filename)
 *                      If == 0, the default sublocation describer will be
 *                      used.
 *                ids:  one or a list of ids that can be used to identify
 *                      the sublocation. This is only relevant for special
 *                      sublocations that are not standard preposition-
 *                      sublocs. These sublocs are most often related
 *                      to a specific subitem and an id could be:
 *                      'under nose', 'on leg', 'in pocket' etc.
 */
public varargs void
add_subloc(string sloc, mixed resp, mixed ids)
{
    int i;
    mixed old;

    cont_sublocs[sloc] = resp;
    if (stringp(ids))
        ids = ({ ids });
    for (i = 0; i < sizeof(ids); i++)
    {
        old = cont_subloc_ids[ids[i]];
        if (sizeof(old))
            cont_subloc_ids[ids[i]] = old + ({ sloc });
        else
            cont_subloc_ids[ids[i]] = ({ sloc });
    }
}

/* 
 * Function name: query_subloc_obj
 * Description:   Get the object corresponding to a subloc string
 */
public object
query_subloc_obj(string sloc) { return cont_sublocs[sloc]; }

/* 
 * Function name: query_sublocs
 * Description:   Get the current list of sublocations for this container
 */
public string *
query_sublocs() { return m_indexes(cont_sublocs); }

/* 
 * Function name: remove_subloc
 * Description:   Remove a named sublocation of this container.
 * Arguments:     sloc: Name of sub location
 *                
 */
public void
remove_subloc(string sloc)
{
    m_delkey(cont_sublocs, sloc);

    map(all_inventory(this_object()), &fix_ob_subloc_when_remove(, sloc));
    cont_subloc_ids = map(cont_subloc_ids, &map_subloc_id(, sloc));
    cont_subloc_ids = filter(cont_subloc_ids, filter_subloc_id);
}

nomask string *
map_subloc_id(string *sloc, string rm_sloc)
{
    int pos;

    if ((pos = member_array(rm_sloc, sloc)) >= 0)
        sloc = exclude_array(sloc, pos, pos);
    return sloc;
}

nomask int
filter_subloc_id(string *sloc, string rm_sloc)
{
    return (sizeof(sloc) > 0);
}

nomask void
fix_ob_subloc_when_remove(object ob, string sloc)
{
    if ((string)ob->query_subloc() == sloc)
        ob->move(this_object(), 0); /* Default sublocation */
    return 0;
}

/* 
 * Function name: subloc_id
 * Description:   Give the sublocation(s) if any for a specific id
 * Arguments:     id: name osublocation
 */
public string *
subloc_id(string id)
{
    return cont_subloc_ids[id];
}

/* 
 * Function name: subloc_cont_access
 * Description:   Check if a sublocation can be accessed or not
 * Arguments:     sloc: Name of the sublocation
 *                acs:  Access type as defined in /sys/subloc.h
 *                for_obj: Living for which the access is to be checked
 * Returns:       1 if accessible; 0 or string error message if not
 */
public mixed
subloc_cont_access(string sloc, string acs, object for_obj)
{
    object slob;

    if (!objectp(for_obj))
        for_obj = previous_object();
    
    if (!sloc)
        slob = this_object();
    else
        slob = cont_sublocs[sloc];

    if (!objectp(slob))
        return SUBL_CODE->subloc_access(sloc, this_object(), acs, for_obj);
    else
        return slob->subloc_access(sloc, this_object(), acs, for_obj);
}

/* 
 * Function name: subinventory
 * Description:   Give the subinventory for a specific sublocation
 * Arguments:     sloc: sublocation
 */
public object *
subinventory(mixed sloc)
{
    return filter(all_inventory(), &subloc_filter(, sloc));
}

nomask int 
subloc_filter(object ob, mixed sloc)
{
    return (ob->query_subloc() == sloc);
}

/* 
 * Function name: show_sublocs
 * Description:   Give a description of each sublocation. This is a default
 *                routine merely calling show_cont_subloc in this object for 
 *                each sublocation.
 * Arguments:     for_obj: The object for which description is given
 *                slocs:   Identifiers for sublocations
 */
public varargs string
show_sublocs(object for_obj, mixed *slocs) 
{
    int il;
    string str;
    mixed data;

    if (!objectp(for_obj))
        for_obj = previous_object();

    if (!sizeof(slocs))
        slocs = m_indexes(cont_sublocs) + ({ 0 });
    else
        slocs = slocs & ( m_indexes(cont_sublocs) + ({ 0 }) );

    for (str = "", il = 0; il < sizeof(slocs); il++)
    {
        data = this_object()->show_cont_subloc(slocs[il], for_obj);

        if (stringp(data))
            str += data;
        else if (data == 0 && slocs[il] != 0)
            m_delkey(cont_sublocs, slocs[il]);
    }

    return str;
}

/* 
 * Function name: show_cont_subloc
 * Description:   Give a description of one sublocation.
 * Arguments:     sloc : The name of the sublocation
 *                for_obj: For whom to show the sublocation
 * Returns:       string or 0 for invalid or 1 for temporary bad
 */
public varargs mixed
show_cont_subloc(string sloc, object for_obj) 
{
    int il;
    string data;
    mixed ob;

    ob = cont_sublocs[sloc];

    if (objectp(ob))
        data = ob->show_subloc(sloc, this_object(), for_obj);

    else if (stringp(ob))
    {
        catch(ob->teleledningsanka());
        ob = find_object(ob);
        if (ob)
            data = ob->show_subloc(sloc, this_object(), for_obj);
        else
            return 1;
    }
    else
    {
        data = SUBL_CODE->show_subloc(sloc, this_object(), for_obj);
        if (!stringp(data) && data == 0)
            return 0;
    }

    if (!stringp(data))
        return 1;

    return data;
}

/*
 * Function name: stat_object
 * Description:   This function is called when a wizard wants to get more
 *                information about an object.
 * Returns:       str - The string to write..
 */
string
stat_object()
{
    string str;
    int tmp;

    str = ::stat_object();

    if (tmp = query_prop(CONT_I_WEIGHT))
        str += "C. Weight: " + tmp + "\n";
    if (tmp = query_prop(CONT_I_MAX_WEIGHT))
        str += "C. Max. Weight: " + tmp + "\n";
    if (tmp = query_prop(CONT_I_VOLUME))
        str += "C. Volume: " + tmp + "\n";
    if (tmp = query_prop(CONT_I_MAX_VOLUME))
        str += "C. Max. Volume: " + tmp + "\n";

    return str;
}

/*
 * Function name: appraise_weight
 * Description:   This function is called when someon tries to appraise weight
 *                of this object.
 * Arguments:     num - use this number instead of skill if given.
 */
public string
appraise_weight(int num)
{
    int value, skill, seed;

    if (!num)
        skill = this_player()->query_skill(SS_APPR_OBJ);
    else
        skill = num;

    skill = 1000 / (skill + 1);
    value = query_prop(CONT_I_WEIGHT);
    sscanf(OB_NUM(this_object()), "%d", seed);
    skill = random(skill, seed);
    value = cut_sig_fig(value + (skill % 2 ? -skill % 70 : skill) *
        value / 100);

    if (value > 10000)
        return (value / 1000) + " kilograms";
    else
        return value + " grams";
}

/*
 * Function name: appraise_volume
 * Description:   This function is called when someon tries to appraise volume
 *                of this object.
 * Arguments:     num - use this number instead of skill if given.
 */
public string
appraise_volume(int num)
{
    int value, skill, seed;

    if (!num)
        skill = this_player()->query_skill(SS_APPR_OBJ);
    else
        skill = num;

    skill = 1000 / (skill + 1);
    value = query_prop(CONT_I_VOLUME);
    sscanf(OB_NUM(this_object()), "%d", seed);
    skill = random(skill, seed);
    value = cut_sig_fig(value + (skill % 2 ? -skill % 70 : skill) *
        value / 100);

    if (value > 10000)
        return (value / 1000) + " liters";
    else
        return value + " milliliters";
}
