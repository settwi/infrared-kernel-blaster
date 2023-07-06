#include "blaster.h"
#include "pwm_bitfields.h"

#define make_u32(x) (*(uint32_t*)&(x))

static const uint32_t CLOCKMAN_PASSWORD = 0x5a;

static unsigned int major_num = -1;
static uint8_t open_ct = 0;

static struct PwmRegs pwm_regs;
static uint32_t old_fsel;

static bool enabled;

static struct file_operations file_ops = {
    // set .owner so that we get auto lock/release
    .owner = THIS_MODULE,
    .read = blaster_read,
    .write = blaster_write,
    .open = blaster_open,
    .release = blaster_release,
};

static ssize_t blaster_read(
    struct file* f, char* user_buf, size_t len, loff_t* off
) {
    bool local_enabled;
    size_t msg_len;

    static ssize_t bytes_read = 0;
    char* msg = NULL;

    uint32_t reg = readl(pwm_regs.sta);
    struct PwmSta sta;

    memcpy(&sta, &reg, sizeof(uint32_t));

    local_enabled = sta.sta1;
    if (local_enabled) msg = "pwm on\n";
    else msg = "pwm off\n";
    if (!msg) return -EINVAL;

    msg_len = strlen(msg) + 1;

    // reset after sending full msg
    if (bytes_read == msg_len) {
        bytes_read = 0;
        return 0;
    }

    for (
        size_t i = bytes_read, j = 0;
        (i < msg_len) && (j < len);
        ++i, ++j, ++bytes_read
    ) {
        put_user(msg[i], (user_buf + j));
    }

    return bytes_read;
}

static void toggle_pwm(void) {
    enabled = !enabled;
    uint32_t v = readl(pwm_regs.ctl);
    struct Ctl c;
    memcpy(&c, &v, sizeof(uint32_t));

    printk(KERN_DEBUG "pwm ctrl regs: %u", v);
    // see broadcom peripherals doc pg 143
    // don't care about channel 2
    c.msen1 = 1;
    c.clrf1 = 0;
    c.usef1 = 0;
    c.pola1 = 0;
    c.sbit1 = 0;
    c.mode1 = 0;
    c.pwen1 = enabled;

    writel(make_u32(c), pwm_regs.ctl);
}

