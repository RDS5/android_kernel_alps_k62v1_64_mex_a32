
#ifndef LINUX_SPI_FINGERPRINT_H
#define LINUX_SPI_FINGERPRINT_H


#define EVENT_HOLD   28
#define EVENT_CLICK  174
#define EVENT_DCLICK 111
#define EVENT_UP     103
#define EVENT_DOWN   108
#define EVENT_LEFT   105
#define EVENT_RIGHT  106

#define FP_MAX_SENSOR_ID_LEN        16
#define FP_MAX_CHIP_INFO_LEN        50
//NAVIGATION_ADJUST_NOREVERSE: 默认值，适配后置指纹模组
#define NAVIGATION_ADJUST_NOREVERSE 0
//NAVIGATION_ADJUST_NOREVERSE: 适配前置指纹模组
#define NAVIGATION_ADJUST_REVERSE 1
#define NAVIGATION_ADJUST_NOTURN 0
#define NAVIGATION_ADJUST_TURN90 90
#define NAVIGATION_ADJUST_TURN180 180
#define NAVIGATION_ADJUST_TURN270 270

#define FP_RESET_RETRIES     4
#define FP_RESET_LOW_US      15000
#define FP_RESET_HIGH1_US    10000
#define FP_RESET_HIGH2_US    1250

#define FPC_TTW_HOLD_TIME    3000

#define FP_DEV_NAME      "fingerprint"
#define FP_CLASS_NAME    "fpsensor"
#define FP_IOC_MAGIC     'f'  //define magic number

//define commands
#define  FP_IOC_CMD_ENABLE_IRQ      _IO(FP_IOC_MAGIC, 1)
#define  FP_IOC_CMD_DISABLE_IRQ     _IO(FP_IOC_MAGIC, 2)
#define  FP_IOC_CMD_SEND_UEVENT     _IO(FP_IOC_MAGIC, 3)
#define  FP_IOC_CMD_GET_IRQ_STATUS  _IO(FP_IOC_MAGIC, 4)
#define  FP_IOC_CMD_SET_WAKELOCK_STATUS  _IO(FP_IOC_MAGIC, 5)
#define  FP_IOC_CMD_SEND_SENSORID        _IO(FP_IOC_MAGIC, 6)
#define  FP_IOC_CMD_SET_IPC_WAKELOCKS      _IO(FP_IOC_MAGIC, 7)
#define  FP_IOC_CMD_SET_POWEROFF      _IO(FP_IOC_MAGIC, 10)
#define  FP_IOC_CMD_ENABLE_SPI_CLK  _IO(FP_IOC_MAGIC, 11)
#define  FP_IOC_CMD_DISABLE_SPI_CLK  _IO(FP_IOC_MAGIC, 12)

#define FPC_LOG
#ifdef FPC_LOG
#define fpc_log_err(fmt, args...) printk(KERN_ERR "%s %d: " fmt, __func__, __LINE__, ## args)
#define fpc_log_info(fmt, args...) printk(KERN_ERR "%s %d: " fmt, __func__, __LINE__, ## args)
#else
#define fpc_log_err(fmt, args...)
#define fpc_log_info(fmt, args...)
#endif

enum module_vendor_info
{
    MODULEID_LOW = 0,
    MODULEID_HIGH,
    MODULEID_FLOATING,
};

enum fprint_pin
{
    FINGERPRINT_RST_PIN = 0,
    FINGERPRINT_SPI_CS_PIN,
    FINGERPRINT_SPI_MO_PIN,
    FINGERPRINT_SPI_MI_PIN,
    FINGERPRINT_SPI_CK_PIN
};

typedef enum
{
    fp_UNINIT = 0,
    fp_LCD_UNBLANK = 1,
    fp_LCD_POWEROFF = 2,
    fp_STATE_UNDEFINE = 255,
} fingerprint_state;

struct fp_data
{
    struct device* dev;
    struct spi_device* spi;
    struct cdev     cdev;
    struct class*    class;
    struct device*   device;
    dev_t             devno;
    struct platform_device* pf_dev;

    struct wakeup_source ttw_wl;
    int irq_gpio;
    int irq;
    int rst_gpio;
    int avdd_en_gpio;
    int vdd_en_gpio;
    int moduleID_gpio;
    int module_vendor_info;
    int navigation_adjust1;
    int navigation_adjust2;
    unsigned int snr_stat;
    unsigned int nav_stat;
    struct input_dev* input_dev;
    int irq_num;
    int event_type;
    int event_code;
    unsigned int spi_clk_counter;
    struct mutex mutex_lock_clk;
    struct mutex lock;
    bool wakeup_enabled;
    bool read_image_flag;
    unsigned int sensor_id;
    unsigned int autotest_input;
    char module_id[64];
    struct regulator* vdd;
    struct regulator* avdd;
    struct pinctrl* pctrl;
    struct pinctrl_state* pins_idle;

    struct pinctrl* pinctrl;
    struct pinctrl_state* pins_default;
    struct pinctrl_state* eint_as_int, *fp_rst_low, *fp_rst_high, *fp_cs_high, *fp_cs_low, *fp_mo_low, *fp_mo_high, *fp_mi_low, *fp_mi_high, *fp_ck_low, *fp_ck_high;

#if defined(CONFIG_FB)
    struct notifier_block fb_notify;
#endif
    atomic_t state;
};


#endif // LINUX_SPI_FINGERPRINT_H

