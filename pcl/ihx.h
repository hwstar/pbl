
#define HEXMAXLINE 256
#define HEXBUFFER 65536

typedef struct{
	unsigned short load_address;
	unsigned size;
	unsigned char *buf;
} ihx_t;

void ihx_free(ihx_t *p);
ihx_t *ihx_read(char *path, unsigned char *buffer, unsigned int max_bytes);
void ihx_debug_dump(ihx_t *ihx);

