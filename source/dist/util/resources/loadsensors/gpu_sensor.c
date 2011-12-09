/*
 * To compile:
 *   cc gpu_sensor.c -lnvidia-ml
 */

/*-
 * Copyright (c) 2011 Open Grid Scheduler.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Open Grid Scheduler
 *      project and its contributors.
 * 4. Neither the name of Open Grid Scheduler nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <nvml.h>

#define SUCCESS 0
#define MATCH   0
#define FAILURE 1

#define LINE_LEN 256
#define MAXDEVNAME 50

typedef unsigned long long ecc_err_t;

static void usage(void)
{
  fprintf(stderr, "usage\n");
  exit(0);
}

static int init_gpuapi(void)
{
  nvmlReturn_t ret = nvmlInit();

  if (ret == NVML_SUCCESS)
    return SUCCESS;
  else
    return FAILURE;
}

static int shutdown_gpuapi(void)
{
  nvmlReturn_t ret = nvmlShutdown();

  if (ret == NVML_SUCCESS)
    return SUCCESS;
  else
    return FAILURE;
}

static int gpu_maintenance(nvmlDevice_t device, char hostname[], int i)
{
  /* future expansion */
  return 0;
}

static int lasthourECC(nvmlDevice_t device, char hostname[], int i)
{
  time_t time_now;
  struct tm *now;
  static int hour = -1;
  static ecc_err_t sb_ecc_err_prev_hr, db_ecc_err_prev_hr;
 
  time_now = time(NULL);
  now      = localtime(&time_now);

  if (now->tm_hour == hour)
  {
     printf("%s:gpu.%d.prevhrsbiteccerror:%llu\n", hostname, i, sb_ecc_err_prev_hr);
     printf("%s:gpu.%d.prevhrdbiteccerror:%llu\n", hostname, i, db_ecc_err_prev_hr);

     return SUCCESS;
  }
  else
  {
     nvmlEnableState_t current, pending;

     if (hour != -1)
     {
        if (nvmlDeviceGetEccMode(device, &current, &pending) == NVML_SUCCESS)
        {
          if (current == NVML_FEATURE_ENABLED)
          {
             ecc_err_t eccCounts;

             if (nvmlDeviceGetTotalEccErrors(device, NVML_SINGLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts) == NVML_SUCCESS)
             {
                sb_ecc_err_prev_hr = eccCounts;
             }

             if (nvmlDeviceGetTotalEccErrors(device, NVML_DOUBLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts) == NVML_SUCCESS)
             {
                db_ecc_err_prev_hr = eccCounts;
             }
          }
        }

        printf("%s:gpu.%d.prevhrsbiteccerror:%llu\n", hostname, i, sb_ecc_err_prev_hr);
        printf("%s:gpu.%d.prevhrdbiteccerror:%llu\n", hostname, i, db_ecc_err_prev_hr);

        if (nvmlDeviceClearEccErrorCounts(device, NVML_VOLATILE_ECC) == NVML_SUCCESS)
        {
          hour = now->tm_hour;
          return SUCCESS;
        }
        else
        {
          return FAILURE;
        }
     }
     else
     {
        if (nvmlDeviceGetEccMode(device, &current, &pending) == NVML_SUCCESS)
        {
          if (current == NVML_FEATURE_ENABLED)
          {
             if (nvmlDeviceClearEccErrorCounts(device, NVML_VOLATILE_ECC) == NVML_SUCCESS)
             {
                hour = now->tm_hour;

                return SUCCESS;
             }
          }
        }
        return FAILURE;
     }
  }
}

