//
// eb-sflash: tool for programming scu slave card remotly with new gateware
// 

//standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
//Etherbone
#include <etherbone.h>
#include "crc8.h"
#include "crc32.h"


#define GSI_ID      0x651
#define CERN_ID     0xce42
#define WB_ASMI_ID  0x48526423


// all scu bus addresses shifted left by one bit
#define WB_SCU_REG    0x80
#define TEMP_REG      0x88
#define ASMI_PARAM    0xa0  //32-Bit
#define ASMI_CMD      0xa6  //16-Bit
#define ASMI_STAT     0xaa  //16-Bit
#define ASMI_ID       0xae  //16-Bit
#define ASMI_BUFFER   0xc0  //16-Bit
#define CID_SYS       0x08
#define CID_GRP       0x0a

#define ASMI_STATUS_CMD   0x1
#define ASMI_ID_CMD       0x2
#define SECTOR_ERASE_CMD  0x3
#define PAGE_WRITE_CMD    0x4
#define PAGE_READ_CMD     0x5
#define RECONFIG_CMD      0x6

#define FLASH_ACCESS      0x0
#define READ_STATUS       0x4
#define READ_ID           0x8
#define SECTOR_ERASE      0xc
#define SET_ADDR          0x10
#define WRITE_BUFFER      0x14
#define FIFO_READ         0x18
#define BUSY_CHECK        0x1c
#define READ_CRC          0x20
#define SET_READ_NUMBER   0x24
#define BULK_ERASE        0x28

#define SDB_DEVICES       3
#define SLAVENR           3
#define PAGE_SIZE         256
#define PAGES_PER_SECTOR  256 
#define SECTOR_SIZE       PAGE_SIZE * PAGES_PER_SECTOR
#define EPCS128ID         0x18
#define EPCS1024ID        0x21
#define MAX_EPCS128_ADDR  0xffff00
#define RPD_SIZE          0x2000000
#define BLANK_CRC         0xfea8a821
#define BLANK_SECTOR_CRC  0xdeab7e4e

static const char* devName;
static const char* program;
static eb_address_t wb_asmi_base;
static eb_device_t device;
static eb_cycle_t  cycle;
static eb_socket_t socket;

eb_data_t flash_page[PAGE_SIZE];
eb_data_t flash_page1[PAGE_SIZE];
unsigned char file_page[PAGE_SIZE];
unsigned char file_sector[SECTOR_SIZE];
unsigned int waddr;
unsigned char *sectors_to_erase;


void itoa(unsigned int n,char s[], int base){
     int i;
 
     i = 0;
     do {                           /* generate digits in reverse order */
         s[i++] = n % base + '0';   /* get next digit */
     } while ((n /= base) > 0);     /* delete it */
     s[i] = '\0';
}

static const unsigned char BitReverseTable256[] = 
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

unsigned char flip(unsigned char byte) {
  return BitReverseTable256[byte];
}


void die_eb(const char* where,eb_status_t status) {
  fprintf(stderr,"%s: %s failed: %s\n",
    program,where, eb_status(status));
  exit(1);
}

void die(const char* where,eb_status_t status) {
  fprintf(stderr,"%s: %s failed: %s\n",
    program,where, eb_status(status));
  exit(1);
}

void read_asmi_id(int slave_nr, eb_data_t* epcsid) {
  eb_status_t status;

  if ((status = eb_device_read(device, wb_asmi_base + READ_ID, EB_BIG_ENDIAN|EB_DATA8, epcsid, 0, eb_block)) != EB_OK)
    die("reading ASMI_ID failed", status);
}

void read_asmi_status(int slave_nr, eb_data_t* epcs_status) {
  eb_status_t status;

  if ((status = eb_device_read(device, wb_asmi_base + READ_STATUS, EB_BIG_ENDIAN|EB_DATA8, epcs_status, 0, eb_block)) != EB_OK)
    die("reading ASMI_STAT failed", status);
}

