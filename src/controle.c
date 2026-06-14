#include <string.h>
#include "controle.h"
#include "logger.h"
#include "configuracao.h"
#include "semphr.h"
#include "supervisao.h"

QueueHandle_t filaComandos = NULL;
extern QueueHandle_t filaLeituras; 
extern SemaphoreHandle_t mutexModo;
extern ModoSistema modoAtual;

void controle_inicializar(void)
{
    filaComandos = xQueueCreate(10, sizeof(ComandoAtuador));
}

void vTaskControle(void *pvParameters)
{
    LeituraSensor   leitura;
    ComandoAtuador  cmd;
    float nivel   = 0.0f;
    float temp    = 0.0f;
    float pressao = 0.0f;
    uint8_t recebidos = 0;
    (void) pvParameters;

    for(;;)
    {
        if(xQueueReceive(filaLeituras, &leitura, portMAX_DELAY) != pdTRUE)
            continue;

        if(!leitura.valido)
            continue;

        switch(leitura.tipo)
        {
            case SENSOR_NIVEL:       nivel   = leitura.valor; recebidos |= 0x01; break;
            case SENSOR_TEMPERATURA: temp    = leitura.valor; recebidos |= 0x02; break;
            case SENSOR_PRESSAO:     pressao = leitura.valor; recebidos |= 0x04; break;
        }

        if(recebidos != 0x07)
            continue;
        recebidos = 0;

        // Verifica modo atual 
        ModoSistema modo;
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        modo = modoAtual;
        xSemaphoreGive(mutexModo);

        if(modo == MODO_SEGURO || modo == MODO_AGUARDANDO_RESET)
            continue;

       
        memset(&cmd, 0, sizeof(cmd));

        if(nivel < NIVEL_MIN)
        {
            cmd.bombaLigada = 1;
            strncpy(cmd.motivo, "nivel baixo", sizeof(cmd.motivo) - 1);
        }
        else if(nivel >= NIVEL_ALERTA)
        {
            cmd.bombaLigada = 0;
            strncpy(cmd.motivo, "nivel alto", sizeof(cmd.motivo) - 1);
        }
        else
        {
            cmd.bombaLigada = (nivel < 40.0f) ? 1 : 0;
            strncpy(cmd.motivo, "controle normal", sizeof(cmd.motivo) - 1);
        }


        cmd.aquecedorLigado = (temp < 28.0f) ? 1 : 0;
        if(temp >= TEMP_ALERTA)
            cmd.aquecedorLigado = 0;

     
        if(pressao >= PRESSAO_ALERTA)
        {
            cmd.valvulaAberta = 1;
            cmd.alarmeLigado  = (pressao >= PRESSAO_CRITICA) ? 1 : 0;
        }

        // Atualiza modo NORMAL / ALERTA
        ModoSistema novoModo = MODO_NORMAL;
        if(nivel >= NIVEL_ALERTA || temp >= TEMP_ALERTA || pressao >= PRESSAO_ALERTA)
        {
            novoModo = MODO_ALERTA;
        }

        xSemaphoreTake(mutexModo, portMAX_DELAY);
        if(modoAtual == MODO_NORMAL || modoAtual == MODO_ALERTA)
            modoAtual = novoModo;
        xSemaphoreGive(mutexModo);

        xQueueSend(filaComandos, &cmd, 0);

        LOG("[Tick %lu] [CONTROLE] [DECISAO] bomba=%s aquecedor=%s valvula=%s alarme=%s",
            (unsigned long)leitura.tick,
            cmd.bombaLigada     ? "ON" : "OFF",
            cmd.aquecedorLigado ? "ON" : "OFF",
            cmd.valvulaAberta   ? "ABERTA" : "FECHADA",
            cmd.alarmeLigado    ? "ON" : "OFF");
    }
}