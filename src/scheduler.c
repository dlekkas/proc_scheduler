#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include <sys/wait.h>
#include <sys/types.h>

#include "proc-common.h"
#include "request.h"

/* Compile-time parameters. */
#define SCHED_TQ_SEC 2                /* time quantum */
#define TASK_NAME_SZ 60               /* maximum size for a task's name */
#define SHELL_EXECUTABLE_NAME "shell" /* executable for shell */

enum prio {
	LOW,   /* low priority */
	HIGH   /* high priority */
};


/* struct that specifies important characteristics
 * of a task (process) - a simplified version of
 * a process control block (PCB) */
struct task_struct{
	int task_no;                   /* process number */
	pid_t task_pid;                /* process PID */
	char exec_name[TASK_NAME_SZ];  /* name of the executable */
	enum prio priority;			   /* task's priority (LOW or HIGH) */
	struct task_struct *next;
	struct task_struct *prev;
};
typedef struct task_struct task_t;

task_t *list_tail, *list_head;
task_t *curr_task;

int running_tasks = 0;     /* number of tasks that are currently claiming the scheduler */
int created_tasks = 0;     /* number of tasks created (used to keep track of serial ID) */


void *safe_malloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL) {
		fprintf(stderr, "Out of memory, failed to allocate %zd bytes\n",
			size);
		exit(1);
	}

	return p;
}


/*
 * adds a task (process) struct to the process list, which is
 * a circularly double linked list
 */

void add_task(int number, pid_t pid, char name[]){
	/* creating and allocating memory for new process struct */
	task_t* new_task = safe_malloc(sizeof(task_t));
	/* updating the new process struct with characteristics of process */
	strncpy(new_task->exec_name, name, TASK_NAME_SZ);
	new_task->task_no = number;
	new_task->task_pid = pid;
	new_task->priority = LOW;
	new_task->prev = list_tail;

	if (list_tail != NULL)
		list_tail->next = new_task;
	else
		list_head = new_task;
	list_tail = new_task;

	/* the process that we add (last till now) should point to the 
	 * first process and the first process should have a pointer to
	 * to the last process in order to create a circularly double 
	 * linked list */
	list_tail->next = list_head;
	list_head->prev = list_tail;

	running_tasks++;
	created_tasks++;
	return;
}


/*
 * removes the process with a specific PID from the 
 * process list 
 */
void remove_task(pid_t pid){
	task_t* temp = curr_task;

	while(temp != NULL){
		if (temp->task_pid == pid)
			break;
		temp = temp->next;
	}

	if (temp->next != NULL)
		temp->next->prev = temp->prev;

	if (temp->prev != NULL)
		temp->prev->next = temp->next;

	free(temp);
	return;
}


/* 
 * Return the next task that should be scheduled:
 * 1) If there are high priority tasks then schedule
 *    the next available high priority task. 
 * 2) If there are not high priority tasks then
 *    schedule the next available task.
 */
task_t* next_task(){
	task_t* temp = curr_task->next;
	int i;
	for (i = 0; i <= running_tasks; i++){
		if (temp->priority == HIGH)
			return temp;
		temp = temp->next;
	}
	/* if there is not any HIGH priority task then 
	 * schedule the next available task */
	return curr_task->next;
}


/* Print a list of all tasks currently being scheduled.  */
static void
sched_print_tasks(void)
{
	task_t* temp = list_head;
	int i;
	for (i = 0; i < running_tasks; i++){
		if (temp->priority == LOW)
			printf("Process Serial ID: %d  - PID: %d  - Name: %s  - Priority: LOW ", 
				temp->task_no, temp->task_pid, temp->exec_name);
		else
			printf("Process Serial ID: %d  - PID: %d  - Name: %s  - Priority: HIGH ", 
				temp->task_no, temp->task_pid, temp->exec_name);
		if (temp->task_no == curr_task->task_no)
			printf("(currently running)");
		printf("\n");
		temp = temp->next;
	}
}

/* Send SIGKILL to a task determined by the value of its
 * scheduler-specific id.
 */
static int
sched_kill_task_by_id(int id)
{
	task_t* temp = list_head;
	int i;
	for (i = 0; i < running_tasks; i++){
		if (temp->task_no == id){
			kill(temp->task_pid, SIGKILL);
			return 0;
		}
		temp = temp->next;
	}
	printf("There is no running process with ID = %d\n", id);
	return 1;
}


