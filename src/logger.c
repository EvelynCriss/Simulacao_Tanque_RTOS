#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "configuracao.h"

// Fila de logs 
QueueHandle_t filaLogs = NULL;

void logger_inicializar(void)
{
    filaLogs = xQueueCreate(20, LOG_MAX);
}

void enviarLog(const char *msg)
{
    char buf[LOG_MAX];
    strncpy(buf, msg, LOG_MAX - 1);
    buf[LOG_MAX - 1] = '\0';
    // timeout 0: se a fila estiver cheia, descarta para não bloquear
    if (filaLogs != NULL) {
        xQueueSend(filaLogs, buf, 0);
    }
}

void vTaskLogger(void *pvParameters)
{
    char buf[LOG_MAX];
    (void) pvParameters;

    for(;;)
    {
        if(xQueueReceive(filaLogs, buf, portMAX_DELAY) == pdTRUE)
        {
            printf("%s\n", buf);
        }
    }
}