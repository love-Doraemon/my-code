#define VERSION "0.62"

#ifdef WIN32
#include <windows.h>
#include <winsock.h>
#include "getopt.h"
#else
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <getopt.h>
#include <errno.h>
#define INVALID_SOCKET (-1)
#include <sys/time.h>
#endif /* WIN32 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
//#include "_public.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef WIN32
/* Windows sockets compatibility defines */
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
int closesocket(int s);

int closesocket(int s) {
  return close(s);
}
#define ioctlsocket ioctl
#define MAKEWORD(a, b)
#define WSAStartup(a, b) (0)
#define  WSACleanup()
#ifdef __MAC__
/* The constants for these are a little screwy in the prelinked
  MSL GUSI lib and we can't rebuild it, so roll with it */
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEAGAIN EAGAIN
#define WSAEINPROGRESS EINPROGRESS
#else
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEAGAIN EAGAIN
#define WSAEINPROGRESS EINPROGRESS
#endif /* __MAC__ */
#define WSAEINTR EINTR
#define SOCKET int
#define GetLastError() (errno)
typedef struct {
  int dummy;
} WSADATA;

void Sleep(long ms);

void Sleep(long ms)
{
  struct timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = ms * 1000;
  select(0, 0, 0, 0, &tv);
}
#else 
/* WIN32 doesn't really have WSAEAGAIN */
#ifndef WSAEAGAIN
#define WSAEAGAIN WSAEWOULDBLOCK
#endif
#endif /* WIN32 */

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef DEBUG
#define PERROR perror
#else
#define PERROR(x) 
#endif /* DEBUG */

/* We've got to get FIONBIO from somewhere. Try the Solaris location
        if it isn't defined yet by the above includes. */
#ifndef FIONBIO
#include <sys/filio.h>
#endif /* FIONBIO */

//#include "match.h"


extern int match(char *s, char *p);
extern int matchNoCase(char *s, char *p);
extern int matchBody(char *s, char *p, int nocase);


SOCKET *seFds = 0;
/* In network order, for network purposes */
struct in_addr *seLocalAddrs = 0;
unsigned short *seLocalPorts = 0;
/* In ASCII and local byte order, for logging purposes */
char **seFromHosts;
int *seFromPorts;
char **seToHosts;
int *seToPorts;

/* Offsets into list of allow and deny rules. Any rules
  prior to globalAllowRules and globalDenyRules are global rules. */

int *seAllowRules = 0;
int *seAllowRulesTotal = 0;
int globalAllowRules = 0;
int *seDenyRules = 0;
int *seDenyRulesTotal = 0;
int globalDenyRules = 0;

SOCKET *reFds = 0;
SOCKET *loFds = 0;
unsigned char *reAddresses = 0;
int *coInputRPos = 0;
int *coInputWPos = 0;
int *coOutputRPos = 0;
int *coOutputWPos = 0;
int *coClosed = 0;
int *coClosing = 0;
int *reClosed = 0;
int *loClosed = 0;
int *coBytesInput = 0;
int *coBytesOutput = 0;
int *coLog = 0;
int *coSe = 0;
char **coInput = 0;
char **coOutput = 0;
char **allowRules = 0;
char **denyRules = 0;
int *denyRulesFor = 0;
int seTotal = 0;
int coTotal = 0;
int allowRulesTotal = 0;
int denyRulesTotal = 0;
int maxfd = 0;
char *logFileName = 0;
char *pidLogFileName = 0;
int logFormatCommon = 0;
FILE *logFile = 0;

/* If 'newsize' bytes can be allocated, *data is set to point
  to them, the previous data is copied, and 1 is returned. 
  If 'size' bytes cannot be allocated, *data is UNCHANGED,
  and 0 is returned. */  

#define SAFE_REALLOC(x, y, z) safeRealloc((void **) (x), (y), (z))

int safeRealloc(void **data, int oldsize, int newsize);

/* 
  se: (se)rver sockets
  re: (re)mote sockets
  lo: (lo)cal sockets (being redirected to)
  co: connections
*/

#define bufferSpace 1024

void readConfiguration();

/* Signal handlers */
void plumber(int s);
void hup(int s);
void term(int s);

void initArrays(void);
void RegisterPID(void);

void selectLoop(void);

//void log(int i, int coSe, int result);

int getAddress(char *host, struct in_addr *iaddr);

char *logMessages[] = {
  "done-local-closed",
  "done-remote-closed",
  "accept-failed -",
  0,
  "local-socket-failed -",
  0,
  "local-bind-failed -",
  0,
  "local-connect-failed -",
  0,
  "not-allowed",
  0,
  "denied",
  0
};
  
#define logDone 0
#define logAcceptFailed 2
#define logLocalSocketFailed 4
#define logLocalBindFailed 6
#define logLocalConnectFailed 8
#define logNotAllowed 10
#define logDenied 12

#define logLocalClosedFirst 0
#define logRemoteClosedFirst 1

/* Option parsing */

typedef struct _rinetd_options RinetdOptions;
struct _rinetd_options
{
  char *conf_file;
};

RinetdOptions options = {
  "/etc/rinetd.conf"
};

int readArgs (int argc,
  char **argv,
  RinetdOptions *options);

int main(int argc, char *argv[])
{
  WSADATA wsaData;
  memset(&wsaData,0,sizeof(WSADATA));

  int result = WSAStartup(MAKEWORD(1, 1), &wsaData);

  if (result != 0) 
  {
    fprintf(stderr,"Your computer was not connected "
                   "to the Internet at the time that "
                   "this program was launched, or you "
                   "do not have a 32-bit "
                   "connection to the Internet.");
    exit(1);
  }

  readArgs(argc, argv, &options);

  signal(SIGPIPE, plumber);
  signal(SIGHUP, hup);
  signal(SIGINT, term);
  signal(SIGTERM, term);

  initArrays();

  readConfiguration();

  RegisterPID();

  selectLoop();

  return 0;
}

