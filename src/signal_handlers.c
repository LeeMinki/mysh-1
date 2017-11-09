#include "signal_handlers.h"
#include <signal.h>

void catch_sigint(int signalNo)
{
  if(signalNo == SIGINT)
    signal(signalNo, SIG_IGN);
}

void catch_sigtstp(int signalNo)
{
  if(signalNo == SIGTSTP)
    signal(signalNo, SIG_IGN);
}
