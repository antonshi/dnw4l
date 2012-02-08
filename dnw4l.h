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
	uint32_t		ram_base;
	uint16_t		idVendor;
	uint16_t		idProduct;

	unsigned char		ep_out;
};

/**
 * struct board - board instance.
 * @name: The board name.
 * @info: The board description.
 * @dev: The USB device representation.
 * @hdl: The handle on USB device.
 * @desc: The device descriptor of the board.
 * @data: The data to transfer.
 * @data_size: The number of bytes to transfer.
 */
struct board {
	char				*name;

	struct board_info		*info;
	struct libusb_device		*dev;
	struct libusb_device_handle	*hdl;

	struct libusb_device_descriptor desc;

	void				*data;
	uint32_t			data_size;
};

#endif /* BOOT_USB_H */