int getConfLine(FILE *in, char *line, int space, int *lnum);

int patternBad(char *pattern);

void readConfiguration(void)
{
  FILE *in;
  char line[16384];
  int lnum = 0;
  int i;
  int ai;
  int di;
  if (seFds) {
    /* Close existing server sockets. */
    for (i = 0; (i < seTotal); i++) {
      if (seFds[i] != -1) {
        closesocket(seFds[i]);
        free(seFromHosts[i]);
        free(seToHosts[i]);
      }
    }  
    /* Free memory associated with previous set. */
    free(seFds);
    free(seLocalAddrs);
    free(seLocalPorts);
    free(seFromHosts);
    free(seFromPorts);
    free(seToHosts);
    free(seToPorts);
    free(seAllowRules);
    free(seDenyRules);
    free(seAllowRulesTotal);
    free(seDenyRulesTotal);
  }
  seTotal = 0;
  if (allowRules) {
    /* Forget existing allow rules. */
    for (i = 0; (i < allowRulesTotal); i++) {
      free(allowRules[i]);
    }  
    /* Free memory associated with previous set. */
    free(allowRules);
    globalAllowRules = 0;
  }
  allowRulesTotal = 0;
  if (denyRules) {
    /* Forget existing deny rules. */
    for (i = 0; (i < denyRulesTotal); i++) {
      free(denyRules[i]);
    }  
    /* Free memory associated with previous set. */
    free(denyRules);
    globalDenyRules = 0;
  }
  denyRulesTotal = 0;
/*
  if (logFileName) {
    free(logFileName);
    logFileName = 0;
  }
*/
  if (pidLogFileName) {
    free(pidLogFileName);
    pidLogFileName = 0;
  }
  /* 1. Count the non-comment lines of each type and
    allocate space for the data. */
  in = fopen(options.conf_file, "r");
  if (!in) {
    fprintf(stderr, "rinetd: can't open %s\n", options.conf_file);
    exit(1);
  }
  while (1) {
    char *t = 0;
    if (!getConfLine(in, line, sizeof(line), &lnum)) {
      break;
    }
    t = strtok(line, " \t\r\n");
    if (!strcmp(t, "logfile")) {   
      continue;
    } else if (!strcmp(t, "pidlogfile")) {   
      continue;
    } else if (!strcmp(t, "logcommon")) {
      continue;
    } else if (!strcmp(t, "allow")) {
      allowRulesTotal++;
    } else if (!strcmp(t, "deny")) {    
      denyRulesTotal++;
    } else {  
      /* A regular forwarding rule */
      seTotal++;  
    }
  }  
  fclose(in);
  seFds = (SOCKET *) malloc(sizeof(int) * seTotal);  
  if (!seFds) {
    goto lowMemory;
  }
  seLocalAddrs = (struct in_addr *) malloc(sizeof(struct in_addr) *
    seTotal);  
  if (!seLocalAddrs) {
    goto lowMemory;
  }
  seLocalPorts = (unsigned short *) 
    malloc(sizeof(unsigned short) * seTotal);  
  if (!seLocalPorts) {
    goto lowMemory;
  }
  seFromHosts = (char **)
    malloc(sizeof(char *) * seTotal);
  if (!seFromHosts) {
    goto lowMemory;
  }
  seFromPorts = (int *)
    malloc(sizeof(int) * seTotal);  
  if (!seFromPorts) {
    goto lowMemory;
  }
  seToHosts = (char **)
    malloc(sizeof(char *) * seTotal);
  if (!seToHosts) {
    goto lowMemory;
  }
  seToPorts = (int *)
    malloc(sizeof(int) * seTotal);  
  if (!seToPorts) {
    goto lowMemory;
  }
  allowRules = (char **)
    malloc(sizeof(char *) * allowRulesTotal);
  if (!allowRules) {
    goto lowMemory;
  }
  denyRules = (char **)
    malloc(sizeof(char *) * denyRulesTotal);
  if (!denyRules) {
    goto lowMemory;
  }
  seAllowRules = (int *)
    malloc(sizeof(int) * seTotal);
  if (!seAllowRules) {
    goto lowMemory;
  }
  seAllowRulesTotal = (int *)
    malloc(sizeof(int) * seTotal);
  if (!seAllowRulesTotal) {
    goto lowMemory;
  }
  seDenyRules = (int *)
    malloc(sizeof(int) * seTotal);
  if (!seDenyRules) {
    goto lowMemory;
  }
  seDenyRulesTotal = (int *)
    malloc(sizeof(int) * seTotal);
  if (!seDenyRulesTotal) {
    goto lowMemory;
  }
  /* 2. Make a second pass to configure them. */  
  i = 0;
  ai = 0;
  di = 0;
  lnum = 0;
  in = fopen(options.conf_file, "r");
  if (!in) {
    goto lowMemory;
  }
  if (seTotal > 0) {
    seAllowRulesTotal[i] = 0;
    seDenyRulesTotal[i] = 0;
  }
  while (1) {
    char *bindAddress;
    unsigned short bindPort;
    char *connectAddress;
    char *bindPortS;
    char *connectPortS;
    unsigned short connectPort;
    struct in_addr iaddr;
    struct sockaddr_in saddr;
    struct servent *service;
    int j;
    if (!getConfLine(in, line, sizeof(line), &lnum)) {
      break;
    }
    bindAddress = strtok(line, " \t\r\n");
    if (!bindAddress) {
      fprintf(stderr, "rinetd: no bind address specified "
        "on line %d.\n", lnum);  
      continue;
    }  
    if (!strcmp(bindAddress, "allow")) {
      char *pattern = strtok(0, " \t\r\n");
      if (!pattern) {
        fprintf(stderr, "rinetd: nothing to allow "
          "specified on line %d.\n", lnum);  
        continue;
      }  
      if (patternBad(pattern)) {
        fprintf(stderr, "rinetd: illegal allow or "
          "deny pattern. Only digits, ., and\n"
          "the ? and * wild cards are allowed. "
          "For performance reasons, rinetd\n"
          "does not look up complete "
          "host names.\n");
        continue;
      }
      
      allowRules[ai] = (char*)malloc(strlen(pattern) + 1);
      if (!allowRules[ai]) {
        goto lowMemory;
      }
      strcpy(allowRules[ai], pattern);
      if (i > 0) {
        if (seAllowRulesTotal[i - 1] == 0) {
          seAllowRules[i - 1] = ai;
        }
        seAllowRulesTotal[i - 1]++;
      } else {
        globalAllowRules++;
      }
      ai++;
    } else if (!strcmp(bindAddress, "deny")) {
      char *pattern = strtok(0, " \t\r\n");
      if (!pattern) {
        fprintf(stderr, "rinetd: nothing to deny "
          "specified on line %d.\n", lnum);  
        continue;
      }  
      denyRules[di] = (char*)malloc(strlen(pattern) + 1);
      if (!denyRules[di]) {
        goto lowMemory;
      }
      strcpy(denyRules[di], pattern);
      if (i > 0) {
        if (seDenyRulesTotal[i - 1] == 0) {
          seDenyRules[i - 1] = di;
        }
        seDenyRulesTotal[i - 1]++;
      } else {
        globalDenyRules++;
      }
      di++;
    } else if (!strcmp(bindAddress, "logfile")) {
      char *nt = strtok(0, " \t\r\n");
      if (!nt) {
        fprintf(stderr, "rinetd: no log file name "
          "specified on line %d.\n", lnum);  
        continue;
      }  
      logFileName = (char*)malloc(strlen(nt) + 1);
      if (!logFileName) {
        goto lowMemory;
      }
      strcpy(logFileName, nt);
    } else if (!strcmp(bindAddress, "pidlogfile")) {
      char *nt = strtok(0, " \t\r\n");
      if (!nt) {
        fprintf(stderr, "rinetd: no PID log file name "
          "specified on line %d.\n", lnum);  
        continue;
      }  
      pidLogFileName = (char*)malloc(strlen(nt) + 1);
      if (!pidLogFileName) {
        goto lowMemory;
      }
      strcpy(pidLogFileName, nt);
    } else if (!strcmp(bindAddress, "logcommon")) {
      logFormatCommon = 1;
    } else {
      /* A regular forwarding rule. */
      bindPortS = strtok(0, " \t\r\n");
      if (!bindPortS) {
        fprintf(stderr, "rinetd: no bind port "
          "specified on line %d.\n", lnum);  
        continue;
      }
      service = getservbyname(bindPortS, "tcp");  
      if (service) {
        bindPort = ntohs(service->s_port);
      } else {
        bindPort = atoi(bindPortS);
      }
      if ((bindPort == 0) || (bindPort >= 65535)) {
        fprintf(stderr, "rinetd: bind port missing "
          "or out of range on line %d.\n", lnum);
        continue;
      }
      connectAddress = strtok(0, " \t\r\n");
      if (!connectAddress) {
        fprintf(stderr, "rinetd: no connect address "
          "specified on line %d.\n", lnum);  
        continue;
      }  
      connectPortS = strtok(0, " \t\r\n");
      if (!connectPortS) {
        fprintf(stderr, "rinetd: no connect port "
          "specified on line %d.\n", lnum);  
        continue;
      }
      service = getservbyname(connectPortS, "tcp");  
      if (service) {
        connectPort = ntohs(service->s_port);
      } else {
        connectPort = atoi(connectPortS);
      }
      if ((connectPort == 0) || (connectPort >= 65535)) {
        fprintf(stderr, "rinetd: bind port missing "
          "or out of range on line %d.\n", lnum);
        continue;
      }
      /* Turn all of this stuff into reasonable addresses */
      if (!getAddress(bindAddress, &iaddr)) {
        fprintf(stderr, "rinetd: host %s could not be "
          "resolved on line %d.\n", 
          bindAddress, lnum);
        continue;
      }  
      /* Make a server socket */
      seFds[i] = socket(PF_INET, SOCK_STREAM, 0);
      if (seFds[i] == INVALID_SOCKET) {
        fprintf(stderr, "rinetd: couldn't create "
          "server socket!\n");
        seFds[i] = -1;
        continue;
      }
#ifndef WIN32
      if (seFds[i] > maxfd) {
        maxfd = seFds[i];
      }
#endif
      saddr.sin_family = AF_INET;
      memcpy(&saddr.sin_addr, &iaddr, sizeof(iaddr));
      saddr.sin_port = htons(bindPort);
      j = 1;
      setsockopt(seFds[i], SOL_SOCKET, SO_REUSEADDR,
        (const char *) &j, sizeof(j));
      if (bind(seFds[i], (struct sockaddr *) 
        &saddr, sizeof(saddr)) == SOCKET_ERROR) 
      {
        /* Warn -- don't exit. */
        fprintf(stderr, "rinetd: couldn't bind to "
          "address %s port %d\n", 
          bindAddress, bindPort);  
        closesocket(seFds[i]);
        seFds[i] = INVALID_SOCKET;
        continue;
      }
      if (listen(seFds[i], 5) == SOCKET_ERROR) {
        /* Warn -- don't exit. */
        fprintf(stderr, "rinetd: couldn't listen to "
          "address %s port %d\n",
          bindAddress, bindPort);  
        closesocket(seFds[i]);
        seFds[i] = INVALID_SOCKET;
        continue;
      }
      ioctlsocket(seFds[i], FIONBIO, &j);
      if (!getAddress(connectAddress, &iaddr)) {
        /* Warn -- don't exit. */
        fprintf(stderr, "rinetd: host %s could not be "
          "resolved on line %d.\n", 
          bindAddress, lnum);
        closesocket(seFds[i]);
        seFds[i] = INVALID_SOCKET;
        continue;
      }  
      seLocalAddrs[i] = iaddr;
      seLocalPorts[i] = htons(connectPort);
      seFromHosts[i] = (char*)malloc(strlen(bindAddress) + 1);
      if (!seFromHosts[i]) {
        goto lowMemory;
      }
      strcpy(seFromHosts[i], bindAddress);
      seFromPorts[i] = bindPort;
      seToHosts[i] = (char*)malloc(strlen(connectAddress) + 1);
      if (!seToHosts[i]) {
        goto lowMemory;
      }
      strcpy(seToHosts[i], connectAddress);
      seToPorts[i] = connectPort;
      i++;
      if (i < seTotal) {
        seAllowRulesTotal[i] = 0;
        seDenyRulesTotal[i] = 0;
      }
    }
  }
  /* Open the log file */
/*
  if (logFile) {
    fclose(logFile);
    logFile = 0;
  }
  if (logFileName) {
    logFile = fopen(logFileName, "a");
    if (!logFile) {
      fprintf(stderr, "rinetd: could not open %s to append.\n",
        logFileName);
    }
  }
*/
  return;
lowMemory:
  fprintf(stderr, "rinetd: not enough memory to start rinetd.\n");
  exit(1);
}

