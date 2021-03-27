#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

MODULE_AUTHOR("liiir1985");
MODULE_DESCRIPTION("WiiU gamepad driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#define HAVE_TIMER_SETUP
#endif

#define GC_REFRESH_TIME	HZ / 100

/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
    #define ADS1015_ADDRESS                 (0x48)    // 1001 000 (ADDR = GND)
/*=========================================================================*/

/*=========================================================================
    CONVERSION DELAY (in mS)
    -----------------------------------------------------------------------*/
    #define ADS1015_CONVERSIONDELAY         (1)
    #define ADS1115_CONVERSIONDELAY         (15)
/*=========================================================================*/

/*=========================================================================
    POINTER REGISTER
    -----------------------------------------------------------------------*/
    #define ADS1015_REG_POINTER_MASK        (0x03)
    #define ADS1015_REG_POINTER_CONVERT     (0x00)
    #define ADS1015_REG_POINTER_CONFIG      (0x01)
    #define ADS1015_REG_POINTER_LOWTHRESH   (0x02)
    #define ADS1015_REG_POINTER_HITHRESH    (0x03)
/*=========================================================================*/

/*=========================================================================
    CONFIG REGISTER
    -----------------------------------------------------------------------*/
    #define ADS1015_REG_CONFIG_OS_MASK      (0x8000)
    #define ADS1015_REG_CONFIG_OS_SINGLE    (0x8000)  // Write: Set to start a single-conversion
    #define ADS1015_REG_CONFIG_OS_BUSY      (0x0000)  // Read: Bit = 0 when conversion is in progress
    #define ADS1015_REG_CONFIG_OS_NOTBUSY   (0x8000)  // Read: Bit = 1 when device is not performing a conversion

    #define ADS1015_REG_CONFIG_MUX_MASK     (0x7000)
    #define ADS1015_REG_CONFIG_MUX_DIFF_0_1 (0x0000)  // Differential P = AIN0, N = AIN1 (default)
    #define ADS1015_REG_CONFIG_MUX_DIFF_0_3 (0x1000)  // Differential P = AIN0, N = AIN3
    #define ADS1015_REG_CONFIG_MUX_DIFF_1_3 (0x2000)  // Differential P = AIN1, N = AIN3
    #define ADS1015_REG_CONFIG_MUX_DIFF_2_3 (0x3000)  // Differential P = AIN2, N = AIN3
    #define ADS1015_REG_CONFIG_MUX_SINGLE_0 (0x4000)  // Single-ended AIN0
    #define ADS1015_REG_CONFIG_MUX_SINGLE_1 (0x5000)  // Single-ended AIN1
    #define ADS1015_REG_CONFIG_MUX_SINGLE_2 (0x6000)  // Single-ended AIN2
    #define ADS1015_REG_CONFIG_MUX_SINGLE_3 (0x7000)  // Single-ended AIN3

    #define ADS1015_REG_CONFIG_PGA_MASK     (0x0E00)
    #define ADS1015_REG_CONFIG_PGA_6_144V   (0x0000)  // +/-6.144V range = Gain 2/3
    #define ADS1015_REG_CONFIG_PGA_4_096V   (0x0200)  // +/-4.096V range = Gain 1
    #define ADS1015_REG_CONFIG_PGA_2_048V   (0x0400)  // +/-2.048V range = Gain 2 (default)
    #define ADS1015_REG_CONFIG_PGA_1_024V   (0x0600)  // +/-1.024V range = Gain 4
    #define ADS1015_REG_CONFIG_PGA_0_512V   (0x0800)  // +/-0.512V range = Gain 8
    #define ADS1015_REG_CONFIG_PGA_0_256V   (0x0A00)  // +/-0.256V range = Gain 16

    #define ADS1015_REG_CONFIG_MODE_MASK    (0x0100)
    #define ADS1015_REG_CONFIG_MODE_CONTIN  (0x0000)  // Continuous conversion mode
    #define ADS1015_REG_CONFIG_MODE_SINGLE  (0x0100)  // Power-down single-shot mode (default)

    #define ADS1015_REG_CONFIG_DR_MASK      (0x00E0)  
    #define ADS1015_REG_CONFIG_DR_128SPS    (0x0000)  // 128 samples per second
    #define ADS1015_REG_CONFIG_DR_250SPS    (0x0020)  // 250 samples per second
    #define ADS1015_REG_CONFIG_DR_490SPS    (0x0040)  // 490 samples per second
    #define ADS1015_REG_CONFIG_DR_920SPS    (0x0060)  // 920 samples per second
    #define ADS1015_REG_CONFIG_DR_1600SPS   (0x0080)  // 1600 samples per second (default)
    #define ADS1015_REG_CONFIG_DR_2400SPS   (0x00A0)  // 2400 samples per second
    #define ADS1015_REG_CONFIG_DR_3300SPS   (0x00C0)  // 3300 samples per second

    #define ADS1015_REG_CONFIG_CMODE_MASK   (0x0010)
    #define ADS1015_REG_CONFIG_CMODE_TRAD   (0x0000)  // Traditional comparator with hysteresis (default)
    #define ADS1015_REG_CONFIG_CMODE_WINDOW (0x0010)  // Window comparator

    #define ADS1015_REG_CONFIG_CPOL_MASK    (0x0008)
    #define ADS1015_REG_CONFIG_CPOL_ACTVLOW (0x0000)  // ALERT/RDY pin is low when active (default)
    #define ADS1015_REG_CONFIG_CPOL_ACTVHI  (0x0008)  // ALERT/RDY pin is high when active

    #define ADS1015_REG_CONFIG_CLAT_MASK    (0x0004)  // Determines if ALERT/RDY pin latches once asserted
    #define ADS1015_REG_CONFIG_CLAT_NONLAT  (0x0000)  // Non-latching comparator (default)
    #define ADS1015_REG_CONFIG_CLAT_LATCH   (0x0004)  // Latching comparator

    #define ADS1015_REG_CONFIG_CQUE_MASK    (0x0003)
    #define ADS1015_REG_CONFIG_CQUE_1CONV   (0x0000)  // Assert ALERT/RDY after one conversions
    #define ADS1015_REG_CONFIG_CQUE_2CONV   (0x0001)  // Assert ALERT/RDY after two conversions
    #define ADS1015_REG_CONFIG_CQUE_4CONV   (0x0002)  // Assert ALERT/RDY after four conversions
    #define ADS1015_REG_CONFIG_CQUE_NONE    (0x0003)  // Disable the comparator and put ALERT/RDY in high state (default)
/*=========================================================================*/

typedef enum
{
  GAIN_TWOTHIRDS    = ADS1015_REG_CONFIG_PGA_6_144V,
  GAIN_ONE          = ADS1015_REG_CONFIG_PGA_4_096V,
  GAIN_TWO          = ADS1015_REG_CONFIG_PGA_2_048V,
  GAIN_FOUR         = ADS1015_REG_CONFIG_PGA_1_024V,
  GAIN_EIGHT        = ADS1015_REG_CONFIG_PGA_0_512V,
  GAIN_SIXTEEN      = ADS1015_REG_CONFIG_PGA_0_256V
} adsGain_t;

#define SLAVE_DEVICE_NAME   ( "ETX_ADS1115" )              // Device and Driver Name

static struct i2c_board_info oled_i2c_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, ADS1015_ADDRESS)
    };

