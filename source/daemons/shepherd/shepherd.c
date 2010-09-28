/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if defined(LINUX)
#  include <grp.h>
#endif

#if defined(CRAY)
#   if !defined(SIGXCPU)
#       define SIGXCPU SIGCPULIM
#   endif
    /* for killm category on Crays */
#   include <sys/category.h>
struct rusage {
   struct timeval ru_stime;
   struct timeval ru_utime;
};
    /* for job/session stuff */
#   include <sys/session.h>
    /* times() */
#   include <sys/times.h>
#endif

#if defined(NECSX4) || defined(NECSX5)
#   include <sys/times.h>
#endif

#include "basis_types.h"
#include "config_file.h"
#include "err_trace.h"
#include "setrlimits.h"
#include "signal_queue.h"
#include "execution_states.h"
#include "sge_signal.h"
#include "sge_time.h"
#include "sge_parse_num_par.h"
#include "sgedefs.h"
#include "exec_ifm.h"
#include "pdc.h"
#include "procfs.h"
#include "builtin_starter.h"
#include "sge_afsutil.h"
#include "qlogin_starter.h"
#include "sge_stdlib.h"
#include "sge_uidgid.h"
#include "sge_feature.h"
#include "sge_unistd.h"
#include "sge_os.h"
#include "sge_pset.h"
#include "sge_dstring.h"
#include "sge_shepconf.h"
#include "sge_mt_init.h"
#include "msg_common.h"
#include "version.h"

#include "sge_reportL.h"

#if defined(IRIX)
#include "sge_processes_irix.h"
#endif

#if defined(SOLARIS) || defined(ALPHA)
/* ALPHA4 only has wait3() prototype if _XOPEN_SOURCE_EXTENDED is defined */
pid_t wait3(int *, int, struct rusage *);
#endif

#define NO_CKPT          0x000
#define CKPT             0x001     /* set for all ckpt jobs                  */
#define CKPT_KERNEL      0x002     /* set for kernel level ckpt jobs         */
#define CKPT_USER        0x004     /* set for userdefined ckpt job           */
#define CKPT_TRANS       0x008     /* set for transparent ckpt jobs          */
#define CKPT_HIBER       0x010     /* set for hibernator ckpt jobs           */
#define CKPT_CPR         0x020     /* set for cpr ckpt jobs                  */
#define CKPT_CRAY        0x040     /* set for cray ckpt jobs                 */
#define CKPT_REST_KERNEL 0x080     /* set for all restarted kernel ckpt jobs */
#define CKPT_REST        0x100     /* set for all restarted ckpt jobs        */
#define CKPT_APPLICATION 0x200     /* application checkpointing              */

#define RESCHEDULE_EXIT_STATUS 99
#define APPERROR_EXIT_STATUS 100

int signalled_ckpt_job = 0;   /* marker if signalled a ckpt job */
int ckpt_signal = 0;          /* signal to send to ckpt job */

static int notify_tasker(u_long32 exit_status);
int main(int argc, char **argv);
static int start_child(char *childname, char *script_file, pid_t *pidp, int timeout, int ckpt_type);
static void forward_signal_to_job(int pid, int timeout, int *postponed_signal, int remaining_alarm, pid_t ctrl_pid[3]);
static int check_ckpttype(void);
static int wait_my_child(int pid, int ckpt_pid, int ckpt_type, struct rusage *rusage, int timeout, int ckpt_interval, char *childname);
static void set_ckpt_params(int, char *, int, char *, int, char *, int, int *);
static void set_ckpt_restart_command(char *, int, char *, int);
static void handle_job_pid(int, int, int *);
static int start_async_command(char *descr, char *cmd);
static void start_clean_command(char *);
static int checkpointed_file_exists(void);
static void create_checkpointed_file(int ckpt_is_in_arena);
static pid_t start_token_cmd(int, char *, char *, char *, char *);
static int do_wait(pid_t);
static void set_sig_handler(int);
static void shepherd_signal_handler(int);


/* overridable control methods */
static void verify_method(char *method_name);
static void shepherd_signal_job(pid_t pid, int sig);

char shepherd_job_dir[2048];

char ckpt_command[2048], migr_command[2048], rest_command[2048],
     clean_command[2048];

int received_signal=0;    /* set by signalhandler, when a signal arrives */
int notify; /* 0 if no notify or # of seconds to delay signal */

static void signal_handler(int signal);
static void set_shepherd_signal_mask(void);
static void change_shepherd_signal_mask(void);

static int exit_status_for_qrsh = 0;

static int do_prolog(int timeout, int ckpt_type);
static int do_epilog(int timeout, int ckpt_type);
static int do_pe_start(int timeout, int ckpt_type, pid_t *pe_pid);
static int do_pe_stop(int timeout, int ckpt_type, pid_t *pe_pid);

void shepherd_deliver_signal(int sig,
                             int pid,
                             int *postponed_signal,
                             int remaining_alarm,
                             pid_t *ctrl_pid);

static int map_signal(int signal);

static int map_signal(int sig) 
{
   int ret = sig;

   if (sig == SIGTTIN) {
      FILE *signal_file = fopen("signal", "r");

      if (signal_file != NULL) {
         fscanf(signal_file, "%d", &ret);
         fclose(signal_file);
      } 
   } else if (sig == SIGTTOU) {
      /* checkpoint signal value */
      ;
   } else if (sig == SIGUSR2) {
#ifdef SIGXCPU
      ret = SIGXCPU;
#else
      ret = SIGKILL;
#endif
   } else if (sig == SIGWINCH) {
      ret = SIGSTOP; 
   } else if (sig == SIGTSTP) {
      ret = SIGKILL;
   } else if (sig == SIGUSR1 || sig == SIGCONT) {
      /* identically */
      ;
   } else {
      /* unmapable signal */
      shepherd_trace("should map unmapable signal");
   }

   if (sig != ret) {
      shepherd_trace_sprintf("mapped signal %s to signal %s", 
                             sge_sys_sig2str(sig), sge_sys_sig2str(ret));
   }
   return ret;
}

static int do_prolog(int timeout, int ckpt_type)
{
   char *prolog;
   char err_str[2*SGE_PATH_MAX+128], command[10000];
   int exit_status;

   shepherd_state = SSTATE_BEFORE_PROLOG;

   /* start prolog */
   prolog = get_conf_val("prolog");
   if (strcasecmp("none", prolog)) {
      int i, n_exit_status = count_exit_status();

      replace_params(prolog, command, sizeof(command)-1, prolog_epilog_variables);
      exit_status = start_child("prolog", command, NULL, timeout, ckpt_type);

      if (n_exit_status<(i=count_exit_status())) {
         shepherd_trace_sprintf("exit states increased from %d to %d\n", n_exit_status, i);
         /* in this case the child didnt get to the exec call or it failed */
         shepherd_trace("failed starting prolog");
         return SSTATE_BEFORE_PROLOG;
      }

      if (exit_status) {
         /*
          * We should not exit here:
          *    We may neither start the job nor pe_start procedure, 
          *    but we should try to run the epilog procedure if it 
          *    exists, to allow some cleanup. The exit status of 
          *    this prolog failure may not get lost.
          */
         switch( exit_status ) {
            case RESCHEDULE_EXIT_STATUS:
               shepherd_state = SSTATE_AGAIN;
               break;
            case APPERROR_EXIT_STATUS:
               shepherd_state = SSTATE_APPERROR;
               break;
            default:
               shepherd_state = SSTATE_PROLOG_FAILED;
         }
         sprintf(err_str, "exit_status of prolog = %d", exit_status);
         shepherd_error_impl(err_str, 0);
         return SSTATE_PROLOG_FAILED;
      }
   } else {
      shepherd_trace("no prolog script to start");
   }

   return 0;
}

static int do_epilog(int timeout, int ckpt_type)
{
   char *epilog;
   char err_str[2*SGE_PATH_MAX+128], command[10000];
   int exit_status;

   shepherd_state = SSTATE_BEFORE_EPILOG;

   epilog = get_conf_val("epilog");
   if (strcasecmp("none", epilog)) {
      int i, n_exit_status = count_exit_status();

      /* start epilog */
      replace_params(epilog, command, sizeof(command)-1, 
                     prolog_epilog_variables);
      exit_status = start_child("epilog", command, NULL, timeout, ckpt_type);
      if (n_exit_status<(i=count_exit_status())) {
         shepherd_trace_sprintf("exit states increased from %d to %d\n", 
                                n_exit_status, i);
         /*
         ** in this case the child didnt get to the exec call or it failed
         ** the status that waitpid and finally start_child returns is
         ** reserved for the exit status of the job
         */
         shepherd_trace("failed starting epilog");
         return SSTATE_BEFORE_EPILOG;
      }

      if (exit_status) {
         switch( exit_status ) {
            case RESCHEDULE_EXIT_STATUS:
               shepherd_state = SSTATE_AGAIN;
               break;
            case APPERROR_EXIT_STATUS:
               shepherd_state = SSTATE_APPERROR;
               break;
            default:
               shepherd_state = SSTATE_EPILOG_FAILED;
         }
         sprintf(err_str, "exit_status of epilog = %d", exit_status);
         shepherd_error_impl(err_str, 0);
         return SSTATE_EPILOG_FAILED;
      }
   }
   else
      shepherd_trace("no epilog script to start");
   
   return 0;
}

static int do_pe_start(int timeout, int ckpt_type, pid_t *pe_pid)
{
   char *pe_start;
   char err_str[2*SGE_PATH_MAX+128], command[10000];
   int exit_status;

   shepherd_state = SSTATE_BEFORE_PESTART;

   /* start pe_start */
   pe_start = get_conf_val("pe_start");
   if (strcasecmp("none", pe_start)) {
      int i, n_exit_status = count_exit_status();

      shepherd_trace(pe_start);
      replace_params(pe_start, command, sizeof(command)-1, 
         pe_variables);
      shepherd_trace(command);

      /* 
         starters of parallel environments may not get killed 
         in case of success - so we save their pid for later use
      */
      exit_status = start_child("pe_start", command, pe_pid, timeout, ckpt_type);
      if (n_exit_status<(i=count_exit_status())) {
         shepherd_trace_sprintf("exit states increased from %d to %d\n", 
                                n_exit_status, i);
         /*
         ** in this case the child didnt get to the exec call or it failed
         ** the status that waitpid and finally start_child returns is
         ** reserved for the exit status of the job
         */
         shepherd_trace("failed starting pe_start");
         return SSTATE_BEFORE_PESTART;
      }
      if (exit_status) {
         /*
          * We should not exit here:
          *    We may not start the job, but we should try to run 
          *    the pe_stop and epilog procedures if they exist, to 
          *    allow some cleanup. The exit status of this pe_start
          *    failure may not get lost.
          */
         switch( exit_status ) {
            case RESCHEDULE_EXIT_STATUS:
               shepherd_state = SSTATE_AGAIN;
               break;
            case APPERROR_EXIT_STATUS:
               shepherd_state = SSTATE_APPERROR;
               break;
            default:
               shepherd_state = SSTATE_PESTART_FAILED;
         }

         sprintf(err_str, "exit_status of pe_start = %d", exit_status);
         shepherd_error_impl(err_str, 0);
         return SSTATE_PESTART_FAILED;
      }
   } else {
      shepherd_trace("no pe_start script to start");
   }

   return 0;
}

