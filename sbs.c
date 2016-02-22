/*
    i2cdump.c - a user-space program to dump I2C registers
    Copyright (C) 2002-2003  Frodo Looijaard <frodol@dds.nl>, and
                             Mark D. Studebaker <mdsxyz123@yahoo.com>
    Copyright (C) 2004-2012  Jean Delvare <jdelvare@suse.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.
*/

#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "i2c-dev.h"
#include "i2cbusses.h"

static void help(void)
{
	fprintf(stderr,
		"Usage: sbs -v \n"
		"    -v : get voltage\n" );
}

static int check_funcs(int file, int size, int pec)
{
	unsigned long funcs;

	/* check adapter functionality */
	if (ioctl(file, I2C_FUNCS, &funcs) < 0) {
		fprintf(stderr, "Error: Could not get the adapter "
			"functionality matrix: %s\n", strerror(errno));
		return -1;
	}

	switch(size) {
	case I2C_SMBUS_BYTE:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE)) {
			fprintf(stderr, MISSING_FUNC_FMT, "SMBus receive byte");
			return -1;
		}
		if (!(funcs & I2C_FUNC_SMBUS_WRITE_BYTE)) {
			fprintf(stderr, MISSING_FUNC_FMT, "SMBus send byte");
			return -1;
		}
		break;

	case I2C_SMBUS_BYTE_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
			fprintf(stderr, MISSING_FUNC_FMT, "SMBus read byte");
			return -1;
		}
		break;

	case I2C_SMBUS_WORD_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_WORD_DATA)) {
			fprintf(stderr, MISSING_FUNC_FMT, "SMBus read word");
			return -1;
		}
		break;

	case I2C_SMBUS_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_BLOCK_DATA)) {
			fprintf(stderr, MISSING_FUNC_FMT, "SMBus block read");
			return -1;
		}
		break;

	case I2C_SMBUS_I2C_BLOCK_DATA:
		if (!(funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK)) {
			fprintf(stderr, MISSING_FUNC_FMT, "I2C block read");
			return -1;
		}
		break;
	}

	if (pec
	 && !(funcs & (I2C_FUNC_SMBUS_PEC | I2C_FUNC_I2C))) {
		fprintf(stderr, "Warning: Adapter does "
			"not seem to support PEC\n");
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int i, j, res;
	int i2cbus = 2;
	int address = 0x0b;
	int size = I2C_SMBUS_I2C_BLOCK_DATA;
	int file;
	char filename[20];
	int block[256], s_length = 0;
	int pec = 0;
	int force = 1;
	int first = 0x00, last = 0xff;
	int flags = 0;
	int value;

    while (1+flags < argc && argv[1+flags][0] == '-')
	{
		switch (argv[1+flags][1])
		{
			case 'v':
				value = 0x0071;
				break;
			default:
				fprintf(stderr, "Error: Unsupported option "
					"\"%s\"!\n", argv[1+flags]);
				help();
				exit(1);
		}
		flags++;
    }

	file = open_i2c_dev(i2cbus, filename, sizeof(filename), 0);
	if (file < 0
	 || check_funcs(file, size, pec)
	 || set_slave_addr(file, address, force))
		exit(1);

	/* handle all but word data */
	if (size != I2C_SMBUS_WORD_DATA ) {
		/* do the block transaction */
		if ( size == I2C_SMBUS_I2C_BLOCK_DATA) {
			unsigned char cblock[288];

			res = i2c_smbus_write_word_data(file, 0x00, value);

			i = i2c_smbus_read_i2c_block_data(file,	0x23, 32, cblock);
			res = cblock[0];
			s_length = res;

			for (i = 0; i < res; i++)
				block[i] = cblock[i];

			if (size != I2C_SMBUS_BLOCK_DATA)
				for (i = res; i < 256; i++)
					block[i] = -1;
		}

#if 0
		printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
		       "    0123456789abcdef\n");
		for (i = 0; i < 256; i+=16) {
			if (size == I2C_SMBUS_BLOCK_DATA && i >= s_length)
				break;
			if (i/16 < first/16)
				continue;
			if (i/16 > last/16)
				break;

			printf("%02x: ", i);
			for (j = 0; j < 16; j++) {
				fflush(stdout);
				/* Skip unwanted registers */
				if (i+j < first || i+j > last) {
					printf("   ");
					if (size == I2C_SMBUS_WORD_DATA) {
						printf("   ");
						j++;
					}
					continue;
				}
				res = block[i+j];

				if (res < 0) {
					printf("XX ");
				} else {
					printf("%02x ", block[i+j]);
					if (size == I2C_SMBUS_WORD_DATA)
						printf("%02x ", block[i+j+1]);
				}
			}
			printf("   ");

			for (j = 0; j < 16; j++) {
				/* Skip unwanted registers */
				if (i+j < first || i+j > last) {
					printf(" ");
					continue;
				}

				res = block[i+j];
				if (res < 0)
					printf("X");
				else
				if ((res & 0xff) == 0x00
				 || (res & 0xff) == 0xff)
					printf(".");
				else
				if ((res & 0xff) < 32
				 || (res & 0xff) >= 127)
					printf("?");
				else
					printf("%c", res & 0xff);
			}
			printf("\n");
		}
#endif
	}

	if( value == 0x0071 )
	{
		short sVolt;
		sVolt = (block[12]<<8 | block[11]);
		fprintf(stdout, "%d mV\r\n", sVolt);
	}

	exit(0);
}
