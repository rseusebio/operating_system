#include <libconfig.h>


int main( )
{   
    config_t config;
    config_setting_t *config_setting;

    printf( "1. read it ok\n");

    config_init( &config );

    printf( "2. read it ok\n");

    char* filename = "./configuration_file.txt";

    if( config_read_file( &config, filename ) <= 0)
    {
        printf( "failed to read file: %s\n", filename );

        printf( "error: %s\n", config_error_text( &config ) );

        return -1;
    }

    printf( "read it ok\n");

    const char *app;

    if( config_lookup_string( &config, "application.window.title", &app) <= 0 )
    {
        printf( "failed to read path\n" );

        printf( "error: %s\n", config_error_text( &config ) );

        return -1;
    }

    printf( "title: %s\n", app );

    config_setting = config_lookup( &config, "application.books" );

    if( config_setting == NULL)
    {
        printf( "failed to read path\n" );

        printf( "error: %s\n", config_error_text( &config ) );

        return -1;
    }

    config_setting_t *object = config_setting_get_elem( config_setting, 0 );

    if( object == NULL)
    {
        printf( "failed to get element\n" );

        printf( "error: %s\n", config_error_text( &config ) );

        return -1;
    }

    if( config_setting_lookup_string( object, "title", &app ) <= 0)
    {
        printf( "failed to get title inside book object\n" );

        printf( "error: %s\n", config_error_text( &config ) );

        return -1;
    }

    
    printf( "book's title: %s\n", app );


    printf( "this should be working.\n" );

    return 0;
}