static struct i2c_adapter *etx_i2c_adapter     = NULL;  // I2C Adapter Structure
static struct i2c_client  *etx_i2c_client_ads1115 = NULL;  // I2C Cient Structure (In our case it is ADC)

struct gc_pad {
	struct input_dev *dev;
	char phys[32];

	struct input_dev *dev2;
	char phys2[32];
	unsigned char player_mode;
};

struct gc {
    struct gc_pad pad;
	struct timer_list timer;
	int used;
	struct mutex mutex;
};

static struct gc *gc_base;

static const short gc_btn[] = {
	BTN_TL2, BTN_TR2, BTN_TL, BTN_TR, BTN_X, BTN_A, BTN_B, BTN_Y,
	BTN_SELECT, BTN_THUMBL, BTN_THUMBR, BTN_START, BTN_0, BTN_1, BTN_2
};

#define GC_BTN_TL2 0
#define GC_BTN_TR2 1
#define GC_BTN_TL 2
#define GC_BTN_TR 3
#define GC_BTN_X 4
#define GC_BTN_A 5
#define GC_BTN_B 6
#define GC_BTN_Y 7
#define GC_BTN_SELECT 8
#define GC_BTN_THUMBL 9
#define GC_BTN_THUMBR 10
#define GC_BTN_START 11
#define GC_BTN_HOME 12
#define GC_BTN_POWER 13
#define GC_BTN_TV 14

static int gc_btn_states[15];

static const short gc_abs[] = {
	ABS_RX, ABS_RY, ABS_X, ABS_Y
};




