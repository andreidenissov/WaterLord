/*
 * DeviceLine.c
 *
 *  Created on: Jul 7, 2014
 *      Author: andreidenissov
 */
#include "DeviceLine.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>

#include "HWDevices.h"

#define MAX_LINES  (4)

typedef struct DeviceLineStr
{
	int id;
	long startTime;
	long endTime;
	int isActive;
	pthread_mutex_t lock;
} DeviceLine;

typedef struct DevicesBlockStr
{
	DeviceLine devices[MAX_LINES];
	pthread_t updater;
	int doUpdate;
} DevicesBlock;

static int initDeviceLine(DeviceLine *line, int id)
{
	line->startTime = 0;
	line->endTime = 0;
	line->isActive = 0;
	line->id = id;
	return pthread_mutex_init(&line->lock, NULL);
}


static DevicesBlock dev;

DevicesUnit getDevicesUnit(int id)
{
    return (DevicesUnit)&dev;
}

static long getSystemTime()
{
	struct timeval  tv;
	gettimeofday(&tv, NULL);

	return (long)(tv.tv_sec * 1000 + tv.tv_usec / 1000) ;
}

static void *procUpdater(void *data)
{
    DevicesBlock *dev = (DevicesBlock *)data;
    int i;

    syslog (LOG_NOTICE, "UPDATER Started!\n");

	while (dev->doUpdate) {
        sleep(2);
        if (!dev->doUpdate)
        	break;
    	for (i = 0; i < MAX_LINES; i++) {
    		getDeviceStatus(dev, i);
    	}
	}
	syslog (LOG_NOTICE, "UPDATER Finished!\n");
    return NULL;
}

int startDevices(DevicesUnit du)
{
	int i, res;
	DevicesBlock *dev = (DevicesBlock *)du;

	for (i = 0; i < MAX_LINES; i++) {
		if ((res = initDeviceLine(&dev->devices[i], i)) != 0)
			return res;
	}
	openHWDevices();
	dev->doUpdate = 1;
	res = 0; //pthread_create(&dev->updater, NULL, &procUpdater, dev);
	return 0;
}

int finishDevices(DevicesUnit du)
{
	DevicesBlock *dev = (DevicesBlock *)du;
	dev->doUpdate = 0;
    pthread_join(dev->updater, NULL);
    closeHWDevices();
    syslog (LOG_NOTICE, "FINISH DEVICES.\n");
    return 0;
}

int switchDeviceON(DevicesUnit dev, int devNo, int secs)
{
	if (devNo < 0 || devNo >= MAX_LINES)
		return (-1);
	DeviceLine *line = &((DevicesBlock *)dev)->devices[devNo];

	pthread_mutex_lock(&line->lock);
    if (!line->isActive) {
    	line->startTime = getSystemTime();
    	line->endTime = line->startTime + secs * 1000;
    	line->isActive = 1;
    	hwDeviceOn(line->id);
    }
	return pthread_mutex_unlock(&line->lock);
}

int switchDeviceOFF(DevicesUnit dev, int devNo)
{
	if (devNo < 0 || devNo >= MAX_LINES)
		return (-1);
	DeviceLine *line = &((DevicesBlock *)dev)->devices[devNo];

	pthread_mutex_lock(&line->lock);
    if (line->isActive) {
    	line->startTime = 0;
    	line->endTime = 0;
    	line->isActive = 0;
    	hwDeviceOff(line->id);
    }
	return pthread_mutex_unlock(&line->lock);
}

int getDevicesNo()
{
    return MAX_LINES;
}

int getDeviceStatus(DevicesUnit dev, int devNo)
{
	int status = 0;

	if (devNo < 0 || devNo >= MAX_LINES)
		return (-1);
	DeviceLine *line = &((DevicesBlock *)dev)->devices[devNo];

	pthread_mutex_lock(&line->lock);
    if (line->isActive) {
    	long tmNow = getSystemTime();
    	if (tmNow > line->endTime) {
        	line->startTime = 0;
        	line->endTime = 0;
        	line->isActive = 0;
        	hwDeviceOff(line->id);
    	} else {
    		status = (int)(line->endTime - tmNow);
    	}
    }
	pthread_mutex_unlock(&line->lock);
	return status;
}






