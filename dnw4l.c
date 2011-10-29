/* dnw4l - DNW for Linux
 *
 * Anton Tikhomirov <tikhomirov.devel@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <libusb-1.0/libusb.h>

#include "dnw4l.h"

/**
 * free_info - free board descriptions;
 * @info: Head of linked list of board descriptions.
 */
static void free_info(struct board_info *head)
{
	struct board_info *next;

	while (head != NULL) {
		next = head->next;
		free(head);
		head = next;
	}
}

/**
 * parse_cline - parse configuration line.
 * @line: Line to parse.
 * @info: Board description.
 */
static int parse_cline(char *line, struct board_info *info)
{
	int ret;
	struct {
		unsigned int ep_out;
		unsigned int ram_base;
		unsigned int idVendor;
		unsigned int idProduct;
		unsigned int bNumConfigurations;
	} info_data;

	ret = sscanf(line, "%31[^','],%2u,0x%8x,0x%8x,0x%8x,%3u",
		     info->dev_name,
		     &info_data.ep_out,
		     &info_data.ram_base,
		     &info_data.idVendor,
		     &info_data.idProduct,
		     &info_data.bNumConfigurations);
	if (ret != 6)
		return -1;

	/* FIXME: check limits */
	info->ep_out = info_data.ep_out;
	info->ram_base = info_data.ram_base;
	info->idVendor = info_data.idVendor;
	info->idProduct = info_data.idProduct;
	info->bNumConfigurations = info_data.bNumConfigurations;

	return 0;
}

/**
 * parse_config - parse configuration file.
 * @file: File to parse.
 */
static struct board_info * parse_config(char *file)
{
	struct board_info *curr, *head;
	char line[256];
	char *p;
	FILE *f;
	int ret;

	head = NULL;

	/* Create default board */
	curr = (struct board_info *) malloc(sizeof (struct board_info));
	if (curr == NULL) {
		fprintf(stderr, "Cannot allocate memory for default board\n");
		return NULL;
	}

	ret = snprintf(curr->dev_name, sizeof (curr->dev_name), "Default");
	if (ret < 0)
		fprintf(stderr, "Cannot create default board configuration\n");
	else {
		/* Default data */
		curr->ep_out = 2;
		curr->ram_base = 0x40008000;
		curr->idVendor = 0x04e8;
		curr->idProduct	= 0x1234;
		curr->bNumConfigurations = 1;

		curr->next = head;
		head = curr;
	}

	if (file == NULL)
		return head;

	f = fopen(file, "r");
	if (f == NULL) {
		fprintf(stderr, "Cannot open config file, "
				"will use default configuration\n");
		return head;
	}

	while (fgets(line, (int) (sizeof line - 1), f) != NULL) {


		/* Comment */
		if (line[0] == '#')
			continue;

		printf("%s\n", line);

		curr = (struct board_info *) malloc(sizeof (struct board_info));
		if (curr == NULL) {
			fprintf(stderr, "Cannot allocate memory"
					"for board description\n");
			fclose(f);
			return head;
		}

		p = strchr(line, '\n');
		if (p != NULL)
			*p = '\0';

		ret = parse_cline(line, curr);
		if (ret < 0) {
			fprintf(stderr, "Cannot parse config line\n");
			free(curr);
			continue;
		}

		curr->next = head;
		head = curr;
	}

	fclose(f);

	return head;
}

/**
 * find_device - find the board among USB devices connected to host.
 * @devs: The USB device list.
 * @brd: The board instance.
 */
static int find_device(libusb_device **devs, struct board *brd)
{
	libusb_device *dev;
	struct board_info *brd_info;
	struct libusb_device_descriptor desc = {0};
	int ret;
	int i = 0;

	while ((dev = devs[i++]) != NULL) {

		ret = libusb_get_device_descriptor(dev, &desc);
		if (ret < 0) {
			fprintf(stderr,
				"Failed to get device descriptor (%d)\n",
				ret);
			return -1;
		}

		for (brd_info = brd->info; brd_info != NULL;
		     brd_info = brd_info->next) {
			if (brd->name != NULL)
				if (STRCMP(brd->name,
					   !=,
					   brd_info->dev_name))
					continue;

			if (board_found(&desc, brd_info)) {
				brd->info = brd_info;
				brd->dev = dev;
				printf("%s found\n", brd->name != NULL ?
				       brd->info->dev_name : "Board");
				return 0;
			}
		}
	}

	return -1;
}

/**
 *  calc_csum - calculate checksum of data to transfer.
 *  @data: The data to transfer.
 *  @len: The number of bytes to transfer.
 */
static uint16_t calc_csum(const unsigned char *data, uint32_t len)
{
	uint16_t csum = 0;

	for (; len != 0; len--)
		csum += (uint16_t) *(data++);

	return csum;
}

/**
 * send_file - transfer data to board.
 * @brd: The board instance.
 *
 * Move the data to the board over USB using bulk transfer.
 */
