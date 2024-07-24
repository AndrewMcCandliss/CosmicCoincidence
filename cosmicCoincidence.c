#include <stdio.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bit_ops.h"


const uint32_t dir = 0x10; 
const int bitrate = 40 * 1000;
const int pulseLength = 1000000 / bitrate;

void toggle_LED(int toggle){
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, toggle);
}

void print32tob(uint32_t value, bool n){
    for(uint32_t i = 32; i > 0; i--){
        int currentBit = (value >> (i-1)) & 1;
        printf("%d", currentBit);
    }
    if(n){
        printf("\n");
    }
    
}
/* Psuedo Code for reading data off of the data lines
Receive data from arduino
Arduino waits for response
RPI responds
Ard sends data. Define the size of the data to avoid the need for flags. Only need to send numbers. Define a 1 as a short pulse and a 0 as a long pulse

gpio_put(dir, 0);
gpio_set_dir_out_masked(detectorMask);
gpio_put_masked(detectorMask, 0xFF);
sleep_us(100);
gpio_put(dir,1);
gpio_set_dir_in_masked(detectorMask);

int fudge = 5;
int dataArray[8] = {0,0,0,0,0,0,0,0};
for(int k = 0; k<dataSize; k++){
    int dataArrayInitial[8] = {0,0,0,0,0,0,0,0};
    for(int i = 0; i<8; i++){ //possibly unecessary loop
        int data = gpio_get(2**i) & (currentState>>i);
        dataArrayInitial[i] = dataArrayInitial[i] | data;
    }sleep_us(20);
    for(int i = 0; i<8; i++){
        int data = gpio_get(2**i) & (currentState>>i);
        dataArray[i] = (dataArray[i]<<k) | (dataArrayInitial[i] ^ data);
    }sleep_us(fudge)
    
} going to try a different route, leaving this code in the meantime
*/
/*
gpio_put(dir, 0);
gpio_set_dir_out_masked(detectorMask);
gpio_put_masked(detectorMask, 0xFF);
sleep_us(100);
gpio_put(dir,1);
gpio_set_dir_in_masked(detectorMask);

int bitrate = 1;
int pulseLength = 1000000 / bitrate;

uint8_t buffer[32];
for(int i = 0; i<32; i++){
    buffer[i] = (gpio_get_all() & currentState);
    sleep_us(pulseLength);
} 
uint32_t data[8];
for (int k = 0; k < 8; k++){
    for(int i = 0; i < 32; i++){
        data[k] = data[k] | (buffer[i]>>k & 1)
    }
}
*/

int main()
{
    stdio_init_all();

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    
    //for debug don't start setup until pin 8 is high
    gpio_init(8); gpio_set_dir(8,0);
    while(gpio_get(8) == 0){
        toggle_LED(1);
        sleep_ms(100);
        toggle_LED(0);
        sleep_ms(100);
    }

    //Detectors should be on pins 11111111b or 0xFF
    gpio_init_mask(0xFF); //inititalize all detector pins and direction pin
    gpio_set_dir_in_masked(0xFF); //set all detector pins to input

    gpio_init(dir);
    gpio_set_dir(dir, 1); //direction pin set to output
    gpio_put(dir,1); //drive direction pin high for data input
    
    uint32_t detectorMask = 0x00; //initialize detectorMask to 0

    while(detectorMask == 0){
        detectorMask = gpio_get_all() & 0xFF; //keep checking for detectors until 1 is found
    }
    sleep_ms(100);
    detectorMask = gpio_get_all() & 0xFF; //check detectors again now that at least one has been found

    printf("Detector found at: " "%d\n", to_ms_since_boot(get_absolute_time()));
    gpio_set_dir_out_masked(detectorMask);
    gpio_put_masked(detectorMask, 0xFF);
    gpio_put(dir,0);
    toggle_LED(1);
    sleep_ms(1000);
    toggle_LED(0);
    gpio_put(dir, 1);
    gpio_set_dir_in_masked(detectorMask);
    while((gpio_get_all() & detectorMask) != 0){continue;}
    


    // Example to turn on the Pico W LED
    //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    uint32_t totalDeadtime = 0;
    uint count = 0;
    while (true) {
        absolute_time_t startState = get_absolute_time();
        uint32_t currentState = gpio_get_all() & detectorMask;
        uint8_t detections = __builtin_popcount(currentState);
        if(detections > 1){
            uint32_t currentTime = to_ms_since_boot(get_absolute_time());
            gpio_put(dir, 0);
            gpio_set_dir_out_masked(detectorMask);
            gpio_put_masked(detectorMask, 0xFF);
            //sleep_us(100);
            //printf("Time between call and response: " "%lld\n", absolute_time_diff_us(startState, get_absolute_time()));
            //toggle_LED(1);
            print32tob(currentState, true);
            gpio_put(dir, 1);
            gpio_set_dir_in_masked(detectorMask);
            while(__builtin_popcount(gpio_get_all() & currentState) != detections){continue;}
            while((gpio_get_all() & detectorMask) != 0){continue;}
            gpio_put(dir,0);
            gpio_set_dir_out_masked(detectorMask);
            gpio_put_masked(detectorMask, 0xFF);
            sleep_us(50);
            gpio_put(dir,1);
            gpio_set_dir_in_masked(detectorMask);
            uint8_t buffer[32];
            for(int i = 0; i<32; i++){
                buffer[i] = (gpio_get_all() & currentState);
                sleep_us(pulseLength);
            } 
            uint32_t data[8] = {0,0,0,0,0,0,0,0};
            for (int k = 0; k < 8; k++){ 
                for(int i = 0; i < 32; i++){
                    data[k] = data[k] | ((buffer[i]>>k & 1)<<(32-i));
                }
            }
            for(int i = 0; i < 2; i++){ 
                print32tob(data[i],true);
            }
            /*uint16_t assembledData[8][3];
            for(int i = 0; i < 8; i++){
                assembledData[i][0] = data[i] & 0xFFF00000;
                assembledData[i][1] = data[i] & 0xFFC00;
                assembledData[i][2] = data[i] & 0x3FF;
                printf("%ld\n", assembledData[i][0]);
            }*/
            


            //toggle_LED(0);
            uint32_t deadtime = absolute_time_diff_us(startState, get_absolute_time());
            totalDeadtime += deadtime;
            count++;
            //sleep_ms(20);
            printf(" %u" " detections " "%u" " Registered " "%ld" "ms" " %ld" "us deadtime" " %ld" "us total deadtime\n",count,detections,currentTime,deadtime,totalDeadtime);
        }
    }
}
