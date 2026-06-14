#ifndef CONFIGURACAO_H
#define CONFIGURACAO_H


 // CONFIGURAÇÃO DO SISTEMA

/* Prioridades das tasks */
#define PRIORIDADE_BAIXA    1
#define PRIORIDADE_MEDIA    2
#define PRIORIDADE_ALTA     3

/* Tamanho da stack das tasks */
#define TASK_STACK_SIZE     256

/* Limites operacionais
 * Nível   : 0-100 %       | Alerta acima de 80, crítico acima de 95
 * Temp    : graus Celsius  | Alerta acima de 70, crítico acima de 90
 * Pressão : bar            | Alerta acima de 3.0, crítico acima de 5.0
 * Os valores foram escolhidos para refletir uma operação industrial típica:
 * há margem entre alerta e crítico para que o controle preventivo atue antes
 * de o sistema precisar entrar em modo seguro.
 */
#define NIVEL_MIN           20.0f
#define NIVEL_ALERTA        80.0f
#define NIVEL_CRITICO       95.0f

#define TEMP_MIN            15.0f
#define TEMP_ALERTA         70.0f
#define TEMP_CRITICA        90.0f

#define PRESSAO_ALERTA      3.0f
#define PRESSAO_CRITICA     5.0f

#define SENSOR_TIMEOUT_TICKS    pdMS_TO_TICKS(5000)

#define EMERGENCIA_APOS_MS      10000

#define LOG_MAX             200

#endif /* CONFIGURACAO_H */