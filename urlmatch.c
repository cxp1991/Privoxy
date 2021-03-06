const char urlmatch_rcs[] = "$Id: urlmatch.c,v 1.65 2011/11/06 11:41:34 fabiankeil Exp $";
/*********************************************************************
 *
 * File        :  $Source: /cvsroot/ijbswa/current/urlmatch.c,v $
 *
 * Purpose     :  Declares functions to match URLs against URL
 *                patterns.
 *
 * Copyright   :  Written by and Copyright (C) 2001-2011
 *                the Privoxy team. http://www.privoxy.org/
 *
 *                Based on the Internet Junkbuster originally written
 *                by and Copyright (C) 1997 Anonymous Coders and
 *                Junkbusters Corporation.  http://www.junkbusters.com
 *
 *                This program is free software; you can redistribute it
 *                and/or modify it under the terms of the GNU General
 *                Public License as published by the Free Software
 *                Foundation; either version 2 of the License, or (at
 *                your option) any later version.
 *
 *                This program is distributed in the hope that it will
 *                be useful, but WITHOUT ANY WARRANTY; without even the
 *                implied warranty of MERCHANTABILITY or FITNESS FOR A
 *                PARTICULAR PURPOSE.  See the GNU General Public
 *                License for more details.
 *
 *                The GNU General Public License should be included with
 *                this file.  If not, you can view it at
 *                http://www.gnu.org/copyleft/gpl.html
 *                or write to the Free Software Foundation, Inc., 59
 *                Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *********************************************************************/


#include "config.h"

#ifndef _WIN32
#include <stdio.h>
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#if !defined(_WIN32) && !defined(__OS2__)
#include <unistd.h>
#endif

#include "project.h"
#include "urlmatch.h"
#include "ssplit.h"
#include "miscutil.h"
#include "errlog.h"

const char urlmatch_h_rcs[] = URLMATCH_H_VERSION;

enum regex_anchoring
{
   NO_ANCHORING,
   LEFT_ANCHORED,
   RIGHT_ANCHORED,
   RIGHT_ANCHORED_HOST
};
static jb_err compile_host_pattern(struct url_spec *url, const char *host_pattern);

/*********************************************************************
 *
 * Function    :  free_http_request
 *
 * Description :  Freez a http_request structure
 *
 * Parameters  :
 *          1  :  http = points to a http_request structure to free
 *
 * Returns     :  N/A
 *
 *********************************************************************/
void free_http_request(struct http_request *http)
{
   assert(http);

   freez(http->cmd);
   freez(http->ocmd);
   freez(http->gpc);
   freez(http->host);
   freez(http->url);
   freez(http->hostport);
   freez(http->path);
   freez(http->ver);
   freez(http->host_ip_addr_str);
#ifndef FEATURE_EXTENDED_HOST_PATTERNS
   freez(http->dbuffer);
   freez(http->dvec);
   http->dcount = 0;
#endif
}


#ifndef FEATURE_EXTENDED_HOST_PATTERNS
/*********************************************************************
 *
 * Function    :  init_domain_components
 *
 * Description :  Splits the domain name so we can compare it
 *                against wildcards. It used to be part of
 *                parse_http_url, but was separated because the
 *                same code is required in chat in case of
 *                intercepted requests.
 *
 * Parameters  :
 *          1  :  http = pointer to the http structure to hold elements.
 *
 * Returns     :  JB_ERR_OK on success
 *                JB_ERR_MEMORY on out of memory
 *                JB_ERR_PARSE on malformed command/URL
 *                             or >100 domains deep.
 *
 *********************************************************************/
jb_err init_domain_components(struct http_request *http)
{
   char *vec[BUFFER_SIZE];
   size_t size;
   char *p;

   http->dbuffer = strdup(http->host);
   if (NULL == http->dbuffer)
   {
      return JB_ERR_MEMORY;
   }

   /* map to lower case */
   for (p = http->dbuffer; *p ; p++)
   {
      *p = (char)tolower((int)(unsigned char)*p);
   }

   /* split the domain name into components */
   http->dcount = ssplit(http->dbuffer, ".", vec, SZ(vec), 1, 1);

   if (http->dcount <= 0)
   {
      /*
       * Error: More than SZ(vec) components in domain
       *    or: no components in domain
       */
      log_error(LOG_LEVEL_ERROR, "More than SZ(vec) components in domain or none at all.");
      return JB_ERR_PARSE;
   }

   /* save a copy of the pointers in dvec */
   size = (size_t)http->dcount * sizeof(*http->dvec);

   http->dvec = (char **)malloc(size);
   if (NULL == http->dvec)
   {
      return JB_ERR_MEMORY;
   }

   memcpy(http->dvec, vec, size);

   return JB_ERR_OK;
}
#endif /* ndef FEATURE_EXTENDED_HOST_PATTERNS */


