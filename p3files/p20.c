#include <stdio.h>
#include <unistd.h>

void main()
{
  printf("Enter p20 with pid=%d\n", getpid());
  sleep(20);
  printf("Exit p20 with pid=%d\n", getpid());
}
