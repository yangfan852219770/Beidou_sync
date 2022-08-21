/**
 * Copyright © Yangfan <yangyangfan@stud.tjut.edu.cn>
 * This program is free software.
 */ 
#include "beidou_read.h"
#include "minmea.h"

#include <ctype.h>
#include <sys/time.h>

bool open_usb_port( const char *port_path, uint16_t *fd, int flags ){
    *fd = open( port_path, flags );
    if( *fd == -1 ){
        perror( "Open serial port error" );
        return false;
    }
    printf( "Open the port:%s success, the file descriptor is: %d.\n", port_path, *fd );
    return true;
}

bool set_parameter_port( struct termios *newtio, struct termios *oldtio, int fd, int baud_rate, int data_bit, char parity_flag, int stop_bit ){
    int i;
    int flag;

    int speed_arr[BAUD_LENGTH] = {B921600, B576000, B460800,B230400, B115200, B19200, B9600, B4800, B2400, B1200, B300};
    int name_arr[BAUD_LENGTH]  = {921600,576000,460800,230400, 115200,  19200,  9600,  4800,  2400,  1200,  300};
    // Save the config of board to restore when close the port
    tcgetattr( fd, oldtio );
    // Initialize the new struct
    bzero(newtio, sizeof( *newtio ));

    /**
     * CLOCAL: Local line
     * CREAD: Enable receiver
     */ 
    newtio->c_cflag |= ( CLOCAL | CREAD );

    // Set data bit
    newtio->c_cflag &= ~CSIZE;

    switch (data_bit)
    {
    case 5:
        newtio->c_cflag |= CS5;
        break;
    case 6:
        newtio->c_cflag |= CS6;
        break;
    case 7:
        newtio->c_cflag |= CS7;
        break;
    case 8:
        newtio->c_cflag |= CS8;
        break;
    default:
        perror( "No such data bit" );
        return false;
    }
    // End set data bit

    // Set parity bit
    switch ( parity_flag ) 
    {
    // Odd parity
    case 'O':
        // Enable parity bit
        newtio->c_cflag |=PARENB;
        newtio->c_iflag |= ( INPCK | ISTRIP );
        // Enable odd parity
        newtio->c_cflag |= PARODD;
        break;
    // Even parity
    case 'E':
        newtio->c_cflag |=PARENB;
        newtio->c_iflag |= ( INPCK | ISTRIP );
        // Enable even parity
        newtio->c_cflag &= ~PARODD;
        break;
    // None parity
    case 'N':
        newtio->c_cflag &= ~PARENB;
        break;
    default:
        perror("No such parity");
        return false;
    }
    // End set parity bit

    // Set stop bit
    switch (stop_bit)
    {
    case 1:
        newtio->c_cflag &= ~CSTOPB; 
        break;
    case 2:
        newtio->c_cflag |= CSTOPB; 
        break;
    default:
        perror("No such stop_bit");
        return false;
    }
    // End set stop bit

    // 设置波特率
    flag = 0;
    for(i=0; i < BAUD_LENGTH; ++i){
        if  (baud_rate == name_arr[i])
        {
            flag = 1;
            cfsetispeed( newtio, speed_arr[i] );
            cfsetospeed( newtio, speed_arr[i] );
            break;
        }
    }
    if(flag == 0){
        perror("Baud rate not in range\n");
        return false;
    }

    // Canonical input 
    //newtio->c_lflag |= (ICANON | ECHO | ECHOE);
    // raw input
    newtio->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // disable software flow control
    newtio->c_iflag &= ~(ISTRIP | IXON | IXOFF | IXANY | BRKINT | IMAXBEL | PARMRK);
  
    newtio->c_iflag |= IGNCR | IGNPAR | IGNBRK;

    // Time to wait for data
    newtio->c_cc[VTIME] = 1; // 十分之一秒为单位
    // Minimum number of characters to read
    newtio->c_cc[VMIN] = 1;

    // Flushes the input and/or output queue
    tcflush( fd, TCSANOW );

    /**
     * Set config to port
     * TCSANOW: Make changes now without waiting for data to complete
     * TCSADRAIN: Wait until everything has been transmitted
     * TCSAFLUSH: Flush input and output buffers and make the change
     */
    if ( tcsetattr( fd, TCSANOW, newtio) != 0) 
    {
        perror ("Set config error" ) ;
        return false;
    } else
        return true;
}

bool nmea_parse_zda_date(nmea_date *nmea_d, const char * sentence){
    const char ch = ',';
    const char *type = "ZDA";
    const char *field = sentence;

    // TODO 校验和是否正确

    // 校验
    if(!field){
         printf("[%s %d] ZDA sentence is null\n", __FUNCTION__, __LINE__);
         return false;
    }
    if(field[0] != '$'){
        printf("[%s %d] ZDA sentence not begin with $\n", __FUNCTION__, __LINE__);
        return false;
    }
    // 是否为ZDA语句
    if(strncmp(field + 3, type, 3) != 0) {
        printf("[%s %d] This sentence is not ZDA\n", __FUNCTION__, __LINE__);
        return false;
    }
    int d = -1, m = -1, y = -1;
    // 跳过首部
    field = strchr(field, ch) + 1;
    // 跳过时分秒
    field = strchr(field, ch) + 1;

    // 日
    if(field){
        char dArr[] = {field[0], field[1], '\0'};
        d = atoi(dArr);
        // 跳过日
        field = strchr(field, ch) + 1;
    }

    // 月
    if(field){
        char mArr[] = {field[0], field[1], '\0'};
        m = atoi(mArr);
        // 跳过月
        field = strchr(field, ch) + 1;
    }
    
    // 年
    if(field){
        char yArr[5];
        strncpy(yArr, field, 4);
        yArr[4] = '\0';
        y = atoi(yArr);
    }
   
    nmea_d->day = d;
    nmea_d->month = m;
    nmea_d->year = y;

    return true;
}