int getConfLine(FILE *in, char *line, int space, int *lnum)
{
  char *p;
  while (1) {
    if (!fgets(line, space, in)) {
      return 0;
    }
    p = line;
    while (isspace(*p)) {
      p++;
    }
    if (!(*p)) {
      /* Blank lines are OK */
      continue;
    }
    if (*p == '#') {
      /* Comment lines are also OK */
      continue;
    }
    (*lnum)++;
    return 1;
  }
}

void initArrays(void)
{
  int j;
  coTotal = 64;
  reFds = (SOCKET *) malloc(sizeof(int) * coTotal);
  loFds = (SOCKET *) malloc(sizeof(int) * coTotal);
  coInputRPos = (int *) malloc(sizeof(int) * coTotal);
  coInputWPos = (int *) malloc(sizeof(int) * coTotal);
  coOutputRPos = (int *) malloc(sizeof(int) * coTotal);
  coOutputWPos = (int *) malloc(sizeof(int) * coTotal);
  coClosed = (int *) malloc(sizeof(int) * coTotal);
  coClosing = (int *) malloc(sizeof(int) * coTotal);
  reClosed = (int *) malloc(sizeof(int) * coTotal);
  loClosed = (int *) malloc(sizeof(int) * coTotal);
  coInput = (char **) malloc(sizeof(char *) * coTotal);
  coOutput = (char **) malloc(sizeof(char *) * coTotal);
  coBytesInput = (int *) malloc(sizeof(int) * coTotal);
  coBytesOutput = (int *) malloc(sizeof(int) * coTotal);
  reAddresses = (unsigned char *) malloc(coTotal * 4);
  coLog = (int *) malloc(sizeof(int) * coTotal);
  coSe = (int *) malloc(sizeof(int) * coTotal);
  if ((!reFds) || (!loFds) || (!coInputRPos) || (!coInputWPos) ||
    (!coOutputRPos) || (!coOutputWPos) || 
    (!coClosed) || (!coClosing) ||
    (!reClosed) || (!loClosed) ||
    (!coInput) || (!coOutput) ||
    (!coBytesInput) || (!coBytesOutput) || 
    (!coLog) || (!coSe) || (!reAddresses)) 
  {
    fprintf(stderr, "rinetd: not enough memory to start rinetd.\n");
    exit(1);
  }  
  for (j = 0; (j < coTotal); j++) {
    coClosed[j] = 1;
    coInput[j] = (char *) malloc(sizeof(char) * bufferSpace);
    coOutput[j] = (char *) malloc(sizeof(char) * bufferSpace);
    if ((!coInput[j]) || (!coOutput[j])) {
      fprintf(stderr, "rinetd: not enough memory to start "
        "rinetd.\n");
      exit(1);
    }
  }
}

