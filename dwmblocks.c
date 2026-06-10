#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <sys/signalfd.h>
#include <poll.h>
#define LENGTH(X) (sizeof(X) / sizeof (X[0]))
#define CMDLENGTH		50
#define STATUSLENGTH (LENGTH(blocks) * CMDLENGTH + 1)

typedef struct {
	char* icon;
	char* command;
	unsigned int interval;
	unsigned int signal;
} Block;
void sighandler();
void buttonhandler(int ssi_int);
void replace(char *str, char old, char new);
void remove_all(char *str, char to_remove);
void getcmds(int time);
void getsigcmds(int signal);
void setupsignals();
int getstatus(char *str, char *last);
int setupX();
void setroot();
void statusloop();
void termhandler(int signum);


#include "config.h"

static Display *dpy;
static int screen;
static Window root;
static char statusbar[LENGTH(blocks)][CMDLENGTH] = {0};
static char statusstr[2][STATUSLENGTH];
static int statusContinue = 1;
static int signalFD;
static int timerInterval = -1;
static void (*writestatus) () = setroot;

void replace(char *str, char old, char new)
{
	for(char * c = str; *c; c++)
		if(*c == old)
			*c = new;
}

// the previous function looked nice but unfortunately it didnt work if to_remove was in any position other than the last character
// theres probably still a better way of doing this
void remove_all(char *str, char to_remove) {
	char *read = str;
	char *write = str;
	while (*read) {
		if (*read != to_remove) {
			*write++ = *read;
		}
		++read;
	}
	*write = '\0';
}

int gcd(int a, int b)
{
	int temp;
	while (b > 0){
		temp = a % b;

		a = b;
		b = temp;
	}
	return a;
}


//opens process *cmd and stores output in *output
void getcmd(const Block *block, char *output)
{
	if (block->signal)
	{
		output[0] = block->signal;
		output++;
	}
    // leave room in the block's CMDLENGTH slot for the signal byte,
    // icon, delimiter, and terminator
    int readsize = CMDLENGTH - (block->signal ? 1 : 0)
                   - (int)strlen(block->icon) - (int)strlen(delim);
    if (readsize < 1)
        readsize = 1;
	int pipefd[2];
	if (pipe(pipefd) < 0)
		return;
	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return;
	}
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		// own process group so a timeout kill catches the whole pipeline,
		// not just the shell
		setpgid(0, 0);
		// undo the blocked signal mask inherited from the parent
		sigset_t mask;
		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);
		execl("/bin/sh", "sh", "-c", block->command, (char *)NULL);
		_exit(127);
	}
	close(pipefd[1]);
	setpgid(pid, pid); // mirror the child's setpgid to close the race

	// read until newline or every writer closes the pipe, but give up
	// after CMDTIMEOUT seconds so a stuck command (or a backgrounded
	// child holding the pipe open) can't freeze the whole bar
	char tmpstr[CMDLENGTH] = "";
	int len = 0;
	int timedout = 0;
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while (len < readsize - 1) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		int remaining = CMDTIMEOUT * 1000
		                - (int)((now.tv_sec - start.tv_sec) * 1000
		                        + (now.tv_nsec - start.tv_nsec) / 1000000);
		if (remaining <= 0) {
			timedout = 1;
			break;
		}
		struct pollfd p = {.fd = pipefd[0], .events = POLLIN};
		int r = poll(&p, 1, remaining);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (r == 0) {
			timedout = 1;
			break;
		}
		ssize_t n = read(pipefd[0], tmpstr + len, readsize - 1 - len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break; // every writer closed the pipe
		len += n;
		if (memchr(tmpstr + len - n, '\n', n))
			break;
	}
	close(pipefd[0]);
	if (timedout) {
		// kill the command's process group and keep the block's
		// previous text; SA_NOCLDWAIT reaps the child
		kill(-pid, SIGTERM);
		return;
	}
	// keep only the first line, like fgets did
	char *nl = strchr(tmpstr, '\n');
	if (nl)
		*nl = '\0';
	int i = strlen(block->icon);
	strcpy(output, block->icon);
    strcpy(output+i, tmpstr);
	remove_all(output, '\n');
	i = strlen(output);
    if ((i > 0 && block != &blocks[LENGTH(blocks) - 1])){
        strcat(output, delim);
    }
}

void getcmds(int time)
{
	const Block* current;
	for(int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if ((current->interval != 0 && time % current->interval == 0) || time == -1){
			getcmd(current,statusbar[i]);
        }
	}
}

void getsigcmds(int signal)
{
	const Block *current;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		current = blocks + i;
		if (current->signal == signal){
			getcmd(current,statusbar[i]);
        }
	}
}