/*********************************************************************
 *
 * Function    :  url_requires_percent_encoding
 *
 * Description :  Checks if an URL contains invalid characters
 *                according to RFC 3986 that should be percent-encoded.
 *                Does not verify whether or not the passed string
 *                actually is a valid URL.
 *
 * Parameters  :
 *          1  :  url = URL to check
 *
 * Returns     :  True in case of valid URLs, false otherwise
 *
 *********************************************************************/
int url_requires_percent_encoding(const char *url)
{
   static const char allowed_characters[128] = {
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',
      '\0', '\0', '\0', '!',  '\0', '#',  '$',  '%',  '&',  '\'',
      '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',  '0',  '1',
      '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',
      '\0', '=',  '\0', '?',  '@',  'A',  'B',  'C',  'D',  'E',
      'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
      'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',
      'Z',  '[',  '\0', ']',  '\0', '_',  '\0', 'a',  'b',  'c',
      'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',
      'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
      'x',  'y',  'z',  '\0', '\0', '\0', '~',  '\0'
   };

   while (*url != '\0')
   {
      const unsigned int i = (unsigned char)*url++;
      if (i >= sizeof(allowed_characters) || '\0' == allowed_characters[i])
      {
         return TRUE;
      }
   }

   return FALSE;

}


/*********************************************************************
 *
 * Function    :  parse_http_url
 *
 * Description :  Parse out the host and port from the URL.  Find the
 *                hostname & path, port (if ':'), and/or password (if '@')
 *
 * Parameters  :
 *          1  :  url = URL (or is it URI?) to break down
 *          2  :  http = pointer to the http structure to hold elements.
 *                       Must be initialized with valid values (like NULLs).
 *          3  :  require_protocol = Whether or not URLs without
 *                                   protocol are acceptable.
 *
 * Returns     :  JB_ERR_OK on success
 *                JB_ERR_MEMORY on out of memory
 *                JB_ERR_PARSE on malformed command/URL
 *                             or >100 domains deep.
 *
 *********************************************************************/
jb_err parse_http_url(const char *url, struct http_request *http, int require_protocol)
{
   int host_available = 1; /* A proxy can dream. */

   /*
    * Save our initial URL
    */
   http->url = strdup(url);
   if (http->url == NULL)
   {
      return JB_ERR_MEMORY;
   }


   /*
    * Check for * URI. If found, we're done.
    */
   if (*http->url == '*')
   {
      if  ( NULL == (http->path = strdup("*"))
         || NULL == (http->hostport = strdup("")) )
      {
         return JB_ERR_MEMORY;
      }
      if (http->url[1] != '\0')
      {
         return JB_ERR_PARSE;
      }
      return JB_ERR_OK;
   }


   /*
    * Split URL into protocol,hostport,path.
    */
   {
      char *buf;
      char *url_noproto;
      char *url_path;

      buf = strdup(url);
      if (buf == NULL)
      {
         return JB_ERR_MEMORY;
      }

      /* Find the start of the URL in our scratch space */
      url_noproto = buf;
      if (strncmpic(url_noproto, "http://",  7) == 0)
      {
         url_noproto += 7;
      }
      else if (strncmpic(url_noproto, "https://", 8) == 0)
      {
         /*
          * Should only happen when called from cgi_show_url_info().
          */
         url_noproto += 8;
         http->ssl = 1;
      }
      else if (*url_noproto == '/')
      {
        /*
         * Short request line without protocol and host.
         * Most likely because the client's request
         * was intercepted and redirected into Privoxy.
         */
         http->host = NULL;
         host_available = 0;
      }
      else if (require_protocol)
      {
         freez(buf);
         return JB_ERR_PARSE;
      }

      url_path = strchr(url_noproto, '/');
      if (url_path != NULL)
      {
         /*
          * Got a path.
          *
          * NOTE: The following line ignores the path for HTTPS URLS.
          * This means that you get consistent behaviour if you type a
          * https URL in and it's parsed by the function.  (When the
          * URL is actually retrieved, SSL hides the path part).
          */
         http->path = strdup(http->ssl ? "/" : url_path);
         *url_path = '\0';
         http->hostport = strdup(url_noproto);
      }
      else
      {
         /*
          * Repair broken HTTP requests that don't contain a path,
          * or CONNECT requests
          */
         http->path = strdup("/");
         http->hostport = strdup(url_noproto);
      }

      freez(buf);

      if ( (http->path == NULL)
        || (http->hostport == NULL))
      {
         return JB_ERR_MEMORY;
      }
   }

   if (!host_available)
   {
      /* Without host, there is nothing left to do here */
      return JB_ERR_OK;
   }

   /*
    * Split hostport into user/password (ignored), host, port.
    */
   {
      char *buf;
      char *host;
      char *port;

      buf = strdup(http->hostport);
      if (buf == NULL)
      {
         return JB_ERR_MEMORY;
      }

      /* check if url contains username and/or password */
      host = strchr(buf, '@');
      if (host != NULL)
      {
         /* Contains username/password, skip it and the @ sign. */
         host++;
      }
      else
      {
         /* No username or password. */
         host = buf;
      }

      /* Move after hostname before port number */
      if (*host == '[')
      {
         /* Numeric IPv6 address delimited by brackets */
         host++;
         port = strchr(host, ']');

         if (port == NULL)
         {
            /* Missing closing bracket */
            freez(buf);
            return JB_ERR_PARSE;
         }

         *port++ = '\0';

         if (*port == '\0')
         {
            port = NULL;
         }
         else if (*port != ':')
         {
            /* Garbage after closing bracket */
            freez(buf);
            return JB_ERR_PARSE;
         }
      }
      else
      {
         /* Plain non-escaped hostname */
         port = strchr(host, ':');
      }

      /* check if url contains port */
      if (port != NULL)
      {
         /* Contains port */
         /* Terminate hostname and point to start of port string */
         *port++ = '\0';
         http->port = atoi(port);
      }
      else
      {
         /* No port specified. */
         http->port = (http->ssl ? 443 : 80);
      }

      http->host = strdup(host);

      freez(buf);

      if (http->host == NULL)
      {
         return JB_ERR_MEMORY;
      }
   }

#ifdef FEATURE_EXTENDED_HOST_PATTERNS
   return JB_ERR_OK;
#else
   /* Split domain name so we can compare it against wildcards */
   return init_domain_components(http);
#endif /* def FEATURE_EXTENDED_HOST_PATTERNS */

}


