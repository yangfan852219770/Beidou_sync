# Makefile
objs = test.o beidou_read.o pps_up.o
test: $(objs)
	gcc -g -o $@ $^ -lwiringPi
test.o : beidou_read.h pps_up.h
beidou_read.o: beidou_read.h
pps_up.o: pps_up.h pps.h
.PHONY : clean
clean : 
	-rm *.o