static int send_file(struct board *brd)
{
	unsigned char *buf;
	void *data = brd->data;
	uint32_t len = brd->data_size;
	uint32_t len_total = len + IMAGE_WRAP_SIZE;
	uint32_t addr = brd->info->ram_base;
	uint16_t csum = calc_csum(data, len);
	int bytes_done = 0;
	int ret = 0;

	printf("csum = 0x%04x\n", (unsigned int) csum);

	/* 4 bytes address, 4 bytes length, data, 2 bytes csum */

	buf = malloc(len_total);
	if (buf == NULL) {
		fprintf(stderr, "Cannot allocate memory\n");
		return -ENOMEM;
	}

	/* endian safeness */
	buf[0] = (unsigned char) (addr & 0xff);
	buf[1] = (unsigned char) ((addr >> 8) & 0xff);
	buf[2] = (unsigned char) ((addr >> 16) & 0xff);
	buf[3] = (unsigned char) ((addr >> 24) & 0xff);

	buf[4] = (unsigned char) (len_total & 0xff);
	buf[5] = (unsigned char) ((len_total >> 8) & 0xff);
	buf[6] = (unsigned char) ((len_total >> 16) & 0xff);
	buf[7] = (unsigned char) ((len_total >> 24) & 0xff);

	memcpy(buf + 8, data, len);

	buf[len + 8] = (unsigned char) ((csum & 0xff));
	buf[len + 9] = (unsigned char) ((csum >> 8) & 0xff);

	printf("send_file: addr = 0x%08x, len = 0x%08x\n", addr, len);

	while (len_total > 0) {
		ret = libusb_bulk_transfer(brd->hdl, brd->info->ep_out,
					   buf, len_total,
					   &bytes_done, 0);
		if (ret < 0) {
			fprintf(stderr, "Bulk transfer failed (%d)\n", ret);
			break;
		}

		len_total -= bytes_done;
	}

	free(buf);

	return ret;
}

static void print_usage(const char *name)
{
	printf("Usage: %s [-h] [-b board_name] [-c config_file] image\n",
	       name);
}

int main(int argc, char **argv)
{
	libusb_device **devs;
	ssize_t cnt;

	struct board brd;
	struct stat st;
	int fd;
	int ret;
	int opt;
	char *filename = NULL;
	char *bconf = NULL;

	brd.name = NULL;
	brd.data = NULL;

	while ((opt = getopt(argc, argv, "b:c:h")) != -1) {
		switch (opt) {
		case 'b':
			brd.name = optarg;
			break;
		case 'c':
			bconf = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			exit(EXIT_SUCCESS);
		default:
			print_usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Expect image file argument\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	filename = argv[optind];

	if (bconf == NULL)
		bconf = DEF_CONF_FILE; 

	ret = libusb_init(NULL);
	if (ret < 0) {
		fprintf(stderr, "Cannot initialize libusb (%d)\n", ret);
		exit(EXIT_FAILURE);
	}

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		fprintf(stderr, "Cannot get device list (%d)\n", (int) cnt);
		ret = (int) cnt;
		goto err;
	}

	/* parse config file */
	brd.info = parse_config(bconf);
	if (brd.info == NULL) {
		fprintf(stderr, "Cannot parse board configuration file\n");
		ret = -1;
		goto err;
	}

	ret = find_device(devs, &brd);
	if (ret < 0) {
		fprintf(stderr, "Cannot find device in bootloader mode\n");
		goto err_list;
	}

	ret = libusb_open(brd.dev, &brd.hdl);
	if (ret < 0) {
		fprintf(stderr, "Unable to open usb device (%d)\n", ret);
		goto err_list;
	}

	ret = libusb_claim_interface(brd.hdl, 0);
	if (ret < 0) {
		fprintf(stderr,
			"Unable to claim usb interface of device (%d)\n", ret);
		goto err_hdl;
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		ret = errno;
		fprintf(stderr, "Cannot open file `%s': %s\n",
			filename, strerror(errno));
		goto err_hdl;
	}

	ret = fstat(fd, &st);
	if (ret < 0) {
		ret = errno;
		fprintf(stderr, "Error to access file `%s': %s\n",
			filename, strerror(errno));
		goto err_fd;
	}

	if (st.st_size <= IMAGE_MAX_SIZE)
		brd.data_size =  st.st_size;
	else {
		fprintf(stderr, "File `%s' is too big\n", filename);
		ret = -1;
		goto err_fd;
	}

	brd.data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (brd.data == MAP_FAILED) {
		ret = errno;
		fprintf(stderr, "Unable to map file `%s': %s\n",
			filename, strerror(errno));
		goto err_fd;
	}

	ret = send_file(&brd);
	if (ret < 0)
		fprintf(stderr, "Error downloading program\n");

err_fd:
	close(fd);
err_hdl:
	libusb_close(brd.hdl);
err_list:
	free_info(brd.info);
	libusb_free_device_list(devs, 1);
err:
	libusb_exit(NULL);

	return ret;
}