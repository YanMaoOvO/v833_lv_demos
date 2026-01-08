#define LV_CONF_INCLUDE_SIMPLE  // 确保在包含 lvgl.h 前定义

#include <stdio.h>    // 必须包含，用于 printf
#include <stdlib.h>   // 必须包含，用于 getenv
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include "lvgl/lvgl.h"
#include "lvgl/lv_conf.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"

// 设置 LVGL 使用自定义时间函数
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "main.cpp"
#define LV_TICK_CUSTOM_SYS_TIME_CUSTOM 1
/*// 屏幕参数
#define FB_WIDTH 1920    // framebuffer 物理宽度
#define FB_HEIGHT 480    // framebuffer 物理高度
#define BUFFER_WIDTH 960 // 每个缓冲区宽度
#define BUFFER_HEIGHT 480 // 每个缓冲区高度
#define SCREEN_WIDTH 960 // 屏幕物理宽度
#define SCREEN_HEIGHT 240 // 屏幕物理高度
#define VISIBLE_WIDTH 720 // 有效显示宽度
#define VISIBLE_HEIGHT 240 // 有效显示高度
#define LEFT_MARGIN 120  // 左边空白区域
#define RIGHT_MARGIN 120 // 右边空白区域
#define BUFFER_OFFSET_Y 240 // UI 显示在下半部分*/
// 屏幕参数
#define FB_WIDTH 480    // framebuffer 物理宽度
#define FB_HEIGHT 1920    // framebuffer 物理高度
#define BUFFER_WIDTH 480 // 每个缓冲区宽度
#define BUFFER_HEIGHT 960 // 每个缓冲区高度
#define SCREEN_WIDTH 240 // 屏幕物理宽度
#define SCREEN_HEIGHT 960 // 屏幕物理高度
#define VISIBLE_WIDTH 720 // 有效显示宽度
#define VISIBLE_HEIGHT 240 // 有效显示高度
#define LEFT_MARGIN 120  // 左边空白区域
#define RIGHT_MARGIN 120 // 右边空白区域
#define BUFFER_OFFSET_Y 240 // UI 显示在下半部分

// 全局变量
static int fb_fd = -1;
static char *fb_buf = NULL;
static struct fb_var_screeninfo var_screeninfo;

// 自定义时间函数
uint32_t custom_tick_get(void);

// 初始化 framebuffer (重命名函数，避免与 LVGL 驱动冲突)
int fbdev_init_custom(void) {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("open /dev/fb0 failed");
        return -1;
    }

    // 获取屏幕信息
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var_screeninfo) < 0) {
        perror("ioctl FBIOGET_VSCREENINFO failed");
        close(fb_fd);
        return -1;
    }

    // 计算缓冲区大小
    size_t buffer_size = var_screeninfo.xres * var_screeninfo.yres * (var_screeninfo.bits_per_pixel / 8);

    // 映射 framebuffer
    fb_buf = (char *)mmap(0, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_buf == MAP_FAILED) {
        perror("mmap failed");
        close(fb_fd);
        return -1;
    }

    printf("Framebuffer initialized: %dx%d, %dbpp\n",
           var_screeninfo.xres, var_screeninfo.yres, var_screeninfo.bits_per_pixel);
    return 0;
}

// 刷新 framebuffer
void fbdev__flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
    // 计算目标位置（考虑左右边距和缓冲区偏移）
    int target_x = area->x1 + LEFT_MARGIN;
    int target_y = area->y1 + BUFFER_OFFSET_Y; // UI 显示在下半部分

    // 计算目标地址
    int bytes_per_pixel = var_screeninfo.bits_per_pixel / 8;
    char *dest = fb_buf + (target_y * var_screeninfo.xres + target_x) * bytes_per_pixel;

    // 处理 32 位颜色（ARGB8888/XRGB8888）
    for(int y = 0; y < (area->y2 - area->y1 + 1); y++) {
        for(int x = 0; x < (area->x2 - area->x1 + 1); x++) {
            // 获取 LVGL 的颜色值
            lv_color_t color = color_p[x];

            // 转换为 ARGB8888 格式（根据你的屏幕调整）
            uint32_t argb = color.full; // LVGL 的 32 位颜色值

            // 检查屏幕实际格式（ARGB 或 XRGB）
            // 如果屏幕需要 BGR 顺序，取消注释下面的行
            // uint32_t bgr = ((argb & 0xFF) << 16) | (argb & 0x00FF00) | ((argb >> 16) & 0xFF);

            // 写入 framebuffer（根据你的屏幕格式选择）
            memcpy(dest + x * 4, &argb, 4);  // ARGB8888
            // memcpy(dest + x * 4, &bgr, 4);  // BGR8888（如果需要）
        }
        dest += var_screeninfo.xres * 4;
        color_p += (area->x2 - area->x1 + 1);
    }

    // 切换缓冲区（双缓冲处理）
    var_screeninfo.yoffset = (var_screeninfo.yoffset == 0) ? BUFFER_OFFSET_Y : 0;
    ioctl(fb_fd, FBIOPAN_DISPLAY, &var_screeninfo);

    // 通知 LVGL 刷新完成
    lv_disp_flush_ready(disp_drv);
}
// 初始化输入设备 (重命名函数，避免与 LVGL 驱动冲突)
int evdev_init_custom(void) {
    return 0; // 由 lvgl 驱动处理
}

int main() {
    lv_init();

    // 初始化 framebuffer 驱动 (使用重命名的函数)
    if (fbdev_init_custom() != 0) {
        printf("Failed to initialize framebuffer driver!\n");
        return -1;
    }

    // 创建显示缓冲区（使用有效区域大小）
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[VISIBLE_WIDTH * VISIBLE_HEIGHT];
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, VISIBLE_WIDTH * VISIBLE_HEIGHT);

    // 创建显示设备
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = fbdev__flush;
    disp_drv.hor_res = VISIBLE_WIDTH;
    disp_drv.ver_res = VISIBLE_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    // 初始化输入设备 (使用重命名的函数)
    if (evdev_init_custom() != 0) {
        printf("Failed to initialize input device!\n");
        return -1;
    }

    // 创建输入设备
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = evdev_read;
    lv_indev_drv_register(&indev_drv);

    // 创建UI
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x808080), 0);

    lv_obj_t* btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 200, 80);
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "HAPPY! NEW YEAR!!!\n 2026");

    printf("UI initialized successfully!\n");

    while(1) {
        lv_timer_handler();
        usleep(5000); // 5ms 延迟
    }

    return 0;
}

// 自定义时间函数实现
uint32_t custom_tick_get(void) {
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        start_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now_ms = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    return (uint32_t)(now_ms - start_ms);
}
