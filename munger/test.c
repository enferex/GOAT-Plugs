#include <stdio.h>


static const char * foo0 = "BarrrrR0";
static const char * const foo1 = "FoooOoOOOOoOoo1";
const char * foo2 = "Bazzzzzzzz!2";
const char * const foo3 = "Bazzzzzzzz!3";
char *foo100 = "FOooO100";
char fooX = 'X';

void fn2(const char *f, const char *f2)
{
    printf("%s\n%s\n%s\n", f, f2, "*Bakkkkkkk*4");
    printf("%s\n", foo100);
}

void fn(void)
{
    static char *foo5 = "foooOOOooo5";
    static const char *foo6 = "XoooOOOooo6";
    const char * const foo7 = "XoooOOOooo7";
    static const char * const foo8 = "XoooOOOooo8";
    char *foo9 = "Foooo9";
    const char *foo10 = "Foooo10";

    printf("%s\n%s\n%s\n%s\n", foo5, foo6, foo7, foo8);
    printf("%s\n%s\n%c", foo9, foo10, fooX);
    fn2("BAR", "BAR");
}


int main(void)
{
   int i;

   printf("Testing!\n");
   printf("%s\n%s\n%s\n", foo0, foo1, foo2);

   for (i=0; i<10; ++i)
     fn();

   return 0; 
}
