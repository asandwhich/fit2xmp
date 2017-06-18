//---------------------------------------------------------------------
// fit2xmp.c - program to parse a fit file and apply location data
// * to xmp files for photos
//---------------------------------------------------------------------

// Do as I say, not as I do
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <errno.h>

// Point this to wherever you have the fit sdk
#define FIT_USE_STDINT_H
#include "fit_sdk/fit_convert.h"

#define RECORD_SIZE 8
#define MAX_LINE_SIZE 1024
#define EXIF_STR_SIZE 64
#define SECONDS_IN_MIN 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY 86400
#define SECONDS_IN_YEAR 31536000
#define DAYS_IN_YEAR 365
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

typedef struct
    {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
    } time_data;

boolean fit_data_loaded = FALSE;
loc_data * position_data = NULL;
uint32_t position_data_cnt = 0;
int32_t seconds_offset = 0;
static const uint8_t days_in_months[] =
    {
    0,  // init at 1
    31, // jan
    28, // feb
    31, // mar
    30, // apr
    31, // may
    30, // jun
    31, // jul
    31, // aug
    30, // sep
    31, // oct
    30, // nov
    31  // dec
    };

//---------------------------------------------------------------------
// time_stamp_to_fit_time - converts a local time to seconds offset
// * no dates before 1990 pls
// * and make sure it is a legitimate date
// * this does no error checking
//---------------------------------------------------------------------
uint32_t time_stamp_to_fit_time
    (
    time_data * date
    )
{
uint32_t fit_time;
uint32_t day_count;
uint32_t i;

fit_time = 0;
day_count = 0;

// account for leap days
for( i = 1990; i < date->year; ++i )
    {
    if( 0 == ( i % 4 ) )
        {
        ++day_count;
        }
    }
// account for leap day of current year if applicable
if( ( 0 == ( date->year % 4 ) ) && ( date->month > 2 ) )
    {
    ++day_count;
    }
// account for single day between 00:00 Dec 31st 1989 and 1990
++day_count;

// add up days for every year in between
day_count += ( date->year - 1990 ) * DAYS_IN_YEAR;

// add in days between start of current year and start of current month
for( i = 1; i < date->month; ++i )
    {
    day_count += days_in_months[i];
    }

// add in days for current month
day_count += date->day - 1;

fit_time += day_count * SECONDS_IN_DAY;
fit_time += date->hour * SECONDS_IN_HOUR;
fit_time += date->min * SECONDS_IN_MIN;
fit_time += date->sec;

return fit_time;
}

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
const FIT_RECORD_MESG * record_message;
uint32_t data_idx;

convert_return = FIT_CONVERT_CONTINUE;
data_idx = 0;

