// qemu-system-i386 -enable-kvm -cpu host -m 6G -net none -drive file=/mnt/wd500/images/windows.img,if=none,id=disk1 -device ide-hd,drive=disk1,bootindex=2 -option-rom rom_code.bin,bootindex=1

void putchar(c);
void setdateseg();
void int13();
int getcodeseg();
int getstackseg();
void hookint13();
int read(ax, cx, dx, es, bx, ds, si);
int old_addr, old_seg;
void call_old_int13();
int test(ds, si);
unsigned char inb(port);
void outb(port, byte);
void ser_put(byte);
unsigned char set_get();

void copy(src_seg, src_off, dst_seg, dst_off, len);
void copy(src_seg, src_off, dst_seg, dst_off, len);

static unsigned int cs;
static unsigned int ss;
static unsigned char hdd_num;
static unsigned char sector_buf[512];

struct ext_read_packet_struct
{
	unsigned char dap;
	unsigned char zero;
	unsigned int n;
	unsigned int offset;
	unsigned int seg;
	unsigned int lba0;
	unsigned int lba1;
	unsigned int lba2;
	unsigned int lba3;
};

struct ext_read_param_struct
{
	unsigned int size;
	unsigned int info;
	unsigned int num_cyl0;
	unsigned int num_cyl1;
	unsigned int num_head0;
	unsigned int num_head1;
	unsigned int num_sect_per_track0;
	unsigned int num_sect_per_track1;
	unsigned int total_sect0;
	unsigned int total_sect1;
	unsigned int total_sect2;
	unsigned int total_sect3;
	unsigned int bytes_per_sector;
#if 0
	unsigned char opt0;
	unsigned char opt1;
	unsigned char opt2;
	unsigned char opt3;
#endif
};

#define STC f |= 1
#define CLC f &= ~1

#define ROM_SIZE 8192

#define NUM_HEADS (256)
#define SECTS_PER_TRACK (63)
#define NUM_CYL (1024)

void puts(s)
char* s;
{
	while (*s)
	{
		putchar(*s);
		s++;
	}
}

void printhex8(num)
unsigned char num;
{
    int i;
    unsigned char hexDigit;

    for (i = 0; i < 2; i++) {
        hexDigit = num >> 4;;
        if (hexDigit < 10) {
            putchar('0' + hexDigit);
        } else {
            putchar('A' + hexDigit - 10);
        }

	num <<= 4;
    }
}

void printhex(num) {
    printhex8(num >> 8);
    printhex8(num & 0xff);
}

void print_sector_buf()
{
	unsigned int i;

	for (i = 0; i < 4; i++)
	{
#if 0
		putchar(sector_buf[i]);
#else
		printhex8(sector_buf[i]);
		putchar(' ');
#endif
	}

	puts("\r\n");
}

unsigned int read_sectors(lba, seg, offs, n_sect)
	unsigned int lba;
	unsigned int seg;
	unsigned int offs;
	unsigned char n_sect;
{
#if 0
	struct ext_read_packet_struct ext_read_packet;

	ext_read_packet.dap = 0x10;
	ext_read_packet.zero = 0;
	ext_read_packet.n = n_sect;
	ext_read_packet.seg = seg;
	ext_read_packet.offset = offs;
	ext_read_packet.lba0 = lba & 0xffff;
	ext_read_packet.lba1 = (lba >> 16) & 0xffff;
	ext_read_packet.lba2 = 0;
	ext_read_packet.lba3 = 0;

	return read(0x4200, 0, 0x0081, 0, 0, ss, &ext_read_packet);
#else
	unsigned int i;

	ser_put(lba & 0xff);
	ser_put((lba >> 8) & 0xff);
	ser_put(n_sect & 0xff);
	ser_put((n_sect >> 8) & 0xff);

	for(i = 0; i < n_sect; i++)
	{
		unsigned int j;
		for (j =0; j < 512; j++)
		{
			sector_buf[j] = ser_get();
		}

		copy(cs, sector_buf, seg, offs + i * 512, 512);
	}

	return n_sect;
#endif
}

unsigned char get_num_drives()
{
	unsigned char n;

	copy(0x0040, 0x0075, ss, &n, 1);
	return n;
}

void set_num_drives(n)
unsigned char n;
{
	copy(ss, &n, 0x0040, 0x0075, 1);
}

/* Registers are popped back off the stack after calling this function
 * which means you can alter them by simply writing to them */
