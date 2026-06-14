#include "main.h"
#include "sensores.h"
#include "controle.h"
#include "atuadores.h"
#include "supervisao.h"
#include "logger.h"
#include "configuracao.h"

// Mutex do tanque (acessado por sensores.c e supervisao.c)
SemaphoreHandle_t mutexTanque = NULL;

/* Helper para criar tasks */
static BaseType_t criarTask(TaskFunction_t fn, const char *nome, UBaseType_t prio)
{
    BaseType_t r = xTaskCreate(fn, nome, TASK_STACK_SIZE, NULL, prio, NULL);
    if(r != pdPASS)
        printf("Erro ao criar task %s\n", nome);
    return r;
}

// Task de emergência simulada
static void vTaskEmergencia(void *pvParameters)
{
    (void) pvParameters;
    vTaskDelay(pdMS_TO_TICKS(EMERGENCIA_APOS_MS));
    LOG("[Tick %lu] [EMERGENCIA] botao de emergencia pressionado",
        (unsigned long)xTaskGetTickCount());
    xSemaphoreGive(semEmergencia);
    vTaskDelete(NULL);
}

// Task de reset simulada
static void vTaskReset(void *pvParameters)
{
    (void) pvParameters;
    for(;;)
    {
        ModoSistema modo;
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        if(modo == MODO_AGUARDANDO_RESET)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            LOG("[Tick %lu] [RESET] operador confirmou reset - liberando sistema",
                (unsigned long)xTaskGetTickCount());
            xSemaphoreGive(semReset);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

int simulacao_tanque_grupo3_main(void)
{
    printf("Projeto - Simulacao de tanque industrial\n");
    printf("Sprint 4 - Concorrencia, Sincronizacao, Eventos e Modo Seguro\n");

    /* Inicializa componentes */
    sensores_inicializar();
    controle_inicializar();
    atuadores_inicializar();
    supervisao_inicializar();
    logger_inicializar();

    // Cria mutex do tanque
    mutexTanque = xSemaphoreCreateMutex();

    // Verifica criação dos recursos 
    if(mutexTanque    == NULL || mutexModo      == NULL ||
       semEmergencia  == NULL || semReset       == NULL ||
       filaLeituras   == NULL || filaComandos   == NULL ||
       filaLogs       == NULL)
    {
        printf("Erro ao criar recursos do FreeRTOS\n");
        return 1;
    }

    // Cria tasks
    if(criarTask(vTaskSensores,   "Sensores",   PRIORIDADE_MEDIA) != pdPASS) return 1;
    if(criarTask(vTaskControle,   "Controle",   PRIORIDADE_ALTA)  != pdPASS) return 1;
    if(criarTask(vTaskAtuadores,  "Atuadores",  PRIORIDADE_MEDIA) != pdPASS) return 1;
    if(criarTask(vTaskSupervisao, "Supervisao", PRIORIDADE_ALTA)  != pdPASS) return 1;
    if(criarTask(vTaskEmergencia, "Emergencia", PRIORIDADE_MEDIA) != pdPASS) return 1;
    if(criarTask(vTaskReset,      "Reset",      PRIORIDADE_MEDIA) != pdPASS) return 1;
    if(criarTask(vTaskLogger,     "Logger",     PRIORIDADE_BAIXA) != pdPASS) return 1;

    vTaskStartScheduler();

    printf("Erro ao iniciar scheduler\n");
    return 1;
}