/**************************************************************************/
/*!
    @brief  Writes 16-bits to the specified destination register
*/
/**************************************************************************/
static void writeRegister(uint8_t reg, uint16_t value) {
	unsigned char buf[3] = {0};
	int ret;
	buf[0] = reg;
	buf[1] = (uint8_t)(value >> 8);
	buf[2] =(uint8_t)(value & 0xFF);
	ret = i2c_master_send(etx_i2c_client_ads1115, buf, 3);
	if(ret<0)
		pr_err("Sending message Error to ADS1115");
}

/**************************************************************************/
/*!
    @brief  Writes 16-bits to the specified destination register
*/
/**************************************************************************/
static uint16_t readRegister(uint8_t reg) 
{
	unsigned char buf[2] = {0};
	int ret;
	buf[0] = ADS1015_REG_POINTER_CONVERT;
	ret = i2c_master_send(etx_i2c_client_ads1115, buf, 1);
	if(ret<0)
		pr_err("Receiving message Error to ADS1115");
	ret = i2c_master_recv(etx_i2c_client_ads1115, buf, 2);
	if(ret<0)
		pr_err("Receiving message Error to ADS1115");
	return *(uint16_t*)buf;
}

/**************************************************************************/
/*!
    @brief  Gets a single-ended ADC reading from the specified channel
*/
/**************************************************************************/
uint16_t readADC_SingleEnded(uint8_t channel) {
  if (channel > 3)
  {
    return 0;
  }
  
  // Start with default values
  uint16_t config = ADS1015_REG_CONFIG_CQUE_NONE    | // Disable the comparator (default val)
                    ADS1015_REG_CONFIG_CLAT_NONLAT  | // Non-latching (default val)
                    ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
                    ADS1015_REG_CONFIG_CMODE_TRAD   | // Traditional comparator (default val)
                    ADS1015_REG_CONFIG_DR_1600SPS   | // 1600 samples per second (default)
                    ADS1015_REG_CONFIG_MODE_SINGLE;   // Single-shot mode (default)

  // Set PGA/voltage range
  config |= GAIN_ONE;

  // Set single-ended input channel
  switch (channel)
  {
    case (0):
      config |= ADS1015_REG_CONFIG_MUX_SINGLE_0;
      break;
    case (1):
      config |= ADS1015_REG_CONFIG_MUX_SINGLE_1;
      break;
    case (2):
      config |= ADS1015_REG_CONFIG_MUX_SINGLE_2;
      break;
    case (3):
      config |= ADS1015_REG_CONFIG_MUX_SINGLE_3;
      break;
  }

  // Set 'start single-conversion' bit
  config |= ADS1015_REG_CONFIG_OS_SINGLE;

  // Write config register to the ADC
  writeRegister(ADS1015_REG_POINTER_CONFIG, config);

  // Wait for the conversion to complete
  //delay(ADS1115_CONVERSIONDELAY);

  // Read the conversion results
  // Shift 12-bit results right 4 bits for the ADS1015
  return readRegister(ADS1015_REG_POINTER_CONVERT);  
}

#ifdef HAVE_TIMER_SETUP
static void gc_timer(struct timer_list *t)
{
	struct gc *gc = from_timer(gc, t, timer);
#else
static void gc_timer(unsigned long private)
{
	struct gc *gc = (void *) private;
#endif
	struct input_dev* dev = gc->pad.dev;
	int axisVal;
	
    gc_btn_states[GC_BTN_THUMBR] = gpio_get_value(25);
	input_report_key(dev, BTN_THUMBR, gc_btn_states[GC_BTN_THUMBR] == 0);

	axisVal = readADC_SingleEnded(0);
	input_abs_set_res(dev, gc_abs[0], axisVal);

    input_sync(dev);
    mod_timer(&gc->timer, jiffies + GC_REFRESH_TIME);
}

static int gc_open(struct input_dev *dev)
{
	struct gc *gc = input_get_drvdata(dev);
	int err;

	err = mutex_lock_interruptible(&gc->mutex);
	if (err)
		return err;

	if (!gc->used++)
		mod_timer(&gc->timer, jiffies + GC_REFRESH_TIME);

	mutex_unlock(&gc->mutex);
	return 0;
}


static void gc_close(struct input_dev *dev)
{
	struct gc *gc = input_get_drvdata(dev);

	mutex_lock(&gc->mutex);
	if (!--gc->used) {
		del_timer_sync(&gc->timer);
	}
	mutex_unlock(&gc->mutex);
}

static int __init gc_setup_pad(struct gc *gc)
{
	struct gc_pad *pad = &gc->pad;
	struct input_dev *input_dev;
	int i;
	int err;

	pad->dev = input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("Not enough memory for input device\n");
		return -ENOMEM;
	}

	input_dev->name = "WiiU Gamepad";
	input_dev->phys = "Input0";
	input_dev->id.bustype = BUS_PARPORT;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;

	input_set_drvdata(input_dev, gc);

	input_dev->open = gc_open;
	input_dev->close = gc_close;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);

    for (i = 0; i < 4; i++)
        input_set_abs_params(input_dev,
                        gc_abs[i], 0, 0xFFFF, 0, 0);

    for (i = 0; i < 15; i++)
	{
		//input_dev->keybit[BIT_WORD(gc_btn[i])] = BIT_MASK(gc_btn[i]);
		__set_bit(gc_btn[i], input_dev->keybit);
	}
	
    err = input_register_device(pad->dev);
	if (err)
    {
        input_free_device(pad->dev);
        pad->dev = NULL;
        return err;
    }
		
	pr_info("Wiiu Gamepad initialized");

	return 0;
}

