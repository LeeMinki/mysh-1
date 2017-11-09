#include "signal.h"
#include <signal.h>


void catch_sigint(int signalNo)
{
  if(signalNo == SIGINT)
    signal(signalNo, SIG_IGN);
}

void catch_sigtstp(int signalNo)
{
  if(signalNo == SIGSTP)
    signal(signalNo, SIG_IGN);
}
