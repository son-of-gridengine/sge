/* util.c -- extra utility functions

   Copyright (C) 2012 Dave Love, University of Liverpool <d.love@liv.ac.uk>

   This file is free software: you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.

   This file is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this file.  If not, a copy can be downloaded
   from http://www.gnu.org/licenses/lgpl.html.
*/

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <msg_common.h>
#include "uti/sge_rmon.h"
#include "uti/sge_log.h"
#include "uti/sge_uidgid.h"

/* Is DIR and existing directory?  */
bool
is_dir(const char *dir)
{
   struct stat statbuf;

   errno = 0;
   if (stat(dir, &statbuf) < 0) {
      errno = ENODEV;
      return false;
   }
   if (!S_ISDIR(statbuf.st_mode)) {
      errno = ENODEV;
      return false;
   }
   return true;
}

/* Replace all occurrences of C1 in first N characters of STR with C2.
   Return STR.  */
char *
replace_char(char *str, size_t n, char c1, char c2)
{
   int i;

   for (i=0; i<n; i++)
      if (str[i] == c1) str[i] = c2;
   return str;
}

/* Read contents of FILE into BUFFER of size LBUFFER.
   Update LBUFFER with the number of bytes read and return BUFFER.
   Doesn't stat the file to find it's length, unlike sge_file2string,
   so works for /proc files.  */
char *
dev_file2string(const char* file, char *buffer, size_t *lbuffer)
{
   int fd;
   size_t i;

   DENTER(TOP_LAYER, "dev_file2string");
   errno = 0;
   buffer[0] = '\0';
   if ((fd = open(file, O_RDONLY)) == -1) {
      *lbuffer = 0;
      ERROR((SGE_EVENT, MSG_FILE_OPENFAILED_SS, file, strerror(errno)));
      DRETURN(NULL);
   }
   if ((i = read(fd, buffer, *lbuffer)) == 0) {
      ERROR((SGE_EVENT, MSG_FILE_FREADFAILED_SS, file, strerror(errno)));
      close(fd);
      *lbuffer = 0;
      buffer[0] = '\0';
      DRETURN(buffer);
   }
   close(fd);
   i = MIN(i, (*lbuffer)-1);
   *lbuffer = i;
   buffer[i] = '\0';
   DRETURN(buffer);
}

/* Return in ISADMIN whether running as SGE admin user or root unless
   an error occurred, indicated by ERROR true.  Intended for
   determining whether to switch2start_user or switch2admin_user in
   library routines that need privileges..  */
void
sge_running_as_admin_user(bool *error, bool *isadmin)
{
   uid_t uid;
   gid_t gid;

   if (sge_user2uid (get_admin_user_name(), &uid, &gid, 1) != 0) {
      *error = true;
      return;
   }
   *error = false;
   *isadmin = (geteuid() == uid) || geteuid() == SGE_SUPERUSER_UID;
}

/* Return true iff FILE can be stat'ed.  */
bool
file_exists(const char *file)
{
   SGE_STRUCT_STAT statbuf;
   return file && *file && (SGE_STAT(file, &statbuf) == 0);
}

/* Copy line-by-line (unbuffered writes) from file SRC to file DST,
   assuming line length < MAX_STRING_SIZE.  */
bool
copy_linewise(const char *src, const char *dst)
{
   FILE *srcfp, *dstfp;
  char buffer[MAX_STRING_SIZE];
  bool ok = true;

  if (!(srcfp = fopen(src, "r")) ||
      !(dstfp = fopen(dst, "w")))
     return false;
  while (fgets(buffer, sizeof buffer, srcfp)) {
     size_t l = strlen(buffer);
     if (fwrite(buffer, 1, l, dstfp) != l) {
        fclose(dstfp);
        fclose(srcfp);
        return false;
     }
     fflush(dstfp);
  }
  if (ferror(srcfp) || ferror(dstfp)) ok = false;
  fclose(dstfp);
  fclose(srcfp);
  return ok;
}
