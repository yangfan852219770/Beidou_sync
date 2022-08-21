#ifndef PPS_UP_H
#define PPS_UP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/timepps.h>

// 打开pps设备
int find_source(char *path, pps_handle_t *handle, int *avail_mode);

// 获取pps时间戳
int fetch_source(pps_handle_t *handle, int *avail_mode, pps_info_t *info);

#endif