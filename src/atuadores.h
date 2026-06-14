#ifndef ATUADORES_H
#define ATUADORES_H

#include "controle.h"

/* Funções públicas */
void atuadores_inicializar(void);
void vTaskAtuadores(void *pvParameters);

#endif /* ATUADORES_H */