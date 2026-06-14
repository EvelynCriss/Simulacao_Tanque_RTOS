#include <string.h>
#include "supervisao.h"
#include "controle.h"
#include "logger.h"
#include "configuracao.h"

ModoSistema  modoAtual               = MODO_NORMAL;
char         causaFalha[80]          = "";
TickType_t   ultimaLeituraSensorTick = 0;

SemaphoreHandle_t semEmergencia  = NULL;
SemaphoreHandle_t semReset       = NULL;
SemaphoreHandle_t mutexModo      = NULL;

void supervisao_inicializar(void)
{
    mutexModo      = xSemaphoreCreateMutex();
    semEmergencia  = xSemaphoreCreateBinary();
    semReset       = xSemaphoreCreateBinary();
}

void ativarModoSeguro(const char *causa)
{
    ComandoAtuador cmdSeguro;
    memset(&cmdSeguro, 0, sizeof(cmdSeguro));
    cmdSeguro.bombaLigada     = 0;
    cmdSeguro.aquecedorLigado = 0;
    cmdSeguro.valvulaAberta   = 1;
    cmdSeguro.alarmeLigado    = 1;
    strncpy(cmdSeguro.motivo, "MODO_SEGURO", sizeof(cmdSeguro.motivo) - 1);

    xQueueSend(filaComandos, &cmdSeguro, 0);

    xSemaphoreTake(mutexModo, portMAX_DELAY);
    modoAtual = MODO_SEGURO;
    strncpy(causaFalha, causa, sizeof(causaFalha) - 1);
    xSemaphoreGive(mutexModo);

    LOG("[Tick %lu] [SUPERVISAO] [MODO_SEGURO] causa=%s",
        (unsigned long)xTaskGetTickCount(), causa);
}

void vTaskSupervisao(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(1000);
    extern SemaphoreHandle_t mutexTanque;
    (void) pvParameters;

    for(;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xCheckPeriod);
        
        // Verifica emergência pendente
        if(xSemaphoreTake(semEmergencia, 0) == pdTRUE)
        {
            LOG("[Tick %lu] [SUPERVISAO] [EMERGENCIA] botao de emergencia acionado",
                (unsigned long)xTaskGetTickCount());
            ativarModoSeguro("emergencia externa");
        }
        
        // Verifica timeout dos sensores
        TickType_t agora = xTaskGetTickCount();
        TickType_t ultimaTick;
        ModoSistema modo;
        
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        ultimaTick = ultimaLeituraSensorTick;
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        if(modo != MODO_SEGURO && modo != MODO_AGUARDANDO_RESET)
        {
            TickType_t diff = agora - ultimaTick;
            if(diff >= SENSOR_TIMEOUT_TICKS)
            {
                LOG("[Tick %lu] [SUPERVISAO] [FALHA_SENSOR] timeout - ultima leitura ha %lu ticks",
                    (unsigned long)agora, (unsigned long)diff);
                ativarModoSeguro("falha de sensor - timeout");
            }
        }
        
        // Gerencia MODO_SEGURO -> AGUARDANDO_RESET -> NORMAL
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        if(modo == MODO_SEGURO)
        {
            LOG("[Tick %lu] [SUPERVISAO] [AGUARDANDO_RESET] sistema bloqueado - aguardando confirmacao",
                (unsigned long)xTaskGetTickCount());

            xSemaphoreTake(mutexModo, portMAX_DELAY);
            modoAtual = MODO_AGUARDANDO_RESET;
            xSemaphoreGive(mutexModo);

            xSemaphoreTake(semReset, portMAX_DELAY);

            xSemaphoreTake(mutexModo, portMAX_DELAY);
            modoAtual = MODO_NORMAL;
            causaFalha[0] = '\0';
            ultimaLeituraSensorTick = xTaskGetTickCount();
            xSemaphoreGive(mutexModo);

            LOG("[Tick %lu] [SUPERVISAO] [RESET] reset confirmado - sistema retornou ao modo NORMAL",
                (unsigned long)xTaskGetTickCount());
        }
    }
}