/*********************************************************************
 *
 * Function    :  unknown_method
 *
 * Description :  Checks whether a method is unknown.
 *
 * Parameters  :
 *          1  :  method = points to a http method
 *
 * Returns     :  TRUE if it's unknown, FALSE otherwise.
 *
 *********************************************************************/
static int unknown_method(const char *method)
{
   static const char * const known_http_methods[] = {
      /* Basic HTTP request type */
      "GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "TRACE", "CONNECT",
      /* webDAV extensions (RFC2518) */
      "PROPFIND", "PROPPATCH", "MOVE", "COPY", "MKCOL", "LOCK", "UNLOCK",
      /*
       * Microsoft webDAV extension for Exchange 2000.  See:
       * http://lists.w3.org/Archives/Public/w3c-dist-auth/2002JanMar/0001.html
       * http://msdn.microsoft.com/library/en-us/wss/wss/_webdav_methods.asp
       */
      "BCOPY", "BMOVE", "BDELETE", "BPROPFIND", "BPROPPATCH",
      /*
       * Another Microsoft webDAV extension for Exchange 2000.  See:
       * http://systems.cs.colorado.edu/grunwald/MobileComputing/Papers/draft-cohen-gena-p-base-00.txt
       * http://lists.w3.org/Archives/Public/w3c-dist-auth/2002JanMar/0001.html
       * http://msdn.microsoft.com/library/en-us/wss/wss/_webdav_methods.asp
       */
      "SUBSCRIBE", "UNSUBSCRIBE", "NOTIFY", "POLL",
      /*
       * Yet another WebDAV extension, this time for
       * Web Distributed Authoring and Versioning (RFC3253)
       */
      "VERSION-CONTROL", "REPORT", "CHECKOUT", "CHECKIN", "UNCHECKOUT",
      "MKWORKSPACE", "UPDATE", "LABEL", "MERGE", "BASELINE-CONTROL", "MKACTIVITY",
   };
   int i;

   for (i = 0; i < SZ(known_http_methods); i++)
   {
      if (0 == strcmpic(method, known_http_methods[i]))
      {
         return FALSE;
      }
   }

   return TRUE;

}


/*********************************************************************
 *
 * Function    :  parse_http_request
 *
 * Description :  Parse out the host and port from the URL.  Find the
 *                hostname & path, port (if ':'), and/or password (if '@')
 *
 * Parameters  :
 *          1  :  req = HTTP request line to break down
 *          2  :  http = pointer to the http structure to hold elements
 *
 * Returns     :  JB_ERR_OK on success
 *                JB_ERR_MEMORY on out of memory
 *                JB_ERR_CGI_PARAMS on malformed command/URL
 *                                  or >100 domains deep.
 *
 *********************************************************************/
