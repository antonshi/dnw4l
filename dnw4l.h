#ifndef BOOT_USB_H
#define BOOT_USB_H __FILE__

#if 0
#define DNW4L_DEBUG
#endif

#ifdef DNW4L_DEBUG
#define DPRINT(fmt, args...)	fprintf(stderr, fmt, ## args)
#else
#define DPRINT(fmt, args...)
#endif

#define DEF_CONF_FILE		"./board.conf"
#define NUM_INFO_FIELDS		8
#define DELIM			','

/* 1 GiB should be enough */
#define IMAGE_MAX_SIZE		(1024 * 1024 * 1024)
#define IMAGE_WRAP_SIZE		10

#define STRCMP(a,R,b)		(strcmp(a,b) R 0)

/**
 *  struct board_info - board description.
 */
struct board_info {
	struct board_info	*next;

	char			dev_name[32];

	unsigned char		ep_out;

	uint32_t		ram_base;

	uint16_t		idVendor;
	uint16_t		idProduct;
	uint8_t			bNumConfigurations;
};

/**
 * struct board - board instance.
 * @name: The board name.
 * @info: The board description.
 * @dev: The USB device representation.
 * @hdl: The handle on USB device.
 * @data: The data to transfer.
 * @data_size: The number of bytes to transfer.
 */
struct board {
	char				*name;

	struct board_info		*info;
	struct libusb_device		*dev;
	struct libusb_device_handle	*hdl;

	void				*data;
	uint32_t			data_size;
};

static void free_info(struct board_info *head);
static int parse_cline(char *line, struct board_info *info);
static struct board_info * parse_config(char *file);
static int find_device(libusb_device **devs, struct board *brd);
static uint16_t calc_csum(const unsigned char *data, uint32_t len);
static int send_file(struct board *brd);
static void print_usage(const char *name);

/**
 * board_found - match connected USB device with known board.
 * @desc: The USB device decriptor of the connected device.
 * @info: The board description.
 */
static inline bool
board_found(struct libusb_device_descriptor *desc, struct board_info *info)
{
	if (desc->idVendor == info->idVendor
	    && desc->idProduct == info->idProduct
	    && desc->bNumConfigurations == info->bNumConfigurations)

		return true;

	return false;
}

#endif /* BOOT_USB_H */
