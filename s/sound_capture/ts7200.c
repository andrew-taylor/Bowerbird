#include "sound_capture.h"
#ifdef TS7200
#include "regmap.h"
#include <sys/mman.h>


struct port_description {
	int	     n_bits;
	int	     mask;
	uint32_t data_register;
	uint32_t direction_register;
} port_descriptions[] = {
	{8, 0xff, GPIO_PADR, GPIO_PADDR},
	{8, 0xff, GPIO_PBDR, GPIO_PBDDR},
	{1, 0x01, GPIO_PCDR, GPIO_PCDDR},
	{0}, // no port d
	{2, 0x03, GPIO_PEDR, GPIO_PFDDR},
	{3, 0x07, GPIO_PFDR, GPIO_PFDDR},
	{2, 0x03, GPIO_PGDR, GPIO_PGDDR},
	{4, 0x0f, GPIO_PHDR, GPIO_PHDDR},
};

typedef struct mapping {
	uint32_t page;
	uint32_t mapping;
} t_mapping;
	
static volatile void *
map_address(uint32_t address) {
	static int page_size, fd, n_mapped_pages, mappings_array_length;
	static t_mapping *mappings_array;
	void *mapping;
	uint32_t page, offset;
	int i;
	
	if (!page_size) {
		if (!(page_size = getpagesize()))
			die("Can not get page size");
		if ((fd = open("/dev/mem", O_RDWR|O_RSYNC)) == -1)
			die("Can not open /dev/mem");
		mappings_array_length = 32;
		mappings_array = salloc(mappings_array_length * sizeof *mappings_array);
	}
	offset = address % page_size;
	page = address - offset;
	for (i = 0; i < n_mapped_pages; i++)
		if (mappings_array[i].page == page)
			break;
	if (i == n_mapped_pages) {
		if (n_mapped_pages == mappings_array_length) {
			mappings_array_length += 32;
			mappings_array = srealloc(mappings_array, mappings_array_length * sizeof *mappings_array);
		}
		mappings_array[i].page = page; 
		if (!(mappings_array[i].mapping = (uint32_t)mmap(0, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page)))
			die("Can not map address 0x%x", address);
		dp(33, "map_address: page %x mapped to %x\n", page,  mappings_array[i].mapping);
		n_mapped_pages++;
	}
	mapping = (void *)(mappings_array[i].mapping + offset);
	dp(33, "map_address(%x) -> %x (offset=%x)\n", address,  mapping, offset);
	return mapping;
}

/*
	value
	00 Watchdog Disabled
	01 250 mS
	02 500 mS
	03 1 second
	04 -- Reserved
	05 2 seconds
	06 4 seconds
	07 8 seconds
*/

void
ts7200_watchdog_set(uint8_t value) {
	volatile uint8_t *s = map_address(0x23800000);
	volatile uint8_t *t = map_address(0x23C00000);
	// write  to the two addresses need to occur within 30us
	*t = 0x05;
	*s = value;
}

void
ts7200_watchdog_feed(void) {
	*(uint8_t *)map_address(0x23C00000) = 0x05;
}

static void ssp_enable(void) {
	volatile uint32_t *sspcr1 = map_address(SSPCR1);
	*sspcr1 = 0x10;
	*(uint32_t *)map_address(SSPCR0) = 0x0F;
	*(uint32_t *)map_address(SSPCPSR) = 0xFE;
	*sspcr1 = 0;
	*sspcr1 = 0x10;
}

//static void ssp_disable(void) {
//	*map_address(SSPCR1) = 0;
//}