jb_err parse_http_request(const char *req, struct http_request *http)
{
   char *buf;
   char *v[10]; /* XXX: Why 10? We should only need three. */
   int n;
   jb_err err;

   memset(http, '\0', sizeof(*http));

   buf = strdup(req);
   if (buf == NULL)
   {
      return JB_ERR_MEMORY;
   }

	/*
	*  get information from http request
	*  1. method = GET, POST, ... => v[0]
	*  2. url = http://www.dantri.com.vn => v[1]
	*  3. version = HTTP/1.1 => v[2]
	*/
   n = ssplit(buf, " \r\n", v, SZ(v), 1, 1);
   if (n != 3)
   {
      freez(buf);
      return JB_ERR_PARSE;
   }

   /*
    * Fail in case of unknown methods
    * which we might not handle correctly.
    *
    * XXX: There should be a config option
    * to forward requests with unknown methods
    * anyway. Most of them don't need special
    * steps.
    */
   if (unknown_method(v[0]))
   {
      log_error(LOG_LEVEL_ERROR, "Unknown HTTP method detected: %s", v[0]);
      freez(buf);
      return JB_ERR_PARSE;
   }

   if (strcmpic(v[2], "HTTP/1.1") && strcmpic(v[2], "HTTP/1.0"))
   {
      log_error(LOG_LEVEL_ERROR, "The only supported HTTP "
         "versions are 1.0 and 1.1. This rules out: %s", v[2]);
      freez(buf);
      return JB_ERR_PARSE;
   }

   http->ssl = !strcmpic(v[0], "CONNECT");

   err = parse_http_url(v[1], http, !http->ssl);
   if (err)
   {
      freez(buf);
      return err;
   }

   /*
    * Copy the details into the structure
    */
   http->cmd = strdup(req);
   http->gpc = strdup(v[0]);
   http->ver = strdup(v[2]);

   freez(buf);

   if ( (http->cmd == NULL)
     || (http->gpc == NULL)
     || (http->ver == NULL) )
   {
      return JB_ERR_MEMORY;
   }

   return JB_ERR_OK;

}


/*********************************************************************
 *
 * Function    :  compile_pattern
 *
 * Description :  Compiles a host, domain or TAG pattern.
 *
 * Parameters  :
 *          1  :  pattern = The pattern to compile.
 *          2  :  anchoring = How the regex should be modified
 *                            before compilation. Can be either
 *                            one of NO_ANCHORING, LEFT_ANCHORED,
 *                            RIGHT_ANCHORED or RIGHT_ANCHORED_HOST.
 *          3  :  url     = In case of failures, the spec member is
 *                          logged and the structure freed.
 *          4  :  regex   = Where the compiled regex should be stored.
 *
 * Returns     :  JB_ERR_OK - Success
 *                JB_ERR_MEMORY - Out of memory
 *                JB_ERR_PARSE - Cannot parse regex
 *
 *********************************************************************/
static jb_err compile_pattern(const char *pattern, enum regex_anchoring anchoring,
                              struct url_spec *url, regex_t **regex)
{
   int errcode;
   char rebuf[BUFFER_SIZE];
   const char *fmt = NULL;

   assert(pattern);
   assert(strlen(pattern) < sizeof(rebuf) - 2);

   if (pattern[0] == '\0')
   {
      *regex = NULL;
      return JB_ERR_OK;
   }

   switch (anchoring)
   {
      case NO_ANCHORING:
         fmt = "%s";
         break;
      case RIGHT_ANCHORED:
         fmt = "%s$";
         break;
      case RIGHT_ANCHORED_HOST:
         fmt = "%s\\.?$";
         break;
      case LEFT_ANCHORED:
         fmt = "^%s";
         break;
      default:
         log_error(LOG_LEVEL_FATAL,
            "Invalid anchoring in compile_pattern %d", anchoring);
   }

   *regex = zalloc(sizeof(**regex));
   if (NULL == *regex)
   {
      free_url_spec(url);
      return JB_ERR_MEMORY;
   }

   snprintf(rebuf, sizeof(rebuf), fmt, pattern);

   errcode = regcomp(*regex, rebuf, (REG_EXTENDED|REG_NOSUB|REG_ICASE));

   if (errcode)
   {
      size_t errlen = regerror(errcode, *regex, rebuf, sizeof(rebuf));
      if (errlen > (sizeof(rebuf) - (size_t)1))
      {
         errlen = sizeof(rebuf) - (size_t)1;
      }
      rebuf[errlen] = '\0';
      log_error(LOG_LEVEL_ERROR, "error compiling %s from %s: %s",
         pattern, url->spec, rebuf);
      free_url_spec(url);

      return JB_ERR_PARSE;
   }

   return JB_ERR_OK;

}


