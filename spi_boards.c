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

#include "types.h"

#include <stdio.h>
#include <string.h>

#include "eeprom_local.h"
#include "eeprom.h"
#include "spi_boards.h"
#include "common.h"
#include "spilbp.h"

extern board_t boards[MAX_BOARDS];
extern int boards_count;

#if (PAGE_SIZE * 16) > 4096
#error This code does not work with EEPROM pages that big
#endif

static void spi_read_page(llio_t *self, u32 addr, void *buff) {
    int i, j;
    u32 wide[4*PAGE_SIZE];

    for(i=0, j=0; i<PAGE_SIZE; i++) {
        wide[j++] = spilbp_write_command(HM2_SPI_DATA_REG, 1, false);
        wide[j++] = 0;
        wide[j++] = spilbp_read_command(HM2_SPI_DATA_REG, 1, false);
        wide[j++] = 0;
    }
    eeprom_access.prefix(self);
    eeprom_access.send_byte(self, SPI_CMD_READ);
    send_address(self, addr);
    spilbp_transact(wide, sizeof(wide));
    eeprom_access.suffix(self);

    for(i=0; i<PAGE_SIZE; i++) ((u8*)buff)[i] = wide[4*i+3];
}

static void spi_write_page(llio_t *self, u32 addr, void *buff) {
    int i, j;
    u32 wide[2*PAGE_SIZE];

    for(i=0, j=0; i<PAGE_SIZE; i++) {
        wide[j++] = spilbp_write_command(HM2_SPI_DATA_REG, 1, false);
        wide[j++] = ((u8*)buff)[i];
    }

    write_enable(self);
    eeprom_access.prefix(self);
    eeprom_access.send_byte(self, SPI_CMD_PAGE_WRITE);
    send_address(self, addr);
    spilbp_transact(wide, sizeof(wide));
    eeprom_access.suffix(self);
    wait_for_write(self);
}

static int spi_board_open(board_t *board) {
    eeprom_init(&(board->llio));
    board->flash_id = read_flash_id(&(board->llio));
    if (board->fallback_support == 1) {
        eeprom_prepare_boot_block(board->flash_id);
        board->flash_start_address = eeprom_calc_user_space(board->flash_id);
    } else {
        board->flash_start_address = 0;
    }
    eeprom_access.read_page = spi_read_page;
    eeprom_access.write_page = spi_write_page;
    return 0;
}

static int spi_board_close(board_t *board) {
    return 0;
}

int spi_boards_init(board_access_t *access) {
    spilbp_init(access);
    return 0;
}

void spi_boards_cleanup(board_access_t *access) {
    spilbp_release();
}

static
int spi_read(llio_t *self, u32 off, void *buf, int sz) {
    spilbp_read(off, buf, sz, true);
    return 1;
}

static
int spi_write(llio_t *self, u32 off, void *buf, int sz) {
    spilbp_write(off, buf, sz, true);
    return 1;
}

void spi_boards_scan(board_access_t *access) {
    u32 buf[4];
    u32 cookie[] = {0x55aacafe, 0x54534f48, 0x32544f4d};
    if(spilbp_read(0x100, &buf, sizeof(buf), true) < 0) return;

    if(memcmp(buf, cookie, sizeof(cookie))) {
        fprintf(stderr, "Unexpected cookie at 0000..000c:\n%08x %08x %08x\n",
            buf[0], buf[1], buf[2]);
        return;
    }

    char ident[8];
    if(spilbp_read(buf[3] + 0xc, &ident, sizeof(ident), true) < 0) return;
    
    if(!memcmp(ident, "MESA7I90", 8)) {
        board_t *board = &boards[boards_count];
        board->type = BOARD_SPI;
        strcpy(board->dev_addr, access->dev_addr);
        strncpy(board->llio.board_name, "7I90", 4);
        board->llio.num_ioport_connectors = 24;
        board->llio.ioport_connector_name[0] = "P1";
        board->llio.ioport_connector_name[1] = "P2";
        board->llio.ioport_connector_name[2] = "P3";
        board->llio.num_leds = 2;
        board->llio.private = board;
        board->llio.verbose = access->verbose;
        board->llio.write = spi_write;
        board->llio.read = spi_read;
        board->llio.write_flash = local_write_flash;
        board->llio.verify_flash = local_verify_flash;
        board->llio.fpga_part_number = "6slx9tqg144";
        board->open = spi_board_open;
        board->close = spi_board_close;
        board->print_info = spi_print_info;
        board->flash = BOARD_FLASH_HM2;
        board->fallback_support = 1;
        boards_count ++;
    } else {
        int i=0;
        for(i=0; i<sizeof(ident); i++)
            if(!isprint(ident[i])) ident[i] = '?';

        fprintf(stderr, "Unknown board: %.8s\n", ident);
    }
}

void spi_boards_release(board_access_t *access) {
    spilbp_release();
}

void spi_print_info(board_t *board) {
}

