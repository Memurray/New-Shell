#include <stdio.h>
#include <unistd.h>

void main()
{
  printf("Enter p30 with pid=%d\n", getpid());
  sleep(30);
  printf("Exit p30 with pid=%d\n", getpid());
}