void selectPass(void);

void selectLoop(void) 
{
  while (1) 
  {
    selectPass();
  }
}

void handleRemoteWrite(int i);
void handleRemoteRead(int i);
void handleLocalWrite(int i);
void handleLocalRead(int i);
void handleCloseFromLocal(int i);
void handleCloseFromRemote(int i);
void handleAccept(int i);
void openLocalFd(int se, int i);
int getAddress(char *host, struct in_addr *iaddr);

void selectPass(void) {
  int i;
  fd_set readfds, writefds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  /* Server sockets */
  for (i = 0; (i < seTotal); i++) {
    if (seFds[i] != INVALID_SOCKET) {
      FD_SET(seFds[i], &readfds);
    }
  }
  /* Connection sockets */
  for (i = 0; (i < coTotal); i++) {
    if (coClosed[i]) {
      continue;
    }
    if (coClosing[i]) {
      if (!reClosed[i]) {
        FD_SET(reFds[i], &writefds);
      }  
      if (!loClosed[i]) {
        FD_SET(loFds[i], &writefds);
      }  
    }
    /* Get more input if we have room for it */
    if ((!reClosed[i]) && (coInputRPos[i] < bufferSpace)) {
      FD_SET(reFds[i], &readfds);
    }
    /* Send more output if we have any */  
    if ((!reClosed[i]) && (coOutputWPos[i] < coOutputRPos[i])) {
      FD_SET(reFds[i], &writefds);
    }  
    /* Accept more output from the local 
      server if there's room */
    if ((!loClosed[i]) && (coOutputRPos[i] < bufferSpace)) {
      FD_SET(loFds[i], &readfds);
    }
    /* Send more input to the local server 
      if we have any */
    if ((!loClosed[i]) && (coInputWPos[i] < coInputRPos[i])) {
      FD_SET(loFds[i], &writefds);
    }  
  }
  select(maxfd + 1, &readfds, &writefds, 0, 0);
  for (i = 0; (i < seTotal); i++) {
    if (seFds[i] != -1) {
      if (FD_ISSET(seFds[i], &readfds)) {
        handleAccept(i);
      }
    }
  }
  for (i = 0; (i < coTotal); i++) {
    if (coClosed[i]) {
      continue;
    }
    if (!reClosed[i]) {
      if (FD_ISSET(reFds[i], &readfds)) {
        handleRemoteRead(i);
      }
    }
    if (!reClosed[i]) {
      if (FD_ISSET(reFds[i], &writefds)) {
        handleRemoteWrite(i);
      }
    }
    if (!loClosed[i]) {
      if (FD_ISSET(loFds[i], &readfds)) {
        handleLocalRead(i);
      }
    }
    if (!loClosed[i]) {
      if (FD_ISSET(loFds[i], &writefds)) {
        handleLocalWrite(i);
      }
    }
    if (loClosed[i] && reClosed[i]) {
      coClosed[i] = 1;
    }  
  }
}

