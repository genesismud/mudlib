/*
   time.c

   Some time functions
*/

#pragma save_binary

/*
 * Time in secs to  nn days xx hours yy minutes ss seconds
 */
string
convtime(int time)
{
    string res;
    int n;
    
    res = "";

    n = time / 86400;
    if (n > 0)
    {
	if (n != 1)
	    res = n + " days";
	else
	    res = n + " day";
	time -= n * 86400;
    }

    n = time / 3600;
    if (n > 0)
    {
	if (strlen(res) != 0)
	    res += " ";

	if (n == 1)
	    res += n + " hour";
	else 
	    res += n + " hours";
	time -= n * 3600;
    }

    n = time / 60;
    if (n > 0)
    {
	if (strlen(res) != 0)
	    res += " ";

	if (n == 1)
	    res += n + " minute";
	else
	    res += n + " minutes";
	time -= n * 60;
    }

    if (time)
    {
	if (strlen(res) != 0)
	    res += " ";

	if (time == 1)
	    res += time + " second";
	else
	    res += time + " seconds";
    }

    return res;
}
