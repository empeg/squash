/* This file is not copyrighted or provided under any license
 * because it is just a memory leak detector.
 */
/*
 * compiled with:
 * gcc -o memleak -lao -ldl ao_memleak_test.c
 */

#include <stdio.h>
#include <ao/ao.h>

int main(int argc, char *argv[]) {
    ao_device *device;
    int default_driver;
    ao_sample_format format;

    ao_initialize();

    default_driver = ao_default_driver_id();

    format.bits = 16;
    format.channels = 2;
    format.rate = 44100;
    format.byte_format = AO_FMT_LITTLE;

    while( 1 ) {
        device = ao_open_live( default_driver, &format, NULL );
        ao_close( device );
    }
    ao_shutdown();

    return 0;
}
