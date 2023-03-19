#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include "fingerprint.h"

#include <linux/platform_data/spi-mt65xx.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
//#include <mt_spi.h>
//#include <mt_spi_hal.h>
extern void mt_spi_disable_master_clk(struct spi_device *spidev);
extern void mt_spi_enable_master_clk(struct spi_device *spidev);

int fingerprint_gpio_reset(struct fp_data* fingerprint);

//extern void adreno_force_waking_gpu(void);
unsigned int snr_flag = 0;

#if defined (CONFIG_HUAWEI_DSM)
#include <dsm/dsm_pub.h>
static struct dsm_dev dsm_fingerprint =
{
    .name = "dsm_fingerprint",
    .device_name = "fpc",
    .ic_name = "NNN",
    .module_name = "NNN",
    .fops = NULL,
    .buff_size = 1024,
};
static struct dsm_client* fingerprint_dclient = NULL;
#endif

typedef enum
{
    SPI_CLK_DISABLE = 0,
    SPI_CLK_ENABLE,
} spi_clk_state;
void fingerprint_gpio_output_dts(struct fp_data* fingerprint, int pin, int level)
{

    // mutex_lock(&spidev_set_gpio_mutex);
    //printk("[fpsensor]fpsensor_gpio_output_dts: gpio= %d, level = %d\n",gpio,level);
    if (pin == FINGERPRINT_RST_PIN)
    {
        if (level)
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_rst_high); }
        else
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_rst_low); }
    }
    else if (pin == FINGERPRINT_SPI_CS_PIN)
    {
        if (level)
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_cs_high); }
        else
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_cs_low); }
    }
    else if (pin == FINGERPRINT_SPI_MO_PIN)
    {
        if (level)
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_mo_high); }
        else
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_mo_low); }
    }
    else if (pin == FINGERPRINT_SPI_CK_PIN)
    {
        if (level)
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_ck_high); }
        else
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_ck_low); }
    }
    else if (pin == FINGERPRINT_SPI_MI_PIN)
    {
        if (level)
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_mi_high); }
        else
        { pinctrl_select_state(fingerprint->pinctrl, fingerprint->fp_mi_low); }
    }

    //mutex_unlock(&spidev_set_gpio_mutex);
}

static ssize_t result_show(struct device* device,
                           struct device_attribute* attribute,
                           char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    return scnprintf(buffer, PAGE_SIZE, "%i\n", fingerprint->autotest_input);
}

static ssize_t result_store(struct device* device,
                            struct device_attribute* attribute,
                            const char* buffer, size_t count)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    fingerprint->autotest_input = simple_strtoul(buffer, NULL, 10);
    sysfs_notify(&fingerprint->pf_dev->dev.kobj, NULL, "result");
    return count;
}

static DEVICE_ATTR(result, S_IRUSR | S_IWUSR, result_show, result_store);
 

static ssize_t gpio_get(struct device* device,
                         struct device_attribute* attribute,
                         char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);

    int irq = __gpio_get_value(fingerprint->irq_gpio);
    fpc_log_info("[fpc] gpio_irq_get : %d\n", irq);
    if ((1 == irq) && (fp_LCD_POWEROFF == atomic_read(&fingerprint->state)))
    {
        //adreno_force_waking_gpu();
    }

    return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device* device,
                       struct device_attribute* attribute,
                       char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);

    int irq = __gpio_get_value(fingerprint->irq_gpio);
    fpc_log_info("[fpc] irq_get : %d\n", irq);
    if ((1 == irq) && (fp_LCD_POWEROFF == atomic_read(&fingerprint->state)))
    {
        //adreno_force_waking_gpu();
    }

    return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}

/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device* device,
                       struct device_attribute* attribute,
                       const char* buffer, size_t count)
{
    //struct fp_data* fingerprint = dev_get_drvdata(device);
    fpc_log_info("[%s]buffer=%s.\n", __func__, buffer);
    return count;
}

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);


static ssize_t read_image_flag_show(struct device* device,
                                    struct device_attribute* attribute,
                                    char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    return scnprintf(buffer, PAGE_SIZE, "%u", (unsigned int)fingerprint->read_image_flag);
}
static ssize_t read_image_flag_store(struct device* device,
                                     struct device_attribute* attribute,
                                     const char* buffer, size_t count)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    fingerprint->read_image_flag = simple_strtoul(buffer, NULL, 10);
    return (ssize_t)count;
}

static DEVICE_ATTR(read_image_flag, S_IRUSR | S_IWUSR, read_image_flag_show, read_image_flag_store);

static ssize_t snr_show(struct device* device,
                        struct device_attribute* attribute,
                        char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    return scnprintf(buffer, PAGE_SIZE, "%d", fingerprint->snr_stat);
}

static ssize_t snr_store(struct device* device,
                         struct device_attribute* attribute,
                         const char* buffer, size_t count)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    fingerprint->snr_stat = simple_strtoul(buffer, NULL, 10);

    if (fingerprint->snr_stat)
    {
        snr_flag = 1;
    }
    else
    { snr_flag = 0; }

    fpc_log_err("snr_store snr_flag = %u\n", snr_flag);
    return count;
}

static DEVICE_ATTR(snr, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, snr_show, snr_store);

static ssize_t nav_show(struct device* device,
                        struct device_attribute* attribute,
                        char* buffer)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);

    if (NULL == fingerprint)
    {return -EINVAL;}

    return scnprintf(buffer, PAGE_SIZE, "%d", fingerprint->nav_stat);
}

static ssize_t nav_store(struct device* device,
                         struct device_attribute* attribute,
                         const char* buffer, size_t count)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);

    if (NULL == fingerprint)
    {return -EINVAL;}

    fingerprint->nav_stat = simple_strtoul(buffer, NULL, 10);
    return count;
}

/*lint -save -e* */
static DEVICE_ATTR(nav, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP, nav_show, nav_store);
/*lint -restore*/

static ssize_t fingerprint_chip_info_show(struct device* device, struct device_attribute* attribute, char* buf)
{
	char sensor_id[FP_MAX_SENSOR_ID_LEN]={0};

	if(NULL == device || NULL == buf)
	{
		fpc_log_err("%s failed,the pointer is null\n", __func__);
		return -EINVAL;
	}

	struct fp_data* fingerprint= dev_get_drvdata(device);
	if (NULL == fingerprint)
	{
		fpc_log_err("%s failed,the parameters is null\n", __func__);
		return -EINVAL;
	}

	snprintf(sensor_id, FP_MAX_SENSOR_ID_LEN, "%x", fingerprint->sensor_id);
	return scnprintf(buf, FP_MAX_CHIP_INFO_LEN, "%s\n", sensor_id);

}

static DEVICE_ATTR(fingerprint_chip_info, S_IRUSR  | S_IRGRP | S_IROTH, fingerprint_chip_info_show, NULL);

