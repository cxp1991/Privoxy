/*
 * replace.c
 *
 *  Created on: May 16, 2013
 *      Author: ceslab
 */

#include "replace.h"
#include "project.h"
#include <stdio.h>
#include <string.h>

/*********************************************************************
 *
 * Function    :  find_and_replace
 *
 * Description :  replace "finder" by "replacer" in orig
 *
 * Parameters  :
 *          1  :  orig = string to implement
 *          2  :  finder = string to replaced
 *          3  :  replacer = string to replace
 *
 * Returns     :  Nothing.
 *
 *********************************************************************/

char* find_and_replace(char *orig, char *finder, char *replacer)
{
	char buffer[BUFFER_SIZE];
	char temp1[BUFFER_SIZE];
	char temp2[BUFFER_SIZE];
	char *reply=NULL;
	int l_index=0;

	memset(buffer,0,sizeof(buffer));
	memset(temp1,0,sizeof(buffer));
	memset(temp2,0,sizeof(buffer));

	while(*orig)
	{
		// Move html keywords
		if('<' == *orig)
		{
			while(('>' != *orig))
			{
				buffer[l_index++] = *orig;
				orig++;
			}
			buffer[l_index++] = '>';
			orig++;
		}
		else // Get 'real' data & replace
		{
			int   j  = 0;
			char *p  = NULL;
			char *p1 = temp1;// original
			char *p2 = temp2;// modified

			// Get 'real' data
			while(('<' != *orig))
			{
				temp1[j++] = *orig;
				orig++;
			}
			temp1[j]='\0';

			// Search & Replace
			strncpy(temp2,p1,strlen(p1));

			while ((p = strstr (p1,finder)))
			{
				sprintf(p2 + (p-p1), "%s%s", replacer, p + strlen(finder));
				p2 = p2 + ((p - p1) + strlen(replacer));
				p1 = p1 + (p - p1 + strlen(finder));
			}

			// Append into buffer
			strcat(buffer,temp2);
			l_index = l_index + strlen(temp2);
		}
	}

	reply = buffer;
	return reply;
}

int check_header_is_html(const char *hdr)
{
	char *p = NULL;

	p = strstr (hdr,"Content-Type: text/html");

	if (p)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

int read_html_buffer(struct iob *html_buffer, char *buffer)
{
	return 0;
}


