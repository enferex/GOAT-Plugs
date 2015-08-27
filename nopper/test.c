#include <stdio.h>


void fn(int) __attribute__((nopper));

void fn(int n)
{
    int i;

    for (i=0; i<n; ++i)
      printf("Foo\n");
}


int main(void)
{
    int x, y;

    x = 2;
    y = 3;
    fn(2+3);

    return 0;
}
