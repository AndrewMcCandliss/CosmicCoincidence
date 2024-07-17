#include <stdio.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bit_ops.h"


const uint32_t dir = 0x10; 

void toggle_LED(int toggle){
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, toggle);
}

void print32tob(uint32_t value, bool n){
    for(uint32_t i = 8; i > 0; i--){
        int currentBit = (value >> (i-1)) & 1;
        printf("%d", currentBit);
    }
    if(n){
        printf("\n");
    }
    
}

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
            //printf("Time between call and response: " "%lld\n", absolute_time_diff_us(startState, get_absolute_time()));
            //toggle_LED(1);
            print32tob(currentState, false);
            gpio_set_dir_in_masked(detectorMask);
            gpio_put(dir, 1);
            //toggle_LED(0);
            uint32_t deadtime = absolute_time_diff_us(startState, get_absolute_time());
            totalDeadtime += deadtime;
            count++;
            printf(" %u" " detections " "%u" " Registered " "%ld" "ms" " %ld" "us deadtime" " %ld" "us total deadtime\n",count,detections,currentTime,deadtime,totalDeadtime);
        }
    }
}