static int do_pe_stop(int timeout, int ckpt_type, pid_t *pe_pid)
{
   char *pe_stop;
   char err_str[2*SGE_PATH_MAX+128], command[10000];
   int exit_status;
   
   pe_stop = get_conf_val("pe_stop");
   if (strcasecmp("none", pe_stop)) {
      int i, n_exit_status = count_exit_status();

      shepherd_state = SSTATE_BEFORE_PESTOP;

      shepherd_trace(pe_stop);
      replace_params(pe_stop, command, sizeof(command)-1, 
         pe_variables);
      shepherd_trace(command);
      exit_status = start_child("pe_stop", command, NULL, timeout, ckpt_type);

      /* send a kill to pe_start process
       *
       * For now, don't do it !!!! This murders the PVMD, and if
       * another job uses it ...
       * shepherd_signal_job(-*pe_pid, SIGKILL);
       */

      if (n_exit_status<(i=count_exit_status())) {
         shepherd_trace_sprintf("exit states increased from %d to %d\n", 
                                n_exit_status, i);
         /*
         ** in this case the child didnt get to the exec call or it failed
         ** the status that waitpid and finally start_child returns is
         ** reserved for the exit status of the job
         */
         shepherd_trace("failed starting pe_stop");
         return SSTATE_BEFORE_PESTOP;
      }

      if (exit_status) {
         /*
          * We should not exit here:
          *    We should try to run the epilog procedure if it 
          *    exists, to allow cleanup. The exit status of this 
          *    pe_start failure may not get lost.
          */
         switch( exit_status ) {
            case RESCHEDULE_EXIT_STATUS:
               shepherd_state = SSTATE_AGAIN;
               break;
            case APPERROR_EXIT_STATUS:
               shepherd_state = SSTATE_APPERROR;
               break;
            default:
               shepherd_state = SSTATE_PESTOP_FAILED;
         }

         sprintf(err_str, "exit_status of pe_stop = %d", exit_status);
         shepherd_error_impl(err_str, 0);
         return SSTATE_PESTOP_FAILED;
      }
   } else {
      shepherd_trace("no pe_stop script to start");
   }

   return 0;
}

/************************************************************************
 This is the shepherd. For each job we start under the control of SGE
 we first start a shepherd.

 shepherd is responsible for:

 - setting up the environment for the job
 - starting the job
 - retrieve the job usage information for the finished job
 - passing through of signals sent by execd to the job

 Strategy: Do all communication with the execd per filesystem. This gives
           us a chance to see what's happening from outside. This decouples
           shepherdd from the execd. In filesystems we trust, in communication
           we do not.

 Each shepherdd has its own directory. The caller has to cd to this directory
 before starting shepherdd.

 Input to the shepherdd:
    "config"-file   holds configuration data (config_file.c)
    "signal"-file   when execd wants a signal to be delivered

 Output from shepherdd:
    "trace"-file    reports activities of shepherdd (err_trace.c)
    "pid"-file      contains pid of shepherdd
    "error"-file    contains error strings. This can be used by execd for
                    problem reports. (err_trace.c):w

    "exit_status"-file 
                    contains the exit status of shepherd after termination.

 

 How can a caller see whats going on:

 If the shepherd was started the "pid" file exists. This file contains the
 pid of the shepherd and can be used to search the process table.

 If shepherd cant be found in the process table and the "exit_status"-file 
 does not exist the shepherd terminated irregular. Only kill(-9) or a system
 breakdown should cause this.

 If the "exit_status"-file exists the shepherd has terminated regular.
 If exit_status==0 everything is fine. If exit_status!=0 a problem occured.
 The error file should give hints what happened.
 exit_status values: see shepherd_states header file

 ************************************************************************/

static void signal_handler(int signal) 
{
   /* may not log in signal handler 
      as long as Async-Signal-Safe functions such as fopen() are used in 
      shepherd logging code */
#if 0
   shepherd_trace_sprintf("received signal %s", sge_sys_sig2str(signal));
#endif

   received_signal = signal;
}

static void show_shepherd_version(void) {

   printf("%s %s\n", GE_SHORTNAME, GDI_VERSION);
   printf("%s %s [options]\n", MSG_GDI_USAGE_USAGESTRING , "sge_shepherd");
   printf("   %-40.40s %s\n", MSG_GDI_USAGE_help_OPT , MSG_GDI_UTEXT_help_OPT);

}

int main(int argc, char **argv) 
{
   char err_str[2*SGE_PATH_MAX+128];
   char *admin_user;
   char *script_file;
   char *pe, *cp;
   pid_t pe_pid;
   int script_timeout;
   int exit_status = 0;
   int uid, i, ret;
   FILE *fp;
   pid_t pid;
   SGE_STRUCT_STAT buf;
   int ckpt_type;
   int return_code = 0;
   int run_epilog, run_pe_stop;
   dstring ds;
   char buffer[256];

   if (argc >= 2) {
      if ( strcmp(argv[1],"-help") == 0) {
         show_shepherd_version();
         return 1;
      }
   }
   sge_mt_init();
	shepherd_trace_init( );

   sge_dstring_init(&ds, buffer, sizeof(buffer));
   shepherd_trace_sprintf("shepherd called with uid = "uid_t_fmt", euid = "uid_t_fmt, 
                          getuid(), geteuid());

   shepherd_state = SSTATE_BEFORE_PROLOG;
   if (getcwd(shepherd_job_dir, 2047) == NULL) {
      shepherd_error_sprintf("can't read cwd - getcwd failed: %s", 
                             strerror(errno));
   }   

   if (argc >= 2 && !strcmp("-bg", argv[1])) {
      foreground = 0;   /* no output to stderr */
   }

   /* make sure pending SIGPIPE signals logged in trace file */ 
   set_sig_handler(SIGPIPE);
   sge_set_def_sig_mask(SIGPIPE, NULL);
   set_shepherd_signal_mask();
      
   config_errfunc = shepherd_error;

   /*
    * read configuration file
    * this is done because we need to know the name of the admin user
    * for file creation
    */
   if ((ret=read_config("config"))) {

      /* if we can't read the config file, try to switch to admin user
       * that our error/trace message can be succesfully logged
       * this is done in case where our getuid() is root and our
       * geteuid() is a normal user id
       */

      if (getuid() == 0 && geteuid() != 0) {
          char name[128];
          if (!sge_uid2user(geteuid(), name, sizeof(name), MAX_NIS_RETRIES)) {
             sge_set_admin_username(name, NULL);
             sge_switch2admin_user();
          }
      }
      if (ret == 1) {
         shepherd_error_sprintf("can't read configuration file: %s", 
                                strerror(errno));
      } else {
         shepherd_error("can't read configuration file: malloc() failure");
      }
   }

   /* init admin user stuff */
   admin_user = get_conf_val("admin_user");
   if (sge_set_admin_username(admin_user, err_str)) {
      shepherd_error(err_str);
   }

   if (sge_switch2admin_user()) {
      shepherd_error(err_str);
   }

	/* finalize initialization of shepherd_trace - give the trace file
	 * to the job owner so not only root/admin user but also he can use
	 * the trace file later.
	 */
	shepherd_trace_chown( get_conf_val("job_owner"));
	
   sprintf(err_str, "starting up %s", feature_get_product_name(FS_VERSION, &ds));
   if (shepherd_trace(err_str)) {
      shepherd_error("can't write to \"trace\" file");
   }
 
   /* do not start job in cases of wrong 
      configuration of job control methods */
   verify_method("terminate_method");
   verify_method("suspend_method");
   verify_method("resume_method");

   /* write our pid to file */
   pid = getpid();

   if (!(fp = fopen("pid", "w"))) {
      shepherd_error_sprintf("can't write my pid to \"pid\" file: %s", 
                             strerror(errno));
   }
   fprintf(fp, pid_t_fmt"\n", pid);
   fflush(fp);
#if (IRIX)
   fsync(fileno(fp));
#endif
   fclose(fp);

   uid = getuid();

   if (uid) {
      shepherd_trace_sprintf("warning: starting not as root (uid=%d)", uid);
   }

   /* Shepherd in own process group */
   i = setpgid(pid, pid);
   shepherd_trace_sprintf("setpgid("pid_t_fmt", "pid_t_fmt") returned %d", 
                          pid, pid, i);

   if ((ckpt_type = check_ckpttype()) == -1) {
      shepherd_error("checkpointing with incomplete specification or unknown "
                     "interface requested");
   }

   script_timeout = atoi(get_conf_val("script_timeout"));
   notify = atoi(get_conf_val("notify"));

   /*
    * Create processor set
    */
   sge_pset_create_processor_set();

   /*
    * this blocks sge_shepherd until the first time the token is
    * sucessfully set, then we start the sge_coshepherd in background
    */
   if ((cp = search_conf_val("use_afs")) && atoi(cp) > 0) {
      char *coshepherd = search_conf_val("coshepherd");
      char *set_token_cmd = search_conf_val("set_token_cmd");
      char *job_owner = search_conf_val("job_owner");
      char *token_extend_time = search_conf_val("token_extend_time");
      char *tokenbuf;

      shepherd_trace("beginning AFS setup");
      if (!(coshepherd && set_token_cmd && job_owner && token_extend_time)) {
         shepherd_state = SSTATE_AFS_PROBLEM;
         shepherd_error("AFS with incomplete specification requested");
      }
      if ((tokenbuf = sge_read_token(TOKEN_FILE)) == NULL) {
         shepherd_state = SSTATE_AFS_PROBLEM;
         shepherd_error("can't read AFS token");
      }   

      if (sge_afs_extend_token(set_token_cmd, tokenbuf, job_owner,
                           atoi(token_extend_time), err_str)) {
         shepherd_state = SSTATE_AFS_PROBLEM;                  
         shepherd_error(err_str);
      }
       
      shepherd_trace(err_str);
      shepherd_trace("sucessfully set AFS token");
         
      memset(tokenbuf, 0, strlen(tokenbuf));
      free(tokenbuf);


      if ((coshepherd_pid = start_token_cmd(0, coshepherd, set_token_cmd,
                                            job_owner, token_extend_time)) < 0) {
         shepherd_state = SSTATE_AFS_PROBLEM;                                    
         shepherd_error("can't start coshepherd");                               
      }
       
      shepherd_trace("AFS setup done");
   }

   pe = get_conf_val("pe");
   if (!strcasecmp(pe, "none")) {
      pe = NULL;
   }

   run_epilog = 1;
   if ((exit_status = do_prolog(script_timeout, ckpt_type))) {
      if (exit_status == SSTATE_BEFORE_PROLOG)
         run_epilog = 0;
   } else if (pending_sig(SIGTTOU)) {
      /* got a signal during prolog causing job to migrate? */ 
      shepherd_trace("got SIGMIGR before job start");
      exit_status = 0; /* no error */
      create_checkpointed_file(0);
   } else if (pending_sig(SIGKILL)) {
      /* got a signal during prolog causing job exit ? */ 
      shepherd_trace("got SIGKILL before job start");
      exit_status = 0; /* no error */
   } else {
      /* start pe_start */
      run_pe_stop = 1;
      if (pe && (exit_status = do_pe_start(script_timeout, ckpt_type, &pe_pid))) {
         if (exit_status == SSTATE_BEFORE_PESTART) {
            run_pe_stop = 0;
         }
      } else {
         shepherd_state = SSTATE_BEFORE_JOB;
         /* start job script */
         script_file = get_conf_val("script_file");
         if (strcasecmp("none", script_file)) {
            if (pending_sig(SIGKILL)) { 
               shepherd_trace("got SIGKILL before job start");
               exit_status = 0; /* no error */
            } else if (pending_sig(SIGTTOU)) { 
               shepherd_trace("got SIGMIGR before job start");
               create_checkpointed_file(0);
               exit_status = 0; /* no error */
            } else {
               exit_status = start_child("job", script_file, NULL, 0, ckpt_type);

               if (count_exit_status()>0) {
                  /*
                  ** in this case the child didn't get to the exec call or it failed
                  ** the status that waitpid and finally start_child returns is
                  ** reserved for the exit status of the job
                  */

                  /* we may not give up here 
                     start pe_stop and epilog before exiting */
                  shepherd_trace("failed starting job");
               } else {
                  bool call_error_impl = false;

                  switch( exit_status ) {
                     case RESCHEDULE_EXIT_STATUS:
                        if( !atoi(get_conf_val("forbid_reschedule"))) {
                           shepherd_state = SSTATE_AGAIN;
                           call_error_impl = true;
                        }
                        break;
                     case APPERROR_EXIT_STATUS:
                        if( !atoi(get_conf_val("forbid_apperror"))) {
                           shepherd_state = SSTATE_APPERROR;
                           call_error_impl = true;
                        }
                        break;
                  }
                  if( call_error_impl ) {
                     sprintf( err_str,
                        "exit_status of job start = %d", exit_status);
                     shepherd_error_impl(err_str,0);
                  }
               } 
            }
         } else {
            shepherd_trace("no script to start");
         }

         clear_queued_signals();
      }

      /* start pe_stop */
      if (pe && run_pe_stop) {
         do_pe_stop(script_timeout, ckpt_type, &pe_pid);
      }
   }

   if (run_epilog) {
      do_epilog(script_timeout, ckpt_type);
   }

   /*
    * Free previously created processor set
    */
   sge_pset_free_processor_set();

   if (coshepherd_pid > 0) {
      shepherd_trace("sending SIGTERM to sge_coshepherd");
      sge_switch2start_user();
      kill(coshepherd_pid, SIGTERM);
      sge_switch2admin_user();
   }

   if (!SGE_STAT("exit_status", &buf) && buf.st_size) {
      /* retrieve first exit status from exit status file */
      if (!(fp = fopen("exit_status", "r")) || (fscanf(fp, "%d\n", &return_code)!=1))
         return_code = ESSTATE_NO_EXITSTATUS;
      fclose(fp);
   } else {
      /* ensure an exit status file exists */
		shepherd_write_exit_status( "0" );
      return_code = 0;
   }

   if(search_conf_val("qrsh_control_port") != NULL) {
      FILE *fd = fopen("shepherd_about_to_exit", "w");
      if(fd) {
         fclose(fd);
      }
      write_exit_code_to_qrsh(exit_status_for_qrsh);
   }
	
	shepherd_trace_exit( );
   return return_code;
}

