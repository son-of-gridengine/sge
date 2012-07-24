/* sge_cgroup.c -- helpers for resource management with Linux cpusets/cgroups

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

/* This is to support aspects of Linux cgroups, if they are present
   (Linux 2.6.24+), and otherwise the older cpuset implementation
   (e.g. Red Hat 5, apparently available sometime post-Linux 2.6.9).
   The two cpuset implementations have the same interface, although
   the mount can be done differently with the cgroup emulation.
   [Actually, this has changed -- see below.]  We tend to refer to
   things as "cgroup" even if it's actually the old cpusets.

   There are existing libraries, libcpuset and libcgroup, but it seems
   best not to depend on them, and it's not clear that it's a good
   idea to mix them, given we want to support both the old cpusets and
   cgroups.

   See SLURM, Condor, and possibly OAR for existing cpuset/cgroup
   resource manager implementations.  Using their source isn't
   currently on due to incompatible licensing (SLURM GPL) or
   implementation language (Condor).

   execd and shepherd will try to do cgroup/cpuset management iff they
   run on Linux with suitable support (checked by entries in
   /proc/self) and directories /dev/cpuset/sge and/or /cgroup/sge and
   (currently) if USE_CGROUPS=true is set in the execd_params.  Execd
   maintains per-job task sub-directories with names in the usual
   $JOB_ID.$SGE_TASK_ID format.  Shepherd creates a directory of that
   named as its pid to be able to identify the child processes.  Thus
   the cpuset structure is
      /dev/cpuset/sge/<job>.<task>/<shepherd pid>
   (SLURM has a user level cpuset under the top level (/slurm), but
   it's not clear why that's needed.)  There is also a "0"
   sub-directory in the task directory which is used for quarantining
   processes at the end of the job.

   The script util/resources/scripts/setup-cgroups-etc can be used
   (e.g. in the execd rc script) to mount the controllers and set up
   the sge directories.  The controller directories are owned by the
   SGE admin user for convenience.  There doesn't currently seem to be
   a need for locking.

   Execd creates a job controller directory, and shepherd will set the
   CPUs of the cpuset consistent with any core binding in force.
   Execd removes the controller after removing itself from the tasks
   and killing any remaining tasks in it (which have presumably escaped
   the process tree).  We don't currently use a release agent.

   Cpuset contents on Red Hat 5:
      cpu_exclusive   memory_pressure          mems
      cpus            memory_pressure_enabled  notify_on_release
      mem_exclusive   memory_spread_page       sched_relax_domain_level
      memory_migrate  memory_spread_slab       tasks

   On Red Hat 6 and Ubuntu 10.04 have extra:  cgroup.procs,
      release_agent, sched_load_balance mem_hardwall.

   Todo: Extend the use of cpusets (for process tracking); clean up
   dead cpusets in execd as for active_jobs; investigate use of memory
   pressure etc.; add cgroups.

   Fixme below:  Add doc headers, catalogue messages, tidy up.

   Sigh.  The file structure has changed, e.g. in Ubuntu 12.04.
   There's a default mount on /sys/fs/cgroup/cpuset with cpuset.mems
   and cpuset.cpus instead of mems and cpus.  It doesn't work to mount
   on /dev/cpuset with -onoprefix, which is supposed to make it
   compatible (but presumably doesn't work for a second mount).  We
   need to be prepared for both.  Presumably it's OK to link the
   filesystem to /dev/cpuset, but we probably need to make the mount
   point configurable.  Maybe just grovel /proc/mounts for
   cpuset/cgroup mounts and look for the sge directory in them.
*/

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <libgen.h>
#include <dirent.h>
#include "msg_common.h"
#include "uti/sge_rmon.h"
#include "uti/sge_log.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_string.h"
#include "sgeobj/sge_conf.h"
#include "uti2/sge_cgroup.h"

#define PID_BSIZE 24            /* enough to hold formatted 64-bit */

/* Can we use the controller GROUP, e.g. cpuset?  */
bool
have_cgroup (group_t group)
{
   struct stat statbuf;

   errno = 0;
#if ! __linux__
   errno = ENOSYS;
   return false;
#endif
   switch(group) {
   case cg_cpuset:
      if (stat("/proc/self/cpuset", &statbuf) != 0) {
         errno = ENOSYS;
         return false;
      }
      return file_exists(cpusetdir"/tasks");
   case cg_cpuacct:
   case cg_freezer:
   case cg_memory:
      if (stat("/proc/self/cgroup", &statbuf) != 0) {
         errno = ENODEV;
         return false;
      }
      if (!is_dir(cgroupdir)) {
         errno = ENODEV;
         return false;
      }
      /* fixme:  read cgroup to check for each when we're using them */
      switch(group) {
      case cg_cpuacct:
      case cg_freezer:
      case cg_memory:
      default:
         return false;
      }
   }
   return true;
}