static int listdevices(char hostname[])
{
  nvmlReturn_t ret;
  unsigned int count = 0, i;

  ret = nvmlDeviceGetCount(&count);

  if (ret != NVML_SUCCESS)
  {
    fprintf(stderr, "nvmlDeviceGetCount err: %d\n", ret);
    return FAILURE;
  }

  for (i=0; i < count; i++)
  {
     nvmlDevice_t device;

     ret = nvmlDeviceGetHandleByIndex(i, &device);

     if (ret != NVML_SUCCESS)
     {
        fprintf(stderr, "nvmlDeviceGetHandleByIndex err: %d\n", ret);
        continue;
     }

     {
        /* house keeping stuff */
        gpu_maintenance(device, hostname, i);
     }

     {
        char name[MAXDEVNAME];

        ret = nvmlDeviceGetName(device, name, sizeof(name));

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.name:%s\n", hostname, i, name);
        }
     }

     {
        nvmlPciInfo_t pci;

        ret = nvmlDeviceGetPciInfo(device, &pci);

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.busId:%s\n", hostname, i, pci.busId);
        }
     }

     {
        unsigned int speed;

        ret = nvmlDeviceGetFanSpeed(device, &speed);

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.fanspeed:%u\n", hostname, i, speed);
        }
        else if (ret == NVML_ERROR_NOT_SUPPORTED)
        {
           printf("%s:gpu.%d.fanspeed:0\n", hostname, i);
        }
     }

     {
        unsigned int clock;

        ret = nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &clock);

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.clockspeed:%u\n", hostname, i, clock);
        }
     }

     {
        nvmlMemory_t memory;

        ret = nvmlDeviceGetMemoryInfo(device, &memory);

        if (ret == NVML_SUCCESS)
        {
          printf("%s:gpu.%d.memfree:%llu\n",  hostname, i, memory.free);
          printf("%s:gpu.%d.memused:%llu\n",  hostname, i, memory.used);
          printf("%s:gpu.%d.memtotal:%llu\n", hostname, i, memory.total);
        }
     }

     {
        nvmlUtilization_t utilization;

        ret = nvmlDeviceGetUtilizationRates(device, &utilization);

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.utilgpu:%u\n", hostname, i, utilization.gpu);
           printf("%s:gpu.%d.utilmem:%u\n", hostname, i, utilization.memory);
        }
     }

     {
        /* house keeping stuff */
        lasthourECC(device, hostname, i);
     }

     {
        nvmlEnableState_t current, pending;

        ret = nvmlDeviceGetEccMode(device, &current, &pending);

        if (ret == NVML_SUCCESS)
        {
           if (current == NVML_FEATURE_ENABLED)
           {
              ecc_err_t eccCounts;


              ret = nvmlDeviceGetTotalEccErrors(device, NVML_SINGLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts);

              if (ret == NVML_SUCCESS)
              {
                 printf("%s:gpu.%d.sbiteccerror:%llu\n", hostname, i, eccCounts);
              }


              ret = nvmlDeviceGetTotalEccErrors(device, NVML_DOUBLE_BIT_ECC, NVML_VOLATILE_ECC, &eccCounts);

              if (ret == NVML_SUCCESS)
              {
                 printf("%s:gpu.%d.dbiteccerror:%llu\n", hostname, i, eccCounts);
              }
           }
        }
     }

     {
        unsigned int temp = 0;

        ret = nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp);

        if (ret == NVML_SUCCESS)
        {
           printf("%s:gpu.%d.temperature:%u\n", hostname, i, temp);
        }

     }
  }

  return SUCCESS;

}

static int init_hostname(char hostname[], size_t len)
{
  if (gethostname(hostname, len) != 0)
     return FAILURE;
  else
     return SUCCESS;
}


int main(int argc, char *argv[])
{
  char buffer[LINE_LEN];
  char hostname[HOST_NAME_MAX];

  if (argc != 1)
  {
   usage();
  }

  if (init_hostname(hostname, HOST_NAME_MAX) != SUCCESS)
  {
     fprintf(stderr, "init_hostname error\n");
     exit(1);
  }

  if (init_gpuapi() != SUCCESS)
  {
     fprintf(stderr, "init_gpuapi error\n");
    // exit(1);
  }

#if !defined(STANDALONE)
  while (fgets(buffer, sizeof(buffer), stdin) != NULL)
#endif
  {
    if (memcmp(buffer, "quit", sizeof("quit")-1) == MATCH)
    {
       shutdown_gpuapi();
       return SUCCESS;        /* done */
    }

    {
       printf("begin\n");

       listdevices(hostname);

       printf("end\n");

       fflush(stdout);
    }
  }

  return SUCCESS;
}