void handleRemoteRead(int i)
{
  int got;
  if (bufferSpace == coInputRPos[i]) {
    return;
  }
  got = recv(reFds[i], coInput[i] + coInputRPos[i],
    bufferSpace - coInputRPos[i], 0);
  if (got == 0) {
    /* Prepare for closing */
    handleCloseFromRemote(i);
    return;
  }
  if (got < 0) {
    if (GetLastError() == WSAEWOULDBLOCK) {
      return;
    }
    if (GetLastError() == WSAEINPROGRESS) {
      return;
    }
    handleCloseFromRemote(i);
    return;
  }
  coBytesInput[i] += got;
  coInputRPos[i] += got;
}

void handleRemoteWrite(int i)
{
  int got;
  if (coClosing[i] && (coOutputWPos[i] == coOutputRPos[i])) {
    reClosed[i] = 1;
    coClosed[i] = 1;
    PERROR("rinetd: local closed and no more output");
    //log(i, coSe[i], logDone | coLog[i]); 
    closesocket(reFds[i]);
    return;
  }
  got = send(reFds[i], coOutput[i] + coOutputWPos[i],
    coOutputRPos[i] - coOutputWPos[i], 0);
  if (got < 0) {
    if (GetLastError() == WSAEWOULDBLOCK) {
      return;
    }
    if (GetLastError() == WSAEINPROGRESS) {
      return;
    }
    handleCloseFromRemote(i);
    return;
  }
  coOutputWPos[i] += got;
  if (coOutputWPos[i] == coOutputRPos[i]) {
    coOutputWPos[i] = 0;
    coOutputRPos[i] = 0;
  }
  coBytesOutput[i] += got;
}

void handleLocalRead(int i)
{
  int got;
  if (bufferSpace == coOutputRPos[i]) {
    return;
  }
  got = recv(loFds[i], coOutput[i] + coOutputRPos[i], 
    bufferSpace - coOutputRPos[i], 0);
  if (got == 0) {
    handleCloseFromLocal(i);
    return;
  }
  if (got < 0) {
    if (GetLastError() == WSAEWOULDBLOCK) {
      return;
    }
    if (GetLastError() == WSAEINPROGRESS) {
      return;
    }
    handleCloseFromLocal(i);
    return;
  }
  coOutputRPos[i] += got;
}

void handleLocalWrite(int i)
{
  int got;
  if (coClosing[i] && (coInputWPos[i] == coInputRPos[i])) {
    loClosed[i] = 1;
    coClosed[i] = 1;
    PERROR("remote closed and no more input");
    //log(i, coSe[i], logDone | coLog[i]);
    closesocket(loFds[i]);
    return;
  }
  got = send(loFds[i], coInput[i] + coInputWPos[i],
    coInputRPos[i] - coInputWPos[i], 0);
  if (got < 0) {
    if (GetLastError() == WSAEWOULDBLOCK) {
      return;
    }
    if (GetLastError() == WSAEINPROGRESS) {
      return;
    }
    handleCloseFromLocal(i);
    return;
  }
  coInputWPos[i] += got;
  if (coInputWPos[i] == coInputRPos[i]) {
    coInputWPos[i] = 0;
    coInputRPos[i] = 0;
  }
}

void handleCloseFromLocal(int i)
{
  int arg;
  coClosing[i] = 1;
  /* The local end fizzled out, so make sure
    we're all done with that */
  PERROR("close from local");
  closesocket(loFds[i]);
  loClosed[i] = 1;
  if (!reClosed[i]) {
#ifndef LINUX 
#ifndef WIN32
    /* Now set up the remote end for a polite closing */

    /* Request a low-water mark equal to the entire
      output buffer, so the next write notification
      tells us for sure that we can close the socket. */
    arg = 1024;
    setsockopt(reFds[i], SOL_SOCKET, SO_SNDLOWAT, 
      &arg, sizeof(arg));  
#endif /* WIN32 */
#endif /* LINUX */
    coLog[i] = logLocalClosedFirst;
  } 
}

