/* SGE GPU load sensor for CUDA/OpenCL devices
Copyright (C) 2012 Dave Love, University of Liverpool

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* 
   SGE needs proper resource management for GPUs and other devices,
   but in the meantime, there's a load sensor like everyone has.

   This currently only does CUDA devices, using the nvidia-ml library
   from the Tesla deployment kit.  (Originally written against the TDK
   1.285 but with conditions from testing against a v1.0 library.)
   The nvidia-ml library is in the driver distribution, for instance,
   but the header only seems to be in the TDK.

   There are only a fairly small number of items dealt with that are
   probably useful for scheduling in the absence of proper resource
   management for the devices.  It would easy to deal with, say,
   temperature and ECCs which could be used to put put queues into an
   alaram state, but such monitoring is probably best done elsewhere,
   and where do you stop?

   Fixme:
   * ATI devices
   * Maybe try dynamic dispatch to functions which are library
     version-dependent
   * Add a some more values, such as compute mode
   * Figure out how to deal with devices that do CUDA and OpenCL, which
     will get double counted
*/

#ifndef _XOPEN_SOURCE
  #define _XOPEN_SOURCE 500
#endif
#ifndef HAVE_NVML
  #define HAVE_NVML 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

#if HAVE_NVML
  #include <nvml.h>
#endif
#if HAVE_OPENCL
  #include <CL/opencl.h>
#endif
#if !HAVE_NVML && !HAVE_OPENCL
  #error "No CUDA or OpenCL support"
#endif

/* It's convenient to share globals amongst CUAD and OpenCL in
   particular.  */
int debug = 0;                  /* Debugging mode */
char host[HOST_NAME_MAX + 1];
unsigned int n_cuda = 0, n_opencl = 0; /* device counts */
char names[1024] = "";                 /* device model name list */

void
usage (int ret, const char *prog)
{
  fprintf (ret ? stderr : stdout,
           "Usage: %s [--help | --debug]\n"
           "Grid Engine load sensor for "
#if HAVE_NVML
  "CUDA"
  #if HAVE_OPENCL
    " and OpenCL"
  #endif
#else
  "CUDA"  
#endif
  " GPUs\n\n", prog);
  fprintf (ret ? stderr : stdout,
           "  -h, --help   Display this help and exit\n"
           "  -d, --debug  Print debugging information to stderr, including\n"
           "               some initial information and messages about library\n"
           "               call failures\n\n");
  if (0 == ret) {
    printf
      ("Possible complex settings:\n"
       "  gpu.ndev            gpu.ndev            INT    <= YES YES 0 0\n"
       "  gpu.names           gpu.names           STRING == YES NO  0 0\n"
#if HAVE_NVML
       "  gpu.cuda.ncuda      gpu.cuda.ncuda      INT    <= YES YES 0 0\n"
       "  gpu.cuda.0.mem_free gpu.cuda.0.mem_free MEMORY <= YES YES 0 0\n"
       "  gpu.cuda.0.clock    gpu.cuda.0.clock    INT    <= YES NO  0 0\n"
#endif
       "    ...\n"
       "where:\n"
       "  gpu.ndev is the total number of GPUs on the host\n"
       "  gpu.names is a semi-colon-separated list of GPU model names\n"
#if HAVE_NVML
       "  gpu.cuda.ncuda is the number of CUDA GPUs on the host\n"
       "  gpu.cuda.N.mem_free is the free memory on CUDA GPU N\n"
       "  gpu.cuda.N.clock is the clock speed of CUDA GPU N (in MHz)\n\n"
       "Example output:\n\n"
       "  begin\n"
       "  comp035:gpu.ndev:2\n"
       "  comp035:gpu.cuda.n_cuda.2\n"
       "  comp035:gpu.cuda.0.mem_free:4290838528\n"
       "  comp035:gpu.cuda.0.clock:799\n"
       "  comp035:gpu.cuda.1.mem_free:4290838528\n"
       "  comp035:gpu.cuda.1.clock:799\n"
       "  comp035:gpu.names:Tesla T10 Processor;Tesla T10 Processor;\n"
       "  end\n"
#endif
       );
  }
  exit (ret);
}

/* stash the hostname we need to print */
void
set_host ()
{
  errno = 0;
  if (gethostname (host, sizeof(host))) {
    if (debug)
      perror ("Can't get host name");
    exit (EXIT_FAILURE);
  }
}

#if HAVE_NVML

#if NVML_API_VERSION == 1
/* Convert return code to string  */
const char* nvmlErrorString(nvmlReturn_t result)
{
  switch (result) {

    /* I'm not sure all the enum symbols are defined in v1.  */
  case 0 /* NVML_SUCCESS */:
    return "The operation was successful";
  case 1 /* NVML_ERROR_UNINITIALIZED */:
    return "NVML was not first initialized with nvmlInit()";
  case 2 /* NVML_ERROR_INVALID_ARGUMENT */:
    return "A supplied argument is invalid";
  case 3 /* NVML_ERROR_NOT_SUPPORTED */:
    return "The requested operation is not available on target device";
  case 4 /* NVML_ERROR_NO_PERMISSION */:
    return "The current user does not have permission for operation";
  case 5 /* NVML_ERROR_ALREADY_INITIALIZED */:
    return "Deprecated: Multiple initializations are now allowed through ref counting";
  case 6 /* NVML_ERROR_NOT_FOUND */:
    return "A query to find an object was unsuccessful";
  case 7  /* NVML_ERROR_INSUFFICIENT_SIZE */:
    return "An input argument is not large enough";
  case 8 /* NVML_ERROR_INSUFFICIENT_POWER */:
    return "A device's external power cables are not properly attached";
  case 9 /* NVML_ERROR_DRIVER_NOT_LOADED */:
    return "NVIDIA driver is not loaded";
  case 10 /* NVML_ERROR_TIMEOUT */:
    return "User provided timeout passed";
  case 999 /* NVML_ERROR_UNKNOWN */:
    return "An internal driver error occurred";
  default:
    return "Unknown error code";
  }
}
#endif