static ssize_t module_id_show(struct device *device, struct device_attribute *attribute, char *buf)
{
    struct fp_data* fingerprint= dev_get_drvdata(device);
    if(NULL == buf || NULL == fingerprint)
    {
        return -EINVAL;
    }
    return scnprintf(buf, sizeof(fingerprint->module_id), "%s", fingerprint->module_id);
}
static ssize_t module_id_store(struct device* device,
                               struct device_attribute* attribute,
                               const char* buffer, size_t count)
{
    struct fp_data* fingerprint = dev_get_drvdata(device);
    if(NULL == fingerprint)
    {
        return -EINVAL;
    }
    strncpy( fingerprint->module_id,buffer ,sizeof(fingerprint->module_id)-1);
    fingerprint->module_id[sizeof(fingerprint->module_id)-1] = '\0';
    return count;
}

static DEVICE_ATTR(module_id, S_IRUSR  | S_IRGRP | S_IROTH, module_id_show, module_id_store);

static struct attribute* attributes[] =
{
    &dev_attr_irq.attr,
    &dev_attr_fingerprint_chip_info.attr,
    &dev_attr_result.attr,
    &dev_attr_read_image_flag.attr,
    &dev_attr_snr.attr,
    &dev_attr_nav.attr,
    &dev_attr_module_id.attr,
    NULL
};

static const struct attribute_group attribute_group =
{
    .attrs = attributes,
};

static irqreturn_t fingerprint_irq_handler(int irq, void* handle)
{
    struct fp_data* fingerprint = handle;
    static int count = 0;
    smp_rmb();

    if (fingerprint->wakeup_enabled )
    {
        __pm_wakeup_event(&fingerprint->ttw_wl, FPC_TTW_HOLD_TIME);
    }
    sysfs_notify(&fingerprint->pf_dev->dev.kobj, NULL, dev_attr_irq.attr.name);
    return IRQ_HANDLED;
}

void fingerprint_get_navigation_adjustvalue(const struct device* dev, struct fp_data* fp_data)
{
    struct device_node* np;
    unsigned int adjust1 = NAVIGATION_ADJUST_NOREVERSE;
    unsigned int adjust2 = NAVIGATION_ADJUST_NOTURN;

#if 0

    if (!dev || !dev->of_node)
    {
        fpc_log_err("%s failed dev or dev node is NULL\n", __func__);
        return;
    }

    np = dev->of_node;
#else
    np = of_find_compatible_node(NULL, NULL, "mediatek,mtk_finger");
#endif

    // TODO: Add property in dts

    (void)of_property_read_u32(np, "fingerprint,navigation_adjust1", &adjust1);

    if (adjust1 != NAVIGATION_ADJUST_NOREVERSE && adjust1 != NAVIGATION_ADJUST_REVERSE)
    {
        adjust1 = NAVIGATION_ADJUST_NOREVERSE;
        fpc_log_err("%s navigation_adjust1 set err only support 0 and 1.\n", __func__);
    }

    (void)of_property_read_u32(np, "fingerprint,navigation_adjust2", &adjust2);

    if (adjust2 != NAVIGATION_ADJUST_NOTURN && adjust2 != NAVIGATION_ADJUST_TURN90 &&
        adjust2 != NAVIGATION_ADJUST_TURN180 && adjust2 != NAVIGATION_ADJUST_TURN270)
    {
        adjust2 = NAVIGATION_ADJUST_NOTURN;
        fpc_log_err("%s navigation_adjust2 set err only support 0 90 180 and 270.\n", __func__);
    }

    fp_data->navigation_adjust1 = (int)adjust1;
    fp_data->navigation_adjust2 = (int)adjust2;

    fpc_log_info("%s get navigation_adjust1 = %d, navigation_adjust2 = %d.\n", __func__,
                 fp_data->navigation_adjust1, fp_data->navigation_adjust2);
    return;
}

int fingerprint_get_dts_data(struct device* dev, struct fp_data* fingerprint)
{
    struct device_node* np = NULL;
    struct platform_device* pdev = NULL;
    int ret = 0;
    //   char const *fingerprint_vdd = NULL;
    //  char const *fingerprint_avdd = NULL;
#if 0

    if (!dev || !dev->of_node)
    {
        fpc_log_err("dev or dev node is NULL\n");
        return -EINVAL;
    }

    np = dev->of_node;
#else
	if(dev == NULL)
	{
		fpc_log_err("fingerprint_get_dts_data: dev is NULL\n");
	}
	fpc_log_err("fingerprint_get_dts_data: of_find_compatible_node1\n");
	np = of_find_compatible_node(NULL, NULL, "mediatek,mtk_finger");
	fpc_log_err("fingerprint_get_dts_data: of_find_compatible_node2\n");
#endif

    if (np)
    {
	fpc_log_err("fingerprint_get_dts_data: of_find_device_by_node1\n");
        pdev = of_find_device_by_node(np);
        fpc_log_err("fingerprint_get_dts_data: of_find_device_by_node2\n");
        if (IS_ERR(pdev))
        {
            fpc_log_err("platform device is null\n");
            return PTR_ERR(pdev);
        }
        fpc_log_err("fingerprint_get_dts_data: devm_pinctrl_get1\n");
        fingerprint->pinctrl = devm_pinctrl_get(&fingerprint->spi->dev);
		fpc_log_err("fingerprint_get_dts_data: devm_pinctrl_get2\n");

        if (IS_ERR(fingerprint->pinctrl))
        {
            ret = PTR_ERR(fingerprint->pinctrl);
            fpc_log_err("fpsensor Cannot find fp pinctrl1.\n");
            return ret;
        }
        fpc_log_err("fingerprint_get_dts_data: pinctrl_lookup_state1\n");
        fingerprint->fp_rst_low = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_rst_low");
        fpc_log_err("fingerprint_get_dts_data: pinctrl_lookup_state2\n");
        if (IS_ERR(fingerprint->fp_rst_low))
        {
            ret = PTR_ERR(fingerprint->fp_rst_low);
            fpc_log_err("fingerprint Cannot find fp pinctrl fp_rst_low!\n");
            return ret;
        }

       // fpc_log_info("fingerprint  find fp pinctrl fp_rst_low!\n");
        fingerprint->fp_rst_high = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_rst_high");

        if (IS_ERR(fingerprint->fp_rst_high))
        {
            ret = PTR_ERR(fingerprint->fp_rst_high);
            fpc_log_err( "fingerprint Cannot find fp pinctrl fp_rst_high!\n");
            return ret;
        }

        fpc_log_info( "fingerprint  find fp pinctrl fp_rst_high!\n");
        fingerprint->eint_as_int = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_eint_as_int"); //eint_in_low; eint

        if (IS_ERR(fingerprint->eint_as_int))
        {
            ret = PTR_ERR(fingerprint->eint_as_int);
            fpc_log_err( "fingerprint Cannot find fp pinctrl eint_as_int!\n");
            return ret;
        }

        fpc_log_info( "fingerprint  find fp pinctrl eint_as_int!\n");
        fingerprint->fp_cs_low  = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_cs_low");

        if (IS_ERR(fingerprint->fp_cs_low ))
        {
            ret = PTR_ERR(fingerprint->fp_cs_low );
            fpc_log_err("fingerprint Cannot find fp pinctrl fp_cs_low!\n");
            return ret;
        }

        fpc_log_info("fingerprint  find fp pinctrl fp_cs_low!\n");
        fingerprint->fp_cs_high = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_cs_high");

        if (IS_ERR(fingerprint->fp_cs_high))
        {
            ret = PTR_ERR(fingerprint->fp_cs_high);
            fpc_log_err( "fingerprint Cannot find fp pinctrl fp_cs_high!\n");
            return ret;
        }

        fpc_log_info( "fingerprint  find fp pinctrl fp_cs_high!\n");
        fingerprint->fp_mo_high = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_mosi_high");

        if (IS_ERR(fingerprint->fp_mo_high))
        {
            ret = PTR_ERR(fingerprint->fp_mo_high);
            fpc_log_err( "fingerprint Cannot find fp pinctrl fp_mo_high!\n");
            return ret;
        }

        fpc_log_info( "fingerprint  find fp pinctrl fp_mo_high!\n");
        fingerprint->fp_mo_low = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_mosi_low");

        if (IS_ERR(fingerprint->fp_mo_low))
        {
            ret = PTR_ERR(fingerprint->fp_mo_low);
            fpc_log_err("fingerprint Cannot find fp pinctrl fp_mo_low!\n");
            return ret;
        }

        fpc_log_info("fingerprint find fp pinctrl fp_mo_low!\n");
        fingerprint->fp_mi_high = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_miso_high");

        if (IS_ERR(fingerprint->fp_mi_high))
        {
            ret = PTR_ERR(fingerprint->fp_mi_high);
            fpc_log_err( "fingerprint Cannot find fp pinctrl fp_mi_high!\n");
            return ret;
        }

        fpc_log_info( "fingerprint find fp pinctrl fp_mi_high!\n");
        fingerprint->fp_mi_low = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_miso_low");

        if (IS_ERR(fingerprint->fp_mi_low))
        {
            ret = PTR_ERR(fingerprint->fp_mi_low);
            fpc_log_err("fingerprint Cannot find fp pinctrl fp_mi_low!\n");
            return ret;
        }

        fpc_log_info("fingerprint find fp pinctrl fp_mi_low!\n");
        fingerprint->fp_ck_high = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_mclk_high");

        if (IS_ERR(fingerprint->fp_ck_high))
        {
            ret = PTR_ERR(fingerprint->fp_ck_high);
            fpc_log_err( "fingerprint Cannot find fp pinctrl fp_ck_high!\n");
            return ret;
        }

        fpc_log_info( "fingerprint  find fp pinctrl fp_ck_high!\n");
        fingerprint->fp_ck_low = pinctrl_lookup_state(fingerprint->pinctrl, "fingerprint_spi_mclk_low");

        if (IS_ERR( fingerprint->fp_ck_low))
        {
            ret = PTR_ERR( fingerprint->fp_ck_low);
            fpc_log_err("fingerprint Cannot find fp pinctrl fp_ck_low!\n");
            return ret;
        }

        fpc_log_info("fingerprint find fp pinctrl fp_ck_low!\n");

        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_SPI_MO_PIN, 0);
        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_SPI_MI_PIN, 0);
        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_SPI_CK_PIN, 0);
        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_SPI_CS_PIN, 0);


    }
    else
    {
        fpc_log_err("fingerprint Cannot find node!\n");
    }

    return 0;
}


