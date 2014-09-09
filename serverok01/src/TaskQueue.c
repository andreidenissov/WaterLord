#include "TaskQueue.h"

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>

#define MAX_TASKS (32)

#define STATUS_TASK_READY     (0)
#define STATUS_TASK_WAITING   (1)
#define STATUS_TASK_ACTIVE    (2)
#define STATUS_TASK_CANCELLED (3)

#define TASK_START_DELAY_SECS  (5)

typedef struct TaskRec
{
	int lineNo;
	int status;
	long startTime;
	long endTime;
	int timeSec;
} Task;

typedef struct TaskQueueRec
{
	Task tasks[MAX_TASKS];
	int size;
	pthread_mutex_t lock;
	pthread_t executor;
	int doRun;
} TaskQueue;

static TaskQueue taskQueue;

static Task *getTaskForLine(int lineNo)
{
    Task *elem = &taskQueue.tasks[0];
    Task *end = &taskQueue.tasks[taskQueue.size];
    while (elem < end) {
        if (elem->lineNo == lineNo && elem->status != STATUS_TASK_CANCELLED) {
        	return elem;
        }
        elem++;
    }
    return NULL;
}

static void getTasksStatus(DeviceStatus *status)
{
	int i, stat, timeLeft;
	for (i = 0; i < MAX_REP_LINES; i++) {
		Task *task = getTaskForLine(i);
		if (task == NULL || task->status == STATUS_TASK_CANCELLED) {
			stat = STATUS_LINE_INACTIVE;
			timeLeft = 0;
		} else if (task->status == STATUS_TASK_ACTIVE) {
			long tmNow = task->endTime - getSystemTime();
			if (tmNow < 0)
				tmNow = 0;
			stat = STATUS_LINE_ACTIVE;
			timeLeft = (int)((tmNow + 999) / 1000);
		} else {
			stat = STATUS_LINE_WAITING;
			timeLeft = task->timeSec;
		}
		(*status)[i].status = stat;
		(*status)[i].time = timeLeft;
	}
}

static void popTask()
{
	int i;
	for (i = 1; i < taskQueue.size; i++)
		taskQueue.tasks[i-1] = taskQueue.tasks[i];
	taskQueue.size--;
}

static void *taskExecutor(void *data);

int initTasks()
{
	int res;

	taskQueue.size = 0;

	res = pthread_mutex_init(&taskQueue.lock, NULL);
	if (res != 0) {
		return res;
	}

	taskQueue.doRun = 1;
	res = pthread_create(&taskQueue.executor, NULL, &taskExecutor, &taskQueue);
	return res;
}

int addTask(int lineNo, int workTime, DeviceStatus *status)
{   int res = (-1);
	pthread_mutex_lock(&taskQueue.lock);
    if (taskQueue.size < MAX_TASKS) {
    	res = taskQueue.size;
    	taskQueue.tasks[res].lineNo = lineNo;
    	taskQueue.tasks[res].timeSec = workTime;
    	taskQueue.tasks[res].startTime = 0;
    	taskQueue.tasks[res].endTime = 0;
    	taskQueue.tasks[res].status = STATUS_TASK_READY;
    	taskQueue.size++;
    }
    getTasksStatus(status);
	pthread_mutex_unlock(&taskQueue.lock);
	return res;
}

int cancelTask(int lineNo, DeviceStatus *status)
{   int cnt = 0;

    pthread_mutex_lock(&taskQueue.lock);
    Task *elem = &taskQueue.tasks[0];
    Task *end = &taskQueue.tasks[taskQueue.size];
    while (elem < end) {
        if (elem->lineNo == lineNo) {
            if (elem->status == STATUS_TASK_ACTIVE) {
            	switchDeviceOFF(getDevicesUnit(0), elem->lineNo);
            }
            elem->status = STATUS_TASK_CANCELLED;
            cnt++;
        }
        elem++;
    }
    getTasksStatus(status);
    pthread_mutex_unlock(&taskQueue.lock);
    return cnt;
}

int cancelAllTasks(DeviceStatus *status)
{   int cnt = 0;

    pthread_mutex_lock(&taskQueue.lock);
    Task *elem = &taskQueue.tasks[0];
    Task *end = &taskQueue.tasks[taskQueue.size];
    while (elem < end) {
        if (elem->status == STATUS_TASK_ACTIVE) {
        	switchDeviceOFF(getDevicesUnit(0), elem->lineNo);
        }
        elem->status = STATUS_TASK_CANCELLED;
        cnt++;
        elem++;
    }
    getTasksStatus(status);
    pthread_mutex_unlock(&taskQueue.lock);
    return cnt;
}

int getStatus(DeviceStatus *status)
{
    pthread_mutex_lock(&taskQueue.lock);
    getTasksStatus(status);
    pthread_mutex_unlock(&taskQueue.lock);
	return 0;
}

long getSystemTime()
{
	struct timeval  tv;
	gettimeofday(&tv, NULL);

	return (long)(tv.tv_sec * 1000 + tv.tv_usec / 1000) ;
}

static void *taskExecutor(void *data)
{
	TaskQueue *queue = (TaskQueue *)data;
    int i, res;
    int sleepTime = 1;
    long tmNow;
    DevicesUnit devs = getDevicesUnit(0);

    syslog (LOG_NOTICE, "TASK EXECUTOR Started!\n");
    sleep(3);

	while (queue->doRun) {
		pthread_mutex_lock(&queue->lock);
		int go = 1;
		while (queue->size > 0 && go) {
            Task *task = &queue->tasks[0];
            go = 0;
            switch (task->status) {
            case STATUS_TASK_READY:
                tmNow = getSystemTime();
                task->startTime = tmNow + TASK_START_DELAY_SECS * 1000;
                task->endTime = task->startTime + task->timeSec * 1000;
                task->status = STATUS_TASK_WAITING;
                break;

            case STATUS_TASK_WAITING:
                tmNow = getSystemTime();
                if (tmNow > task->startTime) {
                	switchDeviceON(devs, task->lineNo, task->timeSec);
                	task->status = STATUS_TASK_ACTIVE;
                	go = 1;
                }
                break;

            case STATUS_TASK_ACTIVE:
            	tmNow = getSystemTime();
            	if (tmNow > task->endTime) {
            	    switchDeviceOFF(devs, task->lineNo);
            	    task->status = STATUS_TASK_CANCELLED;
            	    go = 1;
            	}
            	break;

            case STATUS_TASK_CANCELLED:
            	popTask();
            	go = 1;
            	break;

            default:
            	break;
            }
		}
		res = pthread_mutex_unlock(&queue->lock);
		if (res != 0) {
			syslog (LOG_NOTICE, "ERROR unlocking task queue [%d]\n", res);
		}
		sleep(sleepTime);
	}
	syslog (LOG_NOTICE, "TASK EXECUTOR Finished!\n");
    return NULL;
}


