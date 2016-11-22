//
//    Copyright (C) 2013-2014 Michael Geszkiewicz
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "../anyio.h"
#include "../encoder_module.h"
#ifdef __linux__
#include <pci/pci.h>
#elif _WIN32
#include "libpci/pci.h"
#endif

extern int usleep(useconds_t usec); // static global access variable conflicts with POSIX function in unistd.h

static int device_flag;
static int instance_flag;
static int instance = 0;
static int delay_flag;
static int delay = 50;
static int verbose_flag;
static board_access_t board_access;

static struct option long_options[] = {
    {"device", required_argument, 0, 'd'},
    {"instance", required_argument, 0, 'i'},
    {"delay", required_argument, 0, 't'},
    {"verbose", no_argument, &verbose_flag, 1},
    {0, 0, 0, 0}
};

static void print_short_usage(void) {
    printf("Example program forreading encoder on pci boards\n");
    printf("Syntax:\n");
    printf("  pci_encoder_read --device str [--instance i] [--delay d] [--verbose]\n");
    printf("  --device           select active device name\n");
    printf("  --instance         select encoder instance 'i' for reading, default 0\n");
    printf("  --delay            how long to wait between encoder reads in miliseconds, default 50\n");
    printf("  --verbose          printf additional info during execution\n");
}

static int process_cmd_line(int argc, char *argv[]) {
    int c;

    while (1) {
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long(argc, argv, "", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1) break;
        switch (c) {
            case 0: {
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0) break;
                printf ("option %s", long_options[option_index].name);
                if (optarg)
                    printf (" with arg %s", optarg);
                printf ("\n");
            }
            break;

            case 'd': {
                int i;

                if (device_flag > 0) {
                    printf("Error: multiply --device option\n");
                    exit(-1);
                }
                board_access.device_name = optarg;
                for (i = 0; optarg[i] != '\0'; i++)
                    board_access.device_name[i] = toupper(board_access.device_name[i]);

                device_flag++;
            }
            break;

            case 'i': {
                if (instance_flag > 0) {
                    printf("Error: multiply --instance option\n");
                    exit(-1);
                }
                instance = strtol(optarg, NULL, 10);
                instance_flag++;
            }
            break;

            case 't': {
                if (delay_flag > 0) {
                    printf("Error: multiply --delay option\n");
                    exit(-1);
                }
                delay = strtol(optarg, NULL, 10);
                delay_flag++;
            }
            break;

            case '?':
                /* getopt_long already printed an error message. */
                return -1;
                break;

            default:
                abort();
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    board_t *board = NULL;
    int ret = 0;
    encoder_module_t enc;

    if (argc == 1) {
        print_short_usage();
        return 0;
    }
    if (process_cmd_line(argc, argv) == -1) {
        return -1;
    }

    board_access.verbose = verbose_flag;

    if (anyio_init(&board_access) != 0) {     // init library
        return -1;
    }
    ret = anyio_find_dev(&board_access);      // find board
    if (ret < 0) {
        return -1;
    }
    board = anyio_get_dev(&board_access, 1);  // if found the get board handle
    if (board == NULL) {
        printf("No %s board found\n", board_access.device_name);
        return -1;
    }

    board->open(board);                 // open board for communication
    board->print_info(board);           // print what card it is 
    hm2_read_idrom(&(board->llio.hm2));     // read hostmot2 idrom

    ret = encoder_init(&enc, board, instance, delay);   // init encoder 'instance' module on 'board'
    if (ret < 0) {
        goto fail0;
    }

    while (1) {
        encoder_read(&enc);             // read encoder 
        printf("tsc = %u, raw_counts = %u, velocity = %.2f\n", enc.global_time_stamp, enc.raw_counts, enc.velocity);
        usleep(delay*1000);             // wait delay ms
    }

    encoder_cleanup(&enc);              // cleanup enocder module
    
fail0:
    board->close(board);                // close board communication

    anyio_cleanup(&board_access);             // close library

    return 0;
}