#if 0
ret = of_property_read_string(dev->of_node, "fingerprint,vdd", &fingerprint_vdd);

if (ret)
{
    fpc_log_err("failed to get vdd from device tree\n");
}

if (fingerprint_vdd)
{
    fingerprint->vdd = regulator_get(dev, fingerprint_vdd);

    if (IS_ERR(fingerprint->vdd))
    {
        fpc_log_err("failed to get vdd regulator\n");
    }
}

ret = of_property_read_string(dev->of_node, "fingerprint,avdd", &fingerprint_avdd);

if (ret)
{
    fpc_log_err("failed to get avdd from device tree, some project don't need this power\n");
}

if (fingerprint_avdd)
{
    fingerprint->avdd = regulator_get(dev, fingerprint_avdd);

    if (IS_ERR(fingerprint->avdd))
    {
        fpc_log_err("failed to get avdd regulator\n");
    }
}

fingerprint->avdd_en_gpio = of_get_named_gpio(np, "fingerprint,avdd_en_gpio", 0);

if (!gpio_is_valid(fingerprint->avdd_en_gpio))
{
    fpc_log_err("failed to get avdd enable gpio from device tree, some project don't need this gpio\n");
}
else
{
    fpc_log_info("avdd_en_gpio = %d\n", fingerprint->avdd_en_gpio);
}

fingerprint->vdd_en_gpio = of_get_named_gpio(np, "fingerprint,vdd_en_gpio", 0);

if (!gpio_is_valid(fingerprint->vdd_en_gpio))
{
    fpc_log_err("failed to get vdd enable gpio from device tree, some project don't need this gpio\n");
}
else
{
    fpc_log_info("vdd_en_gpio = %d\n", fingerprint->vdd_en_gpio);
}