void read_asmi_crc(unsigned int asmi_addr, unsigned int bytes_to_read, eb_data_t* crc) {
  eb_status_t status;
  eb_data_t data;
  eb_data_t cmd = 1;

  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
    die("EP eb_cycle_open", status);

  eb_cycle_write(cycle, wb_asmi_base + SET_READ_NUMBER, EB_BIG_ENDIAN|EB_DATA32, bytes_to_read);
  eb_cycle_write(cycle, wb_asmi_base + SET_ADDR, EB_BIG_ENDIAN|EB_DATA32, asmi_addr);
  eb_cycle_read(cycle, wb_asmi_base + FLASH_ACCESS, EB_BIG_ENDIAN|EB_DATA32, &data);
    
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("read data eb_cycle_close", status);

  while (cmd != 0) {
    if ((status = eb_device_read(device, wb_asmi_base + BUSY_CHECK, EB_BIG_ENDIAN|EB_DATA32, &cmd, 0, eb_block)) != EB_OK)
      die("reading BUSY_CHECK failed", status);
  }

  if ((status = eb_device_read(device, wb_asmi_base + READ_CRC, EB_BIG_ENDIAN|EB_DATA32, crc, 0, eb_block)) != EB_OK)
    die("read crc failed", status);
}

void read_asmi_page(eb_data_t* page_buffer, int asmi_addr, eb_data_t* crc) {
  eb_status_t status;
  eb_data_t data;
  int i;
  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
    die("EP eb_cycle_open", status);

  eb_cycle_write(cycle, wb_asmi_base + SET_READ_NUMBER, EB_BIG_ENDIAN|EB_DATA32, PAGE_SIZE);
  eb_cycle_write(cycle, wb_asmi_base + SET_ADDR, EB_BIG_ENDIAN|EB_DATA32, asmi_addr);
  eb_cycle_read(cycle, wb_asmi_base + FLASH_ACCESS, EB_BIG_ENDIAN|EB_DATA32, &data);
  eb_cycle_read(cycle, wb_asmi_base + READ_CRC, EB_BIG_ENDIAN|EB_DATA32, crc);

  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("read data eb_cycle_close", status);

  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
    die("EP eb_cycle_open", status);
  // read from fifo into page buffer
  for(i = 0; i < PAGE_SIZE; i++)
    eb_cycle_read(cycle, wb_asmi_base + FIFO_READ, EB_BIG_ENDIAN|EB_DATA8, &page_buffer[i]);
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("read data eb_cycle_close", status);

}
void write_asmi_page(int slave_nr, eb_data_t* page_buffer, int asmi_addr) {
  eb_status_t status;
  eb_data_t cmd = 1;
  int i;
  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
	  die("EP eb_cycle_open", status);
  for(i = 0; i < PAGE_SIZE; i++) {
    eb_cycle_write(cycle, wb_asmi_base + FLASH_ACCESS, EB_BIG_ENDIAN|EB_DATA8, page_buffer[i]);
  }
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("write data eb_cycle_close", status);
  
  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
	  die("EP eb_cycle_open", status);
  eb_cycle_write(cycle, wb_asmi_base + SET_ADDR, EB_BIG_ENDIAN|EB_DATA32, asmi_addr);
  eb_cycle_write(cycle, wb_asmi_base + WRITE_BUFFER, EB_BIG_ENDIAN|EB_DATA32, asmi_addr);
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("write page cmd eb_cycle_close", status);

  while (cmd != 0) {
    if ((status = eb_device_read(device, wb_asmi_base + BUSY_CHECK, EB_BIG_ENDIAN|EB_DATA32, &cmd, 0, eb_block)) != EB_OK)
      die("reading BUSY_CHECK failed", status);
  }
}
  
void erase_asmi_sector(int asmi_addr) {
  eb_status_t status;
  eb_data_t cmd = 1;

  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
    die("EP eb_cycle_open", status);
  eb_cycle_write(cycle, wb_asmi_base + SECTOR_ERASE, EB_BIG_ENDIAN|EB_DATA32, asmi_addr);
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("SECTOR_ERASE eb_cycle_close", status);

  while (cmd != 0) {
    if ((status = eb_device_read(device, wb_asmi_base + BUSY_CHECK, EB_BIG_ENDIAN|EB_DATA32, &cmd, 0, eb_block)) != EB_OK)
      die("reading BUSY_CHECK failed", status);
  }
}
void erase_asmi_bulk() {
  eb_status_t status;
  eb_data_t cmd = 1;

  if ((status = eb_cycle_open(device,0, eb_block, &cycle)) != EB_OK)
    die("EP eb_cycle_open", status);
  eb_cycle_write(cycle, wb_asmi_base + BULK_ERASE, EB_BIG_ENDIAN|EB_DATA32, 0);
  if ((status = eb_cycle_close(cycle)) != EB_OK)
    die("SECTOR_ERASE eb_cycle_close", status);

  while (cmd != 0) {
    if ((status = eb_device_read(device, wb_asmi_base + BUSY_CHECK, EB_BIG_ENDIAN|EB_DATA32, &cmd, 0, eb_block)) != EB_OK)
      die("reading BUSY_CHECK failed", status);
  }
}

