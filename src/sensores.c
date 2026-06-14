#include <string.h>
#include "sensores.h"
#include "logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Mutex para proteger estadoTanque (definido em main.c) 
extern SemaphoreHandle_t mutexTanque;

// Estado do tanque
EstadoTanque estadoTanque;

// Fila de leituras
QueueHandle_t filaLeituras = NULL;

void sensores_inicializar(void)
{
    filaLeituras = xQueueCreate(10, sizeof(LeituraSensor));
    
    // Estado inicial do tanque 
    estadoTanque.nivel        = 50.0f;
    estadoTanque.temperatura  = 25.0f;
    estadoTanque.pressao      =  2.0f;
    estadoTanque.vazaoEntrada =  1.5f;
    estadoTanque.vazaoSaida   =  0.8f;
}

void atualizarSimulacao(EstadoTanque *t)
{
    t->nivel += t->vazaoEntrada - t->vazaoSaida;
    if(t->nivel > 100.0f) t->nivel = 100.0f;
    if(t->nivel <   0.0f) t->nivel =   0.0f;

    t->temperatura += 0.3f;
    if(t->temperatura > 100.0f) t->temperatura = 100.0f;

    t->pressao = 1.0f + (t->nivel * 0.02f) + (t->temperatura * 0.01f);
}

void vTaskSensores(void *pvParameters)
{
    TickType_t xUltimaLiberacao = xTaskGetTickCount();
    const TickType_t periodo = pdMS_TO_TICKS(1000);
    EstadoTanque copia;
    LeituraSensor leitura;
    extern TickType_t ultimaLeituraSensorTick;
    extern SemaphoreHandle_t mutexModo;
    
    (void) pvParameters;

    for(;;)
    {
        
        xSemaphoreTake(mutexTanque, portMAX_DELAY);
        atualizarSimulacao(&estadoTanque);
        copia = estadoTanque;
        xSemaphoreGive(mutexTanque);

        TickType_t agora = xTaskGetTickCount();

        // Nível
        leitura.tipo   = SENSOR_NIVEL;
        leitura.valor  = copia.nivel;
        leitura.tick   = agora;
        leitura.valido = 1;
        xQueueSend(filaLeituras, &leitura, 0);

        // Temperatura
        leitura.tipo   = SENSOR_TEMPERATURA;
        leitura.valor  = copia.temperatura;
        xQueueSend(filaLeituras, &leitura, 0);

        // Pressão
        leitura.tipo   = SENSOR_PRESSAO;
        leitura.valor  = copia.pressao;
        xQueueSend(filaLeituras, &leitura, 0);

        // Atualiza tick da última leitura
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        ultimaLeituraSensorTick = agora;
        xSemaphoreGive(mutexModo);

        LOG("[Tick %lu] [SENSORES] [LEITURA] nivel=%.1f temp=%.1f pressao=%.2f",
            (unsigned long)agora, copia.nivel, copia.temperatura, copia.pressao);

        vTaskDelayUntil(&xUltimaLiberacao, periodo);
    }
}