void int13handler(f, di, si, bp, bx, dx, cx, ax, es, ds)
{
	unsigned char al = ax & 0xff;
	unsigned char ah = ax >> 8;
	unsigned char dl = dx & 0xff;
	unsigned char n;
	int i;
	int tmp;

//	puts("\r\n");
	
	ss = getstackseg();

	if ((dx & 0xff) != hdd_num)
	{
		STC;
		return;
	}

	switch (ax >> 8)
	{
		case 0x00: // Reset disk system
//			puts("Entered 00h\r\n");
			CLC;
			ax &= 0x00ff;
			return;

		case 0x08: // Get geometry
//			puts("Entered 08h\r\n");
			ax &= 0x00ff;
			dx = (dx & 0x00ff) | ((unsigned char) NUM_HEADS - 1) <<8;
			cx = ((NUM_CYL - 1) << 6) | SECTS_PER_TRACK;
			bx &= 0xff00;
			CLC;
			return;

		case 0x02: // Read sectors
		{
			unsigned char n = ax & 0xff;
			unsigned int c = cx >> 6;
			unsigned char h = dx >> 8;
			unsigned char s = cx & 0x3f;

//			puts("Entered 02h\r\n");
			// XXX: Need 32-bit operation support!
//			unsigned long int lba = (((unsigned long int) c << 7) + ((unsigned long int) h)
//				<< 5) + (s - 1);
			unsigned int lba = ((((unsigned int) c * NUM_HEADS) + ((unsigned int) h)) * SECTS_PER_TRACK) + (s - 1);

//			puts("Entered 02h\r\n");

			tmp = read_sectors(lba, es, bx, n);

#if 0
			puts("Read ");
			printhex(n);
			putchar('@');
			printhex(lba);
			puts("\r\n");
#endif

//			copy(es, bx, cs, sector_buf, 512);
//			print_sector_buf();

			ax = 0x0001;
			CLC;
			return;
		}

		case 0x15: // Read drive type
//			puts("Entered 15h\r\n");
			ax = 0;
			CLC;
			return;

		case 0x41: // Extensions present
//			puts("Entered 41h\r\n");
			if (bx != 0x55aa)
			{
				puts("Unsupported bx=");
				printhex(bx);
				puts("\r\nHalted.");
				#asm
					cli
					hlt
				#endasm
				break;
			}

			ax = 0x0100; // v1.x (??) (see https://mybogi.files.wordpress.com/2011/08/interrupt-13h.pdf)
			bx = 0xaa55;
			cx = 1; // Device Access using the packet structure

			CLC;
			return;

		case 0x42: // Read sectors extended
		{
			unsigned int lba;
			struct ext_read_packet_struct ext_read_packet;

//			puts("Entered 42h\r\n");
			copy(ds, si, ss, &ext_read_packet, sizeof(struct ext_read_packet_struct));

			lba = ext_read_packet.lba0 | ext_read_packet.lba1 << 16;
			tmp = read_sectors(lba, ext_read_packet.seg, ext_read_packet.offset, ext_read_packet.n);

#if 0
			puts("Read ");
			printhex(ext_read_packet.n);
			putchar('@');
			printhex(lba);
			puts("\r\n");
#endif

//			copy(ext_read_packet.seg, ext_read_packet.offset, cs, sector_buf, 512);

			ax &= 0x00ff;
			CLC;
			return;
		}

		case 0x48: // Extended Read Drive Parameters
		{
			struct ext_read_param_struct ext_read_param;
			unsigned int size;

//			puts("Entered 48h\r\n");
			ext_read_param.size = 0;
			copy(ds, si, ss, &size, 0x1e);

			if (size != sizeof(struct ext_read_param_struct))
			{
				puts("INT13h AH=48h only supports size of 0x1a\r\n");
				#asm
					cli
					hlt
				#endasm
			}

	        	ext_read_param.size = sizeof(struct ext_read_param_struct);
	        	ext_read_param.info = 0;
	        	ext_read_param.num_cyl0 = NUM_CYL;
	        	ext_read_param.num_cyl1 = 0;
	        	ext_read_param.num_head0 = NUM_HEADS;
        		ext_read_param.num_head1 = 0;
        		ext_read_param.num_sect_per_track0 = SECTS_PER_TRACK;
        		ext_read_param.num_sect_per_track1 = 0;
        		ext_read_param.total_sect0 = 0;
        		ext_read_param.total_sect1 = 252; // 1024 * 256 * 63 / 65536 = 252
        		ext_read_param.total_sect2 = 0;
        		ext_read_param.total_sect3 = 0;
			ext_read_param.bytes_per_sector = 0x0200;

			copy(ss, &ext_read_param, ds, si, size);

			ax = 0x00ff;
			CLC;
			return;
		}

		default:
			puts("Unsupported ah=");
			printhex8(ax >> 8);
			puts("\r\nHalted.");
			#asm
				cli
				hlt
			#endasm
			return;
	}

	return;
}

void rechecksum()
{
	unsigned char csum;
	int i;

	for (i = 0; i < ROM_SIZE - 1; i++)
	{
		csum -= *(unsigned char*) i;
	}

	*(unsigned char*) (ROM_SIZE - 1) = csum;
}

void kernel_init()
{
	char n;

	cs = getcodeseg();
	ss = getstackseg();

	n = get_num_drives();
	puts("Number of drives at init: ");
	printhex8(n);
	puts("h\r\n");

	rechecksum();
}

/* TODO:
 * From https://www.scs.stanford.edu/nyu/04fa/lab/specsbbs101.pdf
 *   A controller checks if the location at 0040:0075 is zero to determine if it is the first
 *   to install. If it is first, the controller must copy the INT 13h vector over the INT
 *   40h vector so that floppy services are handled properly.
 */
int kernel_main()
{
	char n;
	unsigned int com1;

	cs = getcodeseg();
	ss = getstackseg();

	puts("kernel_main cs =");
	printhex(cs);
	puts("\r\n");

	n = get_num_drives();
	hdd_num = 0x80 + n;
	puts("Number of drives before hook: ");
	printhex8(n);
	puts("h -- Registering hard drive number ");
	printhex8(hdd_num);
	puts("h\r\n");

	set_num_drives(n + 1);

	hookint13();
}

