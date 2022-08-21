#include "pps_up.h"

static struct timespec offset_assert = {0, 0};
struct timespec pps_base;
struct timespec pps_raw;
// 打开pps设备
int find_source(char *path, pps_handle_t *handle, int *avail_mode){

    pps_params_t params;
	int res;
    int pps_fd;
    printf("Find PPS source \"%s\"\n", path);
    
    // 打开pps设备
    pps_fd = open(path, O_RDWR);
    if(pps_fd < 0){
        fprintf(stderr, "Unable to open device \"%s\" (%m)\n", path);
        return pps_fd;
    }

    // 创建pps源
    res = time_pps_create(pps_fd, handle);
	if (res < 0) {
		fprintf(stderr, "Cannot create a PPS source from device "
                "\"%s\" (%m)\n", path);
		return -1;
	}
    printf("Found PPS source \"%s\"\n", path);
    
    // 获得内核支持得pps特性
	res = time_pps_getcap(*handle, avail_mode);
	if (res < 0) {
		fprintf(stderr, "Cannot get capabilities (%m)\n");
		return -1;
	}
	if ((*avail_mode & PPS_CAPTUREASSERT) == 0) {
		fprintf(stderr, "Not support CAPTUREASSERT\n");
		return -1;
	}
    printf("PPS: Set CAPTUREASSERT \n");

    // 设置获得pps信号的时间戳格式
	res = time_pps_getparams(*handle, &params);
	if (res < 0) {
		fprintf(stderr, "Cannot get parameters (%m)\n");
		return -1;
	}
	params.mode |= PPS_CAPTUREASSERT;
	
    // 清除补偿pps信号的时间偏差
	if ((*avail_mode & PPS_OFFSETASSERT) != 0) {
		params.mode |= PPS_OFFSETASSERT;
		params.assert_offset = offset_assert;
	}
	res = time_pps_setparams(*handle, &params);
	if (res < 0) {
		fprintf(stderr, "Cannot set parameters (%m)\n");
		return -1;
	}

    printf("Set PPS success\n");

    return 0;

}

int fetch_source(pps_handle_t *handle, int *avail_mode, pps_info_t *info)
{
	struct timespec timeout;
	pps_info_t infobuf;
	int res;

	/* create a zero-valued timeout */
	timeout.tv_sec = 3;
	timeout.tv_nsec = 0;

retry:
	if (*avail_mode & PPS_CANWAIT){
		res = time_pps_fetch(*handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	}
	else {
		sleep(1);
		res = time_pps_fetch(*handle, PPS_TSFMT_TSPEC, &infobuf,
				   &timeout);
	}
	if (res < 0) {
		if (res == -EINTR) {
			fprintf(stderr, "time_pps_fetch() got a signal!\n");
			goto retry;
		}

		fprintf(stderr, "time_pps_fetch() error %d (%m)\n", res);
		return -1;
	}
	*info = infobuf;
/*
	printf("assert %ld.%09ld, sequence: %ld\n",
	       infobuf.assert_timestamp.tv_sec,
	       infobuf.assert_timestamp.tv_nsec,
	       infobuf.assert_sequence);
	fflush(stdout);
*/
	return 0;
}