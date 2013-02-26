#ifndef __UTIL_H
#define __UTIL_H
bool is_dir(const char *dir);
char *replace_char(char *str, size_t n, char c1, char c2);
char * dev_file2string(const char* file, char *buffer, size_t *lbuffer);
void sge_running_as_admin_user(bool *error, bool *isadmin);
bool file_exists(const char *file);
/* fixme: extend, in the absence of autoconf */
#define HAVE_STRSIGNAL (__linux__ || __sun || __sun__ || __NetBSD__ || __FreeBSD__)
#if !HAVE_STRSIGNAL
char *strsignal(int sig);
#endif
bool copy_linewise(const char *src, const char *dst);
#endif