/*********************************************************************
 *
 * Function    :  compile_url_pattern
 *
 * Description :  Compiles the three parts of an URL pattern.
 *
 * Parameters  :
 *          1  :  url = Target url_spec to be filled in.
 *          2  :  buf = The url pattern to compile. Will be messed up.
 *
 * Returns     :  JB_ERR_OK - Success
 *                JB_ERR_MEMORY - Out of memory
 *                JB_ERR_PARSE - Cannot parse regex
 *
 *********************************************************************/
static jb_err compile_url_pattern(struct url_spec *url, char *buf)
{
   char *p;

   p = strchr(buf, '/');
   if (NULL != p)
   {
      /*
       * Only compile the regex if it consists of more than
       * a single slash, otherwise it wouldn't affect the result.
       */
      if (p[1] != '\0')
      {
         /*
          * XXX: does it make sense to compile the slash at the beginning?
          */
         jb_err err = compile_pattern(p, LEFT_ANCHORED, url, &url->preg);

         if (JB_ERR_OK != err)
         {
            return err;
         }
      }
      *p = '\0';
   }

   /*
    * IPv6 numeric hostnames can contain colons, thus we need
    * to delimit the hostname before the real port separator.
    * As brackets are already used in the hostname pattern,
    * we use angle brackets ('<', '>') instead.
    */
   if ((buf[0] == '<') && (NULL != (p = strchr(buf + 1, '>'))))
   {
      *p++ = '\0';
      buf++;

      if (*p == '\0')
      {
         /* IPv6 address without port number */
         p = NULL;
      }
      else if (*p != ':')
      {
         /* Garbage after address delimiter */
         return JB_ERR_PARSE;
      }
   }
   else
   {
      p = strchr(buf, ':');
   }

   if (NULL != p)
   {
      *p++ = '\0';
      url->port_list = strdup(p);
      if (NULL == url->port_list)
      {
         return JB_ERR_MEMORY;
      }
   }
   else
   {
      url->port_list = NULL;
   }

   if (buf[0] != '\0')
   {
      return compile_host_pattern(url, buf);
   }

   return JB_ERR_OK;

}


#ifdef FEATURE_EXTENDED_HOST_PATTERNS
/*********************************************************************
 *
 * Function    :  compile_host_pattern
 *
 * Description :  Parses and compiles a host pattern..
 *
 * Parameters  :
 *          1  :  url = Target url_spec to be filled in.
 *          2  :  host_pattern = Host pattern to compile.
 *
 * Returns     :  JB_ERR_OK - Success
 *                JB_ERR_MEMORY - Out of memory
 *                JB_ERR_PARSE - Cannot parse regex
 *
 *********************************************************************/
static jb_err compile_host_pattern(struct url_spec *url, const char *host_pattern)
{
   return compile_pattern(host_pattern, RIGHT_ANCHORED_HOST, url, &url->host_regex);
}

#else

/*********************************************************************
 *
 * Function    :  compile_host_pattern
 *
 * Description :  Parses and "compiles" an old-school host pattern.
 *
 * Parameters  :
 *          1  :  url = Target url_spec to be filled in.
 *          2  :  host_pattern = Host pattern to parse.
 *
 * Returns     :  JB_ERR_OK - Success
 *                JB_ERR_MEMORY - Out of memory
 *                JB_ERR_PARSE - Cannot parse regex
 *
 *********************************************************************/
static jb_err compile_host_pattern(struct url_spec *url, const char *host_pattern)
{
   char *v[150];
   size_t size;
   char *p;

   /*
    * Parse domain part
    */
   if (host_pattern[strlen(host_pattern) - 1] == '.')
   {
      url->unanchored |= ANCHOR_RIGHT;
   }
   if (host_pattern[0] == '.')
   {
      url->unanchored |= ANCHOR_LEFT;
   }

   /*
    * Split domain into components
    */
   url->dbuffer = strdup(host_pattern);
   if (NULL == url->dbuffer)
   {
      free_url_spec(url);
      return JB_ERR_MEMORY;
   }

   /*
    * Map to lower case
    */
   for (p = url->dbuffer; *p ; p++)
   {
      *p = (char)tolower((int)(unsigned char)*p);
   }

   /*
    * Split the domain name into components
    */
   url->dcount = ssplit(url->dbuffer, ".", v, SZ(v), 1, 1);

   if (url->dcount < 0)
   {
      free_url_spec(url);
      return JB_ERR_MEMORY;
   }
   else if (url->dcount != 0)
   {
      /*
       * Save a copy of the pointers in dvec
       */
      size = (size_t)url->dcount * sizeof(*url->dvec);

      url->dvec = (char **)malloc(size);
      if (NULL == url->dvec)
      {
         free_url_spec(url);
         return JB_ERR_MEMORY;
      }

      memcpy(url->dvec, v, size);
   }
   /*
    * else dcount == 0 in which case we needn't do anything,
    * since dvec will never be accessed and the pattern will
    * match all domains.
    */
   return JB_ERR_OK;
}