/* Quit if we got an error  */
void
nv_maybe_quit (const char * routine, const nvmlReturn_t ret)
{
  if (NVML_SUCCESS == ret)
    return;
  if (debug)
    fprintf (stderr, "nvml error in %s: %s\n", routine, nvmlErrorString(ret));
  exit (EXIT_FAILURE);
}

/* Debug print if we got an error */
int
nv_maybe_debug (const char *routine, const nvmlReturn_t ret)
{
  if (NVML_SUCCESS == ret)
    return 0;
  if (debug)
    fprintf (stderr, "nvml error from %s: %s\n", routine,
             nvmlErrorString(ret));
  return 1;
}
#endif

/* Do any necessary library initialization */
void
init (void)
{
#if HAVE_NVML
  {
    char version[80];

    nv_maybe_quit ("nvmlInit", nvmlInit());
    if (debug) {
#if NVML_API_VERSION >= 2
      if (nv_maybe_debug ("nvmlSystemGetNVMLVersion",
                          nvmlSystemGetNVMLVersion (version, sizeof version)) == 0)
        fprintf (stderr, "NVML library version: %s\n", version);
#endif
      if (nv_maybe_debug ("nvmlSystemGetDriverVersion",
                          nvmlSystemGetDriverVersion (version, sizeof version)) == 0)
        fprintf (stderr, "NVML driver version: %s\n", version);
    }
  }
#endif
}

/* DO any necessary shutdown stuff */
void
shutdown (void)
{
#if HAVE_NVML
  nv_maybe_quit ("nvmlShutdown", nvmlShutdown());
#endif
}

/* Deal with device counts */
void
set_n_dev (void)
{
  nvmlReturn_t ret;

#if HAVE_NVML
  if ((ret = nvmlDeviceGetCount (&n_cuda)) == NVML_SUCCESS) {
    printf ("%s:gpu.ncuda:%u\n", host, n_cuda);
    } else {  
    n_cuda = 0;
    if (debug)
      fprintf (stderr, "nvmlUnitGetCount: %s\n", nvmlErrorString(ret));
  }
#endif
#if HAVE_OPENCL
#endif
  printf ("%s:gpu.ndev:%u\n", host, n_cuda + n_opencl);
}

#if HAVE_NVML
/* print loads for CUDA devices */
void
print_nvml (void)
{
  unsigned int i;

  if (n_cuda > 0)
    printf ("%s:gpu.cuda.n_cuda.%u\n", host, n_cuda);
  for (i = 0; i < n_cuda; i++) {
    nvmlDevice_t dev;
    char name[80];
    unsigned int uint;
    nvmlMemory_t mem;
    nvmlProcessInfo_t infos[128];

    nv_maybe_quit ("nvmlDeviceGetHandleByIndex",
                   nvmlDeviceGetHandleByIndex (i, &dev));
    /* Build a semicolon-separated list of device names for pattern
       matching in a resource request.  */
    if (nv_maybe_debug ("nvmlDeviceGetName",
                        nvmlDeviceGetName (dev, name, sizeof (name))) != 0)
      continue;
    snprintf (names + strlen (names), sizeof names - strlen (names),
              "%s;", name);
    if (nv_maybe_debug ("nvmlDeviceGetMemoryInfo",
                        nvmlDeviceGetMemoryInfo (dev, &mem)) == 0) {
      printf ("%s:gpu.cuda.%u.mem_free:%llu\n", host, i, mem.free);
    }
    uint = sizeof (infos);
#if NVML_API_VERSION >= 2
    if (nv_maybe_debug ("nvmlDeviceGetComputeRunningProcesses",
                        nvmlDeviceGetComputeRunningProcesses (dev, &uint,
                                                              infos)))
      printf ("%s:gpu.cuda.%u.procs:%u\n", host, i, uint);
#endif
    if (nv_maybe_debug ("nvmlDeviceGetClockInfo",
                        nvmlDeviceGetClockInfo (dev, NVML_CLOCK_SM, &uint)) == 0)
      printf ("%s:gpu.cuda.%u.clock:%u\n", host, i, uint);
  }
}
#endif

#if HAVE_OPENCL
/* print loads for OpenCL devices */
void
print_opencl (void)
{
  ;
}
#endif

/* command line args */
void
parse_args (int argc, char *argv[])
{
  if (argc == 1)
    return;
  else if (argc > 2)
    usage (1, argv[0]);
  else if ((strcmp (argv[1], "--help") == 0)
           || (strcmp (argv[1], "-h") == 0))
    usage (0, argv[0]);
  else if ((strcmp (argv[1], "--debug") == 0)
           || (strcmp (argv[1], "-d") == 0))
    debug = 1;
  /* fixme:  do --version */
  else
    usage (1, argv[0]);
}

int
main (int argc, char *argv[])
{
  char line[80];

  parse_args (argc, argv);
  set_host ();
  init ();
  while (fgets (line, sizeof (line), stdin)) {
    if ((strcmp (line, "quit\n") == 0) || (strcmp (line, "quit") == 0))
      break;
    printf("begin\n");
    set_n_dev ();
#if HAVE_NVML
    if (n_cuda > 0)
      print_nvml ();
#endif
#if HAVE_OPENCL
    if (n_opencl > 0)
      print_opencl ();
#endif
    printf ("%s:gpu.names:%s\n", host, names);
    printf("end\n");
    fflush(stdout);
  }
  shutdown ();
  exit (EXIT_SUCCESS);
}