void handleCloseFromRemote(int i)
{
  int arg;
  coClosing[i] = 1;
  /* The remote end fizzled out, so make sure
    we're all done with that */
  PERROR("close from remote");
  closesocket(reFds[i]);
  reClosed[i] = 1;
  if (!loClosed[i]) {
#ifndef LINUX
#ifndef WIN32
    /* Now set up the local end for a polite closing */

    /* Request a low-water mark equal to the entire
      output buffer, so the next write notification
      tells us for sure that we can close the socket. */
    arg = 1024;
    setsockopt(loFds[i], SOL_SOCKET, SO_SNDLOWAT, 
      &arg, sizeof(arg));  
#endif /* WIN32 */
#endif /* LINUX */
    loClosed[i] = 0;
    coLog[i] = logRemoteClosedFirst;
  }
}

void refuse(int index, int logCode);

void handleAccept(int i)
{
  struct sockaddr addr;
  struct sockaddr_in *sin;
  unsigned char address[4];
  char addressText[64];
  int j;
  socklen_t addrlen;
  int index = -1;
  int o;
  SOCKET nfd;
  addrlen = sizeof(addr);
  nfd = accept(seFds[i], &addr, &addrlen);
  if (nfd == INVALID_SOCKET) {
    //log(-1, i, logAcceptFailed);
    return;
  }
#ifndef WIN32
  if (nfd > maxfd) {
    maxfd = nfd;
  }
#endif /* WIN32 */
  j = 1;
  ioctlsocket(nfd, FIONBIO, &j);
  j = 0;
#ifndef WIN32
  setsockopt(nfd, SOL_SOCKET, SO_LINGER, &j, sizeof(j));
#endif
  for (j = 0; (j < coTotal); j++) {  
    if (coClosed[j]) {
      index = j;
      break;
    }
  }
  if (index == -1) {
    o = coTotal;
    coTotal *= 2;
    if (!SAFE_REALLOC(&reFds, sizeof(int) * o,
      sizeof(SOCKET) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&loFds, sizeof(int) * o,
      sizeof(SOCKET) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coInputRPos, 
      sizeof(int) * o, sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coInputWPos, 
      sizeof(int) * o, sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coOutputRPos, 
      sizeof(int) * o, sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coOutputWPos, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coClosed, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coClosing, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&reClosed, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&loClosed, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coLog, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coSe, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coBytesInput, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&reAddresses, 4 * o, 
      4 * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coBytesOutput, sizeof(int) * o, 
      sizeof(int) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coInput, sizeof(char *) * o,
      sizeof(char *) * coTotal)) 
    {
      goto shortage;
    }
    if (!SAFE_REALLOC(&coOutput, sizeof(char *) * o,
      sizeof(char *) * coTotal)) 
    {
      goto shortage;
    }
    for (j = o; (j < coTotal); j++) {
      coClosed[j] = 1;
      coInput[j] = (char *) 
        malloc(sizeof(char) * bufferSpace);
      if (!coInput[j]) {
        int k;
        for (k = o; (k < j); k++) {
          free(coInput[k]);
          free(coOutput[k]);
        }
        goto shortage;
      }
      coOutput[j] = (char *) 
        malloc(sizeof(char) * bufferSpace);
      if (!coOutput[j]) {
        int k;
        free(coInput[j]);
        for (k = o; (k < j); k++) {
          free(coInput[k]);
          free(coOutput[k]);
        }
        goto shortage;
      }
    }
    index = o;
  }
  coInputRPos[index] = 0;
  coInputWPos[index] = 0;
  coOutputRPos[index] = 0;
  coOutputWPos[index] = 0;
  coClosed[index] = 0;
  coClosing[index] = 0;
  reClosed[index] = 0;
  loClosed[index] = 0;
  reFds[index] = nfd;
  coBytesInput[index] = 0;
  coBytesOutput[index] = 0;
  coLog[index] = 0;
  coSe[index] = i;
  sin = (struct sockaddr_in *) &addr;
  memcpy(address, &(sin->sin_addr.s_addr), 4);
  memcpy(reAddresses + index * 4, address, 4);
  /* Now, do we want to accept this connection? 
    Format it for comparison to a pattern. */
  sprintf(addressText, "%d.%d.%d.%d",
    address[0], address[1], address[2], address[3]);
  /* 1. Check global allow rules. If there are no
    global allow rules, it's presumed OK at
    this step. If there are any, and it doesn't
    match at least one, kick it out. */
  if (globalAllowRules) {
    int good = 0;
    for (j = 0; (j < globalAllowRules); j++) {
      if (match(addressText, allowRules[j])) {
        good = 1;
        break;
      }
    }
    if (!good) {
      refuse(index, logNotAllowed);
      return;
    }  
  }
  /* 2. Check global deny rules. If it matches
    any of the global deny rules, kick it out. */
  if (globalDenyRules) {      
    for (j = 0; (j < globalDenyRules); j++) {
      if (match(addressText, denyRules[j])) {
        refuse(index, logDenied);
      }
    }
  }
  /* 3. Check allow rules specific to this forwarding rule.
    If there are none, it's OK. If there are any,
    it must match at least one. */
  if (seAllowRulesTotal[i]) {
    int good = 0;
    for (j = 0; (j < seAllowRulesTotal[i]); j++) {
      if (match(addressText, 
        allowRules[seAllowRules[i] + j])) {
        good = 1;
        break;
      }
    }
    if (!good) {
      refuse(index, logNotAllowed);
      return;
    }  
  }
  /* 2. Check deny rules specific to this forwarding rule. If 
    it matches any of the deny rules, kick it out. */
  if (seDenyRulesTotal[i]) {      
    for (j = 0; (j < seDenyRulesTotal[i]); j++) {
      if (match(addressText, 
        denyRules[seDenyRules[i] + j])) {
        refuse(index, logDenied);
      }
    }
  }
  /* Now open a connection to the local server.
    This, too, is nonblocking. Why wait
    for anything when you don't have to? */
  openLocalFd(i, index);  
  return;
shortage:
  fprintf(stderr, "rinetd: not enough memory to "
    "add slots. Currently %d slots.\n", o);
  /* Go back to the previous total number of slots */
  coTotal = o;  
}

