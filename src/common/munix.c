/*  munix.c -- special common code for Unix  */

#include "../h/gsupport.h"

#if UNIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *findexe(char *name, char *buf, size_t len);
static char *findonpath(char *name, char *buf, size_t len);
static char *followsym(char *name, char *buf, size_t len);
static char *canonize(char *path);

/*
 *  installdir(prog) -- return allcoated string holding installation directory.
 *
 *  Given the argv[0] by which icont or iconx was executed, and assuming
 *  it was set by the shell or other equally correct invoker, finds and
 *  returns the name of the grandparent directory (the parent of .../bin).
 */
char *installdir(char *prog) {
   char buf1[MaxPath + 6], buf2[MaxPath];

   if (findexe(prog, buf1, sizeof(buf1)) == NULL)
      return NULL;
   if (followsym(buf1, buf2, sizeof(buf2)) != NULL)
      strcpy(buf1, buf2);
   strcat(buf1, "/../..");
   canonize(buf1);
   return salloc(buf1);
   }

/*
 *  findexe(prog, buf, len) -- find absolute executable path, given argv[0]
 *
 *  Finds the absolute path to prog, assuming that prog is the value passed
 *  by the shell in argv[0].  The result is placed in buf, which is returned.
 *  NULL is returned in case of error.
 */

static char *findexe(char *name, char *buf, size_t len) {
   int i, n;
   char *s, *t;
   char tbuf[MaxPath];

   if (name == NULL)
      return NULL;

   /* if name does not contain a slash, search $PATH for file */
   if (strchr(name, '/') != NULL)
      strcpy(buf, name);
   else if (findonpath(name, buf, len) == NULL) 
      return NULL;

   /* if path is not absolute, prepend working directory */
   if (buf[0] != '/') {
      n = strlen(buf) + 1;
      memmove(buf + len - n, buf, n);
      if (getcwd(buf, len - n) == NULL)
         return NULL;
      s = buf + strlen(buf);
      *s = '/';
      memcpy(s + 1, buf + len - n, n);
      }
   canonize(buf);
   return buf;
   }

/*
 *  findonpath(name, buf, len) -- find name on $PATH
 *
 *  Searches $PATH (using POSIX 1003.2 rules) for executable name,
 *  writing the resulting path in buf if found.
 */
static char *findonpath(char *name, char *buf, size_t len) {
   int nlen, plen;
   char *path, *next, *sep, *end;

   nlen = strlen(name);
   path = getenv("PATH");
   if (path == NULL || *path == '\0')
      path = ".";
   end = path + strlen(path);
   for (next = path; next <= end; next = sep + 1) {
      sep = strchr(next, ':');
      if (sep == NULL)
         sep = end;
      plen = sep - next;
      if (plen == 0) {
         next = ".";
         plen = 1;
         }
      if (plen + 1 + nlen + 1 > len)
         return NULL;
      memcpy(buf, next, plen);
      buf[plen] = '/';
      strcpy(buf + plen + 1, name);
      if (access(buf, X_OK) == 0)
         return buf;
      }
   return NULL;
   }

/*
 *  followsym(name, buf, len) -- follow symlink to final destination.
 *
 *  If name specifies a file that is a symlink, resolves the symlink to
 *  its ultimate destination, and returns buf.  Otherwise, returns NULL.
 *
 *  Note that symlinks in the path to name do not make it a symlink.
 *
 *  buf should be long enough to hold name.
 */

#define MAX_FOLLOWED_LINKS 24

static char *followsym(char *name, char *buf, size_t len) {
   int i, n;
   char *s, tbuf[MaxPath];

   strcpy(buf, name);

   for (i = 0; i < MAX_FOLLOWED_LINKS; i++) {
      if ((n = readlink(buf, tbuf, sizeof(tbuf) - 1)) <= 0)
         break;
      tbuf[n] = 0;

      if (tbuf[0] == '/') {
         if (n < len)
            strcpy(buf, tbuf);
         else
            return NULL;
         }
      else {
         s = strrchr(buf, '/');
         if (s != NULL)
            s++;
         else
            s = buf;
         if ((s - buf) + n < len)
            strcpy(s, tbuf);
         else
            return NULL;
         }
      canonize(buf);
      }

   if (i > 0 && i < MAX_FOLLOWED_LINKS)
      return buf;
   else
      return NULL;
   }

/*
 *  canonize(path) -- put file path in canonical form.
 *
 *  Rewrites path in place, and returns it, after excising fragments of
 *  "." or "dir/..".  All leading slashes are preserved but other extra
 *  slashes are deleted.  The path never grows longer except for the
 *  special case of an empty path, which is rewritten to be ".".
 *
 *  No check is made that any component of the path actually exists or
 *  that inner components are truly directories.  From this it follows
 *  that if "foo" is any file path, canonizing "foo/.." produces the path
 *  of the directory containing "foo".
 */

static char *canonize(char *path) {
   int len;
   char *root, *end, *in, *out, *prev;

   /* initialize */
   root = path;			/* set barrier for trimming by ".." */
   end = path + strlen(path);		/* set end of input marker */
   while (*root == '/')		/* preserve all leading slashes */
      root++;
   in = root;				/* input pointer */
   out = root;				/* output pointer */

   /* scan string one component at a time */
   while (in < end) {

      /* count component length */
      for (len = 0; in + len < end && in[len] != '/'; len++)
         ;

      /* check for ".", "..", or other */
      if (len == 1 && *in == '.')	/* just ignore "." */
         in++;
      else if (len == 2 && in[0] == '.' && in[1] == '.') {
         in += 2;			/* skip over ".." */
         /* find start of previous component */
         prev = out;
         if (prev > root)
            prev--;			/* skip trailing slash */
         while (prev > root && prev[-1] != '/')
            prev--;			/* find next slash or start of path */
         if (prev < out - 1
         && (out - prev != 3 || strncmp(prev, "../", 3) != 0)) {
            out = prev;		/* trim trailing component */
            }
         else {
            memcpy(out, "../", 3);	/* cannot trim, so must keep ".." */
            out += 3;
            }
         }
      else {
         memmove(out, in, len);	/* copy component verbatim */
         out += len;
         in += len;
         *out++ = '/';		/* add output separator */
         }

      while (in < end && *in == '/')	/* consume input separators */
         in++;
      }

   /* final fixup */
   if (out > root)
      out--;				/* trim trailing slash */
   if (out == path)
      *out++ = '.';			/* change null path to "." */
   *out++ = '\0';
   return path;			/* return result */
   }

#else                                  /* UNIX */

static char junk;		/* avoid empty module */

#endif					/* UNIX */
