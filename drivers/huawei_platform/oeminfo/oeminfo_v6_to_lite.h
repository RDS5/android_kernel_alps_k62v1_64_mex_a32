

#ifndef INCLUDE_OEMINFO_V6_TO_LITE_H
#define INCLUDE_OEMINFO_V6_TO_LITE_H

typedef enum {
    /* the next id is used for oeminfo_v8 none-wp area */
    OEMINFO_NONEWP_AMSS_VER_TYPE = 1,
    OEMINFO_NONEWP_OEMSBL_VER_TYPE = 2,
    OEMINFO_NONEWP_UPDATE_TYPE = 3,
    OEMINFO_NONEWP_WIFI_TEST = 4,
    OEMINFO_NONEWP_HWSB_LOCK_STATE = 5,
    OEMINFO_NONEWP_HWFB_LOCK_STATE = 6,
    OEMINFO_NONEWP_USER_USAGE_ACTIVDATE_TYPE = 7,
    OEMINFO_NONEWP_USB_PID_INDEX = 8,
    OEMINFO_NONEWP_USB_PID_CUSTOM_TYPE = 9,
    OEMINFO_NONEWP_TPCOLOR_TYPE = 10,
    OEMINFO_NONEWP_CRYPTFS_STATE = 11,
    OEMINFO_NONEWP_ROOT_INFO_TYPE = 12,
    OEMINFO_NONEWP_RESCUEINFO_TYPE =  13,
    OEMINFO_NONEWP_CAEMRA_DF_TYPE = 14,
    OEMINFO_NONEWP_CAMERA_HALL_OVERTURN_DATA =15,
    OEMINFO_NONEWP_AT_LOCK_PASSWORD_SHA256 = 16,
    OEMINFO_NONEWP_DAP_TIME = 17,
    OEMINFO_NONEWP_PSID_DATA = 18,
    OEMINFO_NONEWP_RESOURCE_VERSION = 19,
    OEMINFO_NONEWP_CRYPTFS_DATA = 20,
    OEMINFO_NONEWP_INITRC_TRACER = 21,
    OEMINFO_NONEWP_FBLOCK_STAT_INFO = 22,
    OEMINFO_NONEWP_VENDER_AND_COUNTRY_NAME_COTA = 23,
    OEMINFO_NONEWP_ANTITHIVES_STATE = 24,
    OEMINFO_NONEWP_WIFI_CAL_INDEX = 25,
    OEMINFO_NONEWP_SD_LOCK_PASSWD = 26,
    OEMINFO_NONEWP_MAIN_VERSION = 27,
    OEMINFO_NONEWP_BOOTANIM_SWITCH = 28,
    OEMINFO_NONEWP_BOOTANIM_SHUTFLAG = 29,
    OEMINFO_NONEWP_BOOTANIM_RINGMODE = 30,
    OEMINFO_NONEWP_BOOTANIM_RINGDEX = 31,
    OEMINFO_NONEWP_DATA_PART_FAULT_TIMES = 32,
    OEMINFO_NONEWP_APPS_NO_INSTALL_TYPE = 33,
    OEMINFO_NONEWP_SYSTEM_VERSION = 34,
    OEMINFO_NONEWP_GAMMA_DATA =35,
    OEMINFO_NONEWP_GAMMA_LEN = 36,
    OEMINFO_NONEWP_MCC_MNC_TYPE = 37,
    OEMINFO_NONEWP_OPERATOR_INFO = 38,
    OEMINFO_NONEWP_HOTA_UPDATE_AUTH_LOCK = 39,
    OEMINFO_NONEWP_FACTORY_PRODUCT_VERSION_TYPE = 40,
    OEMINFO_NONEWP_AUDIO_SMARTPA_CALIBRATION = 41,
    OEMINFO_NONEWP_WIFIONLY_IDENTITY = 42,
    OEMINFO_NONEWP_LCD_BRIGHT_COLOR_CALIBRATION = 43,
    OEMINFO_NONEWP_FOTA_MODE = 44,
    OEMINFO_NONEWP_MARKETING_NAME = 45,
    OEMINFO_NONEWP_OVMODE_STATE = 46,
    OEMINFO_NONEWP_OMADM_INFO = 47,
    OEMINFO_NONEWP_ACTIVATION_STATUS_ID = 48,
    OEMINFO_NONEWP_HWPARENTCONTROL_RECOVER_DATA = 49,
    OEMINFO_NONEWP_FIX_HOTA_INFO_INTI = 50,
    OEMINFO_NONEWP_FIX_HOTA_INFO = 51,
    OEMINFO_NONEWP_FIX_HOTA_STRING_INTI = 52,
    OEMINFO_NONEWP_FIX_HOTA_STRING = 53,
    OEMINFO_NONEWP_VENDER_AND_COUNTRY_NAME_COTA_FACTORY = 54,
    OEMINFO_NONEWP_USER_HOTA_UPDATE = 55,
    OEMINFO_NONEWP_USE_DECRESS_BAT_VOL_FLAG = 56,
    OEMINFO_NONEWP_UPDATE_UDID = 57,
    OEMINFO_NONEWP_FACTORY_SW_VERSION = 58,
    OEMINFO_NONEWP_PRELOAD_APP_BATCH = 59,
    OEMINFO_NONEWP_CUST_Y_CABLE_TYPE = 60,
    OEMINFO_NONEWP_SDUPDATE_TYPE = 61,
    OEMINFO_NONEWP_ENABLE_RETREAD = 62,
    OEMINFO_NONEWP_SYSTEM_UPDATE_AUTH_TOKEN_INFO = 63,
    OEMINFO_NONEWP_ENCRYPT_INFO = 64,
    OEMINFO_NONEWP_MALWARE_DIAGNOSE = 65,
    OEMINFO_NONEWP_RETREAD_DIAGNOSE = 66,
    OEMINFO_NONEWP_ROOT_DIAGNOSE = 67,
    OEMINFO_NONEWP_SYSTEM_VERSION_B = 68,
    OEMINFO_NONEWP_ANDROID_VERSION  =69,
    OEMINFO_NONEWP_ANDROID_VERSION_B = 70,
    OEMINFO_NONEWP_SLOT_INFO_A = 71,
    OEMINFO_NONEWP_SLOT_INFO_B = 72,
    OEMINFO_NONEWP_SLOT_STATUS = 73,
    OEMINFO_NONEWP_VERITY_MODE_UPDATE_STATUS = 74,
    OEMINFO_NONEWP_SYSTEM_UPDATE_STATE = 75,
    OEMINFO_NONEWP_BASE_VER_TYPE  = 76,
    OEMINFO_NONEWP_CUST_VER_TYPE =  77,
    OEMINFO_NONEWP_PRELOAD_VER_TYPE = 78,
    OEMINFO_NONEWP_CUSTOM_VERSION = 80,
    OEMINFO_NONEWP_CUSTOM_VERSION_B = 81,
    OEMINFO_NONEWP_PRELOAD_VERSION = 82,
    OEMINFO_NONEWP_PRELOAD_VERSION_B = 83,
    OEMINFO_NONEWP_GROUP_VERSION = 84,
    OEMINFO_NONEWP_GROUP_VERSION_B = 85,
    OEMINFO_NONEWP_BASE_VERSION = 86,
    OEMINFO_NONEWP_BASE_VERSION_B = 87,
    OEMINFO_NONEWP_POWERONLOCK = 88,
    OEMINFO_NONEWP_BDRT_TYPE = 89,
    OEMINFO_NONEWP_COTA_REBOOT_UPDATE_PATH = 90,
    OEMINFO_NONEWP_CUSTOM_C_VERSION = 91,
    OEMINFO_NONEWP_RUNTEST_TYPE = 92,
    OEMINFO_NONEWP_MMITEST_TYPE = 93,
    OEMINFO_NONEWP_OTA_SIMLOCK_ACTIVATED = 94,
    OEMINFO_NONEWP_ATLOCK_TYPE = 95,
    OEMINFO_NONEWP_RUNTEST2_TYPE = 96,
    OEMINFO_NONEWP_RUNTIME_TYPE = 97,
    OEMINFO_NONEWP_MDATE_TYPE = 98,
    OEMINFO_NONEWP_MANUFACTURE_RESET_FLAG = 99,
    OEMINFO_NONEWP_BACKCOLOR = 100,
    OEMINFO_NONEWP_FB_STP_TYPE = 101,
    OEMINFO_NONEWP_RESET_ENTER_INFO = 102,
    OEMINFO_NONEWP_UPDATE_ENTER_INFO = 103,
    OEMINFO_NONEWP_RETAIL_APK = 104,
    OEMINFO_NONEWP_CUST_OVMODE_STATE = 105,
    OEMINFO_NONEWP_PRELOAD_OVMODE_STATE = 106,
    OEMINFO_NONEWP_ESIM_EID = 107,
    OEMINFO_NONEWP_CLOUD_ROM_INSTALL_STAT = 108,
    OEMINFO_NONEWP_DEV_NG_INFO = 109,
    OEMINFO_NONEWP_VSIM_WIFIPOLL_FLAG = 110,
    OEMINFO_NONEWP_VSIM_DOWNLOAD_STATUS = 111,
    OEMINFO_NONEWP_VSIM_WIFI_CONN_STATUS = 112,
    OEMINFO_NONEWP_CAMERA_SPECS = 113,
    OEMINFO_NONEWP_UNBIND_FLAG = 114,
    OEMINFO_NONEWP_DEBUG_EFUSE_FLAG = 115,
    OEMINFO_NONEWP_WIFI_COUNTRY_CODE = 116,
    OEMINFO_NONEWP_CCFLAG = 117,
    OEMINFO_NONEWP_FACT_ODM = 118,
    OEMINFO_NONEWP_RUNTESTB_TYPE = 119,
    OEMINFO_NONEWP_POPUPUSEDCOUNT = 120,
    OEMINFO_NONEWP_MOTORCODE = 121,
    /* the id from 500 to 550 of 8k area are reserved */
    OEMINFO_NONEWP_REUSED_ID_551_8K_VALID_SIZE_448_BYTE = 551,
    OEMINFO_NONEWP_REUSED_ID_552_8K_VALID_SIZE_64_BYTE = 552,
    OEMINFO_NONEWP_FMD_VERIFY_DATA = 553,
    OEMINFO_NONEWP_FCDEG = 554,
    /* the id from 700 to 710 of 64k area are reserved */
    OEMINFO_NONEWP_WCNSS_FIR_BAK_TYPE = 711,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION1_TYPE = 712,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION2_TYPE = 713,
    OEMINFO_NONEWP_FASTBOOT_FMD_DATA = 714,
    OEMINFO_NONEWP_BASE_CLOUD_ROM_LIST = 715,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION3_TYPE = 717,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION4_TYPE = 718,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION5_TYPE = 719,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION6_TYPE = 720,
    OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION7_TYPE = 721,
    OEMINFO_NONEWP_CAMERA_BLC_CALIB = 722,
    OEMINFO_NONEWP_SPECCALIB0 = 723,
    OEMINFO_NONEWP_SPECCALIB1 = 724,
    /* the next id is used for oeminfo_lite root-wp area */
    OEMINFO_ROOTWP_BOARD_CODE_TYPE = 751,
    OEMINFO_ROOTWP_VENDER_AND_COUNTRY_NAME = 752,
    OEMINFO_ROOTWP_IMEI2_NUM = 753,
    OEMINFO_ROOTWP_FACT_INFO = 754,
    OEMINFO_ROOTWP_HWSB_AES_PWD = 755,
    OEMINFO_ROOTWP_SKU_TYPE = 756,
    OEMINFO_ROOTWP_MANUFACTURE_DATE_TYPE = 757,
    OEMINFO_ROOTWP_LGU_SPECIAL_SN = 758,
    OEMINFO_ROOTWP_IMEI_TYPE = 759,
    OEMINFO_ROOTWP_MEID_TYPE =760,
    OEMINFO_ROOTWP_ESN_TYPE = 761,
    OEMINFO_ROOTWP_BT_TYPE = 762,
    OEMINFO_ROOTWP_WIFI_TYPE =763,
    OEMINFO_ROOTWP_FACTORY_INFO_TYPE = 764,
    OEMINFO_ROOTWP_BUILD_NUMBER_TYPE = 766,
    OEMINFO_ROOTWP_SINGLE_SIM = 767,
    OEMINFO_ROOTWP_PRODUCT_MODEL_TYPE = 768,
    OEMINFO_ROOTWP_DEVICE_MODEL = 769,
    OEMINFO_ROOTWP_FRP_OPEN_FLAG = 770,
    OEMINFO_ROOTWP_FRP_KEY = 771,
    OEMINFO_ROOTWP_PRODUCE_PHASE_FLAG = 772,
    OEMINFO_ROOTWP_MMIAUTORUN = 773,
    OEMINFO_ROOTWP_MMIAUTOCFG = 774,
    OEMINFO_ROOTWP_RD_LOCK_STATE = 775,
    OEMINFO_ROOTWP_ACCESS_LOCK_STATE = 776,
    OEMINFO_ROOTWP_ACCESS_KEY = 777,
    OEMINFO_ROOTWP_CONTROL_FLAG = 778,
    OEMINFO_ROOTWP_QRCODE_INFO = 779,
    OEMINFO_ROOTWP_LOG_SWITCH_TYPE = 780,
    OEMINFO_ROOTWP_BASEBAND_VERSION_TYPE = 781,
    OEMINFO_ROOTWP_CONTROL_BIT = 782,
    OEMINFO_ROOTWP_UDID_CODE_TYPE = 783,
    OEMINFO_ROOTWP_SYSDLL_INSTALL_TYPE =786,
    OEMINFO_ROOTWP_TOF_EYE_SAFE_TYPE = 789,
    OEMINFO_ROOTWP_AUTH_ELABEL = 790,

    /* the id from 1250 to 1300 of 8k area are reserved */
    OEMINFO_ROOTWP_DEVICE_CERTIFICATE1 = 1301,
    OEMINFO_ROOTWP_DEVICE_CERTIFICATE2 = 1302,
    OEMINFO_ROOTWP_KEY_ATTESTATION = 1303,

    /* the id from 1450 to 1460 of 64k area are reserved */
    OEMINFO_ROOTWP_NFC_SEID = 1461,
    OEMINFO_ROOTWP_SCREEN_DATA = 1462,

    /* the next id is used for oeminfo_lite powerup-wp area */
    OEMINFO_PWRUPWP_SENSOR_PROXIMITY_PA2240_CALIBRATION = 1501,
    OEMINFO_PWRUPWP_OIS_GYROGAIN = 1502,
    OEMINFO_PWRUPWP_ACCEL_SENSOR_DATA = 1503,
    OEMINFO_PWRUPWP_CAP_PROX_DATA_INFO = 1504,
    OEMINFO_PWRUPWP_SENSOR_LIGHT_BH1745_CALIBRATION = 1505,
    OEMINFO_PWRUPWP_GYRO_SENSOR_DATA = 1506,
    OEMINFO_PWRUPWP_QRCODE_BSN = 1507,
    OEMINFO_PWRUPWP_CAMERA_LASERCALIB = 1508,
    OEMINFO_PWRUPWP_MOD_HASH = 1509,
    OEMINFO_PWRUPWP_TOF_EYE_SAFE_TYPE = 1510,
    OEMINFO_PWRUPWP_LANMAC = 1511,
    /* the id from 2000 to 2050 of 8k area are reserved */
    /* the id from 2200 to 2210 of 64k area are reserved */
    OEMINFO_PWRUPWP_CAMERA_HDC = 2211,
    OEMINFO_PWRUPWP_GMP = 2212,
    /* the next id is used for  oeminfo_lite logo area */
    OEMINFO_NONEWP_ASCEND_TYPE = 2251,
    OEMINFO_NONEWP_LOW_POWER_TYPE = 2252,
    OEMINFO_NONEWP_POWER_CHARGING_TYPE = 2253,
} OeminfoTypeLite;