/********************************************************************
 start child process
 returns exit status of script

PARAMETER

   pidp 

      If NULL the process gets killed after child exit
      to ensure nothing hangs around.

      If non NULL a kill is only sent in case of an error.
      In case of no errors the pid is saved in *pidp 
      for later use.

 *******************************************************************/
static int start_child(
char *childname,        /* prolog, job, epilog */
char *script_file,
pid_t *pidp,
int timeout,
int ckpt_type 
) {
   SGE_STRUCT_STAT buf;
   u_long32 start_time;
   u_long32 end_time;
   int pid;
   struct rusage rusage;
   int  status, child_signal=0, exit_status;
   u_long32 wait_status = 0;
   FILE *fp;
   int core_dumped, ckpt_interval, ckpt_pid;

#if defined(IRIX)
   ash_t ash = 0;
#elif defined(NECSX4) || defined(NECSX5)
	id_t jobid = 0;
#elif defined(CRAY)
   int jobid = 0;
#endif
   
   /* Don't care about checkpointing for "commands other than "job" */
   if (strcmp(childname, "job"))
      ckpt_type = 0;


   if ((ckpt_type & CKPT_REST_KERNEL) && !strcmp(childname, "job")) {
      set_ckpt_restart_command(childname, ckpt_type,
                               rest_command, sizeof(rest_command) -1);
      shepherd_trace("restarting job from checkpoint arena");

      /* reuse old osjobid for the migrated job and forward this one to ptf */
      if (!(fp = fopen("osjobid", "w")))
         shepherd_error("can't open \"osjobid\" file");
      fprintf(fp, "%s\n", get_conf_val("ckpt_osjobid"));
      fclose(fp);

#if defined(IRIX)
      sscanf(get_conf_val("ckpt_osjobid"), "%lld", &ash);
      shepherd_trace_sprintf("reusing old array session handle %lld", ash);
#elif defined(NECSX4) || defined(NECSX5)
		sscanf(get_conf_val("ckpt_osjobid"), "%ld", &jobid);
		shepherd_trace_sprintf("reusing old super-ux jobid %lld", jobid); 
#elif defined(CRAY)
      sscanf(get_conf_val("ckpt_osjobid"),  "%d", &jobid);
      shepherd_trace_sprintf("reusing old unicos jobid %d", jobid);
#endif

      shepherd_trace("restarting job from checkpoint arena");
      pid = start_async_command("restart", rest_command);
   }
   else {

      pid = fork();
      if (pid==0)
         son(childname, script_file, 0);
   }
   if (pid == -1) {
      shepherd_error_sprintf("can't fork \"%s\"", childname);
   }
   shepherd_trace_sprintf("forked \"%s\" with pid %d", childname, pid);

   change_shepherd_signal_mask();
   
   start_time = sge_get_gmt();

   /* Write pid to job_pid file and set ckpt_pid to original job pid 
    * Kill job if we can't write job_pid file and exit with error
    * sets ckpt_pid to 0 for non kernel level checkpointing jobs
    */
   if (!strcmp(childname, "job"))
      handle_job_pid(ckpt_type, pid, &ckpt_pid);

   /* Does not affect pe/prolog/epilog etc. since ckpt_type is set to 0 */
   set_ckpt_params(ckpt_type,
                   ckpt_command, 
                   sizeof(ckpt_command) - 1,
                   migr_command,
                   sizeof(migr_command) - 1,
                   clean_command,
                   sizeof(clean_command) - 1,
                   &ckpt_interval);
   
   if (received_signal > 0 && received_signal != SIGALRM) {
      int sig = map_signal(received_signal);
      int tmp_ret = add_signal(sig);

      received_signal = 0;
      if (tmp_ret) {
         shepherd_trace("failed to store received signal");
      }
   }
 
   if (timeout) {
      shepherd_trace_sprintf("using signal delivery delay of %d seconds", timeout);
      alarm(timeout);
   } else {
      int number_of_signals = get_n_sigs();

      if (number_of_signals > 0) {
         shepherd_trace_sprintf("There are %d signals to deliver. Wait until "
                                "job has been started.", number_of_signals);
         alarm(10);
      }
   }

   if (ckpt_type) {
      shepherd_trace_sprintf("child: %s - pid: %d - ckpt_pid: %d - "
                             "ckpt_interval: %d - ckpt_signal %d", childname, 
                             pid, ckpt_pid, ckpt_interval, ckpt_signal);
   } else {
      shepherd_trace_sprintf("child: %s - pid: %d", childname, pid);
   }
      
   /* Wait until child finishes ----------------------------------------*/         
   status = wait_my_child(pid, ckpt_pid, ckpt_type, 
                          &rusage, timeout, ckpt_interval, childname);
   alarm(0);

   end_time = sge_get_gmt();

   shepherd_trace_sprintf("reaped \"%s\" with pid %d", childname, pid);

   if (ckpt_type) {
      /* 
       * empty file is a hint to reschedule that job. If we already have a
       * checkpoint in the arena there is a dummy string in the file
       */
      if (!checkpointed_file_exists()) {
         create_checkpointed_file(0);
      }
   }

   if (WIFSIGNALED(status)) {
      shepherd_trace_sprintf("%s exited due to signal", childname);

      if (ckpt_type && !signalled_ckpt_job) {
         unlink("checkpointed");
         shepherd_trace_sprintf("%s exited due to signal but not due to checkpoint", childname);
         if (ckpt_type & CKPT_KERNEL) {
            shepherd_trace_sprintf("starting ckpt clean command");
            start_clean_command(clean_command);
         }
      }

      child_signal = WTERMSIG(status);
#ifdef WCOREDUMP
      core_dumped = WCOREDUMP(status);
#else
      core_dumped = status & 80;
#endif
      if (timeout && child_signal == SIGALRM) {
         shepherd_trace_sprintf("%s expired timeout of %d seconds and was "
                                "killed%s", childname, timeout, 
                                core_dumped ? " (core dumped)": "");
      } else {
         shepherd_trace_sprintf("%s signaled: %d%s", childname, 
                                child_signal, 
                                core_dumped ? " (core dumped)": "");
      }

      exit_status = 128 + child_signal;

      wait_status = SGE_SET_WSIGNALED(wait_status, 1);
      wait_status = SGE_SET_WCOREDUMP(wait_status, core_dumped);
      wait_status = SGE_SET_WSIGNAL(wait_status, sge_map_signal(child_signal));
   } else {
      shepherd_trace_sprintf("%s exited not due to signal", childname);

      if (!signalled_ckpt_job && ckpt_type) {
         shepherd_trace_sprintf("checkpointing job exited normally");
         unlink("checkpointed");
         if (ckpt_type & CKPT_KERNEL) {
            shepherd_trace_sprintf("starting ckpt clean command");
            start_clean_command(clean_command);
         }
      }

      if (WIFEXITED(status)) {
         exit_status = WEXITSTATUS(status);
         shepherd_trace_sprintf("%s exited with status %d%s",
            childname, exit_status, 
            (exit_status==RESCHEDULE_EXIT_STATUS || exit_status==APPERROR_EXIT_STATUS)?
            " -> rescheduling":"");

         wait_status = SGE_SET_WEXITED(wait_status, 1);
         wait_status = SGE_SET_WEXITSTATUS(wait_status, exit_status);

      } else {
         /* should be virtually impossible, see wait_my_child() why */
         exit_status = -1;
         shepherd_trace_sprintf("%s did not exit and was not signaled", 
                                childname);
      }
   }

   if (!pidp || exit_status != 0) {
      /* Kill all processes of the process group of the forked child.
         This is to ensure that nothing hangs around. */
      if (!strcmp("job", childname)) {
         if (ckpt_pid)   {
            shepherd_signal_job(-ckpt_pid, SIGKILL);
         }
         else {
            shepherd_signal_job(-pid, SIGKILL);
         }
      }
   } else {
      *pidp = pid;
   }

   /*
   ** write no usage in case of error (son() has written to error file)
   */
   if (!SGE_STAT("exit_status", &buf) && buf.st_size) {
      /*
      ** in this case the child didnt get to the exec call or it failed
      ** the status that waitpid and finally start_child returns is
      ** reserved for the exit status of the job
      */
      notify_tasker(exit_status);
      return exit_status;
   }

   if (!strcmp("job", childname)) {
      if (search_conf_val("rsh_daemon") != NULL) {
         int qrsh_exit_code = get_exit_code_of_qrsh_starter();

         /* normal exit */
         if (WIFEXITED(qrsh_exit_code)) {
            const char *qrsh_error;

            exit_status = WEXITSTATUS(qrsh_exit_code);

            qrsh_error = get_error_of_qrsh_starter();
            if (qrsh_error != NULL) {
               shepherd_error_sprintf("startup of qrsh job failed: "SFN"\n",
                                      qrsh_error);
               FREE(qrsh_error);
            } else {
               shepherd_trace_sprintf("job exited normally, exit code is %d\n", 
                                      exit_status);
            }
         }

         /* qrsh job was signaled */
         if (WIFSIGNALED(qrsh_exit_code)) {
            child_signal = WTERMSIG(qrsh_exit_code);
            exit_status = 128 + child_signal;
            shepherd_trace_sprintf("job exited on signal %d, exit code is "
                                   "%d\n", child_signal, exit_status);
         }
      }
   
      /******* write usage to file "usage" ************/
      fp = fopen("usage", "w");
      if (!fp) {
         shepherd_trace_sprintf("error: can't open \"usage\" file: %s", 
                                strerror(errno));
      } 
      
      shepherd_trace("writing usage file to \"usage\"");

      /* the wait status is returned by japi_wait()
         see sge_reportL.h for bitmask and makro definition */
      fprintf(fp, "wait_status="u32"\n", wait_status);

      fprintf(fp, "exit_status=%d\n", exit_status);
      fprintf(fp, "signal=%d\n", child_signal);

      fprintf(fp, "start_time=%d\n", (int) start_time);
      fprintf(fp, "end_time=%d\n", (int) end_time);
      fprintf(fp, "ru_wallclock="u32"\n", (u_long32) end_time-start_time);

#if defined(NEC_ACCOUNTING_ENTRIES)
      /* Additional accounting information for NEC SX-4 SX-5 */
#if defined(NECSX4) || defined(NECSX5)
#if defined(NECSX4)
      fprintf(fp, "necsx_necsx4="u32"\n", 1);
#elif defined(NECSX5)
      fprintf(fp, "necsx_necsx5="u32"\n", 1);
#endif
      fprintf(fp, "necsx_base_prty="u32"\n", 0);
      fprintf(fp, "necsx_time_slice="u32"\n", 0);
      fprintf(fp, "necsx_num_procs="u32"\n", 0);
      fprintf(fp, "necsx_kcore_min="u32"\n", 0);
      fprintf(fp, "necsx_mean_size="u32"\n", 0);
      fprintf(fp, "necsx_maxmem_size="u32"\n", 0);
      fprintf(fp, "necsx_chars_trnsfd="u32"\n", 0);
      fprintf(fp, "necsx_blocks_rw="u32"\n", 0);
      fprintf(fp, "necsx_inst="u32"\n", 0);
      fprintf(fp, "necsx_vector_inst="u32"\n", 0);
      fprintf(fp, "necsx_vector_elmt="u32"\n", 0);
      fprintf(fp, "necsx_vec_exe="u32"\n", 0);
      fprintf(fp, "necsx_flops="u32"\n", 0);
      fprintf(fp, "necsx_conc_flops="u32"\n", 0);
      fprintf(fp, "necsx_fpec="u32"\n", 0);
      fprintf(fp, "necsx_cmcc="u32"\n", 0);
      fprintf(fp, "necsx_bccc="u32"\n", 0);
      fprintf(fp, "necsx_mt_open="u32"\n", 0);
      fprintf(fp, "necsx_io_blocks="u32"\n", 0);
      fprintf(fp, "necsx_multi_single="u32"\n", 0);
      fprintf(fp, "necsx_max_nproc="u32"\n", 0);
#endif
#endif       

#if defined(SOLARIS) || defined(CRAY)
      fprintf(fp, "ru_utime=%ld\n", rusage.ru_utime.tv_sec);
      fprintf(fp, "ru_stime=%ld\n", rusage.ru_stime.tv_sec);
#else

      fprintf(fp, "ru_utime=%d\n", (int)rusage.ru_utime.tv_sec);
      fprintf(fp, "ru_stime=%d\n", (int)rusage.ru_stime.tv_sec);
      fprintf(fp, "ru_maxrss=%ld\n", rusage.ru_maxrss);
      fprintf(fp, "ru_ixrss=%ld\n", rusage.ru_ixrss);
#if defined(ultrix)
      fprintf(fp, "ru_ismrss=%ld\n", rusage.ru_ismrss);
#endif
      fprintf(fp, "ru_idrss=%ld\n", rusage.ru_idrss);
      fprintf(fp, "ru_isrss=%ld\n", rusage.ru_isrss);
      fprintf(fp, "ru_minflt=%ld\n", rusage.ru_minflt);
      fprintf(fp, "ru_majflt=%ld\n", rusage.ru_majflt);
      fprintf(fp, "ru_nswap=%ld\n", rusage.ru_nswap);
      fprintf(fp, "ru_inblock=%ld\n", rusage.ru_inblock);
      fprintf(fp, "ru_oublock=%ld\n", rusage.ru_oublock);
      fprintf(fp, "ru_msgsnd=%ld\n", rusage.ru_msgsnd);
      fprintf(fp, "ru_msgrcv=%ld\n", rusage.ru_msgrcv);
      fprintf(fp, "ru_nsignals=%ld\n", rusage.ru_nsignals);
      fprintf(fp, "ru_nvcsw=%ld\n", rusage.ru_nvcsw);
      fprintf(fp, "ru_nivcsw=%ld\n", rusage.ru_nivcsw);
#endif

      fclose(fp);

      /* this is SEMPA stuff */
      notify_tasker(exit_status);

      exit_status_for_qrsh = exit_status;

      /* 
       *  we are not interested in exit status of job
       *  except it was the RESCHEDULE_EXIT_STATUS or APPERROR_EXIT_STATUS
       */
      if( exit_status != RESCHEDULE_EXIT_STATUS
       && exit_status != APPERROR_EXIT_STATUS ) {
         exit_status = 0;
      } else if( exit_status == RESCHEDULE_EXIT_STATUS ) {
         shepherd_trace("keep RESCHEDULE_EXIT_STATUS");
      } else if( exit_status == APPERROR_EXIT_STATUS ) {
         shepherd_trace("keep APPERROR_EXIT_STATUS");
      }

   } /* if child == job */

   /* All done. Leave the rest to the execd */
   return exit_status;
}

