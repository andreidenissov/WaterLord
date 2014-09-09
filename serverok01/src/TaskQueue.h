#ifndef _TASK_QUEUE_H_
#define _TASK_QUEUE_H_

#include "DeviceLine.h"

extern int initTasks();
extern int addTask(int lineNo, int workTime, DeviceStatus *status);
extern int cancelTask(int lineNo, DeviceStatus *status);
extern int cancelAllTasks(DeviceStatus *status);
extern int getStatus(DeviceStatus *status);

extern long getSystemTime();

#endif

