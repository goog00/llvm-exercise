#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void func(int *x);

int main() {
  int *ptr = malloc(sizeof(*ptr));
  *ptr = 55;

  printf("Adress of ptr:%p,value of ptr:0x%x\n", ptr, *ptr);

  printf("%d\n", *ptr);

  func(ptr);
  printf("Address of ptr: %p, value of ptr: 0x%x\n", ptr, *ptr);
  printf("%d\n",*ptr);

  return 0;
}

