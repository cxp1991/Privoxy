/*
 * replace.c
 *
 *  Created on: May 16, 2013
 *      Author: ceslab
 */

#include "replace.h"
#include <stdio.h>
#include <string.h>

/*********************************************************************
 *
 * Function    :  replace_str
 *
 * Description :  replace "orig" by "rep" in str
 *
 * Parameters  :
 *          1  :  str = string to implement
 *          2  :  orig = string to replaced
 *          3  :  rep = string to replace
 *
 * Returns     :  Nothing.
 *
 *********************************************************************/

char *replace_str(char *str, char *orig, char *rep)
{
	static char buffer[5000];
	char *p;
	char *p1 = buffer;

	strncpy(buffer,str,strlen(str));

	while ((p = strstr (str,orig)))
	{
		sprintf(p1 + (p-str), "%s%s", rep, p + strlen(orig));
		p1 = p1 + ((p - str) + strlen(rep));
		str = str + (p - str + strlen(orig));
	}

	return buffer;
}