static int isbad(int c) {
    return 
        (c > 0 && c < 32) ||
        (c == 127)        ||
        (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

static ssize_t blaster_write(
    struct file* f, const char* dat, size_t len, loff_t* off
) {
    // len of 1 means empty string
    if (len == 1) {
        return -EINVAL;
    }

    static const size_t MAX_MSG_SZ = 1024;
    if (len > MAX_MSG_SZ) {
        pr_err("message length is too long (max is %zu)\n", MAX_MSG_SZ);
        return -EINVAL;
    }

    char* terminated = kmalloc(len * sizeof(char), GFP_KERNEL);
    if (terminated == NULL) {
        return -ENOMEM;
    }

    // the kernel sometimes pollutes the string and doesn't null-terminate it (??)
    char* root = terminated;
    memset(terminated, 0, len);
    strncpy(terminated, dat, len-1);
    while(isbad(*terminated)) { ++terminated; }

    pr_info("string length: %zu\nstring: %s", len, terminated);

    static const char
        *TOG = "toggle",
        *BLAST = "blast";

    bool good = false;
    if (strlen(TOG) < len && strncmp(terminated, TOG, strlen(TOG)) == 0) {
        toggle_pwm();
        good = true;
        // bytes_read += strlen(TOG) + 1;
    } else if (strlen(BLAST) < len && strncmp(terminated, BLAST, strlen(BLAST)) == 0) {
        pr_info("BLAStOFF!!!\n");
        good = true;
        // bytes_read += strlen(BLAST) + 1;
    }
    else {
        pr_err("invalid command '%s'\n", terminated);
        good = false;
    }

    // pre-whitespace trim
    kfree(root);
    return good? len : -EINVAL;
}

static int blaster_open(struct inode* ino, struct file* f) {
    if (open_ct > 0) {
        return -EBUSY;
    }
    ++open_ct;
    return 0;
}

static int blaster_release(struct inode* ino, struct file* f) {
    --open_ct;
    return 0;
}

static void deinit_pwm(void) {
    // disable pwm at exit
    struct Ctl c;
    memset(&c, 0, sizeof(struct Ctl));
    writel(make_u32(c), pwm_regs.ctl);

    writel(old_fsel, pwm_regs.gpfsel1);
}

static void init_pwm(void) {
    // any random val we need
    uint32_t v;

    // save these for restoration later
    old_fsel = readl(pwm_regs.gpfsel1);

    struct Fsel fs;
    memset(&fs, 0, sizeof(fs));
    // enable PWM out on GPIO#12
    memcpy(&fs, &old_fsel, sizeof(uint32_t));
    // 4 = alt fun 0 aka PWM on GPIO#12
    // 2 = alt fun 5 aka PWM on GPIO#18
    fs.fsel12 = 4;
    writel(make_u32(fs), pwm_regs.gpfsel1);

    // disable PWM
    {
     struct Ctl c;
     memset(&c, 0, sizeof(struct Ctl));
     writel(make_u32(c), pwm_regs.ctl);
    }

    // disable PWM clock
    v = readl(pwm_regs.cman_pwmctl);
    struct ClockmanPwmctl cm;
    memcpy(&cm, &v, sizeof(cm));
    cm.enab = 0;
    cm.passwd = CLOCKMAN_PASSWORD;
    writel(make_u32(cm), pwm_regs.cman_pwmctl);

    // wait until "busy" flag is cleared
    while (
      v = readl(pwm_regs.cman_pwmctl),
      ((struct ClockmanPwmctl*)&v)->busy != 0) { }

    // source 6 = phase-locked loop D peripheral (PLLD)
    v = readl(pwm_regs.cman_pwmdiv);
    struct ClockmanPwmdiv cv;
    memcpy(&cv, &v, sizeof(cm));

    cm.src = 6;
    cm.mash = 0;
    cv.divf = 0;
    cv.divi = 4080;
    cv.passwd = cm.passwd = CLOCKMAN_PASSWORD;

    writel(make_u32(cv), pwm_regs.cman_pwmdiv);
    writel(make_u32(cm), pwm_regs.cman_pwmctl);

    // some duty cycle?
    writel(16, pwm_regs.dat1);
    writel(32, pwm_regs.rng1);

    // re-enable clock (but not the actual PWM)
    memset(&cm, 0, sizeof(cm));
    v = readl(pwm_regs.cman_pwmctl);
    cm = *(struct ClockmanPwmctl*)&v;
    cm.passwd = CLOCKMAN_PASSWORD;
    cm.enab = 1;
    writel(make_u32(cm), pwm_regs.cman_pwmctl);
}

static int __init blaster_init(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &file_ops);
    if (major_num < 0) {
        printk(KERN_ERR "couldn't register dev major num: %d\n", major_num);
        return major_num;
    }

    enabled = false;

    printk(KERN_INFO "blaster module loaded w major num %d\n", major_num);

    // zero out the registers (NULL pointers) before attempt
    memset(&pwm_regs, 0, sizeof(pwm_regs));
    int ret = map_addresses();
    if (ret < 0) {
        printk(KERN_ERR "failed to map memory regions\n");
        goto map_err;
    }

    init_pwm();
    return 0;

map_err:
    unmap_all();
    return ret;
}

static void unmap_all(void) {
    #define SAFE_UNMAP(x, msg) \
        if (x) { iounmap(x); x = NULL; printk(KERN_DEBUG "unmapped %s", msg); }
    SAFE_UNMAP(pwm_regs.ctl, "pwm ctl");
    SAFE_UNMAP(pwm_regs.cman_pwmctl, "cman ctl");
    SAFE_UNMAP(pwm_regs.cman_pwmdiv, "cman div");
    SAFE_UNMAP(pwm_regs.rng1, "rng");
    SAFE_UNMAP(pwm_regs.dat1, "dat");
    SAFE_UNMAP(pwm_regs.gpfsel1, "fsel");
    #undef SAFE_UNMAP
}

static void __exit blaster_exit(void) {
    deinit_pwm();
    unmap_all();

    unregister_chrdev(major_num, DEVICE_NAME);
}

static int map_addresses(void) {
    // when using ioremap, we give it a physical address.
    // for the BCM2835 these start at the following:
    static const phys_addr_t BASE_REG_ADDR      = 0x20000000;

    static const ptrdiff_t PWM_BASE_OFFSET      = 0x0020c000;
    static const ptrdiff_t CLOCKMAN_CTL_OFFSET  = 0x001010a0;
    static const ptrdiff_t CLOCKMAN_DIV_OFFSET  = 0x001010a4;
    static const ptrdiff_t GPFSEL1_OFFSET       = 0x00200004;
    static const phys_addr_t PWM_REGS_BASE = (BASE_REG_ADDR + PWM_BASE_OFFSET);
    static const phys_addr_t CLOCKMAN_CTL_ADDR = (BASE_REG_ADDR + CLOCKMAN_CTL_OFFSET);
    static const phys_addr_t CLOCKMAN_DIV_ADDR = (BASE_REG_ADDR + CLOCKMAN_DIV_OFFSET);
    static const phys_addr_t GPFSEL1_ADDR = (BASE_REG_ADDR + GPFSEL1_OFFSET);

    static const ptrdiff_t CTL_OFFSET  = 0x00;
    static const ptrdiff_t STA_OFFSET  = 0x04;
    static const ptrdiff_t RNG1_OFFSET = 0x10;
    static const ptrdiff_t DAT1_OFFSET = 0x14;

    // use ioremap instead of phys_to_virt
    // because it's specifically for IO devices
    struct PwmRegs r = {
        .ctl = ioremap(PWM_REGS_BASE + CTL_OFFSET, sizeof(uint32_t)),
        .sta = ioremap(PWM_REGS_BASE + STA_OFFSET, sizeof(uint32_t)),
        .rng1 = ioremap(PWM_REGS_BASE + RNG1_OFFSET, sizeof(uint32_t)),
        .dat1 = ioremap(PWM_REGS_BASE + DAT1_OFFSET, sizeof(uint32_t)),
        .cman_pwmctl = ioremap(CLOCKMAN_CTL_ADDR, sizeof(uint32_t)),
        .cman_pwmdiv = ioremap(CLOCKMAN_DIV_ADDR, sizeof(uint32_t)),
        .gpfsel1 = ioremap(GPFSEL1_ADDR, sizeof(uint32_t))
    };

    bool bad = false;
    if (!r.ctl) {
        printk(KERN_ERR "Failed to remap PWM ctrl regs\n");
        bad = 1;
    }
    if (!r.cman_pwmctl) {
        printk(KERN_ERR "Failed to remap PWM clock manager control regs\n");
        bad = 1;
    }
    if (!r.cman_pwmdiv) {
        printk(KERN_ERR "Failed to remap PWM clock manager divisor regs\n");
        bad = 1;
    }
    if (!r.rng1) {
        printk(KERN_ERR "Failed to remap PWM1 range regs\n");
        bad = 1;
    }
    if (!r.dat1) {
        printk(KERN_ERR "Failed to remap PWM1 data regs\n");
        bad = 1;
    }
    if (!r.gpfsel1) {
        printk(KERN_ERR "Failed to remap PWM1 function select regs\n");
        bad = 1;
    }
    if (bad) return -ENOMEM;

    memset(&pwm_regs, 0, sizeof(struct PwmRegs));
    memcpy(&pwm_regs, &r, sizeof(struct PwmRegs));

    return 0;
}

static void check_sta(const char* msg) {
    uint32_t reg = readl(pwm_regs.sta);
    struct PwmSta sta;
    memcpy(&sta, &reg, sizeof(uint32_t));
    printk(KERN_INFO "** %s **", msg);
    printk(KERN_INFO "status register: %u\n", reg);

    // set these to 1 to clear them when re-writing
    if (sta.werr1) {
        pr_err("PWM fifo1 read error\n");
        sta.werr1 = 1;
    }
    if (sta.rerr1) {
        pr_err("PWM fifo1 read error\n");
        sta.rerr1 = 1;
    }
    if (sta.gapo1) {
        pr_err("PWM fifo1 gap occurred\n");
        sta.gapo1 = 1;
    }
    if (sta.berr) {
        pr_err("PWM bus error\n");
        sta.berr = 1;
    }
    // writel(*(uint32_t*)&sta, pwm_regs.sta);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("William Setterberg <william.s.setterberg@gmail.com>");
MODULE_DESCRIPTION(
    "Send arbitrary binary data out using a 38 kHz"
    " infrared signal. Only hardware-enabled GPIO"
    " pins may be used. Default is GPIO#12. To use a"
    " different one you will need to consult the Broadcom"
    " peripherals document and modify the code."
);
MODULE_VERSION("0.1");
module_init(blaster_init);
module_exit(blaster_exit);