/* Return the directory for the given controller GROUP of JOB and TASK
   in buffer DIR or length LDIR (e.g. /dev/cpuset/sge/123.1).  DIR is
   zero-length on failure.  */
static void
get_cgroup_task_dir(group_t group, char *dir, size_t ldir, u_long32 job, u_long32 task)
{
   const char *cdir;

   DENTER(TOP_LAYER, "get_cgroup_task_dir");
   if (cg_cpuset == group)
      cdir = cpusetdir;
   else
      cdir = cgroupdir;
   if (snprintf(dir, ldir, "%s/%d.%d", cdir, job, task) >= ldir) {
      WARNING((SGE_EVENT, "Can't build cgroup_task_dir value"));
      DRETURN_VOID;
   }
   if (!is_dir(dir)) {
      dir[0] = '\0';
      DRETURN_VOID;
   }
   DRETURN_VOID;
}

/* Does the controller directory for the job exist?  */
bool
have_cgroup_task_dir(group_t group, u_long32 job, u_long32 task)
{
   char dir[SGE_PATH_MAX];

   get_cgroup_task_dir(group, dir, sizeof dir, job, task);
   return is_dir(dir);
}

/* Write string RECORD to the task's GROUP controller file CFILE with
   MODE copy or append.  */
bool
write_to_cgroup_proc_file(group_t group, const char *cfile, const char *record, u_long32 job, u_long32 task, pid_t pid)
{
   char path[SGE_PATH_MAX], *cdir;

   if (cg_cpuset == group) cdir = cpusetdir;
   else cdir = cgroupdir;
   snprintf(path, sizeof path, "%s/"sge_u32"."sge_u32"/"pid_t_fmt"/%s",
            cdir, job, task, pid, cfile);
   if (sge_string2file(record, strlen(record), path) == 0)
      return true;
   return false;
}

/* Put the process PID into controller directory DIR.  */
static bool
set_pid_cgroup(pid_t pid, char *dir)
{
   char path[SGE_PATH_MAX], spid[PID_BSIZE];
   int ret;
   bool is_admin, error;

   DENTER(TOP_LAYER, "set_pid_cgroup");
   if (!pid) pid = getpid();
   snprintf(spid, sizeof spid, pid_t_fmt, pid);
   snprintf(path, sizeof path, "%s/tasks", dir);
   sge_running_as_admin_user(&error, &is_admin);
   if (error) {
      CRITICAL((SGE_EVENT, "Can't get admin user"));
      abort();
   }
   /* We can't move tasks unless we have permission to signal them,
      and we need sgeadmin permission for the filesystem.  */
   sge_seteuid(SGE_SUPERUSER_UID);
   errno = 0;
   ret = sge_string2file(spid, strlen(spid), path);
   if (ret != 0)
      WARNING((SGE_EVENT, "Can't put task in controller "SFN": "SFN,
               dir, strerror(errno)));
   if (is_admin)
      ret = sge_switch2admin_user();
   else
      ret = sge_switch2start_user();
   if (ret != 0) {
      CRITICAL((SGE_EVENT, "Can't switch user"));
      DEXIT;
      abort();
   }
   DRETURN(ret ? false : true);
}

/* Put process PID into the task's controller GROUP.  */
bool
set_pid_shepherd_cgroup(group_t group, pid_t pid, u_long32 job, u_long32 task)
{
   char dir[SGE_PATH_MAX];

   get_cgroup_task_dir(group, dir, sizeof dir, job, task);
   if (dir[0] == '\0') return false;
   snprintf(dir+strlen(dir), sizeof(dir)-strlen(dir), "/"pid_t_fmt, pid);
   errno = 0;
   return set_pid_cgroup(pid, dir);
}

/* Copy the controller CFILE file contents from the parent into DIR,
   e.g. to populate "cpus".  */
