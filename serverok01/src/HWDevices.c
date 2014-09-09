/*

 * HWDevices.c
 *
 *  Created on: Jul 8, 2014
 *      Author: andreidenissov
 */
#include "HWDevices.h"
#include <wiringPi.h>
#include <wiringShift.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#define NUM_STATIONS 16

#define SR_CLK_PIN  7
#define SR_NOE_PIN  0
#define SR_DAT_PIN  2
#define SR_LAT_PIN  3

static int hwMode = 1;

int setHWDevicesMode(int mode)
{
	int result = hwMode;

	hwMode = mode;
	return result;
}

int values[NUM_STATIONS];
extern void setShfitRegister(int values[]);

void disableShiftRegisterOutput()
{
  digitalWrite(SR_NOE_PIN, 1);
}

void enableShiftRegisterOutput()
{
  digitalWrite(SR_NOE_PIN, 0);
}

void setShiftRegister(int values[NUM_STATIONS])
{
	if (hwMode == 0)
		return;

    digitalWrite(SR_CLK_PIN, 0);
    digitalWrite(SR_LAT_PIN, 0);

    unsigned char s;
    for(s=0; s<NUM_STATIONS; s++) {
        digitalWrite(SR_CLK_PIN, 0);
        digitalWrite(SR_DAT_PIN, values[NUM_STATIONS-1-s]);
        digitalWrite(SR_CLK_PIN, 1);
  }
  digitalWrite(SR_LAT_PIN, 1);
}

void resetStations()
{
  unsigned char s;
  for(s=0; s<NUM_STATIONS; s++) {
    values[s] = 0;
  }
  setShiftRegister(values);
}

int openHWDevices()
{
	if (wiringPiSetup () == -1)
	    return (-1);

	printf ("wiringPi initialized.\n");

	pinMode (SR_CLK_PIN, OUTPUT);
	pinMode (SR_NOE_PIN, OUTPUT);
	disableShiftRegisterOutput();
	pinMode (SR_DAT_PIN, OUTPUT);
	pinMode (SR_LAT_PIN, OUTPUT);

	printf ("Shift register initialized.\n");

	resetStations();
	enableShiftRegisterOutput();

	return 0;
}

int closeHWDevices()
{
	resetStations();
	return 0;
}

int hwDeviceOn(int devNo)
{
	syslog (LOG_NOTICE, "DEVICE ON: %d {%d}\n", devNo, hwMode);
	values[devNo] = 1;
    setShiftRegister(values);
	return 0;
}

int hwDeviceOff(int devNo)
{
	syslog (LOG_NOTICE, "DEVICE OFF: %d {%d}\n", devNo, hwMode);
	values[devNo] = 0;
    setShiftRegister(values);
	return 0;
}

int hwDeviceAllOff()
{
	syslog (LOG_NOTICE, "ALL DEVICES OFF\n");
	resetStations();
	return 0;
}

