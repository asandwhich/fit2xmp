//---------------------------------------------------------------------
// fit2xmp.c - program to parse a fit file and apply location data
// * to xmp files for photos
//---------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Point this to wherever you have the fit sdk
#include "fit_sdk/fit_convert.h"

#define RECORD_SIZE 8
#define FALSE 0
#define TRUE 1
typedef uint8_t boolean;

typedef struct
    {
    uint32_t time_stamp; // seconds since UTC 00:00 Dec 31 1989
    int32_t lat; // semicircles
    int32_t lon; // semicircles
    uint16_t alt; // 5 * m + 500
    } loc_data;

boolean fit_data_loaded = FALSE;
loc_data * position_data = NULL;
uint32_t position_data_cnt = 0;

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
inline double semicircle_to_deg
    (
    int32_t semi
    )
{
return ( (double) semi / 0x80000000 ) * 180.0;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
inline int32_t deg_to_semicircle
    (
    double deg
    )
{
return (int32_t)( ( deg / 180.0 ) * 0x80000000 );
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
boolean load_fit_file
    (
    char * fit_path
    )
{
FILE * fit_file;
FIT_CONVERT_RETURN convert_return;
uint8_t record_buf[RECORD_SIZE];
uint32_t i;
FIT_RECORD_MESG * record_message;
uint32_t data_idx;

convert_return = FIT_CONVERT_CONTINUE;
data_idx = 0;

if( ( NULL == fit_path ) || fit_data_loaded )
    {
    return FALSE;;
    }

if( NULL == position_data )
    {
    position_data_cnt = 10;
    position_data = malloc( sizeof( loc_data ) * position_data_cnt );
    }

fit_file = fopen( fit_path, "rb" );

if( NULL == fit_file )
    {
    return FALSE;
    }

FitConvert_Init( TRUE );

while( !feof( fit_file ) && ( FIT_CONVERT_CONTINUE == convert_return ) )
    {
    for( i = 0; i < RECORD_SIZE; ++i )
        {
        record_buf[i] = (uint8_t)getc( fit_file );
        }
    do
        {
        convert_return = FitConvert_Read( record_buf, RECORD_SIZE );

        if( FIT_CONVERT_MESSAGE_AVAILABLE == convert_return )
            {
            // Only want records (for loc info)
            if( FIT_MESG_NUM_RECORD == FitConvert_GetMessageNumber() )
                {
                record_message = FitConvert_GetMessageData();
                position_data[data_idx].alt = record_message->altitude;
                position_data[data_idx].lat = record_message->position_lat;
                position_data[data_idx].lon = record_message->position_long;
                position_data[data_idx].time_stamp = record_message->timestamp;

                ++data_idx;
                if( data_idx >= position_data_cnt )
                    {
                    position_data_cnt *= 2;
                    position_data = realloc( position_data, sizeof( loc_data ) * position_data_cnt );
                    }
                }
            }
        } while( FIT_CONVERT_MESSAGE_AVAILABLE == convert_return );
    }

if( data_idx != position_data_cnt ) 
    {
    position_data_cnt = data_idx;
    position_data = realloc( position_data, sizeof( loc_data ) * position_data_cnt );
    }

fit_data_loaded = TRUE;

return TRUE;
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
boolean apply_loc_data_to_path
    (
    char * xmp_path
    )
{



}

//---------------------------------------------------------------------
// print_help - print usage info
//---------------------------------------------------------------------
void print_help
    (
    void
    )
{
printf( "Usage: ./fit2xmp <fit_file.fit> <image1.xmp> <image2.xmp> ...\n" );
printf( "Example: ./fit2xmp 867616029.fit DSC_0129.xmp DSC_0130.xmp\n" );
}

//---------------------------------------------------------------------
//
//---------------------------------------------------------------------
int main
    (
    int argc,
    char ** argv
    )
{
uint32_t i;
char * fit_path;
char ** xmp_paths;
uint32_t xmp_path_count;
uint32_t xmp_buf_size;

xmp_buf_size = 1;
xmp_path_count = 0;
fit_path = NULL;

xmp_paths = malloc( sizeof( char* ) * xmp_buf_size );

if( argc < 3 )
    {
    print_help();
    exit( 1 );
    }

// process the args
for( i = 0; i < argc; ++i )
    {
    if( 0 == strcmp( "--help", argv[i] ) )
        {
        print_help();
        exit( 1 );
        }
    if( NULL != strstr( argv[i], ".fit" ) )
        {
        fit_path = argv[i];
        continue;
        }
    if( NULL != strstr( argv[i], ".xmp" ) )
        {
        xmp_paths[xmp_path_count] = argv[i];
        ++xmp_path_count;
        if( xmp_path_count >= xmp_buf_size )
            {
            xmp_buf_size += 5;
            xmp_paths = realloc( xmp_paths, sizeof( char* ) * xmp_buf_size );
            }
        }
    }

// size buffer properly
if( xmp_path_count != xmp_buf_size )
    {
    xmp_buf_size = xmp_path_count;
    xmp_paths = realloc( xmp_paths, sizeof( char* ) * xmp_buf_size );
    }

// make sure fit file is specified along with at least one xmp path
if( ( NULL == fit_path ) || ( xmp_path_count == 0 ) || ( NULL == xmp_paths[0] ) )
    {
    print_help();
    exit( 1 );
    }

if( load_fit_file( fit_path ) )
    {
    printf( "loaded fit file loc data\n" );
    }
else
    {
    printf( "failed loading fit file\nexiting" );
    exit( 1 );
    }

for( i = 0; i < xmp_path_count; ++i )
    {
    printf( "(%d/%d) applying location info to: %s\n", i + 1, xmp_path_count, xmp_paths[i] );
    if( !apply_loc_data_to_path( xmp_paths[i] ) )
        {
        printf( "failed\n" );
        }
    }

printf( "complete\n" );

return 0;
}
