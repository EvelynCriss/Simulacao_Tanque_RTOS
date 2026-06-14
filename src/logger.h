#ifndef LOGGER_H
#define LOGGER_H

#include "FreeRTOS.h"
#include "queue.h"


extern QueueHandle_t filaLogs;


void logger_inicializar(void);
void enviarLog(const char *msg);
void vTaskLogger(void *pvParameters);


#define LOG(fmt, ...) \
    do { \
        char _buf[LOG_MAX]; \
        snprintf(_buf, LOG_MAX, fmt, ##__VA_ARGS__); \
        enviarLog(_buf); \
    } while(0)

#endif /* LOGGER_H */