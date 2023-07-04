#pragma once

struct Ctl;
struct ClockmanPwmctl;

// holds all the registers for the 
// PWM control
// use the __iomem attribute so that GCC knows it's device-mapped memory
struct PwmRegs {
    volatile uint32_t __iomem* ctl;
    volatile uint32_t __iomem* sta;
    volatile uint32_t __iomem* rng1;
    volatile uint32_t __iomem* dat1;

    volatile uint32_t __iomem* cman_pwmctl;
    volatile uint32_t __iomem* cman_pwmdiv;

    volatile uint32_t __iomem* gpfsel1;
};

// Control registers
// from https://www.raspberrypi.org/app/uploads/2012/02/BCM2835-ARM-Peripherals.pdf
// ~pg 138
struct Ctl {
    uint32_t pwen1: 1;
    uint32_t mode1: 1;
    uint32_t rptl1: 1;
    uint32_t sbit1: 1;
    uint32_t pola1: 1;
    uint32_t usef1: 1;
    uint32_t clrf1: 1;
    uint32_t msen1: 1;
    uint32_t pwen2: 1;
    uint32_t mode2: 1;
    uint32_t rptl2: 1;
    uint32_t sbit2: 1;
    uint32_t pola2: 1;
    uint32_t usef2: 1;
    uint32_t res1:  1;
    uint32_t msen2: 1;
    uint32_t res0:  16;
};
_Static_assert(sizeof(struct Ctl) == sizeof(uint32_t));

// Alternative function selection register
// from the ARM peripherals guide too
struct Fsel {
    uint32_t fsel10: 3;
    uint32_t fsel11: 3;
    // fsel12 controls PPS0 on GPIO # 12 which is physical pin 32
    uint32_t fsel12: 3;
    uint32_t fsel13: 3;
    uint32_t fsel14: 3;
    uint32_t fsel15: 3;
    uint32_t fsel16: 3;
    uint32_t fsel17: 3;
    uint32_t fsel18: 3;
    uint32_t fsel19: 3;
    uint32_t res0: 2;
};
_Static_assert(sizeof(struct Fsel) == sizeof(uint32_t));

// Clock manager registers
// from https://www.scribd.com/doc/127599939/BCM2835-Audio-clocks
struct ClockmanPwmctl {
    uint32_t src:    4;
    uint32_t enab:   1;
    uint32_t kill:   1;
    uint32_t res1:   1;
    uint32_t busy:   1;
    uint32_t flip:   1;
    uint32_t mash:   2;
    uint32_t res0:   13;
    uint32_t passwd: 8;
};
_Static_assert(sizeof(struct ClockmanPwmctl) == sizeof(uint32_t));

struct ClockmanPwmdiv {
    uint32_t divf: 12;
    uint32_t divi: 12;
    uint32_t passwd: 8;
};
_Static_assert(sizeof(struct ClockmanPwmdiv) == sizeof(uint32_t));

struct PwmSta {
    uint32_t full1: 1;
    uint32_t empt1: 1;
    uint32_t werr1: 1;
    uint32_t rerr1: 1;
    uint32_t gapo1: 1;
    uint32_t gapo2: 1;
    uint32_t gapo3: 1;
    uint32_t gapo4: 1;
    uint32_t berr:  1;
    uint32_t sta1:  1;
    uint32_t sta2:  1;
    uint32_t sta3:  1;
    uint32_t sta4:  1;
    uint32_t res0:  19;
};
_Static_assert(sizeof(struct PwmSta) == sizeof(uint32_t));
