#include <terrain.h>
static int room_terrain;

/*
 * Function name: terrain_includes_all
 * Description  : Checks if the room contains terrain of a 
 *                certain type (or combination of types)
 * Arguments    : int terrain - the terrain (or combination of terrains) 
 *                to check for
 * Returns      : in - 1 if terrain matches the room, 0 otherwise
 */
int
terrain_includes_all(int terrain)
{
  return ((room_terrain & terrain) == terrain);
}

/*
 * Function name: terrain_includes_any
 * Description  : Checks if the terrain of the room includes a
 *                certain type
 * Arguments    : int terrain - the terrain (or combination of terrains) 
 *                to check for
 * Returns      : int - 1 if terrain found in the room, 0 otherwise
 */
int
terrain_includes_any(int terrain)
{
  return ((room_terrain & terrain) != 0);
}

/*
 * Function name: terrain_exact
 * Description  : Checks if the terrain of the room exactly matches a 
 *                certain type
 * Arguments    : int terrain - the terrain (or combination of terrains) 
 *                to check for
 * Returns      : int - 1 if terrain matches the room, 0 otherwise
 */
int
terrain_exact(int terrain)
{
  return ((room_terrain ^ terrain) == 0);
}

/*
 * Function name: set_terrain
 * Description  : Set the terrain proerties for the room. This is
 *                normally called from create_room()
 * Arguments    : int terrain - the terrain value to set (see /sys/terrain.h)
 */
void
set_terrain(int terrain)
{
  room_terrain = terrain;
}

/*
 * Function name: query_terrain
 * Description  : Query the room for its terrain properties
 * Returns      : int - terrain value
 */
int
query_terrain()
{
  return room_terrain;
}