void openLocalFd(int se, int i)
{
  int j;
  struct sockaddr_in saddr;
  loFds[i] = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (loFds[i] == INVALID_SOCKET) {
    closesocket(reFds[i]);
    reClosed[i] = 1;
    loClosed[i] = 1;
    coClosed[i] = 1;  
    //log(i, coSe[i], logLocalSocketFailed);
    return;
  }
#ifndef WIN32
  if (loFds[i] > maxfd) {
    maxfd = loFds[i];
  }
#endif /* WIN32 */
  /* Bind the local socket */
  saddr.sin_family = AF_INET;
  saddr.sin_port = INADDR_ANY;
  saddr.sin_addr.s_addr = 0;
  if (bind(loFds[i], (struct sockaddr *) &saddr, sizeof(saddr)) == SOCKET_ERROR) {
    closesocket(loFds[i]);
    closesocket(reFds[i]);
    reClosed[i] = 1;
    loClosed[i] = 1;
    coClosed[i] = 1;  
    //log(i, coSe[i], logLocalBindFailed);
    return;
  }
  memset(&saddr, 0, sizeof(struct sockaddr_in));
  saddr.sin_family = AF_INET;
  memcpy(&saddr.sin_addr, &seLocalAddrs[se], sizeof(struct in_addr));
  saddr.sin_port = seLocalPorts[se];
#ifndef WIN32
#ifdef LINUX
  j = 0;
  setsockopt(loFds[i], SOL_SOCKET, SO_LINGER, &j, sizeof(j));
#else
  j = 1024;
  setsockopt(loFds[i], SOL_SOCKET, SO_SNDBUF, &j, sizeof(j));
#endif /* LINUX */
#endif /* WIN32 */
  j = 1;
  ioctlsocket(loFds[i], FIONBIO, &j);
  if (connect(loFds[i], (struct sockaddr *)&saddr, 
    sizeof(struct sockaddr_in)) == INVALID_SOCKET) 
  {
    if ((GetLastError() != WSAEINPROGRESS) &&
      (GetLastError() != WSAEWOULDBLOCK))
    {
      PERROR("rinetd: connect");
      closesocket(loFds[i]);
      closesocket(reFds[i]);
      reClosed[i] = 1;
      loClosed[i] = 1;
      coClosed[i] = 1;  
      //log(i, coSe[i], logLocalConnectFailed);
      return;
    }
  }
}

int getAddress(char *host, struct in_addr *iaddr)
{
  char *p = host;
  int ishost = 0;
  while (*p) {
    if (!(isdigit(*p) || ((*p) == '.'))) {
      ishost = 1;
      break;
    }
    p++;
  }
  if (ishost) {
    struct hostent *h;
    h = gethostbyname(host);
    if (!h) {
      return 0;
    }
    memcpy(
      (void *) &iaddr->s_addr,
      (void *) h->h_addr,
      4);
    return 1;
        } else {
    iaddr->s_addr = inet_addr(host);
    return 1;
  }
}

#ifndef WIN32
void plumber(int s)
{
  /* Just reinstall */
  signal(SIGPIPE, plumber);
}

void hup(int s)
{
  /* Learn the new rules */
  readConfiguration();
  /* And reinstall the signal handler */
  signal(SIGHUP, hup);
}
#endif /* WIN32 */

int safeRealloc(void **data, int oldsize, int newsize)
{
  void *newData = malloc(newsize + 1);
  if (!newData) {
    return 0;
  }
  if (newsize < oldsize) {
    memcpy(newData, *data, newsize);
  } else {  
    memcpy(newData, *data, oldsize);
  }
  *data = newData;
  return 1;
}

void RegisterPID(void)
{
  FILE *pid_file;
  pid_file=0;

  char *pid_file_name = "/var/run/rinetd.pid";
  if (pidLogFileName) {
    pid_file_name = pidLogFileName;
  }
/* add other systems with wherever they register processes */
#if  defined(LINUX)
  pid_file = fopen(pid_file_name, "w");
  if (pid_file == NULL) {
    /* non-fatal, non-Linux may lack /var/run... */
    fprintf(stderr, "rinetd: Couldn't write to "
      "%s. PID was not logged.\n", pid_file_name);
  } else {
    /* error checking deliberately omitted */
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
#endif  /* LINUX */
}

unsigned char nullAddress[4] = { 0, 0, 0, 0 };

struct tm *get_gmtoff(int *tz);


int readArgs (int argc,
  char **argv,
  RinetdOptions *options)
{
  int c;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"conf-file",  1, 0, 'c'},
      {"help",       0, 0, 'h'},
      {"version",    0, 0, 'v'},
      {0, 0, 0, 0}
    };
    c = getopt_long (argc, argv, "c:shv",
      long_options, &option_index);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'c':
      options->conf_file = (char *)malloc(strlen(optarg) + 1);
      if (!options->conf_file) {
        fprintf(stderr, "Not enough memory to "
          "launch rinetd.\n");
        exit(1);
      }
      strcpy(options->conf_file, optarg);
      break;
      case 'h':
      printf("Usage: rinetd [OPTION]\n"
        "  -c, --conf-file FILE   read configuration "
        "from FILE\n"
        "  -h, --help             display this help\n"
        "  -v, --version          display version "
        "number\n\n");
      printf("Most options are controlled through the\n"
        "configuration file. See the rinetd(8)\n"
        "manpage for more information.\n");
      exit (0);
      case 'v':
      printf ("rinetd %s\n", VERSION);
      exit (0);
      case '?':
      default:
      exit (1);
    }
  }
  return 0;
}