static void verify_method(char *method_name) 
{
   char *override_signal;

   if ((override_signal = search_nonone_conf_val(method_name))) {

      if (override_signal[0] != '/' && 
         shepherd_sys_str2signal(override_signal)<0) {
         shepherd_error_sprintf("cannot convert " SFN " " SFQ" into a "
                                "valid signal number at this machine", 
                                method_name, override_signal);
      } /* else: 
         it must be path of a signal script:
         AH: I'd like to do a stat(2) on the path to ensure that the 
         signal script can be started before starting the job itself.
         But therefore we had to change into the jobs target user .. 
      */
   }
}

/****** shepherd/core/forward_signal_to_job() *********************************
*  NAME
*     forward_signal_to_job() -- Forward signal to job. 
*
*  SYNOPSIS
*     static void forward_signal_to_job(int pid, int timeout, 
*                                       int *postponed_signal, 
*                                       int remaining_alarm, 
*                                       pid_t ctrl_pid[3]) 
*
*  FUNCTION
*     We have gotten a signal. Look for it and forward it to the
*     group of the jobs. 
*
*  INPUTS
*     int pid               - pid of the process (group) to be signaled 
*     int timeout           - timeout value (0 means implicitly the job) 
*     int *postponed_signal - input/output parameter. 
*                             used to detect and prevent repeated 
*                             initiation of notify mechanism
*                             as this can delay final job 
*                             suspension endlessly
*     int remaining_alarm   - if it is necessary to set the alarm
*                             clock, then this timeout value
*                             will be used.
*     pid_t ctrl_pid[3]     - used to store the pids of:
*                             resume_method, stop_method, terminate_method 
*******************************************************************************/
static void forward_signal_to_job(int pid, int timeout, 
                                  int *postponed_signal, 
                                  int remaining_alarm, 
                                  pid_t ctrl_pid[3])
{
   int sig;
   static int replace_qrsh_pid = 1; /* cache */

   if (received_signal==SIGALRM) {
      /* a timeout expired */
      if (timeout) {
         shepherd_trace("SIGALRM with timeout");
         if (get_n_sigs() > 0) {
            /* kill directly - a timeout of a script expired */
            shepherd_trace("timeout expired - killing process");
            shepherd_signal_job(-pid, SIGKILL);
         }
         return;
      } else {
         shepherd_trace("SIGALRM without timeout");
      }
      received_signal = 0;
   }

   /* store signal if we got one */
   if (received_signal) {
      sig = map_signal(received_signal);

      received_signal = 0;
      /* store signal in ring buffer */
      if (add_signal(sig)) {
         shepherd_trace("failed to store received signal");
         return; 
      }     
   }

   /*
   ** jg: in qrsh case, replace job_pid with pid from qrsh_starter
   */
   /* cached info exists? */
   if(replace_qrsh_pid) {
      /* do we signal a qrsh job? */
      if(search_conf_val("qrsh_pid_file") == NULL) {
         replace_qrsh_pid = 0;
      } else {
         char *qrsh_pid_file;
         FILE *fp;
         char buffer[50];
         pid_t qrsh_pid;

         /* read pid from qrsh_starter */
         qrsh_pid_file = get_conf_val("qrsh_pid_file");

         if((fp = fopen(qrsh_pid_file, "r")) != NULL) {
            /* file contains a valid pid */
            if(fscanf(fp, "%d", &qrsh_pid) == 1) {            
               /* set pid from qrsh_starter as job_pid */
               sprintf(buffer, "%d", qrsh_pid);
               add_config_entry("job_pid", buffer); /* !!! should better be add_or_replace */
               replace_qrsh_pid = 0;
            }
            fclose(fp);
         }
      }
   }
    
   /* forward signals */
   if (!timeout) { /* signals for scripts with timeouts are stored */
      sig = get_signal();
      while (sig != -1 && sig != 0) {
         switch (sig) {
         case SIGCONT:
            shepherd_deliver_signal(sig, pid, postponed_signal,
                                    remaining_alarm, &(ctrl_pid[0]));
            break;
         case SIGSTOP:
            shepherd_deliver_signal(sig, pid, postponed_signal, 
                                    remaining_alarm, &(ctrl_pid[1]));
            break;
         case SIGKILL:
            signalled_ckpt_job = 0;
            shepherd_deliver_signal(sig, pid, postponed_signal, 
                                    remaining_alarm, &(ctrl_pid[2]));
            break;
         case SIGTTOU:
            signalled_ckpt_job = 1;
            sig = SIGKILL;
            shepherd_signal_job(-pid, sig);
            shepherd_trace_sprintf("kill(%d, %s) -> checkpoint initiation -> "
                                   "killing job", -pid, sge_sys_sig2str(sig));
            break;
         case SIGUSR1:
            signalled_ckpt_job = 0;
            shepherd_trace_sprintf("kill(%d, %s)", -pid, sge_sys_sig2str(sig));
            shepherd_signal_job(-pid, sig);
            break;
         case SIGUSR2:
            signalled_ckpt_job = 0;
            shepherd_trace_sprintf("kill(%d, %s)", -pid, sge_sys_sig2str(sig));
            shepherd_signal_job(-pid, sig);
            break;
         default:
            shepherd_trace_sprintf("kill(%d, %s)", -pid, sge_sys_sig2str(sig));
            shepherd_signal_job(-pid, sig);
            break;
         }
         sig = get_signal();
      }
   }
}

