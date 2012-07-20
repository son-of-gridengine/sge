#ifndef __SGE_CGROUP_H
#define __SGE_CGROUP_H
#include "basis_types.h"
#include "uti/sge_io.h"
#include "uti2/util.h"

typedef enum {
  /* "cpuset", for instance, clashes with other uses */
  cg_cpuset,
  cg_cpuacct,
  cg_freezer,
  cg_memory
} group_t;

#define cpusetdir "/dev/cpuset/sge"
#define cgroupdir "/cgroups/sge"

bool have_cgroup (group_t group);
bool make_task_cpuset (u_long32 job, u_long32 task);
bool remove_task_cpuset (u_long32 job, u_long32 task);
bool remove_shepherd_cpuset(u_long32 job, u_long32 task, pid_t pid);
bool have_cgroup_task_dir(group_t group, u_long32 job, u_long32 task);
bool write_to_cgroup_proc_file (group_t group, const char *cfile, const char *record, u_long32 job, u_long32 task, pid_t pid);
bool set_pid_shepherd_cgroup (group_t group, pid_t pid, u_long32 job, u_long32 task);
bool make_shepherd_cpuset (u_long32 job, u_long32 task, pid_t pid);
bool empty_shepherd_cpuset (u_long32 job, u_long32 task, pid_t pid);
#endif  /* __SGE_CGROUP_H */
