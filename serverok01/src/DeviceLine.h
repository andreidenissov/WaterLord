/*
 * DeviceLine.h
 *
 *  Created on: Jul 7, 2014
 *      Author: andreidenissov
 */

#ifndef DEVICELINE_H_
#define DEVICELINE_H_

typedef void *DevicesUnit;

#define MAX_REP_LINES (4)

#define STATUS_LINE_EMPTY     (0)
#define STATUS_LINE_INACTIVE  (1)
#define STATUS_LINE_WAITING   (2)
#define STATUS_LINE_ACTIVE    (3)

typedef struct DeviceLineStatusRec
{
	int status;
	int time;
} DeviceLineStatus;

typedef DeviceLineStatus DeviceStatus[MAX_REP_LINES];

extern DevicesUnit getDevicesUnit(int id);
extern int startDevices(DevicesUnit dev);
extern int finishDevices(DevicesUnit dev);
extern int switchDeviceON(DevicesUnit dev, int devNo, int secs);
extern int switchDeviceOFF(DevicesUnit dev, int devNo);
extern int getDevicesNo();
extern int getDeviceStatus(DevicesUnit dev, int devNo);

#endif /* DEVICELINE_H_ */