/* Create a new task.  */
static void
sched_create_task(char *executable)
{

	pid_t pid;
	pid = fork();
	if (pid < 0){
		perror("main: fork");
		exit(1);
	}
	if (pid == 0){
		/* initialize the required arguments for the execve() function call */
		char *newargv[] = { executable, NULL };
		char *newenviron[] = { NULL };

		/* each process stops itself when created, and the 
		 * scheduler starts operating when all the processes
		 * are created and added to the process list */
		raise(SIGSTOP); // equivalent to kill(getpid(), SIGSTOP);

		execve(executable, newargv, newenviron);
		/* execve() only returns on error */
		perror("execve");
		exit(1);
	}
	/* following line asserts that list_tail is not NULL because
	 * of a possible free() of a process */
	list_tail = list_head->prev;
	/* add process to the process list */
	add_task(created_tasks, pid, executable);
}

static int
prioritize_task(int id, enum prio new_priority){
	task_t* temp = list_head;
	int i;
	for (i = 0; i < running_tasks; i++){
		if (temp->task_no == id){
			/*
			if (temp->priority == HIGH && new_priority == LOW)
				high_prio_tasks--;
			else if (temp->priority == LOW && new_priority == HIGH)
				high_prio_tasks++;
			*/
			temp->priority = new_priority;
			return 0;
		}
		temp = temp->next;
	}
	printf("There is no running process with ID = %d\n", id);
	return 1;
}


/* Process requests by the shell.  */
static int
process_request(struct request_struct *rq)
{
	switch (rq->request_no) {
		case REQ_PRINT_TASKS:
			sched_print_tasks();
			return 0;

		case REQ_KILL_TASK:
			return sched_kill_task_by_id(rq->task_arg);

		case REQ_EXEC_TASK:
			sched_create_task(rq->exec_task_arg);
			return 0;

		case REQ_HIGH_TASK:
			return prioritize_task(rq->task_arg, HIGH);
			
		case REQ_LOW_TASK:
			return prioritize_task(rq->task_arg, LOW);

		default:
			return -ENOSYS;
	}
}

/* 
 * SIGALRM handler
 */
static void
sigalrm_handler(int signum)
{
	/* stop the execution of the current running process */
	kill(curr_task->task_pid, SIGSTOP);
}

/* 
 * SIGCHLD handler
 */
static void
sigchld_handler(int signum)
{
	pid_t pid;
	int status;
	while(1){
		/* wait for any child (non-blocking) */
		pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
		if (pid < 0) {
			perror("waitpid");
			exit(1);
		}
		if (pid == 0)
			break;
		explain_wait_status(pid, status);

		/* if the process terminated then remove it from the process list 
		 * and make the current process the next available process to be
		 * scheduled */
		if (WIFEXITED(status) || WIFSIGNALED(status)){
			running_tasks--;
			/* if the terminated process is the process that is currently running 
			 * and is not the last process in the process list then schedule 
			 * the next available process from the process list */
			if ((curr_task->task_pid == pid) && (curr_task->next->task_pid != curr_task->task_pid)){
				remove_task(pid);
				curr_task = next_task();

			}
			/* terminate the scheduler if there are no more processes to be
		     * scheduled (if the pid of the current process is the same with
		 	 * the pid of the next process then this process is the last process
		 	 * because of the circularly double linked list implementation */
			else if (curr_task->next->task_pid == curr_task->task_pid){
				remove_task(pid);
				curr_task = NULL;
				printf("All processes to be scheduled terminated.\n");
				printf("Scheduler terminating...\n");
				exit(0);
			}
			/* if the terminated process is not the one that is currently running
			 * then remove the terminated process from the list and continue with 
			 * the process that is currently running */
			else
				remove_task(pid);
		}

		/* if the process stopped because its available time has ended then
		 * schedule the next available process in the process list */
		if (WIFSTOPPED(status))
			/* current process now becomes the next process in the process list */
			curr_task = next_task();

		/* continue the execution of the process that must be scheduled */
		kill(curr_task->task_pid, SIGCONT);
		/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
		alarm(SCHED_TQ_SEC);
	}
}

/* Disable delivery of SIGALRM and SIGCHLD. */
static void
signals_disable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL) < 0) {
		perror("signals_disable: sigprocmask");
		exit(1);
	}
}

/* Enable delivery of SIGALRM and SIGCHLD.  */
static void
signals_enable(void)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGALRM);
	sigaddset(&sigset, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) < 0) {
		perror("signals_enable: sigprocmask");
		exit(1);
	}
}


/* Install two signal handlers.
 * One for SIGCHLD, one for SIGALRM.
 * Make sure both signals are masked when one of them is running.
 */
static void
install_signal_handlers(void)
{
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGCHLD);
	sigaddset(&sigset, SIGALRM);
	sa.sa_mask = sigset;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction: sigchld");
		exit(1);
	}

	sa.sa_handler = sigalrm_handler;
	if (sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction: sigalrm");
		exit(1);
	}

	/*
	 * Ignore SIGPIPE, so that write()s to pipes
	 * with no reader do not result in us being killed,
	 * and write() returns EPIPE instead.
	 */
	if (signal(SIGPIPE, SIG_IGN) < 0) {
		perror("signal: sigpipe");
		exit(1);
	}
}