/****** shepherd/core/shepherd_deliver_signal() ********************************
*  NAME
*     shepherd_deliver_signal() -- Resume/Suspend/Terminate processgroup
*
*  SYNOPSIS
*     void shepherd_deliver_signal(int sig, 
*                                  int pid, 
*                                  int *postponed_signal, 
*                                  int *remaining_alarm, 
*                                  pid_t *ctrl_pid) 
*
*  FUNCTION
*     Depending on "sig" the function either resumes, suspends or
*     terminates all processes being part of process group "-pid".
*     This function heeds the configured resume, suspend and terminate
*     method just as the notify mechanism.
*
*     "postponed_signal" and "remaining_alarm" will be used to detect
*     repeated signals which arrive during the time between the processes
*     got their notification signal and the final signal/method
*     (e.g. SIGKILL arrives but SIGUSR2 was already sent due to a
*      previous SIGKILL).
*
*     "ctrl_pid" will only be used if a script has to be started. 
*     This variable will then contain the pid of the scripts process.
*
*  INPUTS
*     int sig               - signal (SIGCONT, SIGSTOP or SIGKILL)
*     int pid               - pid 
*     int *postponed_signal - used for notification mechanism 
*     int remaining_alarm   - used for notification mechanism 
*     pid_t *ctrl_pid       - pid of applications which might be started 
*
*  RESULT
*     void - none 
*******************************************************************************/
void shepherd_deliver_signal(int sig, int pid, int *postponed_signal,
                             int remaining_alarm, pid_t *ctrl_pid)
{
   char *method_name[] = { 
      "resume_method", 
      "suspend_method", 
      "terminate_method", 
      NULL 
   };
   char *notify_name[] = { 
      "",                    
      "notify_susp",
      "notify_kill", 
      NULL 
   };
   int signal_array[] = { 
      SIGCONT, 
      SIGSTOP, 
      SIGKILL, 
      0 
   };
   dstring method = DSTRING_INIT;
   int new_sig;
   int index;

   /*
    * Find names of variables stored in the config file
    */
   for (index = 0; method_name[index] != NULL; index++) {
      if (signal_array[index] == sig) {
         break;
      }
   }

   /*
    * When notify is used for SIGSTOP the SIGCONT might be sent by execd before
    * the actual SIGSTOP has been delivered. We must clean this state at this point
    * otherwise the next SIGSTOP signal will be delivered without a notify because 
    * it is then seen just as repeated initiation of notify mechanism 
    */
   if (sig == SIGCONT && notify && *postponed_signal == SIGSTOP)
      *postponed_signal = 0;

   /*
    * Job notification
    */
   if (sig == SIGSTOP || sig == SIGKILL) {
      int seconds = 0;

      if (shepconf_has_to_notify_before_signal(&seconds)) {

         /* sig  *postponed_signal
          *
          * STOP KILL    --> Termination in progress
          *                  Don't send a stop notification
          * KILL KILL    --> Termination notification already handled
          * STOP STOP    --> Suspension notification already handled
          *
          * KILL STOP    ++> Suspension in progress
          *                  Notify the processes about the termination
          */
         if ((sig == SIGSTOP && *postponed_signal == SIGKILL) ||
             (sig == SIGKILL && *postponed_signal == SIGKILL) ||
             (sig == SIGSTOP && *postponed_signal == SIGSTOP)) {
            /*
             * Detect and prevent repeated initiation of notify mechanism
             * as this can delay termination endlessly
             */
            if (sig == *postponed_signal) {
               shepherd_trace_sprintf("ignoring repeated %s notification", 
                                      sge_sys_sig2str(*postponed_signal));
            } else if (sig == SIGSTOP) {
               shepherd_trace_sprintf("termination in progress - ignoring %s "
                                      "notification", 
                                      sge_sys_sig2str(*postponed_signal)); 
            }
            
            alarm(remaining_alarm);
            return;
         } else {
            int notify_signal = 0;

            if (shepconf_has_notify_signal(notify_name[index],&notify_signal)) {
               shepherd_trace_sprintf("kill(%d, %s) -> notification "
                                      "for delayed (%d s) signal %s", -pid,
                                      sge_sys_sig2str(notify_signal),
                                      seconds, sge_sys_sig2str(sig));
            } else {
               shepherd_trace_sprintf("No notif. signal -> notification "
                                      "for delayed (%d s) signal %s",
                                      seconds, sge_sys_sig2str(sig));
            }

            if (notify_signal) {
               *postponed_signal = sig;
               shepherd_signal_job(-pid, notify_signal);
               alarm(notify);
               return;
            } 
         }
      } 
   }

   /*
    * There are three different ways to signal the processes:
    *
    *    a) Userdefined signal 
    *    b) Userdefined method
    *    c) Default signal 
    */
   if (shepconf_has_userdef_signal(method_name[index], &new_sig)) {
      shepherd_trace_sprintf("kill(%d, %s) -> overriddes kill(%d, %s)",
                             -pid, sge_sys_sig2str(new_sig),
                             -pid, sge_sys_sig2str(sig));
      shepherd_signal_job(-pid, new_sig);
   } else if (shepconf_has_userdef_method(method_name[index], &method)) {
      shepherd_trace_sprintf("%s -> overriddes kill(%d, %s)",
                             sge_dstring_get_string(&method), -pid,
                             sge_sys_sig2str(sig));
      if (*ctrl_pid == -999) {
         char command[10000];

         replace_params(sge_dstring_get_string(&method), command,
                        sizeof(command)-1, ctrl_method_variables);
         *ctrl_pid = start_async_command(method_name[index], command);
      } else {
         shepherd_trace_sprintf("Skipped start of suspend: previous command "
                                "(pid= "pid_t_fmt") is still active", 
                                *ctrl_pid);
      }
   } else {
      shepherd_trace_sprintf("kill(%d, %s)", -pid, sge_sys_sig2str(sig));
      shepherd_signal_job(-pid, sig);
   }

}

/*--------------------------------------------------------------------
 * set_shepherd_signal_mask
 * set signal mask that shpher can handle signals from execd
 *--------------------------------------------------------------------*/