/*********************************************************************
 *
 * Function    :  simplematch
 *
 * Description :  String matching, with a (greedy) '*' wildcard that
 *                stands for zero or more arbitrary characters and
 *                character classes in [], which take both enumerations
 *                and ranges.
 *
 * Parameters  :
 *          1  :  pattern = pattern for matching
 *          2  :  text    = text to be matched
 *
 * Returns     :  0 if match, else nonzero
 *
 *********************************************************************/
static int simplematch(const char *pattern, const char *text)
{
   const unsigned char *pat = (const unsigned char *)pattern;
   const unsigned char *txt = (const unsigned char *)text;
   const unsigned char *fallback = pat;
   int wildcard = 0;

   unsigned char lastchar = 'a';
   unsigned i;
   unsigned char charmap[32];

   while (*txt)
   {

      /* EOF pattern but !EOF text? */
      if (*pat == '\0')
      {
         if (wildcard)
         {
            pat = fallback;
         }
         else
         {
            return 1;
         }
      }

      /* '*' in the pattern?  */
      if (*pat == '*')
      {

         /* The pattern ends afterwards? Speed up the return. */
         if (*++pat == '\0')
         {
            return 0;
         }

         /* Else, set wildcard mode and remember position after '*' */
         wildcard = 1;
         fallback = pat;
      }

      /* Character range specification? */
      if (*pat == '[')
      {
         memset(charmap, '\0', sizeof(charmap));

         while (*++pat != ']')
         {
            if (!*pat)
            {
               return 1;
            }
            else if (*pat == '-')
            {
               if ((*++pat == ']') || *pat == '\0')
               {
                  return(1);
               }
               for (i = lastchar; i <= *pat; i++)
               {
                  charmap[i / 8] |= (unsigned char)(1 << (i % 8));
               }
            }
            else
            {
               charmap[*pat / 8] |= (unsigned char)(1 << (*pat % 8));
               lastchar = *pat;
            }
         }
      } /* -END- if Character range specification */


      /*
       * Char match, or char range match?
       */
      if ( (*pat == *txt)
      ||   (*pat == '?')
      ||   ((*pat == ']') && (charmap[*txt / 8] & (1 << (*txt % 8)))) )
      {
         /*
          * Success: Go ahead
          */
         pat++;
      }
      else if (!wildcard)
      {
         /*
          * No match && no wildcard: No luck
          */
         return 1;
      }
      else if (pat != fallback)
      {
         /*
          * Increment text pointer if in char range matching
          */
         if (*pat == ']')
         {
            txt++;
         }
         /*
          * Wildcard mode && nonmatch beyond fallback: Rewind pattern
          */
         pat = fallback;
         /*
          * Restart matching from current text pointer
          */
         continue;
      }
      txt++;
   }

   /* Cut off extra '*'s */
   if(*pat == '*')  pat++;

   /* If this is the pattern's end, fine! */
   return(*pat);

}


/*********************************************************************
 *
 * Function    :  simple_domaincmp
 *
 * Description :  Domain-wise Compare fqdn's.  The comparison is
 *                both left- and right-anchored.  The individual
 *                domain names are compared with simplematch().
 *                This is only used by domain_match.
 *
 * Parameters  :
 *          1  :  pv = array of patterns to compare
 *          2  :  fv = array of domain components to compare
 *          3  :  len = length of the arrays (both arrays are the
 *                      same length - if they weren't, it couldn't
 *                      possibly be a match).
 *
 * Returns     :  0 => domains are equivalent, else no match.
 *
 *********************************************************************/
static int simple_domaincmp(char **pv, char **fv, int len)
{
   int n;

   for (n = 0; n < len; n++)
   {
      if (simplematch(pv[n], fv[n]))
      {
         return 1;
      }
   }

   return 0;

}


/*********************************************************************
 *
 * Function    :  domain_match
 *
 * Description :  Domain-wise Compare fqdn's. Governed by the bimap in
 *                pattern->unachored, the comparison is un-, left-,
 *                right-anchored, or both.
 *                The individual domain names are compared with
 *                simplematch().
 *
 * Parameters  :
 *          1  :  pattern = a domain that may contain a '*' as a wildcard.
 *          2  :  fqdn = domain name against which the patterns are compared.
 *
 * Returns     :  0 => domains are equivalent, else no match.
 *
 *********************************************************************/
