#ifndef SUPERVISAO_H
#define SUPERVISAO_H

#include "FreeRTOS.h"
#include "semphr.h"

// Estados do sistema
typedef enum {
    MODO_NORMAL,
    MODO_ALERTA,
    MODO_SEGURO,
    MODO_AGUARDANDO_RESET
} ModoSistema;


extern ModoSistema  modoAtual;
extern char         causaFalha[80];
extern TickType_t   ultimaLeituraSensorTick;

// Semáforos
extern SemaphoreHandle_t semEmergencia;
extern SemaphoreHandle_t semReset;
extern SemaphoreHandle_t mutexModo;


void supervisao_inicializar(void);
void vTaskSupervisao(void *pvParameters);
void ativarModoSeguro(const char *causa);

#endif /* SUPERVISAO_H */