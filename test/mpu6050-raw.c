#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <bcm2835.h>
#include <mpu6050.h>

int16_t ax, ay, az;
int16_t gx, gy, gz;

static void terminate(int dummy)
{
    bcm2835_i2c_end();
    bcm2835_close();

    _exit(1);
}

static void setup_sighandlers(void)
{
    int i;

    // Catch all signals possible - it is vital we kill the DMA engine
    // on process exit!
    for (i = 0; i < 64; i++) {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminate;
        sigaction(i, &sa, NULL);
    }
}

void setup(void)
{
    // initialize device
    printf("Initializing I2C devices...\n");
    mpu6050_initialize();

    // verify connection
    printf("Testing device connections...\n");
    printf(mpu6050_testConnection() ? "MPU6050 connection successful\n" : "MPU6050 connection failed\n");
}

void loop(void)
{
    // read raw accel/gyro measurements from device
    mpu6050_getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

    // these methods (and a few others) are also available
    //accelgyro.getAcceleration(&ax, &ay, &az);
    //accelgyro.getRotation(&gx, &gy, &gz);

    // display accel/gyro x/y/z values
    printf("a/g: %6hd %6hd %6hd   %6hd %6hd %6hd\n",ax,ay,az,gx,gy,gz);
}

int main()
{
    setup_sighandlers();

    if (!bcm2835_init())
        return 1;
    bcm2835_i2c_begin();

    bcm2835_i2c_setSlaveAddress(MPU6050_DEFAULT_ADDRESS);
    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_148);

    setup();
    for (;;)
        loop();

    bcm2835_i2c_end();
    bcm2835_close();

    return 0;
}
