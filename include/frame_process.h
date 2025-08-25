#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>

#include "ss_mpi_vgs.h"
#include "ss_mpi_gdc.h"
#include "ot_common_gdc.h"
#include "ot_common_vgs.h"
#include "svp.h"


td_void *frame_process(td_void *arg);
td_void *frame_process_cut(td_void *arg);