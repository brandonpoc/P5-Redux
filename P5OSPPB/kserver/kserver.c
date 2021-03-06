#include "../process/process.h"
#include "../process/message.h"
#include "../ascii_io/ascii_i.h"
#include "../ascii_io/ascii_o.h"
#include "../memory/paging.h"
#include "../core/kernel.h"
#include "../core/irq.h"
#include "../timer/timer.h"
#include "kserver.h"

void post_to_kern(unsigned int source, unsigned int command, unsigned int payload) {

    unsigned int *pageTable = (unsigned int*)PAGE_TABLE_ADDRESS;
    int i;
    unsigned char tmp_char;
    process* new_proc;

    switch(command) {

        //message 0: terminate
        //look up the process ID for the calling process
        //and unload that process from the system
        case KS_QUIT:

            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            if(i == 256)
                return;

            endProc(&procTable[i]);

            break;

        //message 1: pchar
        //Print the character in payload to the screen
        case KS_PCHAR:
            pchar((unsigned int)(payload & 0xFF));
            break;

        //message 3: clear
        //Clear the screen
        case KS_CLEAR_SCREEN:
            clear();
            break;

        //message 4: startuserproc
        //Read the string in the userspace at address payload
        //and use it to start a process from the executable at
        //that path, then send a PROC_STARTED message with
        //the ID of the new process in the payload
        case KS_EXEC:
            passMessage(0, source, PROC_STARTED, exec_process((unsigned char*)payload, 0));
            break;

        //message 5: startsuperproc
        //Works the same as the above, but the created process is
        //a superproc. The new superproc may only be created if the
        //calling proc is also a superproc -- we cannot start superprocs
        //from userprocs because that would defeat the purpose. Once we
        //have users set up we will be able to start a superproc if the
        //owning user is a superuser, but that's down the road
        case KS_EXEC_SUPER:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            if(i == 256)
                return;

            if(!(procTable[i].flags & PF_SUPER))
                passMessage(0, source, PROC_STARTED, 0);
            else
                passMessage(0, source, PROC_STARTED, exec_process((unsigned char*)payload, 1));

            break;

        //Same as exec superproc call but starts a v86 process
        case KS_EXEC_V86:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            if(i == 256)
                return;

            if(!(procTable[i].flags & PF_SUPER))
                passMessage(0, source, PROC_STARTED, 0);
            else
                passMessage(0, source, PROC_STARTED, exec_v86((unsigned char*)payload));

            break;

        //Spawns a thread from the calling process and enters it at the 
        case KS_START_THREAD:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            if(i == 256)
                return;
            
            new_proc = makeThread(&procTable[i]);
            passMessage(0, source, KS_START_THREAD, (unsigned int)new_proc);
            
            //If process creation was successful, also message the client
            if(new_proc)
                passMessage(0, new_proc->id, KS_START_THREAD, 0);
                
            break;

        //Toggle debug: sets or resets the process's debug flag
        case KS_TOGGLE_DEBUG:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            if(i == 256)
                return;

            if(procTable[i].flags & PF_DEBUG)
                procTable[i].flags &= ~((unsigned int)PF_DEBUG);
            else
                procTable[i].flags |= PF_DEBUG;

            break;

        //Get build number: Get the build number of the OS
        case KS_GET_BUILD_NO:
            passMessage(0, source, BUILD_NUMBER, P5_BUILD_NUMBER);
            break;

        case KS_GET_PROC_CPU_PCT:
            for(i = 0; i < 256 && (procTable[i].id != payload); i++);

            if(i == 256) {

                passMessage(0, source, KS_GET_PROC_CPU_PCT, 0x0);
                return;
            }


            passMessage(0, source, KS_GET_PROC_CPU_PCT, procTable[i].cpu_pct);
            break;

        case KS_GET_PID:
            passMessage(0, source, KS_GET_PID, source);
            break;

        case KS_PID_FROM_SLOT:
            passMessage(0, source, KS_PID_FROM_SLOT, procTable[payload].id);
            break;

        case KS_REG_IRQ_1:
        case KS_REG_IRQ_2:
        case KS_REG_IRQ_3:
        case KS_REG_IRQ_4:
        case KS_REG_IRQ_5:
        case KS_REG_IRQ_6:
        case KS_REG_IRQ_7:
        case KS_REG_IRQ_8:
        case KS_REG_IRQ_9:
        case KS_REG_IRQ_A:
        case KS_REG_IRQ_B:
        case KS_REG_IRQ_C:
        case KS_REG_IRQ_D:
        case KS_REG_IRQ_E:
        case KS_REG_IRQ_F:

            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;

            if(!(procTable[i].flags & PF_SUPER)) {

                //If the process doesn't have super rights then tell it
                //registration failed
                passMessage(0, source, command, 0);
                return;
            }

            //Attempt to register the IRQ with the process and tell the
            //requester whether or not we succeeded
            passMessage(0, source, command, irq_register(command - KS_REG_IRQ_1 + 1, &procTable[i]));
            break;

        case KS_GET_PHYS_PAGE:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;

            if(!(procTable[i].flags & PF_SUPER)) {

                //If the process doesn't have super rights then tell it
                //that the reservation failed
                passMessage(0, source, command, 0);
                return;
            }

            passMessage(0, source, command, (unsigned int)reserve_physical(payload, 0x1000));
            break;

        case KS_FREE_PHYS_PAGE:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;

            if(!(procTable[i].flags & PF_SUPER)) {

                //If the process doesn't have super rights then tell it
                //that the free failed
                passMessage(0, source, command, 0);
                return;
            }

            free_physical(payload, 0x1000);
            passMessage(0, source, command, 1);
            break;

        case KS_GET_SHARED_PAGES:
            passMessage(0, source, command, (unsigned int)allocate_shared_pages(payload));
            break;
            
        case KS_GET_SHARED_PAGE:
            passMessage(0, source, command, (unsigned int)allocate_shared_page());
            break;

        case KS_TIMER:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);

            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;

            //Return an immediate failure message if we couldn't allocate a timer
            if(!install_timer_entry(&procTable[i], payload))
                passMessage(0, source, command, 0);
            break;

        case KS_GET_IMAGE_SIZE:
            for(i = 0; i < 256 && (procTable[i].id != payload); i++);

            //Fail if the requested process no longer exists
            if(i == 256)
                return;

            passMessage(0, source, command, procTable[i].size);
        break;
        
        case KS_APPEND_PAGE:
            for(i = 0; i < 256 && (procTable[i].id != source); i++);
            
            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;
            
            passMessage(0, source, command, (unsigned int)request_new_page(&procTable[i]));
        break;

        case KS_INSTALL_EXHDLR:
	    for(i = 0; i < 256 && (procTable[i].id != source); i++);
            
            //Fail if the calling process doesn't exist anymore
            if(i == 256)
                return;
			
            procTable[i].exception_handler = (entry_func)payload;
	break;

        case KS_GET_ELAPSED_MS:
            passMessage(0, source, command, get_elapsed_ms());
        break;

        default:
        break;
    }
}