static bool
copy_from_parent(char *dir, const char *cfile)
{
   char parent_path[SGE_PATH_MAX], path[SGE_PATH_MAX], dirx[SGE_PATH_MAX];

   /* For some reason we're not getting the glibc version of dirname
      that doesn't alter its arg.  */
   sge_strlcpy(dirx, dir, sizeof dirx);
   snprintf(path, sizeof path, "%s/%s", dir, cfile);
   snprintf(parent_path, sizeof parent_path, "%s/%s",
            dirname(dirx), cfile);
   return sge_copy_append(parent_path, path, SGE_MODE_COPY) ? false : true;
}

static bool
make_sub_cpuset(char *parent, char *child)
{
   char child_dir[SGE_PATH_MAX];

   DENTER(TOP_LAYER, "make_sub_cpuset");
   snprintf(child_dir, sizeof child_dir, "%s/%s", parent, child);
   if (!is_dir(child_dir)) {
      errno = 0;
      if (mkdir(child_dir, 0755) != 0) {
         WARNING((SGE_EVENT, MSG_FILE_CREATEDIRFAILED_SS, child_dir,
                  strerror(errno)));
         DRETURN(false);
      }
   }
   /* You need to populate the mems and cpus before you can use the
      cpuset -- they're not inherited.  */
   if (file_exists(cpusetdir"/cpuset.mems"))
      DRETURN(copy_from_parent(child_dir, "cpuset.mems") &&
              copy_from_parent(child_dir, "cpuset.cpus"));
   DRETURN(copy_from_parent(child_dir, "mems") &&
           copy_from_parent(child_dir, "cpus"));
}

/* Make the controller directory for the given task.  */
bool
make_task_cpuset(u_long32 job, u_long32 task)
{
   char child[64], path[SGE_PATH_MAX], shortbuf[10];
   size_t l = sizeof shortbuf;

   DENTER(TOP_LAYER, "make_task_cpuset");
   snprintf(path, sizeof path, "%s/mems", cpusetdir);
   if (!file_exists(path))
      snprintf(path, sizeof path, "%s/cpuset.mems", cpusetdir);
   dev_file2string(path, shortbuf, &l);
   if (l <= 1)                  /* we get a newline when it's empty */
      ERROR((SGE_EVENT,
             SFN" needs cpus and mems populated -- cpusets will fail",
             cpusetdir));
   snprintf(child, sizeof child, sge_u32"."sge_u32, job, task);
   INFO((SGE_EVENT, "making task cpuset "SFN"/"SFN, cpusetdir, child));
   if (!make_sub_cpuset(cpusetdir, child))
      return false;
   /* orphan processes end up in "0" in the job dir */
   snprintf(path, sizeof path, "%s/%s", cpusetdir, child);
   DRETURN(make_sub_cpuset(path, "0"));
}

/* Create a cpuset directory corresponding to the shepherd's pid in
   the job cpuset.  */
bool
make_shepherd_cpuset(u_long32 job, u_long32 task, pid_t pid)
{
   char parent[SGE_PATH_MAX], child[64];

   DENTER(TOP_LAYER, "make_shepherd_cpuset");
   snprintf(parent, sizeof parent, "%s/"sge_u32"."sge_u32, cpusetdir,
            job, task);
   snprintf(child, sizeof child, pid_t_fmt, pid);
   DRETURN(make_sub_cpuset(parent, child));
}

/* Put PROC (string version of pid) into the controller DIR */
static bool
reparent_proc(const char *proc, char *dir)
{
   char path[SGE_PATH_MAX];
   pid_t pid = atoi(proc);

   if (!pid) return false;
   /* Don't fail if the process no longer exists.  */
   snprintf(path, sizeof path, "/proc/%s", proc);
   if (!is_dir(path)) return true;
   return set_pid_cgroup(pid, dir);
}

/* Remove the task's controller directory after moving out the
   shepherd and killing anything left.  */