static int domain_match(const struct url_spec *pattern, const struct http_request *fqdn)
{
   char **pv, **fv;  /* vectors  */
   int    plen, flen;
   int unanchored = pattern->unanchored & (ANCHOR_RIGHT | ANCHOR_LEFT);

   plen = pattern->dcount;
   flen = fqdn->dcount;

   if (flen < plen)
   {
      /* fqdn is too short to match this pattern */
      return 1;
   }

   pv   = pattern->dvec;
   fv   = fqdn->dvec;

   if (unanchored == ANCHOR_LEFT)
   {
      /*
       * Right anchored.
       *
       * Convert this into a fully anchored pattern with
       * the fqdn and pattern the same length
       */
      fv += (flen - plen); /* flen - plen >= 0 due to check above */
      return simple_domaincmp(pv, fv, plen);
   }
   else if (unanchored == 0)
   {
      /* Fully anchored, check length */
      if (flen != plen)
      {
         return 1;
      }
      return simple_domaincmp(pv, fv, plen);
   }
   else if (unanchored == ANCHOR_RIGHT)
   {
      /* Left anchored, ignore all extra in fqdn */
      return simple_domaincmp(pv, fv, plen);
   }
   else
   {
      /* Unanchored */
      int n;
      int maxn = flen - plen;
      for (n = 0; n <= maxn; n++)
      {
         if (!simple_domaincmp(pv, fv, plen))
         {
            return 0;
         }
         /*
          * Doesn't match from start of fqdn
          * Try skipping first part of fqdn
          */
         fv++;
      }
      return 1;
   }

}
#endif /* def FEATURE_EXTENDED_HOST_PATTERNS */


/*********************************************************************
 *
 * Function    :  create_url_spec
 *
 * Description :  Creates a "url_spec" structure from a string.
 *                When finished, free with free_url_spec().
 *
 * Parameters  :
 *          1  :  url = Target url_spec to be filled in.  Will be
 *                      zeroed before use.
 *          2  :  buf = Source pattern, null terminated.  NOTE: The
 *                      contents of this buffer are destroyed by this
 *                      function.  If this function succeeds, the
 *                      buffer is copied to url->spec.  If this
 *                      function fails, the contents of the buffer
 *                      are lost forever.
 *
 * Returns     :  JB_ERR_OK - Success
 *                JB_ERR_MEMORY - Out of memory
 *                JB_ERR_PARSE - Cannot parse regex (Detailed message
 *                               written to system log)
 *
 *********************************************************************/
jb_err create_url_spec(struct url_spec *url, char *buf)
{
   assert(url);
   assert(buf);

   memset(url, '\0', sizeof(*url));

   /* Remember the original specification for the CGI pages. */
   url->spec = strdup(buf);
   if (NULL == url->spec)
   {
      return JB_ERR_MEMORY;
   }

   /* Is it a tag pattern? */
   if (0 == strncmpic(url->spec, "TAG:", 4))
   {
      /* The pattern starts with the first character after "TAG:" */
      const char *tag_pattern = buf + 4;
      return compile_pattern(tag_pattern, NO_ANCHORING, url, &url->tag_regex);
   }

   /* If it isn't a tag pattern it must be an URL pattern. */
   return compile_url_pattern(url, buf);
}


/*********************************************************************
 *
 * Function    :  free_url_spec
 *
 * Description :  Called from the "unloaders".  Freez the url
 *                structure elements.
 *
 * Parameters  :
 *          1  :  url = pointer to a url_spec structure.
 *
 * Returns     :  N/A
 *
 *********************************************************************/
void free_url_spec(struct url_spec *url)
{
   if (url == NULL) return;

   freez(url->spec);
#ifdef FEATURE_EXTENDED_HOST_PATTERNS
   if (url->host_regex)
   {
      regfree(url->host_regex);
      freez(url->host_regex);
   }
#else
   freez(url->dbuffer);
   freez(url->dvec);
   url->dcount = 0;
#endif /* ndef FEATURE_EXTENDED_HOST_PATTERNS */
   freez(url->port_list);
   if (url->preg)
   {
      regfree(url->preg);
      freez(url->preg);
   }
   if (url->tag_regex)
   {
      regfree(url->tag_regex);
      freez(url->tag_regex);
   }
}


/*********************************************************************
 *
 * Function    :  port_matches
 *
 * Description :  Compares a port against a port list.
 *
 * Parameters  :
 *          1  :  port      = The port to check.
 *          2  :  port_list = The list of port to compare with.
 *
 * Returns     :  TRUE for yes, FALSE otherwise.
 *
 *********************************************************************/
static int port_matches(const int port, const char *port_list)
{
   return ((NULL == port_list) || match_portlist(port_list, port));
}


/*********************************************************************
 *
 * Function    :  host_matches
 *
 * Description :  Compares a host against a host pattern.
 *
 * Parameters  :
 *          1  :  url = The URL to match
 *          2  :  pattern = The URL pattern
 *
 * Returns     :  TRUE for yes, FALSE otherwise.
 *
 *********************************************************************/