static void set_shepherd_signal_mask(void)   
{
   struct sigaction sigact, sigact_old;
   sigset_t mask;
      
   /* get mask */
   sigprocmask(SIG_SETMASK, NULL, &mask);

   sigact.sa_handler = signal_handler;
   sigact.sa_mask = mask;

   /* SA_INTERRUPT is at least needed on SUN4 to interupt waitxx() 
    * when a signal arrives 
    */

#ifdef SA_INTERRUPT
   sigact.sa_flags = SA_INTERRUPT;
#else
   sigact.sa_flags = 0;
#endif

   sigaction(SIGTTOU, &sigact, &sigact_old); /* init checkpointing    */
   sigaction(SIGTTIN, &sigact, &sigact_old); /* read signal from file */
   sigaction(SIGUSR1, &sigact, &sigact_old);
   sigaction(SIGUSR2, &sigact, &sigact_old);
   sigaction(SIGCONT, &sigact, &sigact_old);
   sigaction(SIGWINCH, &sigact, &sigact_old);
   sigaction(SIGTSTP, &sigact, &sigact_old);
   sigaction(SIGALRM, &sigact, &sigact_old);

   /* allow some more signals */
   sigdelset(&mask, SIGTTOU);
   sigdelset(&mask, SIGTTIN);
   sigdelset(&mask, SIGUSR1);
   sigdelset(&mask, SIGUSR2);
   sigdelset(&mask, SIGCONT);
   sigdelset(&mask, SIGWINCH);
   sigdelset(&mask, SIGTSTP);
   sigdelset(&mask, SIGALRM);

   /* set mask */
   sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void change_shepherd_signal_mask()
{
   sigset_t mask;

   sigprocmask(SIG_SETMASK, NULL, &mask);
   sigdelset(&mask, SIGTERM);
   sigprocmask(SIG_SETMASK, &mask, NULL);
}

/*------------------------------------------------------------------------
 * check_ckpttype
 * currently only check if it's Hibernator checkpointing
 * Return:   -1 error,
 *           0 no checkpointing
 *           Bitmask of checkpointing type
 *------------------------------------------------------------------------*/
static int check_ckpttype(void)
{
   char *ckpt_job, *ckpt_interface, *ckpt_restarted, *ckpt_migr_command, 
        *ckpt_rest_command, *ckpt_command, *ckpt_pid, *ckpt_osjobid, 
        *ckpt_clean_command, *ckpt_dir, *ckpt_signal_str;
   int ckpt_type, ckpt;
   
   ckpt_type = 0;
   
   if ((ckpt_job = get_conf_val("ckpt_job")) && (atoi(ckpt_job) > 0))
      ckpt = 1;
   else 
      ckpt = 0;   

   ckpt_interface = search_conf_val("ckpt_interface");
   ckpt_restarted = search_conf_val("ckpt_restarted");
   ckpt_command = search_conf_val("ckpt_command");
   ckpt_migr_command = search_conf_val("ckpt_migr_command");
   ckpt_rest_command = search_conf_val("ckpt_rest_command");
   ckpt_clean_command = search_conf_val("ckpt_clean_command");
   ckpt_pid = search_conf_val("ckpt_pid");
   ckpt_osjobid = search_conf_val("ckpt_osjobid");
   ckpt_dir = search_conf_val("ckpt_dir");
   ckpt_signal_str = search_conf_val("ckpt_signal");

   ckpt_type = 0;
   if (!ckpt)
      ckpt_type = 0;
   else if (!(ckpt_interface && ckpt_restarted &&  
       ckpt_command && ckpt_migr_command && ckpt_rest_command &&
       ckpt_clean_command && ckpt_pid && ckpt_osjobid && ckpt_dir && ckpt_signal_str ))
      ckpt_type = -1;
   else if (!strcasecmp(ckpt_interface, "hibernator") ||
            !strcasecmp(ckpt_interface, "cpr") ||
            !strcasecmp(ckpt_interface, "cray-ckpt") ||
            !strcasecmp(ckpt_interface, "application-level")) {

      ckpt_type = CKPT | CKPT_KERNEL;

      if (!strcasecmp(ckpt_interface, "hibernator"))
         ckpt_type |= CKPT_HIBER;
      else if (!strcasecmp(ckpt_interface, "cpr"))
         ckpt_type |= CKPT_CPR;
      else if (!strcasecmp(ckpt_interface, "cray-ckpt"))
         ckpt_type |= CKPT_CRAY;
      else if (!strcasecmp(ckpt_interface, "application-level"))
         ckpt_type |= CKPT_APPLICATION;   

      if (atoi(ckpt_restarted) > 1)
         ckpt_type |= CKPT_REST;
        
      if ((atoi(ckpt_restarted) > 1) && !(ckpt_type & CKPT_APPLICATION))
         ckpt_type |= CKPT_REST_KERNEL;
         
      /* special hack to avoid restart from restart command if resart = "none" */
      if ((ckpt_type & CKPT_REST_KERNEL) && !strcasecmp(ckpt_rest_command, "none"))
         ckpt_type &= ~CKPT_REST_KERNEL;    

      if (strcasecmp(ckpt_signal_str, "none")) {
         ckpt_signal = sge_sys_str2signal(ckpt_signal_str);
         if (ckpt_signal == -1)
            ckpt_type = -1;
      }
   }
   else if (!strcasecmp(ckpt_interface, "transparent")) {
      ckpt_type = CKPT | CKPT_TRANS;

      if (atoi(ckpt_restarted) > 1)
         ckpt_type |= CKPT_REST;
          
      if (!strcasecmp(ckpt_signal_str, "none"))  
         ckpt_type = -1;
      else {
         ckpt_signal = sge_sys_str2signal(ckpt_signal_str);
         if (ckpt_signal == -1)
            ckpt_type = -1;
      }      
   }
   else if (!strcasecmp(ckpt_interface, "userdefined")) {
      ckpt_type = CKPT | CKPT_USER;

      if (atoi(ckpt_restarted) > 1)
         ckpt_type |= CKPT_REST; 
      if (strcasecmp(ckpt_signal_str, "none")) {
         ckpt_signal = sge_sys_str2signal(ckpt_signal_str);
         if (ckpt_signal == -1)
            ckpt_type = -1;
      }
   }
      
   return ckpt_type;
}

/*------------------------------------------------------------------------*/
static int wait_my_child(
int pid,                   /* pid of job */
int ckpt_pid,              /* pid of restarted job or same as pid */
int ckpt_type,             /* type of checkpointing */
struct rusage *rusage,     /* accounting information */
int timeout,               /* used for prolog/epilog script, 0 for job */
int ckpt_interval,         /* action depend on ckpt_type */
char *childname            /* "job", "pe_start", ...     */
) {
   int i, npid, status, job_status;
   int ckpt_cmd_pid, migr_cmd_pid;
   int rest_ckpt_interval;
   int inArena, inCkpt, kill_job_after_checkpoint, job_pid;
   int postponed_signal = 0; /* used for implementing SIGSTOP/SIGKILL notifiy mechanism */
   int remaining_alarm;
   pid_t ctrl_pid[3];

#if defined(HPUX)
   struct rusage rusage_hp10;
#endif
#if defined(CRAY) || defined(NECSX4) || defined(NECSX5)
   struct tms t1, t2;
#endif

   memset(rusage, 0, sizeof(*rusage));
   kill_job_after_checkpoint = 0;
   inCkpt = 0;
   rest_ckpt_interval = ckpt_interval;
   job_pid = pid;
   ckpt_cmd_pid = migr_cmd_pid = -999;
   job_status = 0;
   for (i=0; i<3; i++)
      ctrl_pid[i] = -999;

   rest_ckpt_interval = ckpt_interval;

   /* Write info that we already have a checkpoint in the arena */
   if (ckpt_type & CKPT_REST_KERNEL) {
      inArena = 1;
      create_checkpointed_file(1);
   } else {
      inArena = 0;
   }

#if defined(CRAY) || defined(NECSX4) || defined(NECSX5)
   times(&t1);
#endif
   
   do {
      if (ckpt_interval && rest_ckpt_interval)
         alarm(rest_ckpt_interval);

#if defined(CRAY) || defined(NECSX4) || defined(NECSX5)
      npid = waitpid(-1, &status, 0);
#else
      npid = wait3(&status, 0, rusage);
#endif

#if defined(HPUX)
      /* wait3 doesn't return CPU usage */
      getrusage(RUSAGE_CHILDREN, &rusage_hp10);
#endif

      remaining_alarm = alarm(0);
     
      if (npid == -1) {
         shepherd_trace_sprintf("wait3 returned -1");
      } else {
         shepherd_trace_sprintf("wait3 returned %d (status: %d; WIFSIGNALED: %d, "
                          " WIFEXITED: %d, WEXITSTATUS: %d)", npid, status,
                          WIFSIGNALED(status), WIFEXITED(status),
                          WEXITSTATUS(status));
      }

      /* got a signal? should compare errno with EINTR */
      if (npid == -1) {
         /* got SIGALRM */ 
         if (received_signal == SIGALRM) {
            /* notify: postponed signals SIGSTOP, SIGKILL */
            if (postponed_signal) {
               shepherd_trace_sprintf("kill(%d, %s) -> delivering postponed "
                                      "signal", -pid, 
                                      sge_sys_sig2str(postponed_signal));
               shepherd_signal_job(-pid, postponed_signal);
               postponed_signal = 0;
            } else {
               shepherd_trace("no postponed signal");

               /* userdefined ckpt has always ckpt_interval = 0 */ 
               if (ckpt_interval) {
                  if (ckpt_type & CKPT_KERNEL) {
                     shepherd_trace("initiate regular kernel level checkpoint");
                     ckpt_cmd_pid = start_async_command("ckpt", ckpt_command);
                     rest_ckpt_interval = 0;
                     if (ckpt_cmd_pid == -1) {
                        ckpt_cmd_pid = -999;
                        rest_ckpt_interval = ckpt_interval;
                     }
                     else
                        inCkpt = 1;
                  } else if (ckpt_type & CKPT_TRANS) {
                     shepherd_trace("send checkpointing signal to transparent checkpointing job");
                     sge_switch2start_user();
                     kill(-pid, ckpt_signal);
                     sge_switch2admin_user();
                  } else if (ckpt_type & CKPT_USER) {
                     shepherd_trace("send checkpointing signal to userdefined checkpointing job");
                     sge_switch2start_user();
                     kill(-pid, ckpt_signal);
                     sge_switch2admin_user();
                  }
               } 

               forward_signal_to_job(pid, timeout, &postponed_signal,
                                     remaining_alarm, ctrl_pid);
            }
         } else if (ckpt_pid && (received_signal == SIGTTOU)) {
            shepherd_trace("initiate checkpoint due to migration request");
            rest_ckpt_interval = 0; /* disable regular checkpoint */
            if (!inCkpt) {
               migr_cmd_pid = start_async_command("migrate", migr_command);
               if (migr_cmd_pid == -1) {
                  migr_cmd_pid = -999;
               } else {
                  inCkpt = 1;
                  signalled_ckpt_job = 1;
               }
            } else {
               /* kill job after end of ckpt_command */
               kill_job_after_checkpoint = 1; 
            }
         } else {
            forward_signal_to_job(pid, timeout, &postponed_signal, 
                                  remaining_alarm, ctrl_pid);
         }
      }
            
      /* here we reap the control action methods */
      for (i=0; i<3; i++) {
         static char *cm_descr[] = {
            "resume",
            "suspend",
            "terminate"
         };
         if ((npid == ctrl_pid[i]) && ((WIFSIGNALED(status) || WIFEXITED(status)))) {
            shepherd_trace_sprintf("reaped %s command", cm_descr[i]);
            ctrl_pid[i] = -999;
         }
      }

      /* here we reap the checkpoint command */
      if ((npid == ckpt_cmd_pid) && ((WIFSIGNALED(status) || WIFEXITED(status)))) {
         inCkpt = 0;
         shepherd_trace("reaped checkpoint command");
         if (!kill_job_after_checkpoint)
            rest_ckpt_interval = ckpt_interval; 
         ckpt_cmd_pid = -999;
         if (WIFEXITED(status)) {
            shepherd_trace("checkpoint command exited normally");
            if (WEXITSTATUS(status)==0 && !inArena) {
               shepherd_trace("checkpoint is in the arena after regular checkpoint");
               create_checkpointed_file(1);
               inArena = 1;
            }                     
         }
         /* A migration request occured and we just did a regular checkpoint */
         if (kill_job_after_checkpoint) {
             shepherd_trace("killing job after regular checkpoint due to migration request");
             shepherd_signal_job(-pid, SIGKILL);   
         }    
      }

      /* here we reap the migration command */
      if ((npid == migr_cmd_pid) && 
          ((WIFSIGNALED(status) || WIFEXITED(status)))) {
         /*
          * If the migrate command exited but the job did 
          * not stop (due to error in the migrate script) we have
          * to reset internat state. As result further migrate
          * commands will be executed (qmod -f -s).
          */
         inCkpt = 0;
         shepherd_trace("if jobs and shepherd do not exit there is some error "
                        "in the migrate command");


         shepherd_trace("reaped migration checkpoint command");
         migr_cmd_pid = -999;
         if (WIFEXITED(status)) {
            shepherd_trace("checkpoint command exited normally");
            if (WEXITSTATUS(status) == 0 && !inArena) {
               shepherd_trace("checkpoint is in the arena after migration request");
               create_checkpointed_file(1);
               inArena = 1;
            }
         }
      }
      
      /* reap job */
      if ((npid == pid) && ((WIFSIGNALED(status) || WIFEXITED(status)))) {
         shepherd_trace_sprintf("%s exited with exit status %d", 
                                childname, WEXITSTATUS(status));
         job_status = status;
         job_pid = -999;
      }   

      if ((npid == coshepherd_pid) && ((WIFSIGNALED(status) || WIFEXITED(status)))) {
         shepherd_trace("reaped sge_coshepherd");
         coshepherd_pid = -999;
      }   
         
      
   } while ((job_pid > 0) || (migr_cmd_pid > 0) || (ckpt_cmd_pid > 0));

#if defined(CRAY) || defined(NECSX4) || defined(NECSX5)

   times(&t2);

   rusage->ru_utime.tv_sec  = (t2.tms_cutime - t1.tms_cutime) / sysconf(_SC_CLK_TCK);
   rusage->ru_stime.tv_sec  = (t2.tms_cstime - t1.tms_cstime) / sysconf(_SC_CLK_TCK);
   
#endif  /* CRAY */

#if defined(HPUX)
   rusage->ru_utime.tv_sec = rusage_hp10.ru_utime.tv_sec;
   rusage->ru_stime.tv_sec = rusage_hp10.ru_stime.tv_sec;
#endif

   return job_status;
}

/*-------------------------------------------------------------------------
 * set_ckpt_params
 * may not called befor "job_pid" is known and set in the configuration
 * don't do anything for non ckpt jobs except setting ckpt_interval = 0
 *-------------------------------------------------------------------------*/
static void set_ckpt_params(int ckpt_type, char *ckpt_command, int ckpt_len,
                            char *migr_command, int migr_len, 
                            char *clean_command, int clean_len,
                            int *ckpt_interval) 
{
   char *cmd;
   
   strcpy(ckpt_command, "none");
   strcpy(migr_command, "none");
   strcpy(clean_command,"none");

   if (ckpt_type & CKPT_KERNEL) {
      cmd = get_conf_val("ckpt_command");
      if (strcasecmp("none", cmd)) {
         shepherd_trace(cmd);
         replace_params(cmd, ckpt_command, ckpt_len, ckpt_variables);
         shepherd_trace(ckpt_command);
      }

      cmd = get_conf_val("ckpt_migr_command");
      if (strcasecmp("none", cmd)) {
         shepherd_trace(cmd);
         replace_params(cmd, migr_command, migr_len,
                        ckpt_variables);
         shepherd_trace(migr_command);
      }

      cmd = get_conf_val("ckpt_clean_command");
      if (strcasecmp("none", cmd)) {
         shepherd_trace(cmd);
         replace_params(cmd, clean_command, clean_len,
                        ckpt_variables);
         shepherd_trace(clean_command);
      }
   }
      
   /* Don't perform regular checkpoint for userdefined checkpointing */
   if (ckpt_type) {
#if 0
      if (ckpt_type & CKPT_USER)
         *ckpt_interval = 0;
      else   
#endif
         *ckpt_interval = atoi(get_conf_val("ckpt_interval"));
   } else {
      *ckpt_interval = 0;      
   }
}      

/*-------------------------------------------------------------------------
 * set_ckpt_restart_command
 *   may only be called for childname == "job" and in case of restarting
 *   a kernel level checkpointing mechanism
 *   This sets the config entry "job_pid" from "ckpt_pid", which was saved
 *   during the migration. 
 *   "job_pid" is the original pid of the checkpointed job and not the pid 
 *   of the restart command.
 *-------------------------------------------------------------------------*/
static void set_ckpt_restart_command(char *childname, int ckpt_type, 
                                     char *rest_command, int rest_len) 
{
   char *cmd;
   
   strcpy(rest_command, "none");
   
   /* Need to retrieve job pid here */
   if (add_config_entry("job_pid", get_conf_val("ckpt_pid"))) {
      shepherd_error("adding config entry \"job_pid\" failed");
   }
      
   /* Only set rest_command for "job" */
   if ((!strcmp(childname, "job")) && (ckpt_type & CKPT_REST_KERNEL)) {
      cmd = get_conf_val("ckpt_rest_command");
      if (strcasecmp("none", cmd)) {
         shepherd_trace(cmd);
         replace_params(cmd, rest_command, rest_len, ckpt_variables);
         shepherd_trace(rest_command);
      }    
   }
}

/*-------------------------------------------------------------------------
 * handle_job_pid
 * add job_pid to the configuration
 * set it to pid of job or to original pid of job in case of kernel
 * checkpointing
 * set ckpt_pid to 0 for non kernel level checkpointing jobs
 *-------------------------------------------------------------------------*/
static void handle_job_pid(int ckpt_type, int pid, int *ckpt_pid) 
{
   char pidbuf[64];
   FILE *fp;

   /* Set job_pid to real pid or to saved pid for Hibernator restart 
    * for Hibernator restart a part of the job is already done
    */
   if (ckpt_type & CKPT_REST_KERNEL) {
      sprintf(pidbuf, "%s", get_conf_val("ckpt_pid"));
      *ckpt_pid = atoi(get_conf_val("ckpt_pid"));
   } else {   
      sprintf(pidbuf, "%d", pid);
      if (add_config_entry("job_pid", pidbuf))
         shepherd_error("can't add \"job_pid\" entry");
      if (ckpt_type & CKPT_KERNEL)
         *ckpt_pid = pid;
      else
         *ckpt_pid = 0;   
   }
      
   if ((fp = fopen("job_pid", "w"))) {
      fprintf(fp, "%s\n", get_conf_val("job_pid"));
      fclose(fp);
   } else {
      /* No use to go on further */
      shepherd_signal_job(pid, SIGKILL);
      shepherd_error("can't write \"job_pid\" file");   
   } 
}   

/*-------------------------------------------------------------------------*/
static int start_async_command(char *descr, char *cmd)
{
   int pid;
   char err_str[512];
   char *cwd;
   struct passwd *pw=NULL;
      
   pw = sge_getpwnam(get_conf_val("job_owner"));

   if (!pw) {
      shepherd_error_sprintf("can't get password entry for user \"%s\"",
                             get_conf_val("job_owner"));
   }

	/* Create "error" and "exit_status" files here */
	shepherd_error_init( );
                            
   if ((pid = fork()) == -1) {
      shepherd_trace_sprintf("can't fork for starting %s command", descr);
   } else if (pid == 0) {
      int use_qsub_gid;
      gid_t gid;
      char *tmp_str;
      
      shepherd_trace_sprintf("starting %s command: %s", descr, cmd);
      pid = getpid();
      setpgid(pid, pid);  
      setrlimits(0);
      set_environment();
      umask(022);
      tmp_str = search_conf_val("qsub_gid");
      if (tmp_str && strcmp(tmp_str, "no")) {
         use_qsub_gid = 1;
         gid = atol(tmp_str);
      } else {
         use_qsub_gid = 0;       
         gid = 0;
      }
      if (sge_set_uid_gid_addgrp(get_conf_val("job_owner"), NULL, 0, 0, 0, 
                                 err_str, use_qsub_gid, gid) > 0) {
         shepherd_trace(err_str);
         exit(1);
      }   

      sge_close_all_fds(NULL);

      /* we have to provide the async command with valid io file handles
       * else it might fail 
       */
      if((open("/dev/null", O_RDONLY, 0) != 0) || 
         (open("/dev/null", O_WRONLY, 0) != 1) || 
         (open("/dev/null", O_WRONLY, 0) != 2)) {
         shepherd_trace_sprintf("error opening std* file descriptors\n");
         exit(1);
      }   

      foreground = 0;

      cwd = get_conf_val("cwd");
      if (sge_chdir(cwd)) {
         shepherd_trace_sprintf("%s: can't chdir to %s", descr, cwd);
      }   

      sge_set_def_sig_mask(0, NULL);
      start_command(descr, get_conf_val("shell_path"), cmd, cmd, "start_as_command", 0, 0, 0, 0, "");
      return 0;   
   }

   return pid;
}

static int checkpointed_file_exists(void)
{
   SGE_STRUCT_STAT buf;
   return ! SGE_STAT("checkpointed", &buf);
}

static void create_checkpointed_file(int ckpt_is_in_arena)
{
   FILE *fp = NULL;

   fp = fopen("checkpointed", "w");
   if (fp != NULL) {
      if (ckpt_is_in_arena) {
         fprintf(fp, "1\n");
      }
      fclose(fp);
   } else {
      shepherd_error("can't write to file \"checkpointed\"");
   }
}

/*-------------------------------------------------------------------------*/
static void start_clean_command(char *cmd) 
{
   int pid, npid, status;

   shepherd_trace("starting ckpt clean command");

   if (!cmd || *cmd == '\0' || !strcasecmp(cmd, "none")) {
      shepherd_trace("no checkpointing clean command to start");
      return;
   }
         
   if ((pid = start_async_command("clean", cmd)) == -1) {
      shepherd_trace("couldn't start clean command");
      return;
   }   
   
   
   do {
      npid = waitpid(pid, &status, 0);
   } while ((npid <= 0) || (!WIFSIGNALED(status) && !WIFEXITED(status)) ||
            (npid != pid));

   if (WIFSIGNALED(status))
      shepherd_trace("clean command died through signal");
}     

/****************************************************************
 Special version of signal for the shepherd.
 Should be used to signal the job. 
 We have special handling for architectures which have a reliable 
 grouping mechanism like sgi or cray. This version reads the osjobid
 and uses it instead of the pid. If reading or killing fails, the normal
 mechanism is used.
 ****************************************************************/
static void shepherd_signal_job(
pid_t pid,
int sig 
) {
#if defined(IRIX) || defined(CRAY) || defined(NECSX4) || defined(NECSX5)
   FILE *fp;
   static int first = 1;
   int n;
#  if (IRIX)
   static ash_t osjobid = 0;
#	elif defined(NECSX4) || defined(NECSX5)
   char err_str[512];
	static id_t osjobid = 0;
#  elif defined(CRAY)
   static int osjobid = 0;
#  endif
#elif defined(SOLARIS) || defined(LINUX) || defined(ALPHA)
#if 0
   char *cp;
   gid_t add_grp_id;
#endif   
#endif

#if defined(IRIX) || defined(CRAY) || defined(NECSX4) || defined(NECSX5)

   do {

      /* Only root can setup job */
      if (getuid())
         break;

      if (first) {
         fp = fopen("osjobid", "r");
         if (fp) {
#if defined(IRIX)
            n = fscanf(fp, "%lld", &osjobid);
#else
            n = fscanf(fp, "%d", &osjobid);      
#endif
            fclose(fp);

            if (!n) {
               shepherd_trace("can't read \"osjobid\" file");
               break;
            }

            first = 0;
         }
      }

      if (!osjobid) {
         shepherd_trace("value in \"osjobid\" file = 0, not using kill_ash/killm");
         break;
      }

      sge_switch2start_user();
#     if defined(IRIX)
      kill_ash(osjobid, sig, sig == 9);
#     elif defined(CRAY)
      killm(C_JOB, osjobid, sig);
#		elif defined(NECSX4) || defined(NECSX5)
      if (sig == SIGSTOP) {
         if (suspendj(osjobid) == -1) {
            shepherd_trace_sprintf("ERROR(%d): suspendj(%d): %s", errno,
                                   osjobid, strerror(errno));
         } else {
            shepherd_trace_sprintf("suspendj(%d)", osjobid);
         }
      } else if (sig == SIGCONT) {
         if (resumej(osjobid) == -1) {
            shepherd_trace_sprintf("ERROR(%d): resumej(%d): %s", errno,
                                   osjobid, strerror(errno));
         } else {
            shepherd_trace_sprintf("resumej(%d)", osjobid);
         }
      } else {
         if (killj(osjobid, sig) == -1) {
            shepherd_trace_sprintf("ERROR(%d): killj(%d, %d): %s", errno,
                                   osjobid, sig, strerror(errno));
         } else {
            shepherd_trace_sprintf("killj(%d, %d)", osjobid, sig);
         }
      }                   
#     endif
      sge_switch2admin_user();

   } while (0);

#elif defined(SOLARIS) || defined(LINUX) || defined(ALPHA)
#if 0
   cp = search_conf_val("add_grp_id");
   if (cp)
      add_grp_id = atol(cp);
   else
      add_grp_id = 0;

   {
   char err_str[256];

   shepherd_trace_sprintf("pdc_kill_addgrpid: %d %d", (int) add_grp_id , sig);

   sge_switch2start_user();
   pdc_kill_addgrpid(add_grp_id, sig, shepherd_trace);
   sge_switch2admin_user();
   }
#endif
#endif

   /* 980708 SVD - I moved the normal kill code below the special job
      killing code for the Cray and SGI because killing the process first
      may cause the job to be removed which can cause the job kill code
      to fail */

   /* 
    * Normal signaling for OSes without reliable grouping mechanisms and if
    * special signaling fails (e.g. not running as root)
    */

   /*
   ** if child is a qrsh job (config rsh_daemon exists), get pid of started command
   ** and pass signal to that one
   ** if the signal is the kill signal, we first kill the pid of the started command.
   ** subsequent kills are passed to the shepherds child.
   */
   {
      static int first_kill = 1;
   
      if(first_kill == 1 || sig != SIGKILL) {
         if(search_conf_val("qrsh_pid_file") != NULL) {
            char *pid_file_name = NULL;
            FILE *pid_file = NULL;

            pid_file_name = get_conf_val("qrsh_pid_file");

            sge_switch2start_user();
            if((pid_file = fopen(pid_file_name, "r")) != NULL) {
               pid_t qrsh_pid = 0;
               if(fscanf(pid_file, pid_t_fmt, &qrsh_pid) == 1) {
                  pid = -qrsh_pid;
                  shepherd_trace_sprintf("found pid of qrsh client command: " 
                                         pid_t_fmt, pid);
               }
               fclose(pid_file);
            }
            sge_switch2admin_user();
         }
      }

      if(sig == SIGKILL) {
         first_kill = 0;
      }

      shepherd_trace_sprintf("now sending signal %s to pid "pid_t_fmt, 
                             sge_sys_sig2str(sig), pid);
   }

   sge_switch2start_user();
   kill(pid, sig);
   sge_switch2admin_user();
}

static int notify_tasker(u_long32 exit_status) 
{
   FILE *fp;
   char buf[10000], *name, *value;
   int line=0;
   pid_t tasker_pid;
   char pvm_tasker_pid[1000], sig_info_file[1000], pvm_task_id[1000];

   pvm_tasker_pid[0] = sig_info_file[0] = pvm_task_id[0] = '\0';

   if (!(fp = fopen("environment", "r"))) {
      shepherd_error_sprintf("can't open environment file: %s", 
                             strerror(errno));
      return 1;
   }

   while (fgets(buf, sizeof(buf), fp)) {
      line++;

      if (strlen(buf) <= 1)     /* empty line or lastline */
         continue;

      name = strtok(buf, "=");
      if (!name) {
         fclose(fp);
         shepherd_error_sprintf("error reading environment file: line=%d, "
                                "contents:%s", line, buf);
         return 1;
      }
      value = strtok(NULL, "\n");
      if (!value)
         value = "";

      if (!strcmp(name, "PVM_TASKER_PID"))
         strcpy(pvm_tasker_pid, value);
      if (!strcmp(name, "SIG_INFO_FILE"))
         strcpy(sig_info_file, value);
      if (!strcmp(name, "PVM_TASK_ID"))
         strcpy(pvm_task_id, value);

      sge_setenv(name, value);
   }

   fclose(fp);

   if (!pvm_tasker_pid[0] || !sig_info_file[0] || !pvm_task_id[0] ) {
      shepherd_trace("no tasker to notify");
      return 0;
   }

   if (sscanf(pvm_tasker_pid, pid_t_fmt, &tasker_pid)!=1) {
      shepherd_error_sprintf("unable to parse pvm tasker pid from \"%s\"",
                             pvm_tasker_pid);
      return 1;
   }

   if (!(fp = fopen(sig_info_file, "a"))) {
      shepherd_error_sprintf("can't open signal info file \"%s\": %s",
                             sig_info_file, strerror(errno));
   } else {
      char *job_owner;
      struct passwd *pw=NULL;

      fprintf(fp, "%s "u32"\n", pvm_task_id, exit_status); 
      fclose(fp);

      /* sig_info_file has to be removed by tasker 
         and tasker runs in user mode */
      job_owner = get_conf_val("job_owner");
      pw = sge_getpwnam(job_owner);
      if (!pw) {
         shepherd_error_sprintf("can't get password entry for user "
                                "\"%s\"", job_owner);
      }

      chown(sig_info_file, pw->pw_uid, -1);
   }

   shepherd_trace_sprintf("signalling tasker with pid #"pid_t_fmt, tasker_pid);

   sge_switch2start_user();
   kill(tasker_pid, SIGCHLD);
#ifdef SIGCLD
   kill(tasker_pid, SIGCLD);
#endif
   sge_switch2admin_user(); 

   return 0;
}

/*------------------------------------------------------------------*/
static pid_t start_token_cmd(int wait_for_finish, char *cmd, char *arg1, 
                             char *arg2, char *arg3) 
{
   pid_t pid;
   pid_t ret;

   pid = fork();
   if (pid < 0) {
      return -1;
   } else if (pid == 0) {
      if (!wait_for_finish && (getenv("SGE_DEBUG_LEVEL"))) {
         putenv("SGE_DEBUG_LEVEL=0 0 0 0 0 0 0 0");
      }   
      execl(cmd, cmd, arg1, arg2, arg3, NULL);
      exit(1);
   } else if (wait_for_finish) {
        ret = do_wait(pid);
        return ret;
   } else {
      return pid;
   }

   /* just to please insure */
   return -1;
}

/*------------------------------------------------------------------*/
static int do_wait(pid_t pid) 
{
   pid_t npid;
   int status, exit_status;

   /* This loop only ends if the process exited normally or
    * died through signal
    */
   do {
      npid = waitpid(pid, &status, 0);
   } while ((npid <= 0) || (!WIFSIGNALED(status) && !WIFEXITED(status)) ||
            (npid != pid));

   if (WIFEXITED(status)) {
      exit_status = WEXITSTATUS(status);
   } else if (WIFSIGNALED(status)) {
      exit_status = 128 + WTERMSIG(status);
   } else {
      exit_status = 255;
   }

   return exit_status;
}

/*-------------------------------------------------------------------
 * set_sig_handler
 * define a signal handler for SIGPIPE
 * to avoid that unblocked signal will kill us
 *-------------------------------------------------------------------*/
static void set_sig_handler(int sig_num) 
{
   struct sigaction sa;
      
   memset(&sa, 0, sizeof(sa));
   sa.sa_handler = shepherd_signal_handler;
   sigemptyset(&sa.sa_mask);

#ifdef SA_RESTART
   sa.sa_flags = SA_RESTART;
#endif
      
   sigaction(sig_num, &sa, NULL);
}

/*-------------------------------------------------------------------*/
static void shepherd_signal_handler(int dummy) 
{
   /* may not log in signal handler 
      as long as Async-Signal-Safe functions such as fopen() are used in 
      shepherd logging code */
#if 0
   shepherd_trace("SIGPIPE received");
#endif
}