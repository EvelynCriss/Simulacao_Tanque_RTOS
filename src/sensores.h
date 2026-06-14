#ifndef SENSORES_H
#define SENSORES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "configuracao.h"

// Tipos de sensor
typedef enum {
    SENSOR_NIVEL,
    SENSOR_TEMPERATURA,
    SENSOR_PRESSAO
} TipoSensor;

// Estrutura de leitura do sensor
typedef struct {
    TipoSensor  tipo;
    float       valor;
    TickType_t  tick;
    uint8_t     valido;
} LeituraSensor;

extern QueueHandle_t filaLeituras;

// Estado do tanque (recurso compartilhado)
typedef struct {
    float nivel;
    float temperatura;
    float pressao;
    float vazaoEntrada;
    float vazaoSaida;
} EstadoTanque;

extern EstadoTanque estadoTanque;


void sensores_inicializar(void);
void vTaskSensores(void *pvParameters);
void atualizarSimulacao(EstadoTanque *t);

#endif /* SENSORES_H */