static int host_matches(const struct http_request *http,
                        const struct url_spec *pattern)
{
#ifdef FEATURE_EXTENDED_HOST_PATTERNS
   return ((NULL == pattern->host_regex)
      || (0 == regexec(pattern->host_regex, http->host, 0, NULL, 0)));
#else
   return ((NULL == pattern->dbuffer) || (0 == domain_match(pattern, http)));
#endif
}


/*********************************************************************
 *
 * Function    :  path_matches
 *
 * Description :  Compares a path against a path pattern.
 *
 * Parameters  :
 *          1  :  path = The path to match
 *          2  :  pattern = The URL pattern
 *
 * Returns     :  TRUE for yes, FALSE otherwise.
 *
 *********************************************************************/
static int path_matches(const char *path, const struct url_spec *pattern)
{
   return ((NULL == pattern->preg)
      || (0 == regexec(pattern->preg, path, 0, NULL, 0)));
}


/*********************************************************************
 *
 * Function    :  url_match
 *
 * Description :  Compare a URL against a URL pattern.
 *
 * Parameters  :
 *          1  :  pattern = a URL pattern
 *          2  :  url = URL to match
 *
 * Returns     :  Nonzero if the URL matches the pattern, else 0.
 *
 *********************************************************************/
int url_match(const struct url_spec *pattern,
              const struct http_request *http)
{
   if (pattern->tag_regex != NULL)
   {
      /* It's a tag pattern and shouldn't be matched against URLs */
      return 0;
   }

   return (port_matches(http->port, pattern->port_list)
      && host_matches(http, pattern) && path_matches(http->path, pattern));

}


/*********************************************************************
 *
 * Function    :  match_portlist
 *
 * Description :  Check if a given number is covered by a comma
 *                separated list of numbers and ranges (a,b-c,d,..)
 *
 * Parameters  :
 *          1  :  portlist = String with list
 *          2  :  port = port to check
 *
 * Returns     :  0 => no match
 *                1 => match
 *
 *********************************************************************/
int match_portlist(const char *portlist, int port)
{
   char *min, *max, *next, *portlist_copy;

   min = portlist_copy = strdup(portlist);

   /*
    * Zero-terminate first item and remember offset for next
    */
   if (NULL != (next = strchr(portlist_copy, (int) ',')))
   {
      *next++ = '\0';
   }

   /*
    * Loop through all items, checking for match
    */
   while (NULL != min)
   {
      if (NULL == (max = strchr(min, (int) '-')))
      {
         /*
          * No dash, check for equality
          */
         if (port == atoi(min))
         {
            freez(portlist_copy);
            return(1);
         }
      }
      else
      {
         /*
          * This is a range, so check if between min and max,
          * or, if max was omitted, between min and 65K
          */
         *max++ = '\0';
         if(port >= atoi(min) && port <= (atoi(max) ? atoi(max) : 65535))
         {
            freez(portlist_copy);
            return(1);
         }

      }

      /*
       * Jump to next item
       */
      min = next;

      /*
       * Zero-terminate next item and remember offset for n+1
       */
      if ((NULL != next) && (NULL != (next = strchr(next, (int) ','))))
      {
         *next++ = '\0';
      }
   }

   freez(portlist_copy);
   return 0;

}


/*********************************************************************
 *
 * Function    :  parse_forwarder_address
 *
 * Description :  Parse out the host and port from a forwarder address.
 *
 * Parameters  :
 *          1  :  address = The forwarder address to parse.
 *          2  :  hostname = Used to return the hostname. NULL on error.
 *          3  :  port = Used to return the port. Untouched if no port
 *                       is specified.
 *
 * Returns     :  JB_ERR_OK on success
 *                JB_ERR_MEMORY on out of memory
 *                JB_ERR_PARSE on malformed address.
 *
 *********************************************************************/
jb_err parse_forwarder_address(char *address, char **hostname, int *port)
{
   char *p = address;

   if ((*address == '[') && (NULL == strchr(address, ']')))
   {
      /* XXX: Should do some more validity checks here. */
      return JB_ERR_PARSE;
   }

   *hostname = strdup(address);
   if (NULL == *hostname)
   {
      return JB_ERR_MEMORY;
   }

   if ((**hostname == '[') && (NULL != (p = strchr(*hostname, ']'))))
   {
      *p++ = '\0';
      memmove(*hostname, (*hostname + 1), (size_t)(p - *hostname));
      if (*p == ':')
      {
         *port = (int)strtol(++p, NULL, 0);
      }
   }
   else if (NULL != (p = strchr(*hostname, ':')))
   {
      *p++ = '\0';
      *port = (int)strtol(p, NULL, 0);
   }

   return JB_ERR_OK;

}


/*
  Local Variables:
  tab-width: 3
  end:
*/
