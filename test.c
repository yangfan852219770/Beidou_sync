#include "beidou_read.h"
#include "pps_up.h"

#include <sys/timex.h>
#include <time.h>

#define SERIALPATH "/dev/ttyUSB0"
#define BAUD_RATE 115200
#define DATA_BIT 8
#define PARITY_BIT 'N'
#define STOP_BIT 1

int main(int argc, char *argv[])
{
    uint16_t fd;
    // 开启PPS信号
    char pps_cmd[] = "\xF1\xD9\x06\x07\x0F\x00\x40\x42\x0F\x00\x32\x00\x00\x00\x10\x27\x00\x00\x01\x0D\x00\x24\xAB\n";

    pps_handle_t handle;
    int avail_mode;
    pps_info_t realtime_info;

    bool res_serial;

    struct termios oldtio, newtio;
    int n;

    uint8_t buf[READ_MAX_LENGTH];
    int len;
    // 等待1s
    int timeout = 1;

    nmea_sentence_zda nmea_z;
    long zda_sec;
    // 每次收到pps信号时的系统时间整秒数
    long sys_sec;
    // zda与系统时间相差的整秒数
    long offset_sec;
    long offset_nsec;

    struct timex tx;
    int count;
    int adj_ret;

    /* Check the command line */
    if (argc < 2)
        fprintf(stderr, "usage: %s Input PPS path\n", argv[0]);

    int res = find_source(argv[1], &handle, &avail_mode);

    if (res < 0)
    {
        printf("Cannot creat PPS source\n");
        exit(EXIT_FAILURE);
    }

    // 设置NMEA串口 start
    res_serial = open_usb_port(SERIALPATH, &fd, O_RDWR | O_NOCTTY);

    if (!res_serial)
        return -1;
    // 设置串口参数
    res_serial = false;
    res_serial = set_parameter_port(&newtio, &oldtio, fd, BAUD_RATE, DATA_BIT, PARITY_BIT, STOP_BIT);
    if (!res_serial)
        return -1;
    // 设置NMEA串口 end

    // 设置串口设备PPS信号
    n = write(fd, pps_cmd, sizeof(pps_cmd));
    // n = write(fd, sat_cmd, sizeof(sat_cmd));
    if (n > 0)
    {
        printf("PPS set success!\n");
        sleep(1);
    }
    else
    {
        printf("PPS write() failed!\n");
        return 0;
    }

    char offset_path[] = "/home/pi/offset.xls";
    FILE *fp = NULL;
    if ((fp = fopen(offset_path, "w")) == NULL)
        perror("Fail to open file!\n");

    fprintf(fp, "%s\t%s\t\%s\n", "ZDA_sec", "Sys_sec", "Sys_nsec");

    // 绑定pps kernel consumer
    time_pps_kcbind(handle, PPS_KC_HARDPPS, PPS_CAPTUREASSERT, PPS_TSFMT_TSPEC);
    tx.modes = ADJ_STATUS;
    tx.status = (STA_PPSFREQ | STA_PPSTIME);
    adj_ret = adjtimex(&tx);
    if (adj_ret < 0)
    {
        perror("adjtimex set failed");
        exit(0);
    }
    printf("Set pps kernel consumer\n");

    //sleep(15);

    count = 0;
    // 清除串口缓存
    tcflush(fd, TCSANOW);
    // 校准系统时间，秒级别
    while (1)
    {
        // pps信号时的系统时间戳
        int pps_res = fetch_source(&handle, &avail_mode, &realtime_info);
        if (res < 0)
        {
            while (1)
            {
                pps_res = fetch_source(&handle, &avail_mode, &realtime_info);
                if (pps_res > 0)
                {
                    // 绑定pps kernel consumer
                    time_pps_kcbind(handle, PPS_KC_HARDPPS, PPS_CAPTUREASSERT, PPS_TSFMT_TSPEC);
                    tx.modes = ADJ_STATUS;
                    tx.status = (STA_PPSFREQ | STA_PPSTIME);
                    adj_ret = adjtimex(&tx);
                    //sleep(15);
                    // 清除串口缓存
                    tcflush(fd, TCSANOW);
                    break;
                }
            }
        }

        memset(buf, 0, READ_MAX_LENGTH);
        // 读取串口数据
        read_data(fd, buf, &len, timeout);

        printf("****NO.%d*****\n", ++count);
        printf("PPS sec = %ld, nsec = %ld\n", realtime_info.assert_timestamp.tv_sec, realtime_info.assert_timestamp.tv_nsec);
        printf("res = %d, Raw NMEA: %s", len, buf);

        if (len > 30)
        {
            int zda_res = nmea_parse_zda(&nmea_z, buf);
            // 处理系统时间与卫星时间所差的整秒数
            if (zda_res)
            {
                zda_sec = convert_to_sys_second(nmea_z.date.year, nmea_z.date.month, nmea_z.date.day,
                                                nmea_z.time.hours, nmea_z.time.minutes, nmea_z.time.seconds);
                sys_sec = realtime_info.assert_timestamp.tv_sec;
                printf("zda sec = %ld\n", zda_sec);

                offset_sec = zda_sec - sys_sec;
                offset_nsec = offset_sec * NSEC_PER_SEC - realtime_info.assert_timestamp.tv_nsec;
                tx.time.tv_usec = 0;
                tx.modes = ADJ_SETOFFSET;
                printf("Now offset = %ld\n", offset_nsec);
                if (abs(offset_nsec) >= NSEC_PER_SEC)
                {
                    tx.time.tv_sec = offset_sec;
                    printf("adj offset sec = %ld\n", tx.time.tv_sec);
                    adj_ret = adjtimex(&tx);
                }
                fprintf(fp, "%ld\t%ld\t%ld\n", zda_sec, sys_sec, offset_nsec);
            }
        }
    }

    time_pps_kcbind(handle, PPS_KC_HARDPPS, 0, PPS_TSFMT_TSPEC);
    tx.modes = ADJ_STATUS;
    tx.status &= ~(STA_PPSFREQ | STA_PPSTIME);
    adjtimex(&tx);
    // 销毁pps
    time_pps_destroy(handle);
    // Restore the old config
    tcsetattr(fd, TCSANOW, &oldtio);
    // Close the port
    close(fd);
    return 0;
}