double
ts7200_temperature(void) {
	volatile uint32_t *sspdr = map_address(SSPDR);
	volatile uint32_t *chip_select_data = map_address(GPIO_PFDR);
	volatile uint32_t *chip_select_direction = map_address(GPIO_PFDDR);
	uint32_t sensor;
	double temperature;
	ssp_enable();
	*chip_select_direction = 0x04;
	*chip_select_data = 0;
	*sspdr = 0x8000;
	usleep(1000);	
	*chip_select_direction = 0;
	sensor = *sspdr;
	dp(33, "raw sensor value = %x\n", sensor);
	sensor &= 0xffff;
	sensor >>= 3;
	temperature = sensor;
	if (sensor & 0x1000)
		temperature -= (1 << 13);
	temperature *= 0.0625;
	dp(30, "%.2fC\n", temperature);
	return temperature;
}

void
ts7200_gpio(action_t action, int gpio, int bit, uint32_t value) {
	volatile uint32_t *data_register = map_address(port_descriptions[gpio].data_register);
	volatile uint32_t *direction_register = map_address(port_descriptions[gpio].direction_register);
	int n_bits = port_descriptions[gpio].n_bits;
	int mask = port_descriptions[gpio].mask;
	int initial_direction = *direction_register & mask;
	int bitmask = 1 << bit;
	
	dp(30, "gpio=%d bit = %x bitmask=%x value=%d\n", gpio, bit, bitmask,value);
	if (bit >= n_bits)
		die("Bit %d invalid (%d bits)\n",  bit, n_bits);
	switch (action) {
	case A_DIRECTION:
		printf("%s\n", initial_direction&bitmask ? "output" : "input");
		break;
	case A_GET:
		*direction_register = initial_direction & !bitmask;
		dp(33, "*%p = %x\n", direction_register, initial_direction & ~bitmask);
		dp(33, "((*%p = %x) >> %d)  & 0x1 = %x\n", data_register, *data_register,bit,(*data_register >> bit) & 0x1);
		printf("%d\n", (*data_register >> bit) & 0x1);
		dp(33, "*%p = %x\n", direction_register, initial_direction);
		*direction_register = initial_direction;
		break;
	case A_SET_INPUT:
		*direction_register = initial_direction & ~bitmask;
		break;
	case A_SET_OUTPUT:
		*direction_register = initial_direction & bitmask;
		break;
	case A_SET:
		*direction_register |= bitmask;
		dp(33, "*%p = %x | %x\n", direction_register, *direction_register, bitmask);
		if (value) {
			*data_register |= bitmask;
			dp(33, "*%p = %x | %x\n", data_register, *data_register, bitmask);
		} else {
			*data_register &= ~bitmask;
			dp(33, "*%p = %x & %x\n", data_register, *data_register, ~bitmask);
		}
		break;
	default:
		die("Unsupported action '%d'", action);		
	}
}

void
ts7200_pulse(unsigned long pulse_milliseconds, int gpio, int bit, uint32_t value) {
	ts7200_gpio(A_SET, gpio, bit, value);
	msleep(pulse_milliseconds);
	ts7200_gpio(A_SET, gpio, bit, !value);
}

#endif

void
beep(int n_beeps, int beep_msec, int inter_beep_msec) {
	if (get_option_boolean("beep")) {
		dp(10, "beep(%d, %d, %d)\n", n_beeps, beep_msec, inter_beep_msec);
		while (n_beeps--) {
#ifdef TS7200
			if (get_option_boolean("active_high"))
				ts7200_pulse(beep_msec, 1, 1, 1);
			else
				ts7200_pulse(beep_msec, 1, 1, 0);
#endif
			if(n_beeps) msleep(inter_beep_msec);
		}
	}
}


#include <time.h>
void
msleep(int milliseconds) {
	dp(0, "msleep(%d)\n", milliseconds);
	struct timespec t;
	t.tv_sec = milliseconds/1000;
	t.tv_nsec = (milliseconds%1000)*1000000L;
	dp(30, "nanosleep tv_sec=%lu tv_nsec=%lu\n", (unsigned long)t.tv_sec, (unsigned long)t.tv_nsec);
	nanosleep(&t, 0);  // don't worry about signals
}