struct OeminfoV6ToLite {
    oeminfo_type oeminfoV6IdIndex;
    OeminfoTypeLite oeminfoLiteIdIndex;
};

static struct OeminfoV6ToLite g_oeminfoLiteTable[] = {
    { /* ----------- */
        0,
        0
    }, { /* oeminfoid=1 */
        0,
        0
    }, { /* oeminfoid=2 */
        0,
        0
    }, { /* oeminfoid=3 */
        0,
        0
    }, { /* oeminfoid=4 */
        0,
        0
    }, { /* oeminfoid=5 */
        0,
        0
    }, { /* oeminfoid=6 */
        0,
        0
    }, { /* oeminfoid=7 */
        0,
        0
    }, { /* oeminfoid=8 */
        OEMINFO_AMSS_VER_TYPE,
        OEMINFO_NONEWP_AMSS_VER_TYPE
    }, { /* oeminfoid=9 */
        OEMINFO_OEMSBL_VER_TYPE,
        OEMINFO_NONEWP_OEMSBL_VER_TYPE
    }, { /* oeminfoid=10 */
        0,
        0
    }, { /* oeminfoid=11 */
        0,
        0
    }, { /* oeminfoid=12 */
        0,
        0
    }, { /* oeminfoid=13 */
        0,
        0
    }, { /* oeminfoid=14 */
        OEMINFO_UPDATE_TYPE,
        OEMINFO_NONEWP_UPDATE_TYPE
    }, { /* oeminfoid=15 */
        OEMINFO_BOARD_CODE_TYPE,
        OEMINFO_ROOTWP_BOARD_CODE_TYPE
    }, { /* oeminfoid=16 */
        0,
        0
    }, { /* oeminfoid=17 */
        0,
        0
    }, { /* oeminfoid=18 */
        OEMINFO_VENDER_AND_COUNTRY_NAME,
        OEMINFO_ROOTWP_VENDER_AND_COUNTRY_NAME
    }, { /* oeminfoid=19 */
        0,
        0
    }, { /* oeminfoid=20 */
        0,
        0
    }, { /* oeminfoid=21 */
        0,
        0
    }, { /* oeminfoid=22 */
        0,
        0
    }, { /* oeminfoid=23 */
        OEMINFO_WIFI_TEST,
        OEMINFO_NONEWP_WIFI_TEST
    }, { /* oeminfoid=24 */
        OEMINFO_IMEI2_NUM,
        OEMINFO_ROOTWP_IMEI2_NUM
    }, { /* oeminfoid=25 */
        0,
        0
    }, { /* oeminfoid=26 */
        OEMINFO_FACT_INFO,
        OEMINFO_ROOTWP_FACT_INFO
    }, { /* oeminfoid=27 */
        OEMINFO_FACT_ODM,
        OEMINFO_NONEWP_FACT_ODM
    }, { /* oeminfoid=28 */
        OEMINFO_HWSB_AES_PWD,
        OEMINFO_ROOTWP_HWSB_AES_PWD
    }, { /* oeminfoid=29 */
        OEMINFO_HWSB_LOCK_STATE,
        OEMINFO_NONEWP_HWSB_LOCK_STATE
    }, { /* oeminfoid=30 */
        OEMINFO_HWFB_LOCK_STATE,
        OEMINFO_NONEWP_HWFB_LOCK_STATE
    }, { /* oeminfoid=31 */
        0,
        0
    }, { /* oeminfoid=32 */
        OEMINFO_SENSOR_PROXIMITY_PA2240_CALIBRATION,
        OEMINFO_PWRUPWP_SENSOR_PROXIMITY_PA2240_CALIBRATION
    }, { /* oeminfoid=33 */
        0,
        0
    }, { /* oeminfoid=34 */
        0,
        0
    }, { /* oeminfoid=35 */
        0,
        0
    }, { /* oeminfoid=36 */
        0,
        0
    }, { /* oeminfoid=37 */
        0,
        0
    }, { /* oeminfoid=38 */
        OEMINFO_SKU_TYPE,
        OEMINFO_ROOTWP_SKU_TYPE
    }, { /* oeminfoid=39 */
        OEMINFO_MANUFACTURE_DATE_TYPE,
        OEMINFO_ROOTWP_MANUFACTURE_DATE_TYPE
    }, { /* oeminfoid=40 */
        OEMINFO_VSIM_WIFIPOLL_FLAG,
        OEMINFO_NONEWP_VSIM_WIFIPOLL_FLAG
    }, { /* oeminfoid=41 */
        OEMINFO_VSIM_DOWNLOAD_STATUS,
        OEMINFO_NONEWP_VSIM_DOWNLOAD_STATUS
    }, { /* oeminfoid=42 */
        OEMINFO_OIS_GYROGAIN,
        OEMINFO_PWRUPWP_OIS_GYROGAIN
    }, { /* oeminfoid=43 */
        OEMINFO_VSIM_WIFI_CONN_STATUS,
        OEMINFO_NONEWP_VSIM_WIFI_CONN_STATUS
    }, { /* oeminfoid=44 */
        OEMINFO_LGU_SPECIAL_SN,
        OEMINFO_ROOTWP_LGU_SPECIAL_SN
    }, { /* oeminfoid=45 */
        OEMINFO_USER_USAGE_ACTIVDATE_TYPE,
        OEMINFO_NONEWP_USER_USAGE_ACTIVDATE_TYPE
    }, { /* oeminfoid=46 */
        OEMINFO_UNBIND_FLAG,
        OEMINFO_NONEWP_UNBIND_FLAG
    }, { /* oeminfoid=47 */
        OEMINFO_USB_PID_INDEX,
        OEMINFO_NONEWP_USB_PID_INDEX
    }, { /* oeminfoid=48 */
        OEMINFO_IMEI_TYPE,
        OEMINFO_ROOTWP_IMEI_TYPE
    }, { /* oeminfoid=49 */
        OEMINFO_MEID_TYPE,
        OEMINFO_ROOTWP_MEID_TYPE
    }, { /* oeminfoid=50 */
        OEMINFO_ESN_TYPE,
        OEMINFO_ROOTWP_ESN_TYPE
    }, { /* oeminfoid=51 */
        OEMINFO_BT_TYPE,
        OEMINFO_ROOTWP_BT_TYPE
    }, { /* oeminfoid=52 */
        OEMINFO_WIFI_TYPE,
        OEMINFO_ROOTWP_WIFI_TYPE
    }, { /* oeminfoid=53 */
        OEMINFO_FACTORY_INFO_TYPE,
        OEMINFO_ROOTWP_FACTORY_INFO_TYPE
    }, { /* oeminfoid=54 */
        OEMINFO_RUNTESTB_TYPE,
        OEMINFO_NONEWP_RUNTESTB_TYPE
    }, { /* oeminfoid=55 */
        OEMINFO_POPUPUSEDCOUNT,
        OEMINFO_NONEWP_POPUPUSEDCOUNT
    }, { /* oeminfoid=56 */
        OEMINFO_MOTORCODE,
        OEMINFO_NONEWP_MOTORCODE
    }, { /* oeminfoid=57 */
        OEMINFO_USB_PID_CUSTOM_TYPE,
        OEMINFO_NONEWP_USB_PID_CUSTOM_TYPE
    }, { /* oeminfoid=58 */
        OEMINFO_LANMAC,
        OEMINFO_PWRUPWP_LANMAC
    }, { /* oeminfoid=59 */
        0,
        0
    }, { /* oeminfoid=60 */
        0,
        0
    }, { /* oeminfoid=61 */
        0,
        0
    }, { /* oeminfoid=62 */
        0,
        0
    }, { /* oeminfoid=63 */
        0,
        0
    }, { /* oeminfoid=64 */
        OEMINFO_CCFLAG,
        OEMINFO_NONEWP_CCFLAG
    }, { /* oeminfoid=65 */
        OEMINFO_TPCOLOR_TYPE,
        OEMINFO_NONEWP_TPCOLOR_TYPE
    }, { /* oeminfoid=66 */
        OEMINFO_CRYPTFS_STATE,
        OEMINFO_NONEWP_CRYPTFS_STATE
    }, { /* oeminfoid=67 */
        OEMINFO_ROOT_INFO_TYPE,
        OEMINFO_NONEWP_ROOT_INFO_TYPE
    }, { /* oeminfoid=68 */
        OEMINFO_RESCUEINFO_TYPE,
        OEMINFO_NONEWP_RESCUEINFO_TYPE
    }, { /* oeminfoid=69 */
        OEMINFO_TOF_REPAIR_EYE_SAFE_TYPE,
        OEMINFO_ROOTWP_TOF_EYE_SAFE_TYPE
    }, { /* oeminfoid=70 */
        OEMINFO_ACCEL_SENSOR_DATA,
        OEMINFO_PWRUPWP_ACCEL_SENSOR_DATA
    }, { /* oeminfoid=71 */
        OEMINFO_AUTH_ELABEL,
        OEMINFO_ROOTWP_AUTH_ELABEL
    }, { /* oeminfoid=72 */
        OEMINFO_DEBUG_EFUSE_FLAG,
        OEMINFO_NONEWP_DEBUG_EFUSE_FLAG
    }, { /* oeminfoid=73 */
        OEMINFO_WIFI_COUNTRY_CODE,
        OEMINFO_NONEWP_WIFI_COUNTRY_CODE
    }, { /* oeminfoid=74 */
        OEMINFO_CUST_OVMODE_STATE,
        OEMINFO_NONEWP_CUST_OVMODE_STATE
    }, { /* oeminfoid=75 */
        OEMINFO_PRELOAD_OVMODE_STATE,
        OEMINFO_NONEWP_PRELOAD_OVMODE_STATE
    }, { /* oeminfoid=76 */
        OEMINFO_SYSDLL_INSTALL_TYPE,
        OEMINFO_ROOTWP_SYSDLL_INSTALL_TYPE
    }, { /* oeminfoid=77 */
        OEMINFO_RETAIL_APK,
        OEMINFO_NONEWP_RETAIL_APK
    }, { /* oeminfoid=78 */
        OEMINFO_BUILD_NUMBER_TYPE,
        OEMINFO_ROOTWP_BUILD_NUMBER_TYPE
    }, { /* oeminfoid=79 */
        OEMINFO_CAEMRA_DF_TYPE,
        OEMINFO_NONEWP_CAEMRA_DF_TYPE
    }, { /* oeminfoid=80 */
        OEM_CAP_PROX_DATA_INFO,
        OEMINFO_PWRUPWP_CAP_PROX_DATA_INFO
    }, { /* oeminfoid=81 */
        OEMINFO_SINGLE_SIM,
        OEMINFO_ROOTWP_SINGLE_SIM
    }, { /* oeminfoid=82 */
        OEMINFO_CAMERA_HALL_OVERTURN_DATA,
        OEMINFO_NONEWP_CAMERA_HALL_OVERTURN_DATA
    }, { /* oeminfoid=83 */
        OEMINFO_AT_LOCK_PASSWORD_SHA256,
        OEMINFO_NONEWP_AT_LOCK_PASSWORD_SHA256
    }, { /* oeminfoid=84 */
        OEMINFO_RESET_ENTER_INFO,
        OEMINFO_NONEWP_RESET_ENTER_INFO
    }, { /* oeminfoid=85 */
        OEMINFO_UPDATE_ENTER_INFO,
        OEMINFO_NONEWP_UPDATE_ENTER_INFO
    }, { /* oeminfoid=86 */
        OEMINFO_DAP_TIME,
        OEMINFO_NONEWP_DAP_TIME
    }, { /* oeminfoid=87 */
        OEMINFO_PSID_DATA,
        OEMINFO_NONEWP_PSID_DATA
    }, { /* oeminfoid=88 */
        OEMINFO_RESOURCE_VERSION,
        OEMINFO_NONEWP_RESOURCE_VERSION
    }, { /* oeminfoid=89 */
        OEMINFO_CRYPTFS_DATA,
        OEMINFO_NONEWP_CRYPTFS_DATA
    }, { /* oeminfoid=90 */
        0,
        0
    }, { /* oeminfoid=91 */
        OEMINFO_PRODUCT_MODEL_TYPE,
        OEMINFO_ROOTWP_PRODUCT_MODEL_TYPE
    }, { /* oeminfoid=92 */
        OEMINFO_INITRC_TRACER,
        OEMINFO_NONEWP_INITRC_TRACER
    }, { /* oeminfoid=93 */
        OEMINFO_FBLOCK_STAT_INFO,
        OEMINFO_NONEWP_FBLOCK_STAT_INFO
    }, { /* oeminfoid=94 */
        OEMINFO_VENDER_AND_COUNTRY_NAME_COTA,
        OEMINFO_NONEWP_VENDER_AND_COUNTRY_NAME_COTA
    }, { /* oeminfoid=95 */
        OEMINFO_ANTITHIVES_STATE,
        OEMINFO_NONEWP_ANTITHIVES_STATE
    }, { /* oeminfoid=96 */
        OEMINFO_WIFI_CAL_INDEX,
        OEMINFO_NONEWP_WIFI_CAL_INDEX
    }, { /* oeminfoid=97 */
        OEMINFO_DEVICE_MODEL,
        OEMINFO_ROOTWP_DEVICE_MODEL
    }, { /* oeminfoid=98 */
        OEMINFO_FRP_OPEN_FLAG,
        OEMINFO_ROOTWP_FRP_OPEN_FLAG
    }, { /* oeminfoid=99 */
        OEMINFO_FRP_KEY,
        OEMINFO_ROOTWP_FRP_KEY
    }, { /* oeminfoid=100 */
        OEMINFO_SD_LOCK_PASSWD,
        OEMINFO_NONEWP_SD_LOCK_PASSWD
    }, { /* oeminfoid=101 */
        OEMINFO_MAIN_VERSION,
        OEMINFO_NONEWP_MAIN_VERSION
    }, { /* oeminfoid=102 */
        OEMINFO_TOF_EYE_SAFE_TYPE,
        OEMINFO_PWRUPWP_TOF_EYE_SAFE_TYPE
    }, { /* oeminfoid=103 */
        OEMINFO_FB_STP_TYPE,
        OEMINFO_NONEWP_FB_STP_TYPE
    }, { /* oeminfoid=104 */
        OEMINFO_BOOTANIM_SWITCH,
        OEMINFO_NONEWP_BOOTANIM_SWITCH
    }, { /* oeminfoid=105 */
        OEMINFO_BOOTANIM_SHUTFLAG,
        OEMINFO_NONEWP_BOOTANIM_SHUTFLAG
    }, { /* oeminfoid=106 */
        OEMINFO_BOOTANIM_RINGMODE,
        OEMINFO_NONEWP_BOOTANIM_RINGMODE
    }, { /* oeminfoid=107 */
        OEMINFO_BOOTANIM_RINGDEX,
        OEMINFO_NONEWP_BOOTANIM_RINGDEX
    }, { /* oeminfoid=108 */
        OEMINFO_DATA_PART_FAULT_TIMES,
        OEMINFO_NONEWP_DATA_PART_FAULT_TIMES
    }, { /* oeminfoid=109 */
        OEMINFO_CLOUD_ROM_INSTALL_STAT,
        OEMINFO_NONEWP_CLOUD_ROM_INSTALL_STAT
    }, { /* oeminfoid=110 */
        OEMINFO_APPS_NO_INSTALL_TYPE,
        OEMINFO_NONEWP_APPS_NO_INSTALL_TYPE
    }, { /* oeminfoid=111 */
        OEMINFO_SYSTEM_VERSION,
        OEMINFO_NONEWP_SYSTEM_VERSION
    }, { /* oeminfoid=112 */
        OEMINFO_DEV_NG_INFO,
        OEMINFO_NONEWP_DEV_NG_INFO
    }, { /* oeminfoid=113 */
        OEMINFO_CAMERA_SPECS,
        OEMINFO_NONEWP_CAMERA_SPECS
    }, { /* oeminfoid=114 */
        OEMINFO_GAMMA_DATA,
        OEMINFO_NONEWP_GAMMA_DATA
    }, { /* oeminfoid=115 */
        OEMINFO_GAMMA_LEN,
        OEMINFO_NONEWP_GAMMA_LEN
    }, { /* oeminfoid=116 */
        OEMINFO_SENSOR_LIGHT_BH1745_CALIBRATION,
        OEMINFO_PWRUPWP_SENSOR_LIGHT_BH1745_CALIBRATION
    }, { /* oeminfoid=117 */
        OEMINFO_MCC_MNC_TYPE,
        OEMINFO_NONEWP_MCC_MNC_TYPE
    }, { /* oeminfoid=118 */
        OEMINFO_PRODUCE_PHASE_FLAG,
        OEMINFO_ROOTWP_PRODUCE_PHASE_FLAG
    }, { /* oeminfoid=119 */
        OEMINFO_GYRO_SENSOR_DATA,
        OEMINFO_PWRUPWP_GYRO_SENSOR_DATA
    }, { /* oeminfoid=120 */
        OEMINFO_OPERATOR_INFO,
        OEMINFO_NONEWP_OPERATOR_INFO
    }, { /* oeminfoid=121 */
        OEMINFO_HOTA_UPDATE_AUTH_LOCK,
        OEMINFO_NONEWP_HOTA_UPDATE_AUTH_LOCK
    }, { /* oeminfoid=122 */
        OEMINFO_FACTORY_PRODUCT_VERSION_TYPE,
        OEMINFO_NONEWP_FACTORY_PRODUCT_VERSION_TYPE
    }, { /* oeminfoid=123 */
        OEMINFO_AUDIO_SMARTPA_CALIBRATION,
        OEMINFO_NONEWP_AUDIO_SMARTPA_CALIBRATION
    }, { /* oeminfoid=124 */
        OEMINFO_WIFIONLY_IDENTITY,
        OEMINFO_NONEWP_WIFIONLY_IDENTITY
    }, { /* oeminfoid=125 */
        OEMINFO_MMIAUTORUN,
        OEMINFO_ROOTWP_MMIAUTORUN
    }, { /* oeminfoid=126 */
        OEMINFO_LCD_BRIGHT_COLOR_CALIBRATION,
        OEMINFO_NONEWP_LCD_BRIGHT_COLOR_CALIBRATION
    }, { /* oeminfoid=127 */
        OEMINFO_FOTA_MODE,
        OEMINFO_NONEWP_FOTA_MODE
    }, { /* oeminfoid=128 */
        OEMINFO_MMIAUTOCFG,
        OEMINFO_ROOTWP_MMIAUTOCFG
    }, { /* oeminfoid=129 */
        OEMINFO_MARKETING_NAME,
        OEMINFO_NONEWP_MARKETING_NAME
    }, { /* oeminfoid=130 */
        OEMINFO_RD_LOCK_STATE,
        OEMINFO_ROOTWP_RD_LOCK_STATE
    }, { /* oeminfoid=131 */
        OEMINFO_ACCESS_LOCK_STATE,
        OEMINFO_ROOTWP_ACCESS_LOCK_STATE
    }, { /* oeminfoid=132 */
        OEMINFO_ACCESS_KEY,
        OEMINFO_ROOTWP_ACCESS_KEY
    }, { /* oeminfoid=133 */
        OEMINFO_OVMODE_STATE,
        OEMINFO_NONEWP_OVMODE_STATE
    }, { /* oeminfoid=134 */
        OEMINFO_ESIM_EID,
        OEMINFO_NONEWP_ESIM_EID
    }, { /* oeminfoid=135 */
        OEMINFO_OMADM_INFO,
        OEMINFO_NONEWP_OMADM_INFO
    }, { /* oeminfoid=136 */
        OEMINFO_ACTIVATION_STATUS_ID,
        OEMINFO_NONEWP_ACTIVATION_STATUS_ID
    }, { /* oeminfoid=137 */
        OEMINFO_HWPARENTCONTROL_RECOVER_DATA,
        OEMINFO_NONEWP_HWPARENTCONTROL_RECOVER_DATA
    }, { /* oeminfoid=138 */
        OEMINFO_CONTROL_FLAG,
        OEMINFO_ROOTWP_CONTROL_FLAG
    }, { /* oeminfoid=139 */
        OEMINFO_FIX_HOTA_INFO_INTI,
        OEMINFO_NONEWP_FIX_HOTA_INFO_INTI
    }, { /* oeminfoid=140 */
        OEMINFO_FIX_HOTA_INFO,
        OEMINFO_NONEWP_FIX_HOTA_INFO
    }, { /* oeminfoid=141 */
        OEMINFO_FIX_HOTA_STRING_INTI,
        OEMINFO_NONEWP_FIX_HOTA_STRING_INTI
    }, { /* oeminfoid=142 */
        OEMINFO_FIX_HOTA_STRING,
        OEMINFO_NONEWP_FIX_HOTA_STRING
    }, { /* oeminfoid=143 */
        OEMINFO_QRCODE_INFO,
        OEMINFO_ROOTWP_QRCODE_INFO
    }, { /* oeminfoid=144 */
        OEMINFO_QRCODE_BSN,
        OEMINFO_PWRUPWP_QRCODE_BSN
    }, { /* oeminfoid=145 */
        OEMINFO_VENDER_AND_COUNTRY_NAME_COTA_FACTORY,
        OEMINFO_NONEWP_VENDER_AND_COUNTRY_NAME_COTA_FACTORY
    }, { /* oeminfoid=146 */
        OEMINFO_USER_HOTA_UPDATE,
        OEMINFO_NONEWP_USER_HOTA_UPDATE
    }, { /* oeminfoid=147 */
        OEMINFO_LOG_SWITCH_TYPE,
        OEMINFO_ROOTWP_LOG_SWITCH_TYPE
    }, { /* oeminfoid=148 */
        OEMINFO_USE_DECRESS_BAT_VOL_FLAG,
        OEMINFO_NONEWP_USE_DECRESS_BAT_VOL_FLAG
    }, { /* oeminfoid=149 */
        OEMINFO_CAMERA_LASERCALIB,
        OEMINFO_PWRUPWP_CAMERA_LASERCALIB
    }, { /* oeminfoid=150 */
        OEMINFO_UPDATE_UDID,
        OEMINFO_NONEWP_UPDATE_UDID
    }, { /* oeminfoid=151 */
        OEMINFO_BASEBAND_VERSION_TYPE,
        OEMINFO_ROOTWP_BASEBAND_VERSION_TYPE
    }, { /* oeminfoid=152 */
        OEMINFO_FACTORY_SW_VERSION,
        OEMINFO_NONEWP_FACTORY_SW_VERSION
    }, { /* oeminfoid=153 */
        OEMINFO_PRELOAD_APP_BATCH,
        OEMINFO_NONEWP_PRELOAD_APP_BATCH
    }, { /* oeminfoid=154 */
        OEMINFO_CUST_Y_CABLE_TYPE,
        OEMINFO_NONEWP_CUST_Y_CABLE_TYPE
    }, { /* oeminfoid=155 */
        OEMINFO_UDID_CODE_TYPE,
        OEMINFO_ROOTWP_UDID_CODE_TYPE
    }, { /* oeminfoid=156 */
        OEMINFO_BASE_VER_TYPE,
        OEMINFO_NONEWP_BASE_VER_TYPE
    }, { /* oeminfoid=157 */
        OEMINFO_CUST_VER_TYPE,
        OEMINFO_NONEWP_CUST_VER_TYPE
    }, { /* oeminfoid=158 */
        OEMINFO_PRELOAD_VER_TYPE,
        OEMINFO_NONEWP_PRELOAD_VER_TYPE
    }, { /* oeminfoid=159 */
        0,
        0
    }, { /* oeminfoid=160 */
        OEMINFO_POWERONLOCK,
        OEMINFO_NONEWP_POWERONLOCK
    }, { /* oeminfoid=161 */
        OEMINFO_SDUPDATE_TYPE,
        OEMINFO_NONEWP_SDUPDATE_TYPE
    }, { /* oeminfoid=162 */
        0,
        0
    }, { /* oeminfoid=163 */
        OEMINFO_ENABLE_RETREAD,
        OEMINFO_NONEWP_ENABLE_RETREAD
    }, { /* oeminfoid=164 */
        OEMINFO_BDRT_TYPE,
        OEMINFO_NONEWP_BDRT_TYPE
    }, { /* oeminfoid=165 */
        OEMINFO_MOD_HASH,
        OEMINFO_PWRUPWP_MOD_HASH
    }, { /* oeminfoid=166 */
        OEMINFO_SYSTEM_UPDATE_AUTH_TOKEN_INFO,
        OEMINFO_NONEWP_SYSTEM_UPDATE_AUTH_TOKEN_INFO
    }, { /* oeminfoid=167 */
        OEMINFO_ENCRYPT_INFO,
        OEMINFO_NONEWP_ENCRYPT_INFO
    }, { /* oeminfoid=168 */
        OEMINFO_CONTROL_BIT,
        OEMINFO_ROOTWP_CONTROL_BIT
    }, { /* oeminfoid=169 */
        OEMINFO_MALWARE_DIAGNOSE,
        OEMINFO_NONEWP_MALWARE_DIAGNOSE
    }, { /* oeminfoid=170 */
        OEMINFO_RETREAD_DIAGNOSE,
        OEMINFO_NONEWP_RETREAD_DIAGNOSE
    }, { /* oeminfoid=171 */
        OEMINFO_ROOT_DIAGNOSE,
        OEMINFO_NONEWP_ROOT_DIAGNOSE
    }, { /* oeminfoid=172 */
        OEMINFO_SYSTEM_VERSION_B,
        OEMINFO_NONEWP_SYSTEM_VERSION_B
    }, { /* oeminfoid=173 */
        OEMINFO_ANDROID_VERSION,
        OEMINFO_NONEWP_ANDROID_VERSION
    }, { /* oeminfoid=174 */
        OEMINFO_ANDROID_VERSION_B,
        OEMINFO_NONEWP_ANDROID_VERSION_B
    }, { /* oeminfoid=175 */
        OEMINFO_SLOT_INFO_A,
        OEMINFO_NONEWP_SLOT_INFO_A
    }, { /* oeminfoid=176 */
        OEMINFO_SLOT_INFO_B,
        OEMINFO_NONEWP_SLOT_INFO_B
    }, { /* oeminfoid=177 */
        OEMINFO_SLOT_STATUS,
        OEMINFO_NONEWP_SLOT_STATUS
    }, { /* oeminfoid=178 */
        OEMINFO_VERITY_MODE_UPDATE_STATUS,
        OEMINFO_NONEWP_VERITY_MODE_UPDATE_STATUS
    }, { /* oeminfoid=179 */
        OEMINFO_SYSTEM_UPDATE_STATE,
        OEMINFO_NONEWP_SYSTEM_UPDATE_STATE
    }, { /* oeminfoid=180 */
        OEMINFO_CUSTOM_VERSION,
        OEMINFO_NONEWP_CUSTOM_VERSION
    }, { /* oeminfoid=181 */
        OEMINFO_CUSTOM_VERSION_B,
        OEMINFO_NONEWP_CUSTOM_VERSION_B
    }, { /* oeminfoid=182 */
        OEMINFO_PRELOAD_VERSION,
        OEMINFO_NONEWP_PRELOAD_VERSION
    }, { /* oeminfoid=183 */
        OEMINFO_PRELOAD_VERSION_B,
        OEMINFO_NONEWP_PRELOAD_VERSION_B
    }, { /* oeminfoid=184 */
        OEMINFO_GROUP_VERSION,
        OEMINFO_NONEWP_GROUP_VERSION
    }, { /* oeminfoid=185 */
        OEMINFO_GROUP_VERSION_B,
        OEMINFO_NONEWP_GROUP_VERSION_B
    }, { /* oeminfoid=186 */
        OEMINFO_BASE_VERSION,
        OEMINFO_NONEWP_BASE_VERSION
    }, { /* oeminfoid=187 */
        OEMINFO_BASE_VERSION_B,
        OEMINFO_NONEWP_BASE_VERSION_B
    }, { /* oeminfoid=188 */
        OEMINFO_COTA_REBOOT_UPDATE_PATH,
        OEMINFO_NONEWP_COTA_REBOOT_UPDATE_PATH
    }, { /* oeminfoid=189 */
        OEMINFO_CUSTOM_C_VERSION,
        OEMINFO_NONEWP_CUSTOM_C_VERSION
    }, { /* oeminfoid=190 */
        OEMINFO_RUNTEST_TYPE,
        OEMINFO_NONEWP_RUNTEST_TYPE
    }, { /* oeminfoid=191 */
        OEMINFO_MMITEST_TYPE,
        OEMINFO_NONEWP_MMITEST_TYPE
    }, { /* oeminfoid=192 */
        OEMINFO_OTA_SIMLOCK_ACTIVATED,
        OEMINFO_NONEWP_OTA_SIMLOCK_ACTIVATED
    }, { /* oeminfoid=193 */
        OEMINFO_ATLOCK_TYPE,
        OEMINFO_NONEWP_ATLOCK_TYPE
    }, { /* oeminfoid=194 */
        OEMINFO_RUNTEST2_TYPE,
        OEMINFO_NONEWP_RUNTEST2_TYPE
    }, { /* oeminfoid=195 */
        OEMINFO_RUNTIME_TYPE,
        OEMINFO_NONEWP_RUNTIME_TYPE
    }, { /* oeminfoid=196 */
        OEMINFO_MDATE_TYPE,
        OEMINFO_NONEWP_MDATE_TYPE
    }, { /* oeminfoid=197 */
        OEMINFO_MANUFACTURE_RESET_FLAG,
        OEMINFO_NONEWP_MANUFACTURE_RESET_FLAG
    }, { /* oeminfoid=198 */
        0,
        0
    }, { /* oeminfoid=199 */
        OEMINFO_BACKCOLOR,
        OEMINFO_NONEWP_BACKCOLOR
    }, { /* oeminfoid=200 */
        0,
        0
    }, { /* oeminfoid=201 */
        0,
        0
    }, { /* oeminfoid=202 */
        0,
        0
    }, { /* oeminfoid=203 */
        OEMINFO_REUSED_ID_203_8K_VALID_SIZE_448_BYTE,
        OEMINFO_NONEWP_REUSED_ID_551_8K_VALID_SIZE_448_BYTE
    }, { /* oeminfoid=204 */
        OEMINFO_REUSED_ID_204_8K_VALID_SIZE_64_BYTE,
        OEMINFO_NONEWP_REUSED_ID_552_8K_VALID_SIZE_64_BYTE
    }, { /* oeminfoid=205 */
        OEMINFO_FMD_VERIFY_DATA,
        OEMINFO_NONEWP_FMD_VERIFY_DATA
    }, { /* oeminfoid=206 */
        OEMINFO_DEVICE_CERTIFICATE1,
        OEMINFO_ROOTWP_DEVICE_CERTIFICATE1
    }, { /* oeminfoid=207 */
        OEMINFO_DEVICE_CERTIFICATE2,
        OEMINFO_ROOTWP_DEVICE_CERTIFICATE2
    }, { /* oeminfoid=208 */
        OEMINFO_KEY_ATTESTATION,
        OEMINFO_ROOTWP_KEY_ATTESTATION
    }, { /* oeminfoid=209 */
        OEMINFO_FCDEG,
        OEMINFO_NONEWP_FCDEG
    }, { /* oeminfoid=210 */
        0,
        0
    }, { /* oeminfoid=211 */
        0,
        0
    }, { /* oeminfoid=212 */
        0,
        0
    }, { /* oeminfoid=213 */
        0,
        0
    }, { /* oeminfoid=214 */
        0,
        0
    }, { /* oeminfoid=215 */
        0,
        0
    }, { /* oeminfoid=216 */
        0,
        0
    }, { /* oeminfoid=217 */
        0,
        0
    }, { /* oeminfoid=218 */
        0,
        0
    }, { /* oeminfoid=219 */
        0,
        0
    }, { /* oeminfoid=220 */
        0,
        0
    }, { /* oeminfoid=221 */
        0,
        0
    }, { /* oeminfoid=222 */
        0,
        0
    }, { /* oeminfoid=223 */
        0,
        0
    }, { /* oeminfoid=224 */
        0,
        0
    }, { /* oeminfoid=225 */
        0,
        0
    }, { /* oeminfoid=226 */
        0,
        0
    }, { /* oeminfoid=227 */
        0,
        0
    }, { /* oeminfoid=228 */
        0,
        0
    }, { /* oeminfoid=229 */
        0,
        0
    }, { /* oeminfoid=230 */
        0,
        0
    }, { /* oeminfoid=231 */
        0,
        0
    }, { /* oeminfoid=232 */
        0,
        0
    }, { /* oeminfoid=233 */
        0,
        0
    }, { /* oeminfoid=234 */
        0,
        0
    }, { /* oeminfoid=235 */
        0,
        0
    }, { /* oeminfoid=236 */
        0,
        0
    }, { /* oeminfoid=237 */
        0,
        0
    }, { /* oeminfoid=238 */
        0,
        0
    }, { /* oeminfoid=239 */
        0,
        0
    }, { /* oeminfoid=240 */
        0,
        0
    }, { /* oeminfoid=241 */
        0,
        0
    }, { /* oeminfoid=242 */
        0,
        0
    }, { /* oeminfoid=243 */
        0,
        0
    }, { /* oeminfoid=244 */
        0,
        0
    }, { /* oeminfoid=245 */
        0,
        0
    }, { /* oeminfoid=246 */
        0,
        0
    }, { /* oeminfoid=247 */
        0,
        0
    }, { /* oeminfoid=248 */
        0,
        0
    }, { /* oeminfoid=249 */
        0,
        0
    }, { /* oeminfoid=250 */
        0,
        0
    }, { /* oeminfoid=251 */
        0,
        0
    }, { /* oeminfoid=252 */
        0,
        0
    }, { /* oeminfoid=253 */
        0,
        0
    }, { /* oeminfoid=254 */
        0,
        0
    }, { /* oeminfoid=255 */
        0,
        0
    }, { /* oeminfoid=256 */
        0,
        0
    }, { /* oeminfoid=257 */
        0,
        0
    }, { /* oeminfoid=258 */
        0,
        0
    }, { /* oeminfoid=259 */
        0,
        0
    }, { /* oeminfoid=260 */
        0,
        0
    }, { /* oeminfoid=261 */
        0,
        0
    }, { /* oeminfoid=262 */
        0,
        0
    }, { /* oeminfoid=263 */
        0,
        0
    }, { /* oeminfoid=264 */
        0,
        0
    }, { /* oeminfoid=265 */
        0,
        0
    }, { /* oeminfoid=266 */
        0,
        0
    }, { /* oeminfoid=267 */
        0,
        0
    }, { /* oeminfoid=268 */
        0,
        0
    }, { /* oeminfoid=269 */
        0,
        0
    }, { /* oeminfoid=270 */
        0,
        0
    }, { /* oeminfoid=271 */
        0,
        0
    }, { /* oeminfoid=272 */
        0,
        0
    }, { /* oeminfoid=273 */
        0,
        0
    }, { /* oeminfoid=274 */
        0,
        0
    }, { /* oeminfoid=275 */
        0,
        0
    }, { /* oeminfoid=276 */
        0,
        0
    }, { /* oeminfoid=277 */
        0,
        0
    }, { /* oeminfoid=278 */
        0,
        0
    }, { /* oeminfoid=279 */
        0,
        0
    }, { /* oeminfoid=280 */
        0,
        0
    }, { /* oeminfoid=281 */
        0,
        0
    }, { /* oeminfoid=282 */
        0,
        0
    }, { /* oeminfoid=283 */
        0,
        0
    }, { /* oeminfoid=284 */
        0,
        0
    }, { /* oeminfoid=285 */
        0,
        0
    }, { /* oeminfoid=286 */
        0,
        0
    }, { /* oeminfoid=287 */
        0,
        0
    }, { /* oeminfoid=288 */
        0,
        0
    }, { /* oeminfoid=289 */
        0,
        0
    }, { /* oeminfoid=290 */
        0,
        0
    }, { /* oeminfoid=291 */
        0,
        0
    }, { /* oeminfoid=292 */
        0,
        0
    }, { /* oeminfoid=293 */
        0,
        0
    }, { /* oeminfoid=294 */
        0,
        0
    }, { /* oeminfoid=295 */
        0,
        0
    }, { /* oeminfoid=296 */
        0,
        0
    }, { /* oeminfoid=297 */
        0,
        0
    }, { /* oeminfoid=298 */
        0,
        0
    }, { /* oeminfoid=299 */
        0,
        0
    }, { /* oeminfoid=300 */
        0,
        0
    }, { /* oeminfoid=301 */
        OEMINFO_CAMERA_TOF_CALIBRATION1_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION1_TYPE
    }, { /* oeminfoid=302 */
        OEMINFO_CAMERA_TOF_CALIBRATION2_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION2_TYPE
    }, { /* oeminfoid=303 */
        OEMINFO_FASTBOOT_FMD_DATA,
        OEMINFO_NONEWP_FASTBOOT_FMD_DATA
    }, { /* oeminfoid=304 */
        OEMINFO_BASE_CLOUD_ROM_LIST,
        OEMINFO_NONEWP_BASE_CLOUD_ROM_LIST
    }, { /* oeminfoid=305 */
        OEMINFO_CAMERA_TOF_CALIBRATION3_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION3_TYPE
    }, { /* oeminfoid=306 */
        OEMINFO_CAMERA_TOF_CALIBRATION4_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION4_TYPE
    }, { /* oeminfoid=307 */
        OEMINFO_CAMERA_TOF_CALIBRATION5_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION4_TYPE
    }, { /* oeminfoid=308 */
        OEMINFO_CAMERA_TOF_CALIBRATION6_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION4_TYPE
    }, { /* oeminfoid=309 */
        OEMINFO_CAMERA_TOF_CALIBRATION7_TYPE,
        OEMINFO_NONEWP_CAMERA_TOF_CALIBRATION4_TYPE
    }, { /* oeminfoid=310 */
        OEMINFO_CAMERA_BLC_CALIB,
        OEMINFO_NONEWP_CAMERA_BLC_CALIB
    }, { /* oeminfoid=311 */
        OEMINFO_GMP,
        OEMINFO_PWRUPWP_GMP
    }, { /* oeminfoid=312 */
        OEMINFO_SCREEN_DATA,
        OEMINFO_ROOTWP_SCREEN_DATA
    }, { /* oeminfoid=313 */
        0,
        0
    }, { /* oeminfoid=314 */
        0,
        0
    }, { /* oeminfoid=315 */
        0,
        0
    }, { /* oeminfoid=316 */
        0,
        0
    }, { /* oeminfoid=317 */
        0,
        0
    }, { /* oeminfoid=318 */
        0,
        0
    }, { /* oeminfoid=319 */
        0,
        0
    }, { /* oeminfoid=320 */
        0,
        0
    }, { /* oeminfoid=321 */
        0,
        0
    }, { /* oeminfoid=322 */
        0,
        0
    }, { /* oeminfoid=323 */
        0,
        0
    }, { /* oeminfoid=324 */
        OEMINFO_WCNSS_FIR_BAK_TYPE,
        OEMINFO_NONEWP_WCNSS_FIR_BAK_TYPE
    }, { /* oeminfoid=325 */
        OEMINFO_SPECCALIB0,
        OEMINFO_NONEWP_SPECCALIB0
    }, { /* oeminfoid=326 */
        OEMINFO_SPECCALIB1,
        OEMINFO_NONEWP_SPECCALIB1
    }, { /* oeminfoid=327 */
        0,
        0
    }, { /* oeminfoid=328 */
        0,
        0
    }, { /* oeminfoid=329 */
        OEMINFO_CAMERA_HDC,
        OEMINFO_PWRUPWP_CAMERA_HDC
    }, { /* oeminfoid=330 */
        OEMINFO_NFC_SEID,
        OEMINFO_ROOTWP_NFC_SEID
    }, { /* oeminfoid=331 */
        0,
        0
    }, { /* oeminfoid=332 */
        0,
        0
    }, { /* oeminfoid=333 */
        0,
        0
    }, { /* oeminfoid=334 */
        0,
        0
    }, { /* oeminfoid=335 */
        0,
        0
    }, { /* oeminfoid=336 */
        0,
        0
    }, { /* oeminfoid=337 */
        0,
        0
    }, { /* oeminfoid=338 */
        0,
        0
    }, { /* oeminfoid=339 */
        0,
        0
    }, { /* oeminfoid=340 */
        0,
        0
    }, { /* oeminfoid=341 */
        0,
        0
    }, { /* oeminfoid=342 */
        0,
        0
    }, { /* oeminfoid=343 */
        0,
        0
    }, { /* oeminfoid=344 */
        0,
        0
    }, { /* oeminfoid=345 */
        0,
        0
    }, { /* oeminfoid=346 */
        0,
        0
    }, { /* oeminfoid=347 */
        0,
        0
    }, { /* oeminfoid=348 */
        0,
        0
    }, { /* oeminfoid=349 */
        0,
        0
    }, { /* oeminfoid=350 */
        0,
        0
    }, { /* oeminfoid=351 */
        OEMINFO_ASCEND_TYPE,
        OEMINFO_NONEWP_ASCEND_TYPE
    }, { /* oeminfoid=352 */
        OEMINFO_LOW_POWER_TYPE,
        OEMINFO_NONEWP_LOW_POWER_TYPE
    }, { /* oeminfoid=353 */
        OEMINFO_POWER_CHARGING_TYPE,
        OEMINFO_NONEWP_POWER_CHARGING_TYPE
    },
};


#endif /* INCLUDE_OEMINFO_V6_TO_LITE_H */