bool nmea_parse_zda_time(nmea_time *nmea_t, const char *sentence){
    const char ch = ',';
    const char *type = "ZDA";
    const char *field = sentence;

    // TODO 校验和是否正确

    // 校验
    if(!field){
         printf("[%s %d] ZDA sentence is null\n", __FUNCTION__, __LINE__);
         return false;
    }
    if(field[0] != '$'){
        printf("[%s %d] ZDA sentence not begin with $\n", __FUNCTION__, __LINE__);
        return false;
    }
    // 是否为ZDA语句
    if(strncmp(field + 3, type, 3) != 0) {
        printf("[%s %d] This sentence is not ZDA\n", __FUNCTION__, __LINE__);
        return false;
    }

    // 处理时分秒
    // 如果没有解析数据，则为-1
    int h = -1, m = -1, s = -1, u = -1;
    field = strchr(field, ch) + 1;
    if(field){
        char hArr[] = {field[0], field[1], '\0'};
        char mArr[] = {field[2], field[3], '\0'};
        char sArr[] = {field[4], field[5], '\0'};
        h = atoi(hArr);
        m = atoi(mArr);
        s = atoi(sArr);
    }
    
    // 处理毫秒
    field += 6;
    if (*field++ == '.') {
        uint32_t value = 0;
        uint32_t scale = 1000000LU;
        while (isdigit((unsigned char) *field) && scale > 1) {
            value = (value * 10) + (*field++ - '0');
            scale /= 10;
        }
        u = value * scale;
    } else {
        u = 0;
    }

    nmea_t->hours = h;
    nmea_t->minutes = m;
    nmea_t->seconds = s;
    nmea_t->microseconds = u;
    
    return true;
}

bool nmea_parse_zda(nmea_sentence_zda *nmea_zda, const char * sentence){
    // TODO 未处理时区偏差
    
    bool res;
    res = nmea_parse_zda_date(&nmea_zda->date, sentence);
    if(!res)
        return false;

    res = nmea_parse_zda_time(&nmea_zda->time, sentence);
    if(!res)
        return false;

    return true;

}

void read_data(int fd, uint8_t *buf, int *actual_length, int timeout)
{
    int ret;
    int count = 0;
    fd_set rd;
    struct timeval tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    FD_ZERO(&rd);
    FD_SET(fd, &rd);

    // 检查串口数据是否准备好
    ret = select(fd + 1, &rd, NULL, NULL, &tv);
    if(ret > 0){
        if (FD_ISSET(fd, &rd)) {
            *actual_length = read(fd, buf, READ_MAX_LENGTH);
        }
    }
    else if(ret == 0){
        printf("[%s %d]timeout\n", __FUNCTION__, __LINE__);
        return;
    }
    else{
        printf("[%s %d] select error!\n", __FUNCTION__, __LINE__);
        return;
    }
}

uint64_t convert_to_sys_second(int year,int month,int day,int hour,int min,int sec){

    long time_sec;
    struct tm tptr;

    // 时间信息转换
    tptr.tm_year = year - 1900;
    tptr.tm_mon = month - 1;
    tptr.tm_mday = day;
    tptr.tm_hour = hour;	
    tptr.tm_min = min;
    tptr.tm_sec = sec;
	tptr.tm_isdst = 0; // 夏令时，0表示不启用

    //将时间转化成秒
	time_sec = mktime(&tptr);

    return time_sec;
}

bool set_clock(int year,int month,int day,int hour,int min,int sec, int nano_sec){

    struct tm tptr;
	struct timespec now, ts;
	long time_sec;
	int res;

    //获取修改前的系统时间
    time_sec = convert_to_sys_second(year, month, day, hour, min, sec);
	if(time_sec <= 0){
		printf("[%s %d]Fail to convert time to second\n", __FUNCTION__, __LINE__);
		return false;
	}
    printf("Set clock sec = %ld, nsec = %d\n", time_sec, nano_sec);

    // 设置时钟
	ts.tv_sec = time_sec;
	ts.tv_nsec = nano_sec;

	res = clock_settime(CLOCK_REALTIME, &ts);
	if(res < 0){
		printf("[%s %d]Fail to set system clock\n", __FUNCTION__, __LINE__);
		return false;
	}
    printf("Set successfully system clock...\n");
	return true;
}

bool calculate_clock_offset(struct timespec *sys_clock, nmea_sentence_zda *nmea_zda, long serial_delay, uint64_t system_call, struct timespec *offset){
    
    long zda_sec;

    // 将zda时间转化为秒
    zda_sec = convert_to_sys_second(nmea_zda->date.year, nmea_zda->date.month, nmea_zda->date.day,
                            nmea_zda->time.hours, nmea_zda->time.minutes, nmea_zda->time.seconds);
    if(zda_sec < 0){
		printf("[%s %d]Fail to convert zda time to second\n", __FUNCTION__, __LINE__);
		return false;
	}
    printf("***ZDA sec = %ld\n", zda_sec);
   
    offset->tv_sec = sys_clock->tv_sec - zda_sec;
    offset->tv_nsec = sys_clock->tv_nsec - serial_delay - system_call;

    return true;
}