#endif
#if 1
//goodix
#define BUFFER_LENTH 16
#define FP_SENSOR_ID_ADDR 0x0000
int fingerprint_goodix_read_hwid(struct fp_data* fingerprint)
{
	struct spi_message msg;
	struct spi_transfer *xfer = NULL;
	unsigned char *tmp_buf = NULL;
	int error = 0;
	int i = 0;		

	xfer = kzalloc(sizeof(*xfer) * 2, GFP_KERNEL);
	if (xfer == NULL) {
	    fpc_log_err("no memory for SPI transfer\n");
		return -EINVAL;
	}
	tmp_buf = kzalloc(BUFFER_LENTH, GFP_KERNEL);
	if (tmp_buf == NULL) {
	    fpc_log_err("no memory for SPI tmp_buf\n");
	    if (xfer != NULL)
		{
			kfree(xfer);
            		xfer = NULL;			
		}
		return -EINVAL;
	}
	/* switch to DMA mode if transfer length larger than 32 bytes */

	spi_message_init(&msg);
	*tmp_buf = 0xF0;
	*(tmp_buf + 1) = (unsigned char)((FP_SENSOR_ID_ADDR >> 8) & 0xFF);
	*(tmp_buf + 2) = (unsigned char)(FP_SENSOR_ID_ADDR & 0xFF);
	xfer[0].tx_buf = tmp_buf;
	xfer[0].rx_buf = NULL,
	xfer[0].len = 3;
	xfer[0].delay_usecs = 5;
	xfer[0].cs_change = 0,
	xfer[0].speed_hz = 1 * 1000u,
	/*.len    = cmd_size + reg_data->reg_size,*/
	xfer[0].tx_dma = 0,
	xfer[0].rx_dma = 0,
	xfer[0].bits_per_word = 8,
	spi_message_add_tail(&xfer[0], &msg);
    error = spi_sync(fingerprint->spi, &msg);
    if (error < 0)
	    fpc_log_err("spi_sync failed.\n");

	spi_message_init(&msg);
	/* memset((tmp_buf + 4), 0x00, data_len + 1); */
	/* 4 bytes align */
	*(tmp_buf + 4) = 0xF1;
	xfer[1].tx_buf = tmp_buf + 4;
	xfer[1].rx_buf = tmp_buf + 4;
	xfer[1].cs_change = 0,
	xfer[1].speed_hz = 1 * 1000u,
	/*.len    = cmd_size + reg_data->reg_size,*/
	xfer[1].tx_dma = 0,
	xfer[1].rx_dma = 0,
	xfer[1].bits_per_word = 8,
	xfer[1].len = 4 + 1;
	xfer[1].delay_usecs = 5;
	
	spi_message_add_tail(&xfer[1], &msg);
    error = spi_sync(fingerprint->spi, &msg);
    if (error < 0)
	    fpc_log_err("spi_sync failed.\n");

	/* copy received data */
	//memcpy(rx_buf, (tmp_buf + 5), 4);
	for (i = 0; i < BUFFER_LENTH; i++)
	{	
      fpc_log_info("[fpc] hwid = %x \n", *(tmp_buf + i)); 
	} 
    fingerprint->sensor_id = (*(tmp_buf + 5)) | (*(tmp_buf + 8) << 8);	
	fpc_log_info("[fpc] sensor_id = %x \n", fingerprint->sensor_id); 
	kfree(tmp_buf);
	if (tmp_buf != NULL)
		tmp_buf = NULL;

    kfree(xfer);
	if (xfer != NULL)
		xfer = NULL;
	
    if (fingerprint->sensor_id == 0x2504)
        return 1;
    else    
        return 0 ;
}
#endif 
#if 1
//silead
#define FP_REG_HWID_MAX_SIZE 8
int fingerprint_silead_read_hwid(struct fp_data* fingerprint)
{
	int error = 0;
	struct spi_message msg;
	unsigned char rx[6] = {0x00};
	unsigned char tx[6] = {0xfc , 0x00, 0x00, 0x00, 0x00, 0x00};
	
       struct spi_transfer cmd = {
                .tx_buf = tx,
                .rx_buf = rx,
                .len = 6,
                .bits_per_word = 8,
                .delay_usecs = 1,
                .speed_hz = 1 * 1000 * 1000,                
       };
       spi_message_init(&msg);
       spi_message_add_tail(&cmd, &msg);

       error = spi_sync(fingerprint->spi, &msg);
       if (error < 0)
           fpc_log_err("%s : spi_sync failed.\n", __func__);
	   
      fpc_log_info("[fpc] hwid = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x", rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]); 
      fingerprint->sensor_id = rx[4] | (rx[5] << 8);

      if (fingerprint->sensor_id == 0x6152)
          	return 1;
      else  
	  	return 0 ;
}
#endif 


//fpc
int fingerprint_fpc_read_hwid(struct fp_data* fingerprint)
{
        struct spi_message msg;
        unsigned char temp_buffer[8];
        int error = 0;
        struct spi_transfer cmd = {
                .cs_change = 0,
                .delay_usecs = 1,
                .speed_hz = 1*1000*1000u,
                .tx_buf = temp_buffer,
                .rx_buf = temp_buffer,
                /*.len    = cmd_size + reg_data->reg_size,*/
                .len = 8,
                .tx_dma = 0,
                .rx_dma = 0,
                .bits_per_word = 8,
        };
       temp_buffer[0] = 0xFC;
       spi_message_init(&msg);
       spi_message_add_tail(&cmd,  &msg);
       error = spi_sync(fingerprint->spi, &msg);
       if (error < 0)
           fpc_log_err("[fpc] %s : spi_sync failed.\n", __func__);
      fingerprint->sensor_id = temp_buffer[2] | (temp_buffer[1] << 8);
      fpc_log_info("[fpc] hwid = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
			temp_buffer[0], temp_buffer[1], temp_buffer[2], temp_buffer[3],
			temp_buffer[4], temp_buffer[5], temp_buffer[6], temp_buffer[7]);
      if (fingerprint->sensor_id == 0x0612) 
          return 1;
      else 
          return 0;
}


#if 1
static int fingerprint_fpc_clear_irq(struct fp_data* fingerprint)
{
	int irq = -1;
	int irq_count = 0;
	struct spi_message msg;
	unsigned char temp_buffer[8];
	int error = 0;
	struct spi_transfer cmd = {
		.cs_change = 0,
		.delay_usecs = 1,
		.speed_hz = 1*1000*1000u,
		.tx_buf = temp_buffer,
		.rx_buf = temp_buffer,
		.len = 2,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 8,
	};
	temp_buffer[0] = 0x1C;
	spi_message_init(&msg);
	spi_message_add_tail(&cmd,  &msg);
	error = spi_sync(fingerprint->spi, &msg);
	  do{
                         irq = __gpio_get_value(fingerprint->irq_gpio);

                         if ((0 == irq) || ( irq_count > 1000 ))
                         {
                                break;
                         }
                         mdelay(1);
                         irq_count++;
                 }while(1);
                 fpc_log_info("[fpc] irq_get :level %d ,irq_count = %d\n", irq, irq_count);
	return 0;

}
#endif
int fingerprint_gpio_reset(struct fp_data* fingerprint)
{
    int error = 0;
    int counter = FP_RESET_RETRIES;

    while (counter)
    {
        counter--;

        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_RST_PIN, 1);
//        udelay(FP_RESET_HIGH1_US);
        mdelay(10);
        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_RST_PIN, 0);
//        udelay(FP_RESET_LOW_US);
        mdelay(10);
        fingerprint_gpio_output_dts(fingerprint, FINGERPRINT_RST_PIN, 1);
        udelay(FP_RESET_HIGH2_US);
        mdelay(10);
    }

    fpc_log_info("Exit!\n");
    return error;
}

static int fingerprint_reset_init(struct fp_data* fingerprint)
{
    int error = 0;
#if 0

    if (gpio_is_valid(fingerprint->rst_gpio))
    {
        fpc_log_info("Assign RESET -> GPIO%d\n", fingerprint->rst_gpio);
        error = gpio_request(fingerprint->rst_gpio, "fingerprint_reset");

        if (error)
        {
            fpc_log_err("gpio_request (reset) failed\n");
            return error;
        }

        // TODO:
        error = gpio_direction_output(fingerprint->rst_gpio, 0);

        if (error)
        {
            fpc_log_err("gpio_direction_output (reset) failed\n");
            return error;
        }
    }
    else
    {
        fpc_log_info("Using soft reset\n");
    }

#endif
    return error;
}

