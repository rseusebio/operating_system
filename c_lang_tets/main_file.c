#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "other_file1.c"
#include "other_file2.c"

int main( )
{
    p1.name = "Rafael";
    p1.age  = 25;

    change_person( );

    custom_print( p1.name );
}