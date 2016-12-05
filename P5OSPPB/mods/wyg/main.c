#include "context.h"
#include "desktop.h"
#include "button.h"
#include "inttypes.h"
#include "wygcommands.h"
#include "../include/p5.h"
#include "../include/registrar.h"
#include "../include/gfx.h"
#include "../include/memory.h"
#include "../include/wyg.h"
#include "../include/key.h"
#include "../include/mouse.h"

//================| Entry Point |================//

short mouse_x;
short mouse_y;
message temp_msg;
unsigned char inbuf[12];
bitmap* back_buf;

//Our desktop object needs to be sharable by our main function
//as well as our mouse event callback
Desktop* desktop;

//The callback that our mouse device will trigger on mouse updates
void moveMouse(unsigned long packed_data) {

    short x_off, y_off;
    unsigned char buttons;

    buttons = (unsigned char)((packed_data >> 24) & 0xFF);
    x_off = (short)((unsigned char)(packed_data & 0xFF));
    y_off = (short)((unsigned char)((packed_data >> 9) & 0xFF));

    if(packed_data & 0x100)
        mouse_x -= x_off;
    else
        mouse_x += x_off;

    if(packed_data & 0x20000)    
        mouse_y += y_off;
    else
        mouse_y -= y_off;

    if(mouse_x < 0)
        mouse_x = 0;

    if(mouse_x > desktop->window.width - MOUSE_WIDTH)
        mouse_x = desktop->window.width - MOUSE_WIDTH;

    if(mouse_y < 0)
        mouse_y = 0;
    
    if(mouse_y > desktop->window.height - MOUSE_HEIGHT)
        mouse_y = desktop->window.height - MOUSE_HEIGHT;

    Desktop_process_mouse(desktop, (unsigned short)mouse_x, (unsigned short)mouse_y, buttons);
}

void scans(int c, char* b) {

    unsigned char temp_char;
    int index = 0;

    for(index = 0 ; index < c-1 ; ) {
        temp_char = getch();

        if(temp_char != 0) {
            b[index] = temp_char;
            pchar(b[index]);

            if(b[index] == '\n') {
                b[index] = 0;
                break;
            }

            index++;

            if(index == c-1)
                pchar('\n');
        }
    }

    b[index+1] = 0;
}

void showModes(void) {


    unsigned short mode_count;
    unsigned short i;
    screen_mode* mode;

    prints("Enumerating modes...");
    mode_count = enumerateModes();
    prints("done\n");

    prints("\nAvailible modes:\n");
    for(i = 1; i <= mode_count; i++) {

        mode = getModeDetails(i);
        prints("    ");
        printDecimal((unsigned int)i);
        prints(") ");
        printDecimal((unsigned int)mode->width);
        pchar('x');
        printDecimal((unsigned int)mode->height);
        prints(", ");
        printDecimal((unsigned int)mode->depth);
        prints("bpp");

        if(mode->is_linear)
            prints(" linear");

        pchar('\n');
    }
}

void screenThread() {

    while(1) {

        setCursor(0, 0);
        drawBitmap(back_buf);
        sleep(20);
    }
}