static int fingerprint_irq_init(struct fp_data* fingerprint)
{
    int error = 0;

#if 0

    if (gpio_is_valid(fingerprint->irq_gpio))
    {
        fpc_log_info("Assign IRQ -> GPIO%d\n", fingerprint->irq_gpio);
        error = gpio_request(fingerprint->irq_gpio, "fingerprint_irq");

        if (error)
        {
            fpc_log_err("gpio_request (irq) failed\n");
            return error;
        }

        error = gpio_direction_input(fingerprint->irq_gpio);

        if (error)
        {
            fpc_log_err("gpio_direction_input (irq) failed\n");
            return error;
        }
    }
    else
    {
        fpc_log_err("invalid irq gpio\n");
        return -EINVAL;
    }

    fingerprint->irq = gpio_to_irq(fingerprint->irq_gpio);

    if (fingerprint->irq < 0)
    {
        fpc_log_err("gpio_to_irq failed\n");
        error = fingerprint->irq;
        return error;
    }

#else

    struct device_node* node;
   // u32 ints[2] = {0, 0};

    //spidev_gpio_as_int(fingerprint);
    //pinctrl_select_state(fingerprint->pinctrl, fingerprint->eint_as_int);

    node = of_find_compatible_node(NULL, NULL, "mediatek,mtk_finger");
    if ( node)
    {
		fingerprint->irq_gpio = of_get_named_gpio(node, "fingerprint_eint_gpio",0);
		if(fingerprint->irq_gpio<0)
		{
			fpc_log_err("[fpc] %s read fingerprint,eint_gpio faile\n",__func__);
		}
		
       fingerprint->irq = irq_of_parse_and_map(node, 0);  // get irq number
        if (fingerprint->irq < 0)
        {
			fpc_log_err("[fpc] fingerprint irq_of_parse_and_map fail!!\n");
			return -EINVAL;
        }

        fpc_log_info(" [fpc] fingerprint->irq= %d,fingerprint>irq_gpio = %d\n", fingerprint->irq, fingerprint->irq_gpio);
    }
    else
    {
        fpc_log_err("[fpc] fingerprint null irq node!!\n");
        return -EINVAL;
    }

#endif
    return error;
}
#if 0
static int fingerprint_power_init(struct fp_data* fingerprint)
{
    int error = 0;

    fpc_log_info("Enter!\n");

    // TODO: Do not use regulator

    if (fingerprint->avdd)
    {
        error = regulator_set_voltage(fingerprint->avdd, 2800000, 3000000);

        if (error)
        {
            fpc_log_err("set avdd voltage failed\n");
            goto out_err;
        }

        error = regulator_enable(fingerprint->avdd);

        if (error)
        {
            fpc_log_err("enable avdd failed\n");
            goto out_err;
        }
    }
    else
    {
        fpc_log_err("fingerprint->avdd is NULL, some project don't need this power\n");
    }

    if (fingerprint->vdd)
    {
        error = regulator_set_voltage(fingerprint->vdd, 1800000, 1800000);

        if (error)
        {
            fpc_log_err("set vdd voltage failed\n");
            goto out_err;
        }

        error = regulator_enable(fingerprint->vdd);

        if (error)
        {
            fpc_log_err("enable vdd failed\n");
            goto out_err;
        }
    }
    else
    {
        fpc_log_err("fingerprint->vdd is NULL\n");
        return -EINVAL;
    }

    if (gpio_is_valid(fingerprint->avdd_en_gpio))
    {
        fpc_log_info("fingerprint_avdd_en_gpio -> GPIO%d\n", fingerprint->avdd_en_gpio);
        error = gpio_request(fingerprint->avdd_en_gpio, "fingerprint_avdd_en_gpio");

        if (error)
        {
            fpc_log_err("gpio_request (avdd_en_gpio) failed\n");
            goto out_err;
        }

        error = gpio_direction_output(fingerprint->avdd_en_gpio, 1);

        if (error)
        {
            fpc_log_err("gpio_direction_output (avdd_en_gpio) failed\n");
            goto out_err;
        }

        mdelay(100);
    }
    else
    {
        fpc_log_info("fingerprint->avdd_en_gpio is NULL, some project don't need this gpio\n");
    }

    if (gpio_is_valid(fingerprint->vdd_en_gpio))
    {
        fpc_log_info("fingerprint_vdd_en_gpio -> GPIO%d\n", fingerprint->vdd_en_gpio);
        error = gpio_request(fingerprint->vdd_en_gpio, "fingerprint_vdd_en_gpio");

        if (error)
        {
            fpc_log_err("gpio_request (vdd_en_gpio) failed\n");
            goto out_err;
        }

        error = gpio_direction_output(fingerprint->vdd_en_gpio, 1);

        if (error)
        {
            fpc_log_err("gpio_direction_output (vdd_en_gpio) failed\n");
            goto out_err;
        }
    }
    else
    {
        fpc_log_info("fingerprint->vdd_en_gpio is NULL, some project don't need this gpio\n");
    }

    //out_err:
    return error;
}
#endif
static int fingerprint_key_remap_reverse(int key)
{
    switch (key)
    {
        case EVENT_LEFT:
            key = EVENT_RIGHT;
            break;

        case EVENT_RIGHT:
            key = EVENT_LEFT;
            break;

        default:
            break;
    }

    return key;
}

static int fingerprint_key_remap_turn90(int key)
{
    switch (key)
    {
        case EVENT_LEFT:
            key = EVENT_UP;
            break;

        case EVENT_RIGHT:
            key = EVENT_DOWN;
            break;

        case EVENT_UP:
            key = EVENT_RIGHT;
            break;

        case EVENT_DOWN:
            key = EVENT_LEFT;
            break;

        default:
            break;
    }

    return key;
}

static int fingerprint_key_remap_turn180(int key)
{
    switch (key)
    {
        case EVENT_LEFT:
            key = EVENT_RIGHT;
            break;

        case EVENT_RIGHT:
            key = EVENT_LEFT;
            break;

        case EVENT_UP:
            key = EVENT_DOWN;
            break;

        case EVENT_DOWN:
            key = EVENT_UP;
            break;

        default:
            break;
    }

    return key;
}

static int fingerprint_key_remap_turn270(int key)
{
    switch (key)
    {
        case EVENT_LEFT:
            key = EVENT_DOWN;
            break;

        case EVENT_RIGHT:
            key = EVENT_UP;
            break;

        case EVENT_UP:
            key = EVENT_LEFT;
            break;

        case EVENT_DOWN:
            key = EVENT_RIGHT;
            break;

        default:
            break;
    }

    return key;
}

static int fingerprint_key_remap(const struct fp_data* fingerprint, int key)
{
    if (key != EVENT_RIGHT && key != EVENT_LEFT && key != EVENT_UP && key != EVENT_DOWN)
    {
        return key;
    }

    if (fingerprint->navigation_adjust1 == NAVIGATION_ADJUST_REVERSE)
    {
        key = fingerprint_key_remap_reverse(key);
    }

    switch (fingerprint->navigation_adjust2)
    {
        case NAVIGATION_ADJUST_TURN90:
            key = fingerprint_key_remap_turn90(key);
            break;

        case NAVIGATION_ADJUST_TURN180:
            key = fingerprint_key_remap_turn180(key);
            break;

        case NAVIGATION_ADJUST_TURN270:
            key = fingerprint_key_remap_turn270(key);
            break;

        default:
            break;
    }

    return key;
}

static void fingerprint_input_report(struct fp_data* fingerprint, int key)
{
    key = fingerprint_key_remap(fingerprint, key);
    fpc_log_info("key = %d\n", key);
    input_report_key(fingerprint->input_dev, key, 1);
    input_sync(fingerprint->input_dev);
    input_report_key(fingerprint->input_dev, key, 0);
    input_sync(fingerprint->input_dev);
}

static int fingerprint_open(struct inode* inode, struct file* file)
{
    struct fp_data* fingerprint;
    fingerprint = container_of(inode->i_cdev, struct fp_data, cdev);
    file->private_data = fingerprint;
    return 0;
}

static int fingerprint_get_irq_status(struct fp_data* fingerprint)
{
    int status = 0;
    status = __gpio_get_value(fingerprint->irq_gpio);
    return status;
}

static void fingerprint_spi_clk_switch(struct fp_data* fingerprint, spi_clk_state ctrl)
{
    if (NULL == fingerprint || NULL == fingerprint->spi)
    {
        fpc_log_err("the fingerprint or fingerpint->spi is NULL, clk control failed!\n");
        return;
    }

    mutex_lock(&fingerprint->mutex_lock_clk);
    if (SPI_CLK_DISABLE == ctrl)
    {
        if(fingerprint->spi_clk_counter > 0){
            fingerprint->spi_clk_counter--;
        }

        if(fingerprint->spi_clk_counter == 0){
            mt_spi_disable_master_clk(fingerprint->spi);
        }
        else {
            fpc_log_info("the disable clk is not match, the spi_clk_counter = %d\n", fingerprint->spi_clk_counter);
		}
    }
    else
    {
        if(fingerprint->spi_clk_counter == 0){
            mt_spi_enable_master_clk(fingerprint->spi);
        }
        else {
            fpc_log_info("the enable clk is not match, the spi_clk_counter = %d\n", fingerprint->spi_clk_counter);
        }
        fingerprint->spi_clk_counter++;
    }
    mutex_unlock(&fingerprint->mutex_lock_clk);
}

static long fingerprint_base_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{

    int error = 0;

    struct fp_data* fingerprint;
    void __user* argp = (void __user*)arg;
    int key;
    int status;
    unsigned int sensor_id;
    fingerprint = (struct fp_data*)file->private_data;

    //fpc_log_info("[fpc] dev_fingerprint ioctl cmd : 0x%x\n", cmd);

    if (_IOC_TYPE(cmd) != FP_IOC_MAGIC)
    { return -ENOTTY; }

    switch (cmd)
    {
        case FP_IOC_CMD_ENABLE_IRQ:
            if (0 == fingerprint->irq_num)
            {
                enable_irq(fingerprint->irq);
                fingerprint->irq_num = 1;
            }
            break;

        case FP_IOC_CMD_DISABLE_IRQ:
            if (1 == fingerprint->irq_num)
            {
                disable_irq(fingerprint->irq);
                fingerprint->irq_num = 0;
            }
            break;

        case FP_IOC_CMD_SEND_UEVENT:
            if (copy_from_user(&key, argp, sizeof(key)))
            {
                fpc_log_err("copy_from_user failed");
                return -EFAULT;
            }
            fingerprint_input_report(fingerprint, key);
            break;

        case FP_IOC_CMD_GET_IRQ_STATUS:
            status = fingerprint_get_irq_status(fingerprint);
            error = copy_to_user(argp, &status, sizeof(status));
            if (error)
            {
                fpc_log_err("copy_to_user failed, error = %d", error);
                return -EFAULT;
            }
            break;

        case FP_IOC_CMD_SET_WAKELOCK_STATUS:
            if (copy_from_user(&key, argp, sizeof(key)))
            {
                fpc_log_err("copy_from_user failed");
                return -EFAULT;
            }
            if (key == 1)
            {
                fingerprint->wakeup_enabled = true;
            }
            else
            {
                fingerprint->wakeup_enabled = false;
            }
            break;

        case FP_IOC_CMD_SEND_SENSORID:
            if (copy_from_user(&sensor_id, argp, sizeof(sensor_id)))
            {
                fpc_log_err("copy_from_user failed\n");
                return -EFAULT;
            }
            fingerprint->sensor_id = sensor_id;
            fpc_log_info("sensor_id = %x\n", sensor_id);
            break;
        case FP_IOC_CMD_SET_IPC_WAKELOCKS:
            fpc_log_info("MTK do not support the CMD 7\n");
            break;

            // TODO: add reset IOCTL
        case FP_IOC_CMD_SET_POWEROFF:
            fpc_log_info("MTK do not support the CMD 10\n");
            break;

        case FP_IOC_CMD_ENABLE_SPI_CLK:
            fingerprint_spi_clk_switch(fingerprint, SPI_CLK_ENABLE);
            break;

        case FP_IOC_CMD_DISABLE_SPI_CLK:
            fingerprint_spi_clk_switch(fingerprint, SPI_CLK_DISABLE);
            break;

        default:
            fpc_log_err("error = -EFAULT\n");
            error = -EFAULT;
            break;
    }

    return error;
}

static long fingerprint_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    return fingerprint_base_ioctl(file,cmd,arg);
}

static long fingerprint_compat_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    return fingerprint_base_ioctl(file,cmd,compat_ptr(arg));
}

static int fingerprint_release(struct inode* inode, struct file* file)
{
    fpc_log_info("Enter!\n");
    return 0;
}
#if 0
static int fingerprint_get_module_info(struct fp_data* fingerprint)
{
    int error = 0;
    //int pd_value = 0, pu_value = 0;

    error = gpio_request(fingerprint->moduleID_gpio, "fingerprint_moduleID");

    if (error)
    {
        fpc_log_err("gpio_request (moduleID) failed\n");
        return error;
    }

    // TODO: Get pinctrl state from dts file

    fingerprint->pctrl = devm_pinctrl_get(&fingerprint->spi->dev);

    if (IS_ERR(fingerprint->pctrl))
    {
        fpc_log_err("failed to get moduleid pin\n");
        error = -EINVAL;
        return error;
    }

    fingerprint->pins_default = pinctrl_lookup_state(fingerprint->pctrl, "default");

    if (IS_ERR(fingerprint->pins_default))
    {
        fpc_log_err("failed to lookup state fingerprint_moduleid_default\n");
        error = -EINVAL;
        goto error_pinctrl_put;
    }

    fingerprint->pins_idle = pinctrl_lookup_state(fingerprint->pctrl, "idle");

    if (IS_ERR(fingerprint->pins_idle))
    {
        fpc_log_err("failed to lookup state fingerprint_moduleid_idle\n");
        error = -EINVAL;
        goto error_pinctrl_put;
    }

    error = pinctrl_select_state(fingerprint->pctrl, fingerprint->pins_default);

    if (error < 0)
    {
        fpc_log_err("failed to select state fingerprint_moduleid_default\n");
        error = -EINVAL;
        goto error_pinctrl_put;
    }

    udelay(10);

    pu_value = gpio_get_value_cansleep(fingerprint->moduleID_gpio);
    fpc_log_info("PU module id gpio = %d \n", pu_value);


    error = pinctrl_select_state(fingerprint->pctrl, fingerprint->pins_idle);

    if (error < 0)
    {
        fpc_log_err("failed to select state fingerprint_moduleid_idle\n");
        error = -EINVAL;
        return error;
    }

    udelay(10);

    pd_value = gpio_get_value_cansleep(fingerprint->moduleID_gpio);
    fpc_log_info("PD module id gpio = %d \n", pd_value);

    if (pu_value == pd_value)
    {
        if (pu_value == 1)
        {
            fingerprint->module_vendor_info = MODULEID_HIGH;
            fpc_log_info("fingerprint moduleID pin is HIGH\n");
        }

        if (pd_value == 0)
        {
            fingerprint->module_vendor_info = MODULEID_LOW;
            fpc_log_info("fingerprint moduleID pin is LOW\n");
        }
    }
    else
    {
        fingerprint->module_vendor_info = MODULEID_FLOATING;
        fpc_log_info("fingerprint moduleID pin is FLOATING\n");
    }

    return error;

error_pinctrl_put:
    devm_pinctrl_put(fingerprint->pctrl);

    return error;
}
#endif

static ssize_t fpsensor_read(struct file* filp, char __user* buf, size_t count, loff_t* f_pos)
{
    fpc_log_err("kp Not support read opertion in TEE version\n");
    return -EFAULT;
}

static const struct file_operations fingerprint_fops =
{
    .owner			= THIS_MODULE,
    .open			= fingerprint_open,
    .release		= fingerprint_release,
    .unlocked_ioctl	= fingerprint_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = fingerprint_compat_ioctl,
#endif
    .read =        fpsensor_read,
};

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block* self, unsigned long event, void* data)
{
    struct fb_event* evdata = data;
    int* blank;
    struct fp_data* fingerprint = container_of(self, struct fp_data, fb_notify);

    if (evdata && evdata->data && fingerprint)
    {
        if (event == FB_EVENT_BLANK)
        {
            blank = evdata->data;

            if (*blank == FB_BLANK_UNBLANK)
            { atomic_set(&fingerprint->state, fp_LCD_UNBLANK); }
            else if (*blank == FB_BLANK_POWERDOWN)
            { atomic_set(&fingerprint->state, fp_LCD_POWEROFF); }
        }
    }

    return 0;
}
#endif

static int fingerprint_probe(struct spi_device* spi)
{
	struct device* dev = NULL;
	struct device_node* np = NULL;
	struct fp_data* fingerprint = NULL;
    int error = 0;
	int irq = 0;
	int irq_count = 0;
	int ret = 0;

	if(NULL ==spi)
	{
		fpc_log_err("spi is null\n");
	    return -EIO;
	}
    //int irqf;
	printk(KERN_ERR "%s: enter\n", __func__);
	dev = &spi->dev;
	if(dev == NULL)
	{
		printk(KERN_ERR "%s: FINGER dev is null\n", __func__);
	}
#if 0
    np = dev->of_node;
#else
    np = of_find_compatible_node(NULL, NULL, "mediatek,mtk_finger");
    if (!np)
    {
        fpc_log_err("dev->of_node not found\n");
        error = -EINVAL;
        goto exit;
    }
#endif

    fingerprint = devm_kzalloc(dev, sizeof(*fingerprint), GFP_KERNEL);
    if (!fingerprint)
    {
        fpc_log_err("failed to allocate memory for struct fp_data\n");
        error = -ENOMEM;
        goto exit;
    }

#if defined (CONFIG_HUAWEI_DSM)
    if (!fingerprint_dclient)
    {
	fpc_log_err("fingerprint_dclient error\n");
	fingerprint_dclient = dsm_register_client(&dsm_fingerprint);
    }
#endif

    fpc_log_info("fingerprint driver for Android P\n");
    fingerprint->dev = dev;
    dev_set_drvdata(dev, fingerprint);
    fingerprint->spi = spi;
    fingerprint->spi->mode = SPI_MODE_0;
    fingerprint->spi->bits_per_word = 8;
    fingerprint->spi->chip_select = 0;
    // spi_setup(spi);
    //fingerprint_spi_clk_switch(fingerprint->spi, SPI_CLK_ENABLE);
    fingerprint_get_dts_data(&spi->dev, fingerprint);

#if defined(CONFIG_FB)
    fingerprint->fb_notify.notifier_call = NULL;
#endif
    atomic_set(&fingerprint->state, fp_UNINIT);
    error = fingerprint_irq_init(fingerprint);
    if (error)
    {
        fpc_log_err("fingerprint_irq_init failed, error = %d\n", error);
        goto exit;
    }

    fingerprint_get_navigation_adjustvalue(&spi->dev, fingerprint);

    error = fingerprint_reset_init(fingerprint);

    if (error)
    {
        fpc_log_err("fingerprint_reset_init failed, error = %d\n", error);
        goto exit;
    }

#if 0
    error = fingerprint_power_init(fingerprint);

    if (error)
    {
        fpc_log_err("fingerprint_power_init failed, error = %d\n", error);
        goto exit;
    }

    error = fingerprint_get_module_info(fingerprint);

    if (error < 0)
    {
        fpc_log_err("unknow vendor info, error = %d\n", error);
    }

#endif
    fingerprint->class = class_create(THIS_MODULE, FP_CLASS_NAME);

    error = alloc_chrdev_region(&fingerprint->devno, 0, 1, FP_DEV_NAME);

    if (error)
    {
        fpc_log_err("alloc_chrdev_region failed, error = %d\n", error);
        goto exit;
    }
	printk(KERN_ERR "%s sssss %d\n",__func__,fingerprint->devno);
    fingerprint->device = device_create(fingerprint->class, NULL, fingerprint->devno,
                                        NULL, "%s", FP_DEV_NAME);

    cdev_init(&fingerprint->cdev, &fingerprint_fops);
    fingerprint->cdev.owner = THIS_MODULE;

    error = cdev_add(&fingerprint->cdev, fingerprint->devno, 1);

    if (error)
    {
        fpc_log_err("cdev_add failed, error = %d\n", error);
        goto exit;
    }

    fingerprint->input_dev = devm_input_allocate_device(dev);

    if (!fingerprint->input_dev)
    {
        error = -ENOMEM;
        fpc_log_err("devm_input_allocate_device failed, error = %d\n", error);
        goto exit;
    }

    fingerprint->input_dev->name = "fingerprint";
    /* Also register the key for wake up */
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_UP);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_DOWN);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_LEFT);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_RIGHT);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_CLICK);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_HOLD);
    input_set_capability(fingerprint->input_dev, EV_KEY, EVENT_DCLICK);
    set_bit(EV_KEY, fingerprint->input_dev->evbit);
    set_bit(EVENT_UP, fingerprint->input_dev->evbit);
    set_bit(EVENT_DOWN, fingerprint->input_dev->evbit);
    set_bit(EVENT_LEFT, fingerprint->input_dev->evbit);
    set_bit(EVENT_RIGHT, fingerprint->input_dev->evbit);
    set_bit(EVENT_CLICK, fingerprint->input_dev->evbit);
    set_bit(EVENT_HOLD, fingerprint->input_dev->evbit);
    set_bit(EVENT_DCLICK, fingerprint->input_dev->evbit);

    error = input_register_device(fingerprint->input_dev);
    if (error)
    {
        fpc_log_err("input_register_device failed, error = %d\n", error);
        goto exit;
    }
	
    fingerprint->pf_dev = platform_device_alloc(FP_DEV_NAME, -1);
    if (!fingerprint->pf_dev)
    {
        error = -ENOMEM;
        fpc_log_err("platform_device_alloc failed, error = %d\n", error);
        goto exit;
    }

    error = platform_device_add(fingerprint->pf_dev);
    if (error)
    {
        fpc_log_err("platform_device_add failed, error = %d\n", error);
        platform_device_del(fingerprint->pf_dev);
        goto exit;
    }
    else
    {
        dev_set_drvdata(&fingerprint->pf_dev->dev, fingerprint);

        error = sysfs_create_group(&fingerprint->pf_dev->dev.kobj, &attribute_group);
        if (error)
        {
            fpc_log_err("sysfs_create_group failed, error = %d\n", error);
            goto exit;
        }
    }

    //device_init_wakeup(dev, 1);
    wakeup_source_init(&fingerprint->ttw_wl, "fpc_ttw_wl");

    mutex_init(&fingerprint->lock);
    mutex_init(&fingerprint->mutex_lock_clk);
