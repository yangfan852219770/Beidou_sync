/*
    USB to UART
*/

#ifndef _BEIDOU_READ_H
#define _BEIDOU_READ_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h> /* Unix standard function def */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>   /* File control def */
#include <termios.h> /* POSIX terminal control def */
#include <errno.h>

#define READ_MAX_LENGTH 128
#define NSEC_PER_SEC 1000000000L
#define BAUD_LENGTH 11

typedef struct nmea_time {
    int hours;
    int minutes;
    int seconds;
    int microseconds;
}nmea_time;
typedef struct nmea_date {
    int day;
    int month;
    int year;
}nmea_date;

typedef struct nmea_sentence_zda {
    struct nmea_time time;
    struct nmea_date date;
    int hour_offset;
    int minute_offset;
}nmea_sentence_zda;

/**
 * Open the port
 * port_path: The path of port
 * fd: File descriptor for the port
 * flags: The mode of accessing the port
 * Return true or false.
 */
bool open_usb_port( const char *port_path, uint16_t *fd, int flags );

/**
 * Set parameters of the port, such as baud rate and parity
 * oldtio: The old config of board
 * newtio: The new config of board
 * fd: File descriptor for the port
 * baud_rate: The baud of your Beidou board
 * data_bit: The bits of data in a frame
 * parity_flag: The parity of your frame, Maybe even, odd or none
 * stop_bit: The bits of stop. Maybe 1 bit or 2 bits
 * Return true or false.
 */ 
bool set_parameter_port( struct termios * oldtio, struct termios * newtio, int fd, int baud_rate, int data_bit, char parity_flag, int stop_bit );

/**
 * Read and parse the NMEA data 
 * fd: File descriptor for the port
 * 
 */
void read_data(int fd, unsigned char *buf, int *actual_length, int timeout);

// 解析zda的时分秒
bool nmea_parse_zda_time(nmea_time *nmea_t, const char * sentence);

// 解析zda的日月年
bool nmea_parse_zda_date(nmea_date *nmea_d, const char * sentence);

// 解析zda所有数据
bool nmea_parse_zda(nmea_sentence_zda *nmea_zda, const char * sentence);

// 转化为系统时间的秒数
uint64_t convert_to_sys_second(int year,int month,int day,int hour,int min,int sec); 

// 设置系统时钟
bool set_clock(int year,int month,int day,int hour,int min,int sec, int nano_sec);

// 计算系统时钟偏移 方法1
bool calculate_clock_offset(struct timespec *sys_clock, nmea_sentence_zda *nmea_zda, long serial_delay, uint64_t system_call, struct timespec *offset);

#endif