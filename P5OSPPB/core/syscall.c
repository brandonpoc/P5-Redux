#include "syscall.h"
#include "../process/process.h"
#include "../timer/timer.h"
#include "../process/message.h"
#include "../ascii_io/ascii_o.h"

unsigned int gcount = 0;

unsigned int lineBuf;

void syscall_exec(void) {

    message temp_message;

    switch(_syscall_number) {

        //Legacy for V86
        case 0:
        break;

        //Process post message
        case 1:
            passMessage(p->id, p->ctx.ebx, p->ctx.ecx, p->ctx.edx);
        break;

        //Process recieve message
        case 2:
            //If there is a message in the queue, send it
            //otherwise, put the process to sleep
            if(getMessage(p, &temp_message)) {

                p->ctx.eax = 1; //a 1 in eax is 'message found'
                p->ctx.ebx = temp_message.source;
                p->ctx.ecx = temp_message.command;
                p->ctx.edx = temp_message.payload;
            } else {

                p->flags |= PF_WAITMSG;
            }
        break;

        default:
        break;
    }
}
