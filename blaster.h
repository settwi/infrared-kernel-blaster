#pragma once

#include <asm/io.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static const char* DEVICE_NAME = "ir-blaster";

// static void delay_writel(uint32_t v, volatile void* loc);
static int blaster_open(struct inode*, struct file*);
static int blaster_release(struct inode*, struct file*);
static ssize_t blaster_read(struct file*, char*, size_t, loff_t*);
static ssize_t blaster_write(struct file*, const char*, size_t, loff_t*);

static int map_addresses(void);
static void unmap_all(void);
static void init_pwm(void);
static void deinit_pwm(void);
static void check_sta(const char* msg);
