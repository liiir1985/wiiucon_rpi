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
#include "wiringPi.h"

MODULE_AUTHOR("liiir1985");
MODULE_DESCRIPTION("WiiU gamepad driver");
MODULE_LICENSE("MIT");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#define HAVE_TIMER_SETUP
#endif

#define GC_REFRESH_TIME	HZ/100

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

static const short gc_abs[] = {
	ABS_RX, ABS_RY, ABS_X, ABS_Y
};
#ifdef HAVE_TIMER_SETUP
static void gc_timer(struct timer_list *t)
{
	struct gc *gc = from_timer(gc, t, timer);
#else
static void gc_timer(unsigned long private)
{
	struct gc *gc = (void *) private;
#endif

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

    for (i = 0; i < 4; i++)
        input_set_abs_params(input_dev,
                        gc_abs[i], -1, 1, 0, 0);

    for (i = 0; i < 15; i++)
        __set_bit(gc_btn[i], input_dev->keybit);

	
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
	kfree(gc);
}

static int __init gc_init(void)
{
    gc_base = gc_probe();
    if (IS_ERR(gc_base))
    {
        return -ENODEV;
    }
	return 0;
}

static void __exit gc_exit(void)
{
	if (gc_base)
		gc_remove(gc_base);
}

module_init(gc_init);
module_exit(gc_exit);