if( ( NULL == fit_path ) || fit_data_loaded )
    {
    return FALSE;
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
            switch( FitConvert_GetMessageNumber() )
                {
                case FIT_MESG_NUM_RECORD:
                    {
                    record_message = (FIT_RECORD_MESG*)FitConvert_GetMessageData();
                    if( ( 0x7FFFFFFF != record_message->position_lat ) && ( 0x7FFFFFFF != record_message->position_long ) )
                        {
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
                    break;
                    }
                case FIT_MESG_NUM_ACTIVITY:
                    {
                    const FIT_ACTIVITY_MESG * act_mesg;
                    act_mesg = (FIT_ACTIVITY_MESG*)FitConvert_GetMessageData();
                    seconds_offset = act_mesg->local_timestamp - act_mesg->timestamp;
                    break;
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

if( NULL != fit_file )
    {
    fclose( fit_file );
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
FILE * xmp_file;
char ** xmp_lines;
uint32_t line_count;
uint32_t xmp_lines_size;
uint32_t line_size;
char line[MAX_LINE_SIZE];
char lat_format[] = "exif:GPSLatitude=\"%02u,%02u.%04u%c\"\n";
char lon_format[] = "exif:GPSLongitude=\"%03u,%02u.%04u%c\"\n";
char alt_format[] = "exif:GPSAltitude=\"%u/%u\"\n";
char alt_ref_format[] = "exif:GPSAltitudeRef=\"%u\"\n";
char method_format[] = "exif:GPSProcessingMethod=\"MANUAL\"\n";
uint32_t i;
time_data photo_time;
uint32_t photo_time_sec;
int32_t smallest_diff;
uint32_t closest_idx;
uint32_t converted_time;
uint32_t insert_idx;
int32_t diff;
double deg;
double min;
double sec;
double alt;

xmp_file = NULL;
xmp_lines_size = 5;
line_count = 0;
insert_idx = 0;

if( !fit_data_loaded )
    {
    return FALSE;
    }

xmp_file = fopen( xmp_path, "r+" );
if( NULL == xmp_file )
    {
    return FALSE;
    }

xmp_lines = malloc( sizeof( char* ) * xmp_lines_size );

while( fgets( line, MAX_LINE_SIZE, xmp_file ) )
    {
    line_size = sizeof( char ) * ( strlen( line ) + 1 );
    xmp_lines[line_count] = malloc( line_size );
    memcpy( xmp_lines[line_count], line, line_size );


    if( NULL != strstr( xmp_lines[line_count], "CreateDate" ) )
        {
        // dont hate me cause i scanf, with bad sizes
        if( 6 != sscanf( xmp_lines[line_count],
                         " xmp:CreateDate=\"%4hu-%2hhu-%2hhu%*c%2hhu:%2hhu:%2hhu.%*u\"",
                         &photo_time.year,
                         &photo_time.month,
                         &photo_time.day,
                         &photo_time.hour,
                         &photo_time.min,
                         &photo_time.sec ) )
            {
            printf( "Couldn't read photo creation date\n" );
            // let the memory go to waste
            fclose( xmp_file );
            return FALSE;
            }
        insert_idx = line_count;
        }

    ++line_count;
    if( line_count >= xmp_lines_size )
        {
        xmp_lines_size *= 2;
        xmp_lines = realloc( (void*)xmp_lines, sizeof( char* ) * xmp_lines_size );
        }
    }
if( line_count != xmp_lines_size )
    {
    xmp_lines_size = line_count;
    xmp_lines = realloc( (void*)xmp_lines, sizeof( char* ) * xmp_lines_size );
    }

photo_time_sec = time_stamp_to_fit_time( &photo_time );

// slow ass search for closest fit record
smallest_diff = 0x7FFFFFFF;
closest_idx = 0;
for( i = 0; i < position_data_cnt; ++i )
    {
    converted_time = position_data[i].time_stamp + seconds_offset;
    diff = photo_time_sec - converted_time;
    if( abs( diff ) < abs( smallest_diff ) )
        {
        smallest_diff = diff;
        closest_idx = i;
        }
    }

// write it back out
fseek( xmp_file, 0, SEEK_SET );
for( i = 0; i < xmp_lines_size; ++i )
    {
    fprintf( xmp_file, xmp_lines[i] );
    if( i == insert_idx )
        {
        deg = semicircle_to_deg( position_data[closest_idx].lat );
        min = 60 * ( deg - (int32_t)deg );
        sec = 10000 * ( min - (int32_t)min );
        fprintf( xmp_file,
                 lat_format,
                 (uint32_t)floor( fabs( deg ) ),
                 (uint32_t)floor( fabs( min ) ),
                 (uint32_t)floor( fabs( sec ) ),
                 ( deg < 0 ) ? 'S' : 'N' );

        deg = semicircle_to_deg( position_data[closest_idx].lon );
        min = 60 * ( deg - (int32_t)deg );
        sec = 10000 * ( min - (int32_t)min );
        fprintf( xmp_file,
                 lon_format,
                 (uint32_t)floor( fabs( deg ) ),
                 (uint32_t)floor( fabs( min ) ),
                 (uint32_t)floor( fabs( sec ) ),
                 ( deg < 0 ) ? 'W' : 'E' );
        fprintf( xmp_file, method_format );

        alt = ( position_data[closest_idx].alt - 500.0 ) / 5.0;

        fprintf( xmp_file, alt_ref_format, ( alt < 0 ) ? 1 : 0 ); // TODO: support below sea level
        fprintf( xmp_file, alt_format, (uint32_t)( alt * 100 ), 100 );
        }
    }

// wasteful
if( NULL != xmp_lines )
    {
    for( i = 0; i < xmp_lines_size; ++i )
        {
        if( NULL != xmp_lines[i] )
            {
            free( xmp_lines[i] );\
            }
        }
    free( xmp_lines );
    }
if( NULL != xmp_file )
    {
    fclose( xmp_file );
    }
// lat, lon has format of deg,min.minW where W is cardinal
   //exif:GPSLatitude="39,13.45N"
   //exif:GPSProcessingMethod="MANUAL"
   //exif:GPSLongitude="96,37.25W"
   //exif:GPSAltitudeRef="0" // 0 for sea level, 1 for below sea level
   //exif:GPSAltitude="120000/100" // rational number representing meters

return TRUE;
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
printf( "Example: ./fit2xmp -5 867616029.fit DSC_0129.xmp DSC_0130.xmp\n" );
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
for( i = 0; i < (uint32_t)argc; ++i )
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

if( NULL != xmp_paths )
    {
    free( xmp_paths );
    }
if( NULL != position_data )
    {
    free( position_data );
    }

printf( "complete\n" );

return 0;
}
