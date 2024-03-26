#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "hal/spi_types.h"
#include "driver/gpio.h"

#define BUILT_IN_LED    13
#define ON          1
#define OFF         0
#define ENABLE_RGB_POWER_PIN  21 //RGB LED not usable unless 21 is active.
#define RGB_CLK 45
#define RGB_DATA 40

spi_device_handle_t spi;   
const uint8_t STEPS_TO_TRANSITION = 50;
const uint8_t delayUnitsBetweenColourChanges = 25;
const uint8_t delayUnitsBetweenFadeFrames = 3;

typedef struct RGB_Pairs {
    uint8_t r1;
    uint8_t g1;
    uint8_t b1;
    uint8_t r2;
    uint8_t g2;
    uint8_t b2;
} RGB_Pairs_t;

void initApa102(){
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10000,   // Clock out at 1 MHz with 1 us cycle
        .mode = 0,                 // SPI mode 0 - the clock signal starts with a low signal
        .spics_io_num = -1,        // CS pin
        .queue_size = 7,           // Queue 7 transactions at a time
    };

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = RGB_DATA,
        .sclk_io_num = RGB_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };

    // Initialize the SPI
    spi_bus_initialize(FSPI_HOST, &buscfg, 1);

    // Define SPI handle
    spi_bus_add_device(FSPI_HOST, &devcfg, &spi);

    gpio_set_direction(ENABLE_RGB_POWER_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ENABLE_RGB_POWER_PIN, ON);
}

void setApa102Colour(uint8_t r, uint8_t g, uint8_t b){

    uint8_t rgbBits[] = {
        //Start Frame
        0x00, 0x00, 0x00, 0x00,

        //LED Frame
        0b11100111, //3 bits //5 bits brightness
        b, //blue
        g, //green
        r, //red

        //End Frame
        0xFF, 0xFF, 0xFF, 0xFF
    };

    spi_transaction_t trans;
    memset(&trans, 0, sizeof(trans));

    trans.length = 96;              // length in bits
    trans.tx_buffer = rgbBits;

    spi_device_transmit(spi, &trans);
}

uint8_t rangeAndStepToValue(uint8_t min, uint8_t max, uint8_t step){
    float val = (float)(max - min);
    val = val / (float)STEPS_TO_TRANSITION;
    val = (val * (float)step) + (float)min; //steps

    return (uint8_t)val;
}

void startupColourSequence(){
    const uint8_t timeBetween = delayUnitsBetweenColourChanges;
    setApa102Colour(0xFF, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0xFF, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0xFF);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0xFF, 0x00, 0xFF);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0xFF, 0xFF, 0x00);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);      
    setApa102Colour(0x00, 0xFF, 0xFF);    
    vTaskDelay(timeBetween);   
    setApa102Colour(0x00, 0x00, 0x00);    
    vTaskDelay(timeBetween);   
}

//Task function to fade from one colour to another
void fadeBetweenColours(void *pvParameters){
    ESP_LOGI("FAD", "fadeBetweenColours");

    RGB_Pairs_t *colours = (RGB_Pairs_t*)pvParameters;
    uint8_t i = 0;
    
    while(1){
        for (i=0; i<STEPS_TO_TRANSITION; i++){

            uint8_t r = rangeAndStepToValue(colours->r1, colours->r2, i);
            uint8_t g = rangeAndStepToValue(colours->g1, colours->g2, i);
            uint8_t b = rangeAndStepToValue(colours->b1, colours->b2, i);
        
            setApa102Colour(r, g, b);    
            vTaskDelay(delayUnitsBetweenFadeFrames);   
        }

        for (i=STEPS_TO_TRANSITION; i>0; i--){

            uint8_t r = rangeAndStepToValue(colours->r1, colours->r2, i);
            uint8_t g = rangeAndStepToValue(colours->g1, colours->g2, i);
            uint8_t b = rangeAndStepToValue(colours->b1, colours->b2, i);
        
            setApa102Colour(r, g, b);    
            vTaskDelay(delayUnitsBetweenFadeFrames);   
        }

    }

}

void fadeFromRGBtoRGB(uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2){
    ESP_LOGI("FAD", "fadeFromRGBtoRGB");

    TaskHandle_t xHandle = NULL;

    static RGB_Pairs_t pairs;
    pairs.r1 = r1;
    pairs.g1 = g1;
    pairs.b1 = b1;
    pairs.r2 = r2;
    pairs.g2 = g2;
    pairs.b2 = b2;
    
    ESP_LOGI("FAD", "Fade between %d %d %d -> %d %d %d", pairs.r1, pairs.g1, pairs.b1, pairs.r2, pairs.g2, pairs.b2);

    xTaskCreate(fadeBetweenColours, "fadeBetweenColours", 2048, &pairs, 3, &xHandle);
}

void app_main(void)
{

    initApa102();

    ESP_LOGI("APP", "App started with Serial output. Ensure serial comms don't block by sending a new line at the end.");

    gpio_set_direction(BUILT_IN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BUILT_IN_LED, ON);

    startupColourSequence();

    fadeFromRGBtoRGB(0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00);

    while(1){
        ESP_LOGI("APP", "App main loop.");
        vTaskDelay(1000);
   }

    ESP_LOGI("APP", "Done.");
}
