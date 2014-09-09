/*
 ============================================================================
 Name        : Serverok01.c
 Author      : ASD
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "DeviceLine.h"
#include "TaskQueue.h"

static char *MyDaemonName = "WaterLord-0.1";

static void make_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

#if 0
    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);
#endif

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
    {
        close (x);
    }

    /* Open the log file */
    openlog ("water-lord", LOG_PID, LOG_DAEMON);
}


#define MAX_CLIENTS (4)

#define SERVER_PORT (2345)

#define MSG_LENGTH (128)

typedef struct ClientDescRec
{
	int id;
	int conn; // connection socket
	pthread_t runner;
	int isActive;
	long tmLastAction;
} ClientDesc;

typedef struct MsgDescRec
{
    char data[MSG_LENGTH];
    int sock;
} MsgDesc;

static int readMsg(MsgDesc *msg)
{
    int pos = 0;
    int rem = MSG_LENGTH;
    int res;
    while (rem > 0) {
    	res = read(msg->sock, &msg->data[0], rem);
    	if (res < 0)
    		return res;
    	pos += res;
    	rem -= res;
    }
    return 0;
}

static int sendMsg(MsgDesc *msg)
{
    int pos = 0;
    int rem = MSG_LENGTH;
    int res;
    while (rem > 0) {
    	res = write(msg->sock, &msg->data[0], rem);
    	if (res < 0)
    		return res;
    	pos += res;
    	rem -= res;
    }
    return 0;
}

static void setClientFree(ClientDesc *client)
{
	close(client->conn);
	client->conn = (-1);
	client->isActive = 0;
	client->tmLastAction = 0;
}

// Signal handling
static void sigHandler(int sig)
{
	syslog (LOG_NOTICE, "GOT SIGNAL: >>>%d<<<\n\n", sig);
	if (sig == SIGINT || sig == SIGKILL || sig == SIGTERM || sig == SIGSTOP) {
	    hwDeviceAllOff();
	    syslog (LOG_NOTICE, "WaterLord terminated by signal %d\n.", sig);
	    closelog();
	    exit(0);
	}
}

static char showLineStatus(int stat)
{
	if (stat == STATUS_LINE_ACTIVE)
		return 'A';
	else if (stat == STATUS_LINE_WAITING)
		return 'W';
	else
		return 'I';
}

static void showStatus(const DeviceLineStatus *stat, char *buf)
{
	sprintf(buf, "[%c %d]", showLineStatus(stat->status), stat->time);
}

static void *oneClient(void *cl)
{
	int go = 1;
	ClientDesc *client = (ClientDesc *)cl;
	MsgDesc msg;
	int lNo, lTm;
	int res;
	char cmd;
	DeviceStatus devStat;

	msg.sock = client->conn;
	syslog (LOG_NOTICE, "CLIENT [%d] running...\n", client->id);

	res = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (res != 0) {
		syslog (LOG_NOTICE, "SET CANCELLATION STATE for CLIENT [%d] failed...\n", client->id);
	    return client;
	}

	while (go) {

		//syslog (LOG_NOTICE, "<<<[%d]\n", client->id);
		if (readMsg(&msg) < 0) {
			syslog (LOG_NOTICE, "ERROR reading message client:%d\n", client->id);
			break;
		}
		client->tmLastAction = getSystemTime();
		sscanf(&msg.data[0], "%c %d %d", &cmd, &lNo, &lTm);
        syslog (LOG_NOTICE, "got cmd [%d] %c %d => %d\n", client->id, cmd, lNo, lTm);

        switch (cmd) {
        case 'x': case 'X':
        	go = 0;
        	break;

        case 's': case 'S':
        default:
        	getStatus(&devStat);
        	break;

        case 'a': case 'A':
        	addTask(lNo, lTm, &devStat);
        	break;

        case 'c': case 'C':
        	cancelTask(lNo, &devStat);
        	break;

        case 'm': case 'M':
        	setHWDevicesMode(lNo);
        	break;
        }

        {
        	char ds0[16], ds1[16], ds2[16], ds3[16];
        	showStatus(&devStat[0], &ds0[0]);
        	showStatus(&devStat[1], &ds1[0]);
        	showStatus(&devStat[2], &ds2[0]);
        	showStatus(&devStat[3], &ds3[0]);
        	sprintf(&msg.data[0], "%s %s %s %s ", ds0, ds1, ds2, ds3);
        }

		if (sendMsg(&msg) < 0) {
			syslog (LOG_NOTICE, "ERROR sending message client:%d\n", client->id);
			break;
		}

	}
	setClientFree(client);
	syslog (LOG_NOTICE, "CLIENT [%d] exiting...\n", client->id);
	return client;
}