void setupsignals()
{
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGALRM); // Timer events
	sigaddset(&signals, SIGUSR1); // Button events
	// All signals assigned to blocks
	for (size_t i = 0; i < LENGTH(blocks); i++)
		if (blocks[i].signal > 0)
			sigaddset(&signals, SIGRTMIN + blocks[i].signal);
	// Create signal file descriptor for pooling
	signalFD = signalfd(-1, &signals, SFD_CLOEXEC);
	// Block all real-time signals
	for (int i = SIGRTMIN; i <= SIGRTMAX; i++) sigaddset(&signals, i);
	sigprocmask(SIG_BLOCK, &signals, NULL);
	// Do not transform children into zombies
	struct sigaction sigchld_action = {
  		.sa_handler = SIG_DFL,
  		.sa_flags = SA_NOCLDWAIT
	};
	sigaction(SIGCHLD, &sigchld_action, NULL);

}

int getstatus(char *str, char *last)
{
	strcpy(last, str);
	str[0] = '\0';
    for(int i = 0; i < LENGTH(blocks); i++) {
		strcat(str, statusbar[i]);
        if (i == LENGTH(blocks) - 1)
            strcat(str, " ");
    }
	str[strlen(str)-1] = '\0';
	return strcmp(str, last);//0 if they are the same
}

int setupX()
{
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "dwmblocks: Failed to open display\n");
		return 0;
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	return 1;
}

void setroot()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only set root if text has changed.
		return;
	XStoreName(dpy, root, statusstr[0]);
	XFlush(dpy);
}

void pstdout()
{
	if (!getstatus(statusstr[0], statusstr[1]))//Only write out if text has changed.
		return;
	printf("%s\n",statusstr[0]);
	fflush(stdout);
}


void statusloop()
{
	setupsignals();
    // first figure out the default wait interval by finding the
    // greatest common denominator of the intervals
    for(int i = 0; i < LENGTH(blocks); i++){
        if(blocks[i].interval){
            timerInterval = gcd(blocks[i].interval, timerInterval);
        }
    }
    getcmds(-1);     // Fist time run all commands
    raise(SIGALRM);  // Schedule first timer event
    int ret;
    struct pollfd pfd[] = {{.fd = signalFD, .events = POLLIN}};
    while (statusContinue) {
        // Wait for new signal
        ret = poll(pfd, sizeof(pfd) / sizeof(pfd[0]), -1);
        if (ret < 0) {
            // Interrupted by a handled signal (e.g. SIGTERM); let the loop
            // condition decide whether to keep going
            if (errno == EINTR) continue;
            break;
        }
        if (!(pfd[0].revents & POLLIN)) break;
        sighandler(); // Handle signal
    }
}

void sighandler()
{
	static int time = 0;
	struct signalfd_siginfo si;
	int ret = read(signalFD, &si, sizeof(si));
	if (ret < 0) return;
	int signal = si.ssi_signo;
	switch (signal) {
		case SIGALRM:
			// Execute blocks and schedule the next timer event
			getcmds(time);
			alarm(timerInterval);
			time += timerInterval;
			break;
		case SIGUSR1:
			// Handle buttons
			buttonhandler(si.ssi_int);
			return;
		default:
			// Execute the block that has the given signal
			getsigcmds(signal - SIGRTMIN);
			break;
	}
	writestatus();
}

void buttonhandler(int ssi_int)
{
	char button[2] = {'0' + ssi_int & 0xff, '\0'};
	pid_t process_id = getpid();
	int sig = ssi_int >> 8;
	const Block *current = NULL;
	for (int i = 0; i < LENGTH(blocks); i++)
	{
		if (blocks[i].signal == sig) {
			current = blocks + i;
			break;
		}
	}
	if (!current)
		return;
	if (fork() == 0)
	{
		char shcmd[1024];
		sprintf(shcmd,"%s && kill -%d %d",current->command, current->signal+34,process_id);
		char *command[] = { "/bin/sh", "-c", shcmd, NULL };
		setenv("BLOCK_BUTTON", button, 1);
		setsid();
		// Undo the blocked signal mask inherited from the parent so the
		// clicked block's command runs with normal signal delivery
		sigset_t mask;
		sigemptyset(&mask);
		sigprocmask(SIG_SETMASK, &mask, NULL);
		execvp(command[0], command);
		exit(EXIT_SUCCESS);
	}
}


void termhandler(int signum)
{
	statusContinue = 0;
}

int main(int argc, char** argv)
{
	for(int i = 0; i < argc; i++)
	{
		if (!strcmp("-d",argv[i]))
			delim = argv[++i];
		else if(!strcmp("-p",argv[i]))
			writestatus = pstdout;
	}
	if (!setupX())
		return 1;
	signal(SIGTERM, termhandler);
	signal(SIGINT, termhandler);
	statusloop();
	close(signalFD);
	XCloseDisplay(dpy);
	return 0;
}