static void
do_shell(char *executable, int wfd, int rfd)
{
	char arg1[10], arg2[10];
	char *newargv[] = { executable, NULL, NULL, NULL };
	char *newenviron[] = { NULL };

	sprintf(arg1, "%05d", wfd);
	sprintf(arg2, "%05d", rfd);
	newargv[1] = arg1;
	newargv[2] = arg2;

	raise(SIGSTOP);
	execve(executable, newargv, newenviron);

	/* execve() only returns on error */
	perror("scheduler: child: execve");
	exit(1);
}

/* Create a new shell task.
 *
 * The shell gets special treatment:
 * two pipes are created for communication and passed
 * as command-line arguments to the executable.
 */
static pid_t
sched_create_shell(char *executable, int *request_fd, int *return_fd)
{
	pid_t p;
	int pfds_rq[2], pfds_ret[2];

	if (pipe(pfds_rq) < 0 || pipe(pfds_ret) < 0) {
		perror("pipe");
		exit(1);
	}

	p = fork();
	if (p < 0) {
		perror("scheduler: fork");
		exit(1);
	}

	if (p == 0) {
		/* Child */
		close(pfds_rq[0]);
		close(pfds_ret[1]);
		do_shell(executable, pfds_rq[1], pfds_ret[0]);
		assert(0);
	}
	/* Parent */
	close(pfds_rq[1]);
	close(pfds_ret[0]);
	*request_fd = pfds_rq[0];
	*return_fd = pfds_ret[1];
	return p;
}

static void
shell_request_loop(int request_fd, int return_fd)
{
	int ret;
	struct request_struct rq;

	/*
	 * Keep receiving requests from the shell.
	 */
	for (;;) {
		if (read(request_fd, &rq, sizeof(rq)) != sizeof(rq)) {
			perror("scheduler: read from shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}

		signals_disable();
		ret = process_request(&rq);
		signals_enable();

		if (write(return_fd, &ret, sizeof(ret)) != sizeof(ret)) {
			perror("scheduler: write to shell");
			fprintf(stderr, "Scheduler: giving up on shell request processing.\n");
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int nproc, i;
	curr_task = NULL;
	list_head = NULL;
	/* Two file descriptors for communication with the shell */
	static int request_fd, return_fd;

	/* Create the shell. */
	pid_t sched_pid;
	sched_pid = sched_create_shell(SHELL_EXECUTABLE_NAME, &request_fd, &return_fd);

	/* add scheduler to the process list */
	add_task(0, sched_pid, SHELL_EXECUTABLE_NAME);

	/*
	 * For each of argv[1] to argv[argc - 1],
	 * create a new child process, add it to the process list.
	 */
	 pid_t pid;
	 for (i = 1; i < argc; i++){
		pid = fork();
		if (pid < 0){
			perror("main: fork");
			exit(1);
		}
		if (pid == 0){
			/* initialize the required arguments for the execve() function call */
			char executable[TASK_NAME_SZ];
			strncpy(executable, argv[i], TASK_NAME_SZ);
			char *newargv[] = { executable, NULL };
			char *newenviron[] = { NULL };

			/* each process stops itself when created, and the 
			 * scheduler starts operating when all the processes
			 * are created and added to the process list */
			raise(SIGSTOP); // equivalent to kill(getpid(), SIGSTOP);

			execve(executable, newargv, newenviron);
			/* execve() only returns on error */
			perror("execve");
			exit(1);
		}
		/* add process to the process list */
		add_task(i, pid, argv[i]);
	}		

	nproc = argc;  /* number of proccesses goes here */

	/* Wait for all children to raise SIGSTOP before exec()ing. */
	wait_for_ready_children(nproc);

	/* Install SIGALRM and SIGCHLD handlers. */
	install_signal_handlers();

	if (nproc == 0) {
		fprintf(stderr, "Scheduler: No tasks. Exiting...\n");
		exit(1);
	}

	/* set the alarm -> SIGALRM signal will be sent after SHED_TQ_SEC seconds */
	alarm(SCHED_TQ_SEC);

	/* start the execution of the first process in the
	 * process list by sending a SIGCONT signal because
	 * all the processes to be scheduled have raised the
	 * SIGSTOP signal until all the processes to be 
	 * are created */
	curr_task = list_head;
	kill(curr_task->task_pid, SIGCONT);

	shell_request_loop(request_fd, return_fd);

	/* Now that the shell is gone, just loop forever
	 * until we exit from inside a signal handler.
	 */
	while (pause())
		;

	/* Unreachable */
	fprintf(stderr, "Internal error: Reached unreachable point\n");
	return 1;

}
