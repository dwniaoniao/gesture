#include <Arduino_FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <Wire.h>
#include <ADXL345.h>

ADXL345 adxl;
struct accData{
    int x;
    int y;
    int z;
};

QueueHandle_t accDataQueue;
SemaphoreHandle_t mutex_serial;

void setup(){
    mutex_serial = xSemaphoreCreateMutex();
    if(!mutex_serial){
        while(1);
    }
    accDataQueue = xQueueCreate(10, sizeof(struct accData));
    if(!accDataQueue){
        while(1);
    }
    xTaskCreate(taskGetAccData, "getAccData", 128, NULL, 1, NULL);
    xTaskCreate(taskSerial, "serial", 128, NULL, 2, NULL);
    xTaskCreate(taskParseAccData, "parseAccData", 128, NULL, 2, NULL);
}

void loop(){}

void taskGetAccData(void *pvParameters){
    (void)pvParameters;
    adxl.powerOn();

    //set activity/ inactivity thresholds (0-255)
    adxl.setActivityThreshold(75); //62.5mg per increment
    adxl.setInactivityThreshold(75); //62.5mg per increment
    adxl.setTimeInactivity(10); // how many seconds of no activity is inactive?
    
    //look of activity movement on this axes - 1 == on; 0 == off 
    adxl.setActivityX(1);
    adxl.setActivityY(1);
    adxl.setActivityZ(1);
    
    //look of inactivity movement on this axes - 1 == on; 0 == off
    adxl.setInactivityX(1);
    adxl.setInactivityY(1);
    adxl.setInactivityZ(1);
    
    for(;;){
        struct accData accDataRead;
        adxl.readXYZ(&accDataRead.x, &accDataRead.y, &accDataRead.z);
        xQueueSend(accDataQueue, &accDataRead, portMAX_DELAY);
        vTaskDelay(1);
    }
}

void taskSerial(void *pvParameters){
    (void)pvParameters;
    Serial.begin(9600);

    for(;;){
        struct accData accDataRead;
        if(xQueueReceive(accDataQueue, &accDataRead, portMAX_DELAY) == pdTRUE){
            if(xSemaphoreTake(mutex_serial, portMAX_DELAY) == pdTRUE){
                Serial.print("\t\t\t\t\t\t\t");
                Serial.print(accDataRead.x);
                Serial.print("\t");
                Serial.print(accDataRead.y);
                Serial.print("\t");
                Serial.println(accDataRead.z);
                xSemaphoreGive(mutex_serial);
            }
        }
        vTaskDelay(1);
    }
}

#define RTH_UD 500
#define FTH_UD 0
#define RTH_LR 300
#define FTH_LR -300
#define D_UP_DETECTED 100
#define D_DOWN_DETECTED 100
#define D_LEFT_DETECTED 100
#define D_RIGHT_DETECTED 100

void taskParseAccData(void *pvParameters){
    (void)pvParameters;
    
    static bool foo = false;
    static bool fooBak = false;
    static TickType_t start_rising_ud = 0;
    static TickType_t start_falling_ud = 0;
    static bool up = false;
    static bool down = false;

    static bool bar = false;
    static bool barBak = false;
    static TickType_t start_rising_lr = 0;
    static TickType_t start_falling_lr = 0;
    static bool left = false;
    static bool right = false;

    for(;;){
        struct accData accDataRead;
        if(xQueueReceive(accDataQueue, &accDataRead, portMAX_DELAY) == pdTRUE){
            if(accDataRead.z > RTH_UD){
                foo = true;
                start_falling_ud = xTaskGetTickCount();
            }
            if(accDataRead.z < FTH_UD){
                foo = false;
                start_rising_ud = xTaskGetTickCount();
            }
            if(foo && !fooBak){
                if((xTaskGetTickCount() - start_rising_ud) * portTICK_PERIOD_MS < D_DOWN_DETECTED){
                    up = false;
                    down = true;
                }
            }
            if(!foo && fooBak){
                if((xTaskGetTickCount() - start_falling_ud) * portTICK_PERIOD_MS < D_UP_DETECTED){
                    up = true;
                    down = false;
                }
            }
            fooBak = foo;

            if(accDataRead.y > RTH_LR){
                bar = true;
                start_falling_lr = xTaskGetTickCount();
            }
            if(accDataRead.y < FTH_LR){
                bar = false;
                start_rising_lr = xTaskGetTickCount();
            }
            if(bar && !barBak){
                if((xTaskGetTickCount() - start_rising_lr) * portTICK_PERIOD_MS < D_RIGHT_DETECTED){
                    left = false;
                    right = true;
                }
            }
            if(!bar && barBak){
                if((xTaskGetTickCount() - start_falling_lr) * portTICK_PERIOD_MS < D_LEFT_DETECTED){
                    left = true;
                    right = false;
                }
            }
            barBak = bar;

            if(xSemaphoreTake(mutex_serial, portMAX_DELAY) == pdTRUE){
                if(accDataRead.x > 100){
                    Serial.print("Backwarding");
                } else if(accDataRead.x < -100){
                    Serial.print("Forwarding");
                }
                if(accDataRead.y > 100){
                    Serial.print("  Turning right");
                } else if(accDataRead.y < -100){
                    Serial.print("  Turning left");
                }
                if(up){
                    Serial.print("  up");
                } else if(down){
                    Serial.print("  down");
                }
                if(left){
                    Serial.print("  left");
                } else if(right){
                    Serial.print("  right");
                }
                Serial.println();
                xSemaphoreGive(mutex_serial);
            }
        }
        // vTaskDelay(50 / portTICK_PERIOD_MS);
        vTaskDelay(1);
    }
}
