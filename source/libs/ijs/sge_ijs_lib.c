/* Portions of this code are Copyright 2011 Univa Inc. */
/* fixme: autoconf tests for headers */
#if __APPLE__ || __INTERIX
#  include <termios.h>
#  include <sys/ioctl.h>
#  include <grp.h>
#elif __hpux
#  include <termios.h>
#  include <stropts.h>
#elif __sun
#  include <stropts.h>
#  include <termio.h>
#elif __FreeBSD__ || (__NetBSD__ || __OpenBSD__)
#  include <termios.h>
#else
#  include <termio.h>
#endif

#include "uti/sge_rmon.h"

#include "sge_ijs_comm.h"

int continue_handler (COMM_HANDLE *comm_handle, char *hostname) {
  DENTER(TOP_LAYER, "ijs_suspend: continue_handler");
  DEXIT;
  return 0;
}

int suspend_handler (COMM_HANDLE *comm_handle, char *hostname, int b_is_rsh, int b_suspend_remote, unsigned int pid, dstring *dbuf) {
  DENTER(TOP_LAYER, "ijs_suspend: suspend_handler");
  DEXIT;
  return 1;
}
