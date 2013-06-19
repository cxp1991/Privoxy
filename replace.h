/*
 * replace.h
 *
 *  Created on: May 16, 2013
 *      Author: ceslab
 */

#ifndef REPLACE_H_
#define REPLACE_H_

/*
*  Find "finder" in "orig" and repacle it with "replacer"
*/
char* find_and_replace(char *orig, char *finder, char *replacer);

/*
 *  Check header is html header
 */
int check_header_is_html(const char *hdr);

/*
 * Read html body data
 */
int read_html_buffer(html_buffer,buffer);

#endif /* REPLACE_H_ */