//Create and draw a few rectangles and exit
void main(void) {

    unsigned int parent_pid;
    screen_mode* mode;
    unsigned short num;
    unsigned int current_handle;
    int i;
    unsigned int src_pid;
    unsigned char* instr;
    unsigned int strlen;

    //Get the 'here's my pid' message from init
    getMessage(&temp_msg);
    parent_pid = temp_msg.source;
    prints("[WYG] Starting WYG GUI services.\n");

    //First thing, register as a WYG service with the registrar
    postMessage(REGISTRAR_PID, REG_REGISTER, SVC_WYG);
    getMessage(&temp_msg);

    if(!temp_msg.payload) {

        prints("\n[WYG] failed to register WYG service.\n");
        postMessage(REGISTRAR_PID, REG_DEREGISTER, SVC_WYG);
        postMessage(parent_pid, 0, 0); //Tell the parent we're done registering
        terminate();
    }

    prints("[WYG] Getting key service...\n");
    if(!initKey()) {
        
        postMessage(REGISTRAR_PID, REG_DEREGISTER, SVC_WYG);
        postMessage(parent_pid, 0, 0); //Tell the parent we're done registering
        terminate();
    }

    prints("[WYG] Getting mouse service...\n");
    if(!initMouse()) {
        
        //Don't need to terminate, but do need to display a warning to the user
    }

    prints("[WYG] Getting graphics service... \n");
    if(!initGfx()) {
        
        //prints("\n[WYG] failed to get the GFX server.\n");
        postMessage(REGISTRAR_PID, REG_DEREGISTER, SVC_WYG);
        postMessage(parent_pid, 0, 0); //Tell the parent we're done registering
        terminate();
    }

    //Prompt user for a screen mode
    showModes();
    prints("mode: ");
    scans(10, inbuf);
    num = inbuf[0] > '9' ? inbuf[0] - 'A' + 10 : inbuf[0] - '0';

    if(!setScreenMode(num)) {

        //prints("[WYG] Could not set screen mode.\n");
        postMessage(REGISTRAR_PID, REG_DEREGISTER, SVC_WYG);
        postMessage(parent_pid, 0, 0); //Tell the parent we're done registering
        terminate();
    }

//    installExceptionHandler((void*)exceptionHandler);

    if(num) {

        mode = getModeDetails(num);
    } else {

        //prints("[WYG] Staying in text mode.\n");
        postMessage(REGISTRAR_PID, REG_DEREGISTER, SVC_WYG);
        postMessage(parent_pid, 0, 0); //Tell the parent we're done registering
        terminate();
    }
    	    
    postMessage(parent_pid, 0, 1); //Tell the parent we're done registering

    //Create backbuffer
    back_buf = newBitmap(mode->width, mode->height);
    
    //Would realistically be on a vsync interrupt
    if(!startThread())
        screenThread();

    //Fill this in with the info particular to your project
    Context* context = Context_new(0, 0, 0);
    context->buffer = (unsigned long*)back_buf->data;
    context->width = mode->width;
    context->height = mode->height;

    //Create the desktop 
    desktop = Desktop_new(context);

    //Set up the initial mouse position
    mouse_x = desktop->window.width / 2 - 1;
    mouse_y = desktop->window.height / 2 - 1;

    //Main message loop
    while(1) {

        getMessage(&temp_msg);

        src_pid = temp_msg.source;

        switch(temp_msg.command) {

            case WYG_CREATE_WINDOW:
                postMessage(src_pid, WYG_CREATE_WINDOW,
                            WYG_create_window(desktop, temp_msg.payload));
            break;
            
            case WYG_GET_CONTEXT:
                postMessage(src_pid, WYG_GET_CONTEXT,
                            WYG_get_window_context_id(desktop, temp_msg.payload));
            break;
            
            case WYG_GET_DIMS:
                postMessage(src_pid, WYG_GET_DIMS,
                            WYG_get_window_dimensions(desktop, temp_msg.payload));
            break;
            
            case WYG_GET_LOCATION:
                postMessage(src_pid, WYG_GET_LOCATION, 
                            WYG_get_window_location(desktop, temp_msg.payload));
            break;
            
            case WYG_MOVE_WINDOW:
                current_handle = temp_msg.payload;
                getMessageFrom(&temp_msg, src_pid, WYG_POINT);
                WYG_move_window(desktop, current_handle, temp_msg.payload);
            break;

            case WYG_INSTALL_WINDOW:
                current_handle = temp_msg.payload;
                getMessageFrom(&temp_msg, src_pid, WYG_WHANDLE);
                WYG_install_window(desktop, current_handle, temp_msg.payload);
            break;

            case WYG_SHOW_WINDOW:
                WYG_show_window(desktop, temp_msg.payload);
            break;
            
            case WYG_RAISE_WINDOW:
                WYG_raise_window(desktop, temp_msg.payload);
            break;

            case WYG_REPAINT_WINDOW:
                WYG_invalidate_window(desktop, temp_msg.payload);
                postMessage(src_pid, WYG_REPAINT_WINDOW, 1);
            break;

            case WYG_SET_TITLE:
                current_handle = temp_msg.payload;
                postMessage(src_pid, WYG_SET_TITLE, 1);
                strlen = getStringLength(src_pid);
                instr = (char*)malloc(strlen);
                getString(src_pid, instr, strlen);
                WYG_set_window_title(desktop, current_handle, instr);
                free((void*)instr);
            break;
            
            case WYG_DESTROY:
                WYG_destroy_window(desktop, temp_msg.payload);
                postMessage(src_pid, WYG_DESTROY, 1);
            break;
            
            case WYG_GET_FRAME_DIMS:
                postMessage(src_pid, WYG_GET_FRAME_DIMS,
                            WYG_get_frame_dims());
            break;

            case MOUSE_SEND_UPDATE:
                moveMouse(temp_msg.payload);
            break;

            default:
            break;
        }
    }

}
