#include "atuadores.h"
#include "logger.h"

void atuadores_inicializar(void)
{
    
}

void vTaskAtuadores(void *pvParameters)
{
    ComandoAtuador cmd;
    (void) pvParameters;

    for(;;)
    {
        if(xQueueReceive(filaComandos, &cmd, portMAX_DELAY) != pdTRUE)
            continue;

        LOG("[Tick %lu] [ATUADOR] [COMANDO] bomba=%s aquecedor=%s valvula=%s alarme=%s motivo=%s",
            (unsigned long)xTaskGetTickCount(),
            cmd.bombaLigada     ? "ON" : "OFF",
            cmd.aquecedorLigado ? "ON" : "OFF",
            cmd.valvulaAberta   ? "ABERTA" : "FECHADA",
            cmd.alarmeLigado    ? "ON" : "OFF",
            cmd.motivo[0] != '\0' ? cmd.motivo : "-");
    }
}