void reconfig(int slave_nr, int asmi_addr) {

}

int kbhit(void)
{
  struct termios oldt, newt;
  int ch;
  int oldf;
 
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
  ch = getchar();
 
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  fcntl(STDIN_FILENO, F_SETFL, oldf);
 
  if(ch != EOF)
  {
    ungetc(ch, stdin);
    return 1;
  }
 
  return 0;
}


void show_help() {
  printf("Usage: eb-sflash [OPTION] <proto/host/port>\n");
  printf("\n");
  printf("-h          show the help for this program\n");
  printf("-b          blank check the flash\n");
  printf("-e <size>   erase sectors up to size\n");
  printf("-w <file>   write programming file into flash\n");
  printf("-v <file>   verify flash against programming file\n");
  printf("-n          no erase before writing; use with -w\n");
}

unsigned int how_many_sectors(unsigned int size) {
    unsigned int pages_in_file;
    unsigned int needed_sectors;
    //how many sectors need to be erased?
    pages_in_file = size / PAGE_SIZE;
    needed_sectors = pages_in_file / PAGES_PER_SECTOR;
    if (pages_in_file % PAGES_PER_SECTOR)
      needed_sectors += 1;
    return needed_sectors;
}

void erase_flash(int epcsid, int needed_sectors) {
  int i;
  //delete sector
  for (i = 0; i < needed_sectors; i++) {
    //erase_asmi_sector(i * PAGE_SIZE * PAGES_PER_SECTOR);
    if (sectors_to_erase[i]) {
      printf("erase epcs addr 0x%x\r", i * PAGE_SIZE * PAGES_PER_SECTOR);
      fflush(stdout);
      erase_asmi_sector(i * 0x10000);
    }
  }
}


