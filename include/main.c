#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define DISP_BUF_SIZE (240 * 240)

void lcdRefresh();
void lcdOpen();
void lcdClose();
void lcdBrightness(int brightness);
uint32_t custom_tick_get();

int wake = 1;
int dispd = 0;  	//背光
int fbd = 0;    	//帧缓冲设备
int powerd = 0; 	//电源按钮
int homed = 0;		//主页按钮

int main(void)
{
	printf("ciallo lvgl\n");
	#if LV_USE_PERF_MONITOR
	printf("monitor on\n");
	#endif
	
	daemon(1,0);
	//daemon函数将本程序置于后台，脱离终端
	//若要进行调试，请注释掉这一行，使程序可以在终端打印信息
    
    fbd = open("/dev/fb0", O_RDWR);
    dispd = open("/dev/disp", O_RDWR);
    lcdOpen();
    touchOpen();
    lcdBrightness(10);

    lv_init();
    fbdev_init();
    
    static lv_color_t buf[DISP_BUF_SIZE];
    
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.flush_cb   = fbdev_flush;
    disp_drv.hor_res    = 240;
    disp_drv.ver_res    = 240;
    lv_disp_drv_register(&disp_drv);

    evdev_init();
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
    
    powerd = open("/dev/input/event1", O_RDWR);
    fcntl(powerd, 4,2048);
    homed = open("/dev/input/event2", O_RDWR);
    fcntl(homed, 4,2048);

    lv_demo_widgets();

    while(1) {
    	if(wake) {
        	lv_timer_handler();
        	lcdRefresh();
        }
        readKeyPower();
        readKeyHome();
        usleep(5000);
    }

    close(fbd);
    close(dispd);
    close(powerd);
    close(homed);
    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}

void lcdOpen() {
    int buffer[8] = {0};
    buffer[1] = 1;
    
    ioctl(dispd, 0xFu, buffer);
    printf("[lcd]opened\n");
}

void lcdClose() {
    char buffer[24] = {0};

    ioctl(dispd, 0xFu, buffer);
    printf("[lcd]closed\n");
}

void touchOpen() {
	int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "1", 1u);
    close(tpd);
    printf("[tp]opened\n");
}

void touchClose() {
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "0", 1u);
    close(tpd);
    printf("[tp]closed\n");
}

void lcdRefresh() {
    int buffer[8] = {0};
    
	ioctl(fbd, 0x4606u, buffer);
}

void lcdBrightness(int brightness) {
	int buffer[8] = {0};
    buffer[1] = 260 * brightness / 100;
    
	ioctl(dispd, 0x102u, buffer);
}

void readKeyPower(){
	char buffer[16] = {0};
	while (read(powerd, buffer, 0x10u) > 0) {
		if(buffer[10] != 0x74) return;
		
		if(buffer[12] == 0x00) {
			//printf("[key]power_up\n");
			if(wake){
				wake = 0;
				sysSleep();
			}
			else{
				wake = 1;
				sysWake();
			}
		}
		else {
			//printf("[key]power_down\n");
		}
	}
}

void readKeyHome(){
	char buffer[16] = {0};
	while (read(homed, buffer, 0x10u) > 0) {
		if(buffer[10] != 0x73) return;

		if(buffer[12] == 0x00) {
			//printf("[key]home_up\n");
		}
		else {
			//printf("[key]home_down\n");
		}
	}
}

void sysWake(){
	touchOpen();
    lcdOpen();
}

void sysSleep(){
	touchClose();
	lcdClose();
	/*
	//这是真睡死过去
	system("echo 8 > /proc/sys/kernel/printk");
    system("echo \"0\" >/sys/class/rtc/rtc0/wakealarm");
    system("echo N > /sys/module/printk/parameters/console_suspend");
    system("echo \"mem\" > /sys/power/state");
    system("echo 0 > /proc/sys/kernel/printk");
    */
}
