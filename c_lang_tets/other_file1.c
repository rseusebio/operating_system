#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "other_file2.c"

void custom_print( char *str )
{
    printf( "your string: %s\n", str );
}

void change_person( )
{
    p1.name = "Kanye West";
    p1.age  = 37;
}