#if 0
    irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
    error = devm_request_threaded_irq(dev, fingerprint->irq,
                                      NULL, fingerprint_irq_handler, irqf,
                                      "fingerprint", fingerprint);
#else

    error = request_threaded_irq(fingerprint->irq, NULL, fingerprint_irq_handler,
                                 IRQF_TRIGGER_RISING | IRQF_ONESHOT, "fingerprint", fingerprint);

#endif

    if (error)
    {
        fpc_log_err("failed to request irq %d\n", fingerprint->irq);
        goto exit;
    }

    //disable_irq(fingerprint->irq);
    fingerprint->irq_num = 0;

    /* Request that the interrupt should be wakeable */
    enable_irq_wake(fingerprint->irq);
    fingerprint->wakeup_enabled = true;
	
    fingerprint->snr_stat = 0;


#if defined(CONFIG_FB)

    if (fingerprint->fb_notify.notifier_call == NULL)
    {
        fingerprint->fb_notify.notifier_call = fb_notifier_callback;
        fb_register_client(&fingerprint->fb_notify);
    }

#endif

    fingerprint_gpio_reset(fingerprint);

/*reserve the debug code*/
#if 0
		do{
			irq = __gpio_get_value(fingerprint->irq_gpio);

			if ((1 == irq) || ( irq_count > 1000 ))
			{
				break;
			}
			mdelay(1);
			irq_count++;
		}while(1);
		fpc_log_info("[fpc] irq_get : %d ,irq_count = %d\n", irq, irq_count);
#endif
#if 0
       fingerprint_fpc_clear_irq(fingerprint); //clear irq to check if irq level change to 0	
        if (fingerprint_fpc_read_hwid(fingerprint)) { 
            fpc_log_info("[fpc] fpc hardware id : 0x%x\n", fingerprint->sensor_id);
           // set_fp_ta_name("fp_server_fpc", strlen("fp_server_fpc"));
        } else if (fingerprint_silead_read_hwid(fingerprint)) {
	        fpc_log_info("[fpc] silead hardware id : 0x%x\n", fingerprint->sensor_id);
           // set_fp_ta_name("fp_server_silead", strlen("fp_server_silead"));
        } else if (fingerprint_goodix_read_hwid(fingerprint)) {
	        fpc_log_info("[fpc] goodix hardware id : 0x%x\n", fingerprint->sensor_id);
           // set_fp_ta_name("fp_server_goodix", strlen("fp_server_goodix"));
        } else fpc_log_info("[fpc] cannot find matched FPS\n");
		
    fingerprint_fpc_read_hwid(fingerprint);
    fpc_log_info("fingerprint probe is successful!\n");
#endif

    return error;

exit:
    fpc_log_info("fingerprint probe failed!\n");
#if defined (CONFIG_HUAWEI_DSM)

    if (error && !dsm_client_ocuppy(fingerprint_dclient))
    {
        dsm_client_record(fingerprint_dclient, "fingerprint_probe failed, error = %d\n", error);
        dsm_client_notify(fingerprint_dclient, DSM_FINGERPRINT_PROBE_FAIL_ERROR_NO);
    }

#endif
    return error;
}

static int fingerprint_remove(struct spi_device* spi)
{
    struct  fp_data* fingerprint = dev_get_drvdata(&spi->dev);
    sysfs_remove_group(&fingerprint->pf_dev->dev.kobj, &attribute_group);
    cdev_del(&fingerprint->cdev);
    unregister_chrdev_region(fingerprint->devno, 1);
    input_free_device(fingerprint->input_dev);
    mutex_destroy(&fingerprint->lock);
    mutex_destroy(&fingerprint->mutex_lock_clk);
    wakeup_source_trash(&fingerprint->ttw_wl);
#if defined(CONFIG_FB)

    if (fingerprint->fb_notify.notifier_call != NULL)
    {
        fingerprint->fb_notify.notifier_call = NULL;
        fb_unregister_client(&fingerprint->fb_notify);
    }
#endif
    return 0;
}

static int fingerprint_suspend(struct device* dev)
{
    return 0;
}

static int fingerprint_resume(struct device* dev)
{
    return 0;
}

static const struct dev_pm_ops fingerprint_pm =
{
    .suspend = fingerprint_suspend,
    .resume = fingerprint_resume
};
#if 0
static struct mt_chip_conf fpsensor_spi_conf =
{
    .setuptime = 20,
    .holdtime = 20,
    .high_time = 50,
    .low_time = 50,
    .cs_idletime = 5,
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .tx_endian = 0,
    .rx_endian = 0,
    .cpol = 0,
    .cpha = 0,
    .com_mod = FIFO_TRANSFER,
    .pause = 1,
    .finish_intr = 1,
    .deassert = 0,
    .tckdly = 0,
};
#endif
static struct mtk_chip_config fpsensor_spi_conf =
{
    .rx_mlsb = 1,
    .tx_mlsb = 1,
    .cs_pol = 0,
	.sample_sel = 0,
}; 
static struct spi_board_info spi_fp_board_info[] __initdata =
{
    [0] = {
        .modalias = "fingerprint",
        .bus_num = 0,
        .chip_select = 0,
        .mode = SPI_MODE_0,
        .controller_data = &fpsensor_spi_conf, //&spi_conf
    },
};

// TODO: All following should consider platform
static struct of_device_id fingerprint_of_match[] =
{
    //{ .compatible = "fpc,fingerprint", },
    { .compatible = "mediatek,mtk_finger", },
    {}
};

MODULE_DEVICE_TABLE(of, fingerprint_of_match);

static struct spi_driver fingerprint_driver =
{
    .driver = {
        .name	= "fingerprint",
        .owner	= THIS_MODULE,
        .of_match_table = fingerprint_of_match,
        .pm = &fingerprint_pm
    },
    .probe  = fingerprint_probe,
    .remove = fingerprint_remove
};

static int __init fingerprint_init(void)
{
    int status = 0;

    spi_register_board_info(spi_fp_board_info, ARRAY_SIZE(spi_fp_board_info));

    status = spi_register_driver(&fingerprint_driver);
    if (status < 0)
    {
        printk(KERN_ERR "%s:status is %d\n", __func__, status);
        return -EINVAL;
    }
    return 0;
}

static void __exit fingerprint_exit(void)
{
    spi_unregister_driver(&fingerprint_driver);
}
//module_init(fingerprint_init);
late_initcall(fingerprint_init);
module_exit(fingerprint_exit);

MODULE_LICENSE("GPL v2");