int main(int argc, char * const* argv) {
  eb_status_t status;
  struct sdb_device sdbDevice[SDB_DEVICES];

  int nDevices;  
  int slave_id = 0;
  eb_data_t epcsid;
  eb_data_t epcs_status;

  char *wvalue = NULL;
  int rflag = 0;
  char *vvalue = NULL; 
  int bflag = 0;
  int eflag = 0;
  int nflag = 0;
  int index;
  int c;
 
  int i;
  int epcs_addr = 0;
  eb_data_t crc_hw = 0;
  unsigned int crc = 0;


  opterr = 0;

  while ((c = getopt (argc, argv, "w:rv:bhen")) != -1)
    switch (c)
      {
        case 'e':
          eflag = 1;
          break;
        case 'w':
          wvalue = optarg;
          break;
        case 'n':
          nflag = 1;
          break;
        case 'r':
          rflag = 1;
          break;
        case 'v':
          vvalue = optarg;
          break;
        case 'b':
          bflag = 1;
          break;
        case 'h':
          show_help();
          exit(1);
        case '?':
          if (optopt == 'w' || optopt == 'v' || optopt == 's')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
        return 1;
      default:
        abort ();
      }


  // assign non option arguments
  index = optind;

  

  if (argc < 2 || argc-optind < 1) {
    printf("program needs at least the device name of the etherbone device\n");
    printf("e.g. %s dev/wbm0 -s1\n", argv[0]);
    exit(0);
  }

  if (index < argc) {
    devName = argv[index];
    index++;
  }
 
 
  errno = 0;


  if (index < argc) {
    char *p;
    errno = 0;
    long conv = strtol(argv[index], &p, 16);
    index++;
    
    if (errno != 0 || *p != '\0' || conv < 0 || conv > RPD_SIZE) {
      printf("epcs address out of range 0x0 - 0xfc0000\n");
    } else {
      epcs_addr = conv;    
    }
  }
  
  /* Open a socket supporting only 32-bit operations.
   * As we are not exporting any slaves^Mwe don't care what port we get => 0.
   * This function always returns immediately.
   * EB_ABI_CODE helps detect if the application matches the library.
   */
  if ((status = eb_socket_open(EB_ABI_CODE,0, EB_ADDR32|EB_DATA32, &socket)) != EB_OK)
    die("eb_socket_open",status);
  
  /* Open the remote device with 3 attemptis to negotiate bus width.
   * This function is blocking and may stall the thread for up to 3 seconds.
   * If you need asynchronous open^Msee eb_device_open_nb.
   * Note: the supported widths can never be more than the socket supports.
   */
  if ((status = eb_device_open(socket,devName, EB_ADDR32|EB_DATA32, 3, &device)) != EB_OK)
    die("eb_device_open",status);

  nDevices = 1;
  if ((status = eb_sdb_find_by_identity(device, GSI_ID, WB_ASMI_ID, sdbDevice, &nDevices)) != EB_OK)
    die("find_by_identiy failed", status);

  if (nDevices == 0)
    die("no WB_ASMI found", EB_FAIL);
  if (nDevices > 1)
    die("more then one WB_ASMI", EB_FAIL);
  
  /* Record the address of the device */
  wb_asmi_base = sdbDevice[0].sdb_component.addr_first;
  

  read_asmi_id(slave_id, &epcsid);
  /*if ((int)epcsid != EPCS128ID) {
    printf("No EPCS128 found!\n");
    exit(1);
  }*/
    
  read_asmi_status(slave_id, &epcs_status);
   

  printf("EPCSID: 0x%"EB_DATA_FMT"\n", epcsid);
  printf("EPCS STATUS: 0x%"EB_DATA_FMT"\n", epcs_status);
  
  FILE *fp;

  //erase sectors
  if (eflag == 1) {
    int size = epcs_addr;
    int needed_sectors;
    if (size % PAGE_SIZE) {
      printf("size is not a multiple of %d\n", PAGE_SIZE);
      exit(1);
    }

    needed_sectors = how_many_sectors(size);
    printf("%d sector(s) will be erased.\n", needed_sectors);  
    sectors_to_erase = (unsigned char*)calloc(needed_sectors, sizeof(unsigned char));
    for(i = 0; i<needed_sectors; i++)
      sectors_to_erase[i] = 1;
   
    erase_flash(epcsid, needed_sectors);

    printf("%d sectors erased.                \n", needed_sectors);
  }


  //write file
  if (wvalue != NULL) {
    if ((fp = fopen(wvalue, "r")) == NULL) {
      printf("open of programming file not successful.\n");
      exit(1);
    }
    struct stat buf;
    stat(wvalue, &buf);
    int size = buf.st_size;
    int needed_sectors;
    int blank_page;
    int j;
    if (size % PAGE_SIZE) {
      printf("size of programming file is not a multiple of %d\n", PAGE_SIZE);
      exit(1);
    }
    printf("filesize: %d bytes\n", size);
    needed_sectors = how_many_sectors(size);

    //analyse sectors
    sectors_to_erase = (unsigned char*)calloc(needed_sectors, sizeof(unsigned char));
    if (sectors_to_erase == NULL) {
      printf("calloc failed!\n");
      exit(1);
    }
    
    waddr      = 0;
    j          = 0;
    blank_page = 0;
    fseek(fp, 0, SEEK_SET);
    while( fread(&file_sector, 1,  SECTOR_SIZE, fp) == SECTOR_SIZE) {
      //reverse bits before writing
      for(i = 0; i < SECTOR_SIZE; i++)
        file_sector[i] = reverse(file_sector[i]);
      crc = 0;
      crc = crc32_word(crc, &file_sector, SECTOR_SIZE);
      printf("epcs addr 0x%x checked\r", waddr);
      fflush(stdout);
      //check crc of written data
      read_asmi_crc(waddr, SECTOR_SIZE, &crc_hw);
      if (BLANK_SECTOR_CRC != (crc_hw)) {
        //sector needs to be erased
        sectors_to_erase[j++] = 1;
        blank_page++;
      }
      waddr += SECTOR_SIZE;
    }
    printf("Number of sectors to be erased: %d\n", blank_page);

    if (nflag == 0) {
      erase_flash(epcsid, needed_sectors);
    }

    //read in data from file
    waddr = 0;
    fseek(fp, 0, SEEK_SET);
    while( fread(&file_page, 1,  PAGE_SIZE, fp) == PAGE_SIZE) {
      //reverse bits before writing
      for(i = 0; i < PAGE_SIZE; i++)
        file_page[i] = reverse(file_page[i]);
      crc = 0;
      crc = crc32_word(crc, &file_page, PAGE_SIZE);

      for(i = 0; i < PAGE_SIZE; i++)
        flash_page[i] = file_page[i];

      write_asmi_page(slave_id, &flash_page[0], waddr);
      printf("                                                      \r");
      printf("epcs addr 0x%x written\r", waddr);
      fflush(stdout);

      //check crc of written data
      read_asmi_crc(waddr, PAGE_SIZE, &crc_hw);
      if (crc != (crc_hw)) {
          printf("\ncrc wrong in page 0x%x: 0x%x != 0x%"EB_DATA_FMT"\n", waddr, crc, crc_hw);
          exit(1);
      }
        
      waddr += PAGE_SIZE;
    }
    free(sectors_to_erase);
    fclose(fp);
    printf("                                             \r");
    printf("New image written to epcs.\n");
  }

  //read page
  if (rflag == 1) {
    read_asmi_page(&flash_page[0], epcs_addr, &crc_hw);
    printf("epcs addr 0x%x: \n", epcs_addr);

    
    for(i = 0; i < PAGE_SIZE; i++)
      printf("0x%"EB_DATA_FMT" ", flash_page[i]);

    printf("\n");
  }

  //verify flash against programming file
  if (vvalue != NULL) { 
   
    printf("Starting Verify...\n"); 
    if ((fp = fopen(vvalue, "r")) == NULL) {
      printf("open of programming file not successful.\n");
      exit(1);
    }
    struct stat buf;
    stat(vvalue, &buf);
    int size = buf.st_size;
    if (size % PAGE_SIZE) {
      printf("size of programming file is not a multiple of %d\n", PAGE_SIZE);
      exit(1);
    }
    printf("filesize: %d bytes\n", size);

    waddr = 0;
    while( fread(&file_page, 1,  PAGE_SIZE, fp) == PAGE_SIZE) {

      //reverse bits before writing
      for(i = 0; i < PAGE_SIZE; i++)
        file_page[i] = reverse(file_page[i]);
      crc = 0;
      crc = crc32_word(crc, &file_page, PAGE_SIZE);

      for(i = 0; i < PAGE_SIZE; i++)
        flash_page[i] = file_page[i];

      //check crc of written data
      read_asmi_crc(waddr, PAGE_SIZE, &crc_hw);
      if (crc != (crc_hw)) {
          printf("\ncrc wrong in page 0x%x: 0x%x != 0x%"EB_DATA_FMT"\n", waddr, crc, crc_hw);
          exit(1);
      }
        
      printf("epcs addr 0x%x checked\r", waddr);
      fflush(stdout);
      waddr += PAGE_SIZE;
    }
    fclose(fp);
    printf("                                             \r");
    printf("Verify successful!\n");
  }

  //blank check
  if (bflag == 1) { 
   
    printf("Starting blank check...\n"); 
    waddr = 0;
    while( waddr < RPD_SIZE ) {

      //check crc of written data
      read_asmi_crc(waddr, SECTOR_SIZE, &crc_hw);
      if (BLANK_SECTOR_CRC != (crc_hw)) {
          printf("\ncrc wrong in page 0x%x: 0x%x != 0x%"EB_DATA_FMT"\n", waddr, BLANK_SECTOR_CRC, crc_hw);
          exit(1);
      }
      waddr += SECTOR_SIZE;
    }
    printf("blank check successful!\n");
  }

  /* close handler cleanly */
  if ((status = eb_device_close(device)) != EB_OK)
    die("eb_device_close",status);
  if ((status = eb_socket_close(socket)) != EB_OK)
    die("eb_socket_close",status);
  
  return 0;
}
