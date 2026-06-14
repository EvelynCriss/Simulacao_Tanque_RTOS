#ifndef CONTROLE_H
#define CONTROLE_H

#include "sensores.h"

// Comando para os atuadores 
typedef struct {
    uint8_t bombaLigada;
    uint8_t aquecedorLigado;
    uint8_t valvulaAberta;
    uint8_t alarmeLigado;
    char    motivo[50];
} ComandoAtuador;

extern QueueHandle_t filaComandos;


void controle_inicializar(void);
void vTaskControle(void *pvParameters);

#endif /* CONTROLE_H */