/* This is taken from
   http://gridengine.sunsource.net/issues/show_bug.cgi?id=2552.  It
   was posted without a licence statement, which makes it implicitly
   under the new BSD licence according to the sunsource.net terms at
   http://www.sunsource.net/TUPPCP.html.

   It is intended to be used as an LD_PRELOADed shared object to allow
   core dumps from daemons under Linux if the SGE admin user isn't
   root (and is obviously applicable to daemons other than SGE's).

   Dave Love <d.love@liverpool.ac.uk>  2010-06.  */

/*
 * Place holder library to ensure prctl(PR_SET_DUMPABLE, 1) is done after
 * each setuid(), seteuid(), setgid(), and setegid()
 */
#include <sys/types.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/prctl.h>

static pthread_once_t only_once = PTHREAD_ONCE_INIT;

static int (*setuid_func)(uid_t uid) = NULL;
static int (*seteuid_func)(uid_t uid) = NULL;
static int (*setgid_func)(gid_t gid) = NULL;
static int (*setegid_func)(gid_t gid) = NULL;

static void init_funcs(void)
{
    setuid_func = (int (*)(uid_t uid))dlsym(RTLD_NEXT,  "setuid");
   seteuid_func = (int (*)(uid_t uid))dlsym(RTLD_NEXT, "seteuid");
    setgid_func = (int (*)(gid_t uid))dlsym(RTLD_NEXT,  "setgid");
   setegid_func = (int (*)(gid_t uid))dlsym(RTLD_NEXT, "setegid");
}

#define WRAP(func, clib_func, t) \
int func(t uid) \
{ \
   pthread_once(&only_once, init_funcs); \
   int ret = clib_func(uid); \
   prctl(PR_SET_DUMPABLE, 1, 42, 42, 42); \
   return ret; \
}

WRAP(setuid,  setuid_func,  uid_t)
WRAP(seteuid, seteuid_func, uid_t)
WRAP(setgid,  setgid_func,  gid_t)
WRAP(setegid, setegid_func, gid_t)