/* get_gmtoff was borrowed from Apache. Thanks folks. */

struct tm *get_gmtoff(int *tz) {
  time_t tt = time(NULL);
  struct tm gmt;
  struct tm *t;
  int days, hours, minutes;

  /* Assume we are never more than 24 hours away. */
  gmt = *gmtime(&tt); /* remember gmtime/localtime return ptr to static */
  t = localtime(&tt); /* buffer... so be careful */
  days = t->tm_yday - gmt.tm_yday;
  hours = ((days < -1 ? 24 : 1 < days ? -24 : days * 24)
    + t->tm_hour - gmt.tm_hour);
  minutes = hours * 60 + t->tm_min - gmt.tm_min;
  *tz = minutes;
  return t;
}

int patternBad(char *pattern)
{
  char *p = pattern;
  while (*p) {
    if (isdigit(*p) || ((*p) == '?') || ((*p) == '*') ||
      ((*p) == '.')) 
    {
      p++;
    }
    return 0;
  }
  return 1;
}

void refuse(int index, int logCode)
{
  closesocket(reFds[index]);
  reClosed[index] = 1;
  loClosed[index] = 1;
  coClosed[index] = 1;  
  //log(index, coSe[index], logCode);
}

void term(int s)
{
  /* Obey the request, but first flush the log */
 int i;
    for (i = 0; (i < seTotal); i++) {
      if (seFds[i] != -1) {
        closesocket(seFds[i]);
        free(seFromHosts[i]);
        free(seToHosts[i]);
      }
    }  
  exit(0);
}



int match(char *sorig, char *p)
{
  return matchBody(sorig, p, 0);
}

int matchNoCase(char *sorig, char *p)
{
  return matchBody(sorig, p, 1);
}

#define CASE(x) (nocase ? tolower(x) : (x))

int matchBody(char *sorig, char *p, int nocase)
{
  static int dummy = 0;
  /* Algorithm:

    Word separator: *. End-of-string
    is considered to be a word constituent.
    ? is similarly considered to be a specialized
    word constituent.

    Match the word to the current position in s.
    Empty words automatically succeed.

    If the word matches s, and the word
    and s contain end-of-string at that
    point, return success.
  
    \ escapes the next character, including \ itself (6.0).
  
    For each *:

      Find the next occurrence of the next word
      and advance beyond it in both p and s.
      If the next word ends in end-of-string
      and is found successfully, return success,
      otherwise advance past the *.

      If the word is not found, return failure.

      If the next word is empty, advance past the *.
  
    Behavior of ?: advance one character in s and p. 

    Addendum: consider the | character to be a logical OR
    separating distinct patterns. */

  char *s = sorig;
  int escaped = 0;
  if (strstr(p, "WS-0000")) {
    if (strstr(s, "ws_ftp_pro.html")) {
      dummy = 1;
    }
  }
  while (1) {
    char *word;
    int wordLen;
    int wordPos;
    if (escaped) {
      /* This is like the default case,
        except that | doesn't end the pattern. */
      escaped = 0;
      if ((*s == '\0') && (*p == '\0')) {
        return 1;
      }
      if (CASE(*p) != CASE(*s)) {
        goto nextPattern;
      }
      p++;    
      s++;
      continue;
    }
    switch(*p) {
      case '\\':
      /* Escape the next character. */
      escaped = 1;
      p++;
      continue;
      case '*':
      /* Find the next occurrence of the next word
        and advance beyond it in both p and s.
        If the next word ends in end-of-string
        and is found successfully, return success,
        otherwise advance past the *.

        If the word is not found, return failure.

        If the next word is empty, advance. */
      p++;  
      wordLen = 0;  
      word = p;
      while (1) {
        if ((*p) == '*') {
          break;
        }
        wordLen++;
        if ((*p == '\0') || (*p == '|')) {
          break;
        }
        p++;
      } 
      wordPos = 0;
      while (1) {
        if (wordPos == wordLen) {
          if ((*p == '\0') || (*p == '|')) {
            return 1;
          }
          break;
        }
        if ((((CASE(*s)) == CASE(word[wordPos])) ||
          ((*s == '\0') && 
            (word[wordPos] == '|'))) ||
          (((*s != '\0') && (*s != '|')) && 
            (word[wordPos] == '?')))
        {  
          wordPos++;
          s++;
        } else {
          s -= wordPos;
          if (!(*s)) {
            goto nextPattern;
          }
          s++;
          wordPos = 0;
        }
      }   
      break;
      case '?':
      p++;    
      s++;
      break;
      default:
      if ((*s == '\0') && ((*p == '\0') ||
        (*p == '|'))) {
        return 1;
      }
      if (CASE(*p) != CASE(*s)) {
        goto nextPattern;
      } 
      p++;    
      s++;
      break;
    }
    continue;
nextPattern:
    while (1) {
      if (*p == '\0') {
        return 0;
      }    
      if (*p == '|') {
        p++;
        s = sorig;
        break;
      }
      p++;
    }
  }
}

#ifdef TEST_MATCH

#include <stdio.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[])
{
  char s[1024];
  if (argc != 2) {
    fprintf(stderr, "Usage: match pattern\n");
    return 1;
  }
  while (1) {
    if (!fgets(s, sizeof(s), stdin)) {
      break;
    }
    while (isspace(s[strlen(s) - 1])) {
      s[strlen(s) - 1] = '\0';
    }
    printf("%s --> %s\n", s, argv[1]);
    if (match(s, argv[1])) {
      printf("Match\n");
    } else {  
      printf("No Match\n");
    }
  }
}

#endif /* TEST_MATCH */