static ClientDesc clients[MAX_CLIENTS];

static ClientDesc *getFreeClient()
{
	int i;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i].isActive == 0)
			return &clients[i];
	}
	return NULL;
}

static int startClient(ClientDesc *client, int conn)
{
	client->conn = conn;

	int res = pthread_create(&client->runner, NULL, &oneClient, client);
	if (res == 0) {
		client->tmLastAction = getSystemTime();
		client->isActive = 1;
		return 0;
	}
	client->isActive = 0;
	return (-1);
}

static int ClientsWatchdogGo = 0;
static long oneClientInactiveTimeout = 15*60*1000;

static void *ClientsWatchdog(void *param)
{
	int i;
	void *thRes = NULL;

	while (ClientsWatchdogGo) {
		long tmNow = getSystemTime();
		for (i=0; i < MAX_CLIENTS; i++) {
			ClientDesc *cl = &clients[i];
			if ((cl->isActive != 0) && (tmNow - cl->tmLastAction > oneClientInactiveTimeout)) {
                pthread_cancel(cl->runner);
                pthread_join(cl->runner, &thRes);
                syslog (LOG_NOTICE, "Client [%d] cancelled by Watchdog.\n", cl->id);
                setClientFree(cl);
			}
		}
		sleep(10);
	}
}

static pthread_t clients_cleanup;

int main(void)
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	int i, res;
    int go = 1;
    void *thRes = NULL;

    make_daemon();

    ClientsWatchdogGo = 1;
    res = pthread_create(&clients_cleanup, NULL, &ClientsWatchdog, NULL);
    if (res != 0) {
    	syslog (LOG_NOTICE, "CANNOT start ClientsWatchdog [%d]\n", res);
    	exit(-3);
    }

	for (i=0; i < MAX_CLIENTS; i++)
		setClientFree(&clients[i]);

	syslog (LOG_NOTICE, "WaterLord started.");

	if (signal(SIGINT, &sigHandler) == SIG_ERR) {
	    syslog (LOG_NOTICE, "\ncan't set signal handler!\n");
	    exit(-3);
	}

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SERVER_PORT);

	startDevices(getDevicesUnit(0));
	initTasks();

	res = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (res < 0) {
    	syslog (LOG_NOTICE, "ERROR binding server port [%d]", SERVER_PORT);
    	exit(1);
    }

	res = listen(listenfd, 10);
    if (res < 0) {
    	syslog (LOG_NOTICE, "ERROR on listen server port [%d]", SERVER_PORT);
    	exit(1);
    }

	while (go)
	{
		syslog (LOG_NOTICE, "LISTENING...\n");
	    connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
	    if (connfd < 0) {
	    	syslog (LOG_NOTICE, "ERROR on accept server port [%d]", SERVER_PORT);
	    	sleep(5);
	    	continue;
	    }

	    ClientDesc *client = getFreeClient();
	    if (client == NULL) {
	    	close(connfd);
	    	syslog (LOG_NOTICE, "ERROR: too many connected clients! [%d]", MAX_CLIENTS);
	        sleep(5);
	       continue;
	    }

	    startClient(client, connfd);
	 }

    finishDevices(getDevicesUnit(0));
    ClientsWatchdogGo = 0;
    pthread_join(clients_cleanup, &thRes);

    syslog (LOG_NOTICE, "WaterLord terminated.");
    closelog();
	return EXIT_SUCCESS;
}