bool
remove_shepherd_cpuset(u_long32 job, u_long32 task, pid_t pid)
{
   char dir[SGE_PATH_MAX], taskfile[SGE_PATH_MAX], spid[PID_BSIZE];
   FILE *fp;
   bool rogue = false;

   DENTER(TOP_LAYER, "remove_shepherd_cpuset");
   snprintf(dir, sizeof dir, "%s/"sge_u32"."sge_u32"/"pid_t_fmt,
            cpusetdir, job, task, pid);
   snprintf(taskfile, sizeof taskfile, "%s/tasks", dir);
   /* We should have an empty task list.  If we can't remove it, kill
      anything there.  Arguably this should be repeated in case of a
      race against things spawning.  */
   errno = 0;
   if (rmdir(dir) == 0) DRETURN(true);
   /* EBUSY means it still has tasks.  */
   if (errno != EBUSY) {
      ERROR((SGE_EVENT, MSG_FILE_RMDIRFAILED_SS, dir, strerror(errno)));
      DRETURN(false);
   }
   if (!(fp = fopen(taskfile, "r"))) {
      WARNING((SGE_EVENT, MSG_FILE_NOOPEN_SS, taskfile, strerror(errno)));
      DRETURN(false);
   }
   while (fgets(spid, sizeof spid, fp)) {
      char buf[MAX_STRING_SIZE], cfile[SGE_PATH_MAX], *cmd;
      size_t l = sizeof buf;

      if (!rogue)
         WARNING((SGE_EVENT, "rogue process(es) found for task %d.%d",
                  job, task));
      rogue = true;
      replace_char(spid, strlen(spid), '\n', '\0');
      snprintf(cfile, sizeof cfile, "/proc/%s/cmdline", spid);
      errno = 0;
      cmd = dev_file2string(cfile, buf, &l);
      if (l) INFO((SGE_EVENT, "rogue: "SFN2, replace_char(cmd, l, '\0', ' ')));
      /* Move the task away to avoid waiting for it to die.  */
      /* Fixme:  Keep the cpusetdir tasks open and just write to that.  */
      reparent_proc(spid, cpusetdir);
      pid = atoi(spid);
      if (pid) kill(pid, SIGKILL);
   }
   fclose(fp);
   errno = 0;
   if (rmdir(dir) == 0) DRETURN(true);
   ERROR((SGE_EVENT, MSG_FILE_RMDIRFAILED_SS, dir, strerror(errno)));
   DRETURN(false);
}

/* Remove the job task cpuset directory after first removing shepherd
   sub-directories.  */
bool
remove_task_cpuset(u_long32 job, u_long32 task)
{
#if __linux__                   /* using non-portable dirent-isms */
   char dirpath[SGE_PATH_MAX];
   DIR *dir;
   struct dirent *dent;

   DENTER(TOP_LAYER, "remove_task_cpuset");
   snprintf(dirpath, sizeof dirpath, "%s/"sge_u32"."sge_u32,
            cpusetdir, job, task);
   INFO((SGE_EVENT, "removing task cpuset "SFN, dirpath));
   if (!is_dir(dirpath)) return true;
   errno = 0;
   /* Maybe this shoud be made reentrant, though there's existing code
      which does the same sort of thing in the same context.  */
   if ((dir = opendir(dirpath)) == NULL) {
      ERROR((SGE_EVENT, MSG_FILE_CANTOPENDIRECTORYX_SS, dirpath,
             strerror(errno)));
      DRETURN(false);
   }
   while ((dent = readdir(dir)))
      if ((DT_DIR == dent->d_type) && sge_strisint(dent->d_name))
         remove_shepherd_cpuset(job, task, atoi(dent->d_name));
   closedir(dir);

   if (rmdir(dirpath) != 0) {
      ERROR((SGE_EVENT, MSG_FILE_RMDIRFAILED_SS, dirpath, strerror(errno)));
      DRETURN(false);
   }
   DRETURN(true);
#else
   return true;
#endif  /* __linux__ */
}

/* Move the shepherd tasks to the job cpuset 0 and remove the shepherd
   cpuset.  */
bool
empty_shepherd_cpuset(u_long32 job, u_long32 task, pid_t pid)
{
   char dir[SGE_PATH_MAX], taskfile[SGE_PATH_MAX], taskfile0[SGE_PATH_MAX];
   bool ret;

   snprintf(dir, sizeof dir, "%s/"sge_u32"."sge_u32"/"pid_t_fmt,
            cpusetdir, job, task, pid);
   if (!is_dir(dir)) return true;
   snprintf(taskfile, sizeof taskfile, "%s/tasks", dir);
   snprintf(taskfile0, sizeof taskfile0, "%s/"sge_u32"."sge_u32"/0/tasks",
            cpusetdir, job, task);
   errno = 0;
   sge_seteuid(SGE_SUPERUSER_UID);
   /* This could fail at least if a task is respawning.
      It needs to be done task-wise (line-wise) according to cpuset(7).  */
   ret = copy_linewise(taskfile, taskfile0);
   if (sge_switch2admin_user() != 0)
      abort();
   if (ret != 0) return false;
   /* If rmdir fails, execd will try to kill it. */
   return rmdir(dir) ? false : true;
}
