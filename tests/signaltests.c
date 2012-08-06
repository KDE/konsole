/*
    Author: Kasper Laudrup <laudrup@stacktrace.dk> 
    This code is in the public domain.
    From patch from bko 214908
*/
#include <stdio.h>
#include <signal.h>
#include <string.h>

int signals[] = {SIGSTOP, SIGCONT, SIGHUP, SIGINT, SIGTERM, SIGKILL, SIGUSR1, SIGUSR2};

print_signal_name(int signal) {
  char* signame;
  switch(signal) {
  case SIGSTOP:
    signame = "SIGSTOP";
    break;
  case SIGCONT:
    signame = "SIGCONT";
    break;
  case SIGHUP:
    signame = "SIGHUP";
    break;
  case SIGINT:
    signame = "SIGINT";
    break;
  case SIGTERM:
    signame = "SIGTERM";
    break;
  case SIGKILL:
    signame = "SIGKILL";
    break;
  case SIGUSR1:
    signame = "SIGUSR1";
    break;
  case SIGUSR2:
    signame = "SIGUSR1";
    break;
  default:
    signame = "UNKNOWN";
    break;
  }
  printf("Caught signal: %s\n", signame);
}

void handler(int signal) {
}

int main(int argc, char *argv[])
{
  sigset_t waitset;
  struct sigaction sigact;
  int signal, result, i;
  int signals_size = sizeof(signals) / sizeof(int);

  sigemptyset(&sigact.sa_mask);
  sigemptyset(&waitset);
  sigact.sa_flags = 0;
  sigact.sa_handler = handler;
  for (i=0; i<signals_size; i++) {
    sigaction(signals[i], &sigact, NULL);
    sigaddset(&waitset, signals[i]);
  }

  printf("Waiting for signal\n");
  result = sigwait(&waitset, &signal);
  if (result == 0) {
    print_signal_name(signal);
    return 0;
  } else {
    char* error = strerror(result);
    printf("Error calling sigwait: &s\n", error);
    return 1;
  }
}

