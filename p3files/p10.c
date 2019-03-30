#include <stdio.h>
#include <unistd.h>

void main()
{
  printf("Enter p10 with pid=%d\n", getpid());
  sleep(10);
  printf("Exit p10 with pid=%d\n", getpid());
}
