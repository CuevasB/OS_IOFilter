// Contains the Write Module, moddymod2.c contains Read

// Operating Systems (COP4600)
// Programming Assignment #4: I/O Filtering
// Submission Date: 4/20/2018
// Submission By: Brandon Cuevas, Jacquelyn Law, Lorraine Yerger
// File: moddymod.c
// Reference Used: http://derekmolloy.ie/writing-a-linux-kernel-module-part-2-a-character-device/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define BUFFER_SIZE 1024
#define DEVICE_NAME "moddymod"
#define CLASS_NAME "mod"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brandon Jacquelyn Lorraine");
MODULE_DESCRIPTION("Programming Assignment #4: I/O Filtering");
MODULE_VERSION("0.1");

static int majorNumber;

// Try to export mainBuffer so moddymod2.c can access it
char mainBuffer[BUFFER_SIZE]= {0};
static int bufferOccupation = 0;
static DEFINE_MUTEX(moddymod_mutex);
EXPORT_SYMBOL(mainBuffer);
EXPORT_SYMBOL(bufferOccupation);
EXPORT_SYMBOL(moddymod_mutex);

static int bufferWriteIndex = 0;
static struct class *modClass = NULL;
static struct device *modDevice = NULL;

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static char *injectionString = "Undefeated 2018 National Champions UCF";
static int injectionLength = 38;

static struct file_operations fops = {
	.open = dev_open,
	.write = dev_write,
	.release = dev_release
};

/** @brief This function is called whenever the device is being written to from user space
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
// Also replaces "UCF" with "Undefeated 2018 National Champions UCF"
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
	int bytesToReceive = len;
	int bytesToInject = 0;
	int injectionIndex = 0;
	int receiveIndex = 0;
	int injectionCount = 0;

	// While there is still room in the buffer and bytes to recieve
	while (bytesToReceive > 0 && bufferOccupation < BUFFER_SIZE)
	{
		// Check if UCF is about to be written and inject if necessary (short-circuit-logic reliant)
		if ((buffer[receiveIndex] == 'F') &&
		    (bufferOccupation > 1) &&
		    (mainBuffer[(bufferWriteIndex + BUFFER_SIZE - 1) % BUFFER_SIZE] == 'C') &&
		    (mainBuffer[(bufferWriteIndex + BUFFER_SIZE - 2) % BUFFER_SIZE] == 'U'))
		{
			// Push bufferWriteIndex back to begin replacing "UCF"
			bufferWriteIndex = (bufferWriteIndex + BUFFER_SIZE - 2) % BUFFER_SIZE;
			bufferOccupation -= 2;
			
			bytesToInject = injectionLength;
			injectionIndex = 0;

			while (bytesToInject > 0 && bufferOccupation < BUFFER_SIZE)
			{
				// Put byte in main buffer at current write index
				sprintf(mainBuffer + bufferWriteIndex, "%c", injectionString[injectionIndex++]);
				bufferOccupation++;
				bufferWriteIndex++;
				bytesToInject--;

				if (bufferWriteIndex > BUFFER_SIZE - 1)
				{
					bufferWriteIndex = 0;
				}

				injectionCount++;
			}

			bytesToReceive--;
			receiveIndex++;

			// Adjust for counting purposes
			injectionCount -= 3;
		}
		// Write normally otherwise
		else
		{
			// Put byte in main buffer at current write index
			sprintf(mainBuffer + bufferWriteIndex, "%c", buffer[receiveIndex++]);
			bufferOccupation++;
			bufferWriteIndex++;
			bytesToReceive--;

			if (bufferWriteIndex > BUFFER_SIZE - 1)
			{
				bufferWriteIndex = 0;
			}
		}
	}

	printk(KERN_INFO "moddymod: Wrote %d characters to the device\n", len - bytesToReceive + injectionCount);

	// Release mutex
   	mutex_unlock(&moddymod_mutex);
	
	return len - bytesToReceive + injectionCount;
}

/** @brief The device open function that is called each time the device is opened
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep) {
   printk(KERN_INFO "moddymod: Device has been opened\n");
   if (!mutex_trylock(&moddymod_mutex))
   {
	printk(KERN_ALERT "moddymod: Device in use by another process");
	return -EBUSY;
   }
   return 0;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep) {
   printk(KERN_INFO "moddymod: Device successfully closed\n");
   return 0;
}

int init_module(void) {
	printk(KERN_INFO "moddymod: Installing moddymod.\n");

	// Try to dynamically allocate a major number for the device
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber < 0) {
		printk(KERN_ALERT "moddymod failed to register a major number \n");
		return majorNumber;
	}
	printk(KERN_INFO "moddymod: Registered correctly with major number %d\n", majorNumber);

	// Register the device class
   	modClass = class_create(THIS_MODULE, CLASS_NAME);
   	if (IS_ERR(modClass))
	{
		// Check for error and clean up if there is
      		unregister_chrdev(majorNumber, DEVICE_NAME);
      		printk(KERN_ALERT "Failed to register device class\n");
      		return PTR_ERR(modClass);          // Return pointer error
   	}
  	printk(KERN_INFO "moddymod: Device class registered correctly\n");

   	// Register the device driver
   	modDevice = device_create(modClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(modDevice))
	{
		// Clean up if there is an error
		class_destroy(modClass);
		// Repeated code but the alternative is goto statements
      		unregister_chrdev(majorNumber, DEVICE_NAME);
      		printk(KERN_ALERT "Failed to create the device\n");

		return PTR_ERR(modDevice);
	}

   	printk(KERN_INFO "moddymod: Device class created correctly\n");

	// Initialize mutex
	mutex_init(&moddymod_mutex);

	printk(KERN_INFO "moddymod: Mutex initialized\n");

	return 0;
}

void cleanup_module(void) {
	printk(KERN_INFO "moddymod: Removing moddymod\n");

	device_destroy(modClass, MKDEV(majorNumber, 0));     // remove the device
	class_unregister(modClass);                          // unregister the device class
	class_destroy(modClass);                             // remove the device class
   	unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number
	
	// Destroy dynamically allocated mutex
	mutex_destroy(&moddymod_mutex);
}
