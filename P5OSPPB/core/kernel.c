#include "../process/process.h"
#include "kernel.h"
#include "../ascii_io/ascii_i.h"
#include "../ascii_io/ascii_o.h"
#include "cicomp.h"
#include "../fat12/hiinter.h"
#include "commands.h"
#include "util.h"
#include "int.h"
#include "../ascii_io/keyboard.h"
#include "../memory/memory.h"
#include "../memory/paging.h"
#include "../memory/gdt.h"
#include "../fs/fs.h"
#include "../fs/ramfs.h"
#include "../block/block.h"
#include "../block/ramdisk.h"
#include "../timer/timer.h"


extern long _pkgoffset;
extern char _imagename;
char prompt[] = "P5-> ";
char inbuf[50];
char* usrCode = (char*)0x80000;

int main(void) {

    unsigned int i, doffset, *sizes;
    unsigned char *dcount;
    context* ctx;
    block_dev* ram0;
    FILE hellofile;
    unsigned char listBuf[256];
    int tempCh = 0;
    int key_stat;

    __asm__ ("cli");

    initScreen();
    setColor(0x1F);
    clear();

/* fix this
  if(!enableA20())
  {
        prints("FATAL ERROR: Could not enable the A20 line.\nP5 will now halt.");
        while(1);
  }
*/
    prints("Setting up interrupt table...");
    initIDT();
    installExceptionHandlers();
    prints("Done.\nSetting up the GDT...");
    initGdt();
    prints("Done.\nTurning on paging...");
    init_mmu();
    prints("Done.\nSetting up keyboard...");

    if((key_stat = keyboard_init()) != 1) {
        prints("Failed (");
        printHexByte((unsigned char)(key_stat & 0xFF));
        prints(")\n[P5]: No input device availible.\n");
    } else {
        prints("Done.\n");
    }

    //REMOVE ME
    //Hang so we can see what the keyboard messages say
    while(1);

    init_memory();
    setupKeyTable();
    startProcessManagement();
    init_timer();
    timer_on();
    prints("WELCOME TO P5\n");

    //ksize = (unsigned int*)0x100005;

    dcount = (unsigned char*)((char*)0x100000+_pkgoffset);
    sizes = (unsigned int*)((char*)0x100001+_pkgoffset);

    if(!dcount) {

        prints("No modules found.\n");
    } else {

        doffset = 5 + ((dcount[0]-1)*4);
        for(i = 0; i < dcount[0]; i++) {
            prints("0x");
            printHexDword(sizes[i]);
            prints(" byte module found at image+0x");
            printHexDword(doffset);
            pchar('\n');
            doffset += sizes[i];
        }
    }

    //Print kernel size and version
    prints("Image: ");
    prints(&_imagename);
    prints("\nSize: ");
    printHexDword(_pkgoffset);
    prints("b\n");


    /*
    //Test V86 mode
    prints("Installing V86 code...");
    usrCode[0] = 0xB8;
    usrCode[1] = 0x13;
    usrCode[2] = 0x00;
    usrCode[3] = 0xCD;
    usrCode[4] = 0x10;
    usrCode[5] = 0xB8;
    usrCode[6] = 0x00;
    usrCode[7] = 0x00;
    usrCode[8] = 0xCD;
    usrCode[9] = 0xFF;
    prints("Done.\n");
    prints("Entering V86 mode\n");
    ctx = newV86Proc();
    setProcEntry(ctx, (void*)usrCode);
    startProc(ctx);
    */

    prints("listBuf is at 0x"); printHexDword((unsigned int)listBuf); prints("\n");
    prints("Initializing filesystem\n");
    fs_init();

    //create a ramdisk device from the extents of the kernel payload
    //then install its fs driver and finally mount the ramdisk on root
    prints("Calculating offset to ramdisk\n");
    doffset = 0x100005 + _pkgoffset;
    prints("Creating new ramdisk block device ram0...");
    ram0 = blk_ram_new(doffset, sizes[0]);
    prints("Done\nInstalling ramfs filesystem driver...");
    fs_install_driver(get_ramfs_driver());
    prints("Done\nAttaching ramfs filesystem on ram0...");
    fs_attach(FS_RAMFS, ram0, ":");

    //This is the final culmination of all our FS and process work
    prints("Done\n");
    file_list(":", listBuf);

    for(i = 0; listBuf[i]; i++)
        if(listBuf[i] == ':') listBuf[i] = '\n';

    prints("Directory listing: \n");
    prints(listBuf);
    file_open(":test.txt", &hellofile);

    if(!hellofile.id) {

        prints("File could not be opened.");
        while(1);
    }

    prints("File contents:\n\n");

    while((tempCh = file_readb(&hellofile)) != EOF)
        pchar((char)tempCh);

    //__asm__ ("sti");
    enterProc(exec_process(":usr.mod", 1));

    prints("Initial process could not be started.\n");
    while(1);
}


//Start usr prompt
void sys_console() {

    while(1){
        prints(prompt);
        //scans(50, inbuf);
        //parse(inbuf);
        while(1);
    }
}
