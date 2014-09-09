/*
 * HWDevices.h
 *
 *  Created on: Jul 8, 2014
 *      Author: andreidenissov
 */

#ifndef HWDEVICES_H_
#define HWDEVICES_H_

extern int setHWDevicesMode(int mode);

extern int openHWDevices();

extern int closeHWDevices();

extern int hwDeviceOn(int devNo);

extern int hwDeviceOff(int devNo);

extern int hwDeviceAllOff();

#endif /* HWDEVICES_H_ */