static struct gc __init *gc_probe(void)
{
	struct gc *gc;
	int err;

	gc = kzalloc(sizeof(struct gc), GFP_KERNEL);
	if (!gc) {
		pr_err("Not enough memory\n");
		err = -ENOMEM;
		return ERR_PTR(err);
	}

	mutex_init(&gc->mutex);
	#ifdef HAVE_TIMER_SETUP
	timer_setup(&gc->timer, gc_timer, 0);
	#else
	setup_timer(&gc->timer, gc_timer, (long) gc);
	#endif
	err = gpio_request(25,"THUMBR");
	if(err)
	{
		if (gc->pad.dev)
        {
            input_unregister_device(gc->pad.dev);
        }
        kfree(gc);
        return ERR_PTR(err);
	}
	
	err = gpio_direction_input(25);
 	if(err)
	{
		if (gc->pad.dev)
        {
            input_unregister_device(gc->pad.dev);
        }
        kfree(gc);
        return ERR_PTR(err);
	}
	//pinMode(6, INPUT);
	//pullUpDnControl(6, PUD_UP);

    err = gc_setup_pad(gc);
    if (err)
    {
        if (gc->pad.dev)
        {
            input_unregister_device(gc->pad.dev);
        }
        kfree(gc);
        return ERR_PTR(err);
    }

	/* setup common pins for each pad type */
	

	return gc;
}

static void gc_remove(struct gc *gc)
{
    if (gc->pad.dev)
    {
        input_unregister_device(gc->pad.dev);
    }
	gpio_free(25);
	kfree(gc);
}

/*
** This function getting called when the slave has been found
** Note : This will be called only once when we load the driver.
*/
static int etx_oled_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    return 0;
}
/*
** This function getting called when the slave has been removed
** Note : This will be called only once when we unload the driver.
*/
static int etx_oled_remove(struct i2c_client *client)
{   
    return 0;
}
/*
** Structure that has slave device id
*/
static const struct i2c_device_id etx_oled_id[] = {
        { SLAVE_DEVICE_NAME, 0 },
        { }
};
MODULE_DEVICE_TABLE(i2c, etx_oled_id);
/*
** I2C driver Structure that has to be added to linux
*/
static struct i2c_driver etx_oled_driver = {
        .driver = {
            .name   = SLAVE_DEVICE_NAME,
            .owner  = THIS_MODULE,
        },
        .probe          = etx_oled_probe,
        .remove         = etx_oled_remove,
        .id_table       = etx_oled_id,
};

static int __init gc_init(void)
{
	//wiringPiSetup();
    gc_base = gc_probe();
    if (IS_ERR(gc_base))
    {
        return -ENODEV;
    }
	
	etx_i2c_adapter     = i2c_get_adapter(1);
    
    if( etx_i2c_adapter != NULL )
    {
        etx_i2c_client_ads1115 = i2c_new_client_device(etx_i2c_adapter, &oled_i2c_board_info);
        
        if( etx_i2c_client_ads1115 != NULL )
        {
            i2c_add_driver(&etx_oled_driver);
        }
        
        i2c_put_adapter(etx_i2c_adapter);
    }
    
    pr_info("ADS1115 Added!!!\n");
	return 0;
}

static void __exit gc_exit(void)
{
	if (gc_base)
		gc_remove(gc_base);

	i2c_unregister_device(etx_i2c_client_ads1115);
    i2c_del_driver(&etx_oled_driver);
    pr_info("ADS1115 Removed!!!\n");
}

module_init(gc_init);
module_exit(gc_exit);
