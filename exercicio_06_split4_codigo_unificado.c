
 // Sprint 4 - Concorrência, Sincronização, Eventos e Modo Seguro
// Grupo 3
 

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

 //CONFIGURAÇÃO

#define TASK_STACK_SIZE     256

#define PRIORIDADE_BAIXA    1
#define PRIORIDADE_MEDIA    2
#define PRIORIDADE_ALTA     3

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

// Timeout de sensor: se nenhuma leitura chegar em X ticks, considera falha 
#define SENSOR_TIMEOUT_TICKS    pdMS_TO_TICKS(5000)

// Tempo até a emergência simulada disparar
#define EMERGENCIA_APOS_MS      10000

// Tempo até o operador confirmar o reset 
#define RESET_APOS_MS           8000

 // TIPOS E ENUMS

typedef enum {
    SENSOR_NIVEL,
    SENSOR_TEMPERATURA,
    SENSOR_PRESSAO
} TipoSensor;

typedef struct {
    TipoSensor  tipo;
    float       valor;
    TickType_t  tick;
    uint8_t     valido;
} LeituraSensor;

typedef struct {
    uint8_t bombaLigada;
    uint8_t aquecedorLigado;
    uint8_t valvulaAberta;
    uint8_t alarmeLigado;
    char    motivo[50];
} ComandoAtuador;

typedef enum {
    MODO_NORMAL,
    MODO_ALERTA,
    MODO_SEGURO,
    MODO_AGUARDANDO_RESET
} ModoSistema;


 // ESTADO DO TANQUE (recurso compartilhado — protegido por mutexTanque)

typedef struct {
    float nivel;
    float temperatura;
    float pressao;
    float vazaoEntrada;
    float vazaoSaida;
} EstadoTanque;

static EstadoTanque estadoTanque;


// MODO DO SISTEMA (recurso compartilhado — protegido por mutexModo)


static ModoSistema  modoAtual               = MODO_NORMAL;
static char         causaFalha[80]          = "";
static TickType_t   ultimaLeituraSensorTick = 0;


// HANDLES DE FILAS, SEMÁFOROS E MUTEX

static QueueHandle_t     filaLeituras   = NULL;
static QueueHandle_t     filaComandos   = NULL;
static QueueHandle_t     filaLogs       = NULL;

static SemaphoreHandle_t semEmergencia  = NULL;
static SemaphoreHandle_t semReset       = NULL;   /* NOVO: confirmação explícita de reset */
static SemaphoreHandle_t mutexTanque    = NULL;
static SemaphoreHandle_t mutexModo      = NULL;


// HELPER: envia string para a fila de logs (não bloqueia tarefas críticas)

#define LOG_MAX 200

static void enviarLog( const char *msg )
{
    char buf[LOG_MAX];
    strncpy( buf, msg, LOG_MAX - 1 );
    buf[LOG_MAX - 1] = '\0';
    // timeout 0: se a fila estiver cheia, descarta para não bloquear 
    xQueueSend( filaLogs, buf, 0 );
}

#define LOG(fmt, ...) \
    do { \
        char _buf[LOG_MAX]; \
        snprintf(_buf, LOG_MAX, fmt, ##__VA_ARGS__); \
        enviarLog(_buf); \
    } while(0)


 // SIMULAÇÃO DO TANQUE

static void atualizarSimulacao( EstadoTanque *t )
{
    t->nivel += t->vazaoEntrada - t->vazaoSaida;
    if( t->nivel > 100.0f ) t->nivel = 100.0f;
    if( t->nivel <   0.0f ) t->nivel =   0.0f;

    t->temperatura += 0.3f;
    if( t->temperatura > 100.0f ) t->temperatura = 100.0f;

    t->pressao = 1.0f + ( t->nivel * 0.02f ) + ( t->temperatura * 0.01f );
}

 //HELPER: cria task com verificação

static BaseType_t criarTask( TaskFunction_t fn,
                             const char    *nome,
                             UBaseType_t    prio )
{
    BaseType_t r = xTaskCreate( fn, nome, TASK_STACK_SIZE, NULL, prio, NULL );
    if( r != pdPASS )
        printf( "Erro ao criar task %s\n", nome );
    return r;
}


 // TASK SENSORES

static void vTaskSensores( void *pvParameters )
{
    TickType_t      xUltimaLiberacao = xTaskGetTickCount();
    const TickType_t periodo         = pdMS_TO_TICKS( 1000 );
    EstadoTanque    copia;
    LeituraSensor   leitura;

    ( void ) pvParameters;

    for( ;; )
    {
        // Região crítica curta: copia o estado e sai do mutex 
        xSemaphoreTake( mutexTanque, portMAX_DELAY );
        atualizarSimulacao( &estadoTanque );
        copia = estadoTanque;
        xSemaphoreGive( mutexTanque );

        TickType_t agora = xTaskGetTickCount();

        // Nível
        leitura.tipo   = SENSOR_NIVEL;
        leitura.valor  = copia.nivel;
        leitura.tick   = agora;
        leitura.valido = 1;
        xQueueSend( filaLeituras, &leitura, 0 );

        // Temperatura
        leitura.tipo   = SENSOR_TEMPERATURA;
        leitura.valor  = copia.temperatura;
        xQueueSend( filaLeituras, &leitura, 0 );

        // Pressão
        leitura.tipo   = SENSOR_PRESSAO;
        leitura.valor  = copia.pressao;
        xQueueSend( filaLeituras, &leitura, 0 );

        // Atualiza tick da última leitura — usado pela Supervisão para detectar timeout
        xSemaphoreTake( mutexModo, portMAX_DELAY );
        ultimaLeituraSensorTick = agora;
        xSemaphoreGive( mutexModo );

        LOG( "[Tick %lu] [SENSORES] [LEITURA] nivel=%.1f temp=%.1f pressao=%.2f",
             (unsigned long)agora, copia.nivel, copia.temperatura, copia.pressao );

        vTaskDelayUntil( &xUltimaLiberacao, periodo );
    }
}


 // TASK CONTROLE

static void vTaskControle( void *pvParameters )
{
    LeituraSensor   leitura;
    ComandoAtuador  cmd;


    float nivel   = 0.0f;
    float temp    = 0.0f;
    float pressao = 0.0f;
    uint8_t recebidos = 0;  

    ( void ) pvParameters;

    for( ;; )
    {
        if( xQueueReceive( filaLeituras, &leitura, portMAX_DELAY ) != pdTRUE )
            continue;

        if( !leitura.valido )
            continue;

        switch( leitura.tipo )
        {
            case SENSOR_NIVEL:       nivel   = leitura.valor; recebidos |= 0x01; break;
            case SENSOR_TEMPERATURA: temp    = leitura.valor; recebidos |= 0x02; break;
            case SENSOR_PRESSAO:     pressao = leitura.valor; recebidos |= 0x04; break;
        }

        // Processa somente quando tiver os três sensores do ciclo 
        if( recebidos != 0x07 )
            continue;
        recebidos = 0;

        // Verifica se está em modo seguro — se sim, não gera novos comandos normais 
        ModoSistema modo;
        xSemaphoreTake( mutexModo, portMAX_DELAY );
        modo = modoAtual;
        xSemaphoreGive( mutexModo );

        if( modo == MODO_SEGURO || modo == MODO_AGUARDANDO_RESET )
            continue;

        // Lógica de controle
        memset( &cmd, 0, sizeof(cmd) );

        // Bomba: liga se nível baixo, desliga se alto
        if( nivel < NIVEL_MIN )
        {
            cmd.bombaLigada = 1;
            strncpy( cmd.motivo, "nivel baixo", sizeof(cmd.motivo) - 1 );
        }
        else if( nivel >= NIVEL_ALERTA )
        {
            cmd.bombaLigada = 0;
            strncpy( cmd.motivo, "nivel alto", sizeof(cmd.motivo) - 1 );
        }
        else
        {
            cmd.bombaLigada = ( nivel < 40.0f ) ? 1 : 0;
            strncpy( cmd.motivo, "controle normal", sizeof(cmd.motivo) - 1 );
        }

        // Aquecedor: liga se frio, desliga se quente
        cmd.aquecedorLigado = ( temp < 28.0f ) ? 1 : 0;
        if( temp >= TEMP_ALERTA )
            cmd.aquecedorLigado = 0;

        // Válvula e alarme
        if( pressao >= PRESSAO_ALERTA )
        {
            cmd.valvulaAberta = 1;
            cmd.alarmeLigado  = ( pressao >= PRESSAO_CRITICA ) ? 1 : 0;
        }

        // Atualiza modo NORMAL / ALERTA 

        ModoSistema novoModo = MODO_NORMAL;
        if( nivel   >= NIVEL_ALERTA  ||
            temp    >= TEMP_ALERTA   ||
            pressao >= PRESSAO_ALERTA )
        {
            novoModo = MODO_ALERTA;
        }

        xSemaphoreTake( mutexModo, portMAX_DELAY );
        if( modoAtual == MODO_NORMAL || modoAtual == MODO_ALERTA )
            modoAtual = novoModo;
        xSemaphoreGive( mutexModo );

        xQueueSend( filaComandos, &cmd, 0 );

        LOG( "[Tick %lu] [CONTROLE] [DECISAO] bomba=%s aquecedor=%s valvula=%s alarme=%s",
             (unsigned long)leitura.tick,
             cmd.bombaLigada     ? "ON"     : "OFF",
             cmd.aquecedorLigado ? "ON"     : "OFF",
             cmd.valvulaAberta   ? "ABERTA" : "FECHADA",
             cmd.alarmeLigado    ? "ON"     : "OFF" );
    }
}

// TASK ATUADORES

static void vTaskAtuadores( void *pvParameters )
{
    ComandoAtuador cmd;

    ( void ) pvParameters;

    for( ;; )
    {
        if( xQueueReceive( filaComandos, &cmd, portMAX_DELAY ) != pdTRUE )
            continue;

        LOG( "[Tick %lu] [ATUADOR] [COMANDO] bomba=%s aquecedor=%s valvula=%s alarme=%s motivo=%s",
             (unsigned long)xTaskGetTickCount(),
             cmd.bombaLigada     ? "ON"     : "OFF",
             cmd.aquecedorLigado ? "ON"     : "OFF",
             cmd.valvulaAberta   ? "ABERTA" : "FECHADA",
             cmd.alarmeLigado    ? "ON"     : "OFF",
             cmd.motivo[0] != '\0' ? cmd.motivo : "-" );
    }
}

 // HELPER: ativa modo seguro (pode ser chamado de qualquer task)


static void ativarModoSeguro( const char *causa )
{
    ComandoAtuador cmdSeguro;
    memset( &cmdSeguro, 0, sizeof(cmdSeguro) );
    cmdSeguro.bombaLigada     = 0;
    cmdSeguro.aquecedorLigado = 0;
    cmdSeguro.valvulaAberta   = 1;
    cmdSeguro.alarmeLigado    = 1;
    strncpy( cmdSeguro.motivo, "MODO_SEGURO", sizeof(cmdSeguro.motivo) - 1 );

    xQueueSend( filaComandos, &cmdSeguro, 0 );

    xSemaphoreTake( mutexModo, portMAX_DELAY );
    modoAtual = MODO_SEGURO;
    strncpy( causaFalha, causa, sizeof(causaFalha) - 1 );
    xSemaphoreGive( mutexModo );

    LOG( "[Tick %lu] [SUPERVISAO] [MODO_SEGURO] causa=%s",
         (unsigned long)xTaskGetTickCount(), causa );
}


/* =========================================================
 * TASK SUPERVISAO
 * Fica bloqueada no semáforo de emergência.
 * Quando acorda: registra causa e ativa modo seguro.
 * Também monitora timeout de sensor via verificação periódica.
 *
 * Retorno ao MODO_NORMAL: bloqueado em semReset, aguardando
 * confirmação explícita da task vTaskReset (simula operador).
 * Não há mais retorno automático por tempo (vTaskDelay).
 * ========================================================= */

static void vTaskSupervisao( void *pvParameters )
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xCheckPeriod = pdMS_TO_TICKS(1000); // verifica a cada 1 segundo
    (void) pvParameters;

    for( ;; )
    {
        // Aguarda o próximo ciclo
        vTaskDelayUntil(&xLastWakeTime, xCheckPeriod);
        
        // 1. Verifica se há emergência pendente (não bloqueante)
        if( xSemaphoreTake(semEmergencia, 0) == pdTRUE )
        {
            LOG( "[Tick %lu] [SUPERVISAO] [EMERGENCIA] botao de emergencia acionado",
                 (unsigned long)xTaskGetTickCount() );
            ativarModoSeguro("emergencia externa");
        }
        
        // 2. Verifica timeout dos sensores
        TickType_t agora = xTaskGetTickCount();
        TickType_t ultimaTick;
        ModoSistema modo;
        
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        ultimaTick = ultimaLeituraSensorTick;
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        // Só verifica timeout se não estiver já em modo seguro/aguardando
        if( modo != MODO_SEGURO && modo != MODO_AGUARDANDO_RESET )
        {
            TickType_t diff = agora - ultimaTick;
            if( diff >= SENSOR_TIMEOUT_TICKS )
            {
                LOG( "[Tick %lu] [SUPERVISAO] [FALHA_SENSOR] timeout - ultima leitura ha %lu ticks (limite=%lu)",
                     (unsigned long)agora, (unsigned long)diff, (unsigned long)SENSOR_TIMEOUT_TICKS );
                ativarModoSeguro("falha de sensor - timeout");
            }
        }
        
        // 3. Verifica se está em MODO_SEGURO e precisa trocar para AGUARDANDO_RESET
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        if( modo == MODO_SEGURO )
        {
            LOG( "[Tick %lu] [SUPERVISAO] [AGUARDANDO_RESET] sistema bloqueado - aguardando confirmacao do operador",
                 (unsigned long)xTaskGetTickCount() );

            xSemaphoreTake(mutexModo, portMAX_DELAY);
            modoAtual = MODO_AGUARDANDO_RESET;
            xSemaphoreGive(mutexModo);

            // Bloqueia até o reset ser confirmado
            xSemaphoreTake(semReset, portMAX_DELAY);

            // Retorna ao modo normal
            xSemaphoreTake(mutexModo, portMAX_DELAY);
            modoAtual = MODO_NORMAL;
            causaFalha[0] = '\0';
            ultimaLeituraSensorTick = xTaskGetTickCount();
            xSemaphoreGive(mutexModo);

            LOG( "[Tick %lu] [SUPERVISAO] [RESET] reset confirmado - sistema retornou ao modo NORMAL",
                 (unsigned long)xTaskGetTickCount() );
        }
    }
}

// TASK EMERGENCIA
static void vTaskEmergencia( void *pvParameters )
{
    ( void ) pvParameters;

    vTaskDelay( pdMS_TO_TICKS(EMERGENCIA_APOS_MS) );

    LOG( "[Tick %lu] [EMERGENCIA] botao de emergencia pressionado",
         (unsigned long)xTaskGetTickCount() );

    // Libera o semáforo / acorda a task Supervisao 
    xSemaphoreGive( semEmergencia );

    // Task encerra após disparar uma vez 
    vTaskDelete( NULL );
}


 //TASK RESET
static void vTaskReset( void *pvParameters )
{
    (void) pvParameters;

    for( ;; )
    {
        ModoSistema modo;
        
        xSemaphoreTake(mutexModo, portMAX_DELAY);
        modo = modoAtual;
        xSemaphoreGive(mutexModo);
        
        if( modo == MODO_AGUARDANDO_RESET )
        {
            // Aguarda 5 segundos simulando o operador verificando o sistema
            vTaskDelay(pdMS_TO_TICKS(5000));
            
            LOG( "[Tick %lu] [RESET] operador confirmou reset - liberando sistema",
                 (unsigned long)xTaskGetTickCount() );
            
            xSemaphoreGive(semReset);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* 
 * TASK LOGGER
 * Consome filaLogs e imprime. Baixa prioridade para não
 * prejudicar tasks críticas.
  */

static void vTaskLogger( void *pvParameters )
{
    char buf[LOG_MAX];

    ( void ) pvParameters;

    for( ;; )
    {
        if( xQueueReceive( filaLogs, buf, portMAX_DELAY ) == pdTRUE )
        {
            printf( "%s\n", buf );
        }
    }
}


// MAIN


int exercicio_06_split4_main( void )
{
    printf( "Projeto - Simulacao de tanque industrial\n" );
    printf( "Sprint 4 - Concorrencia, Sincronizacao, Eventos e Modo Seguro\n" );

    // Estado inicial do tanque
    estadoTanque.nivel        = 50.0f;
    estadoTanque.temperatura  = 25.0f;
    estadoTanque.pressao      =  2.0f;
    estadoTanque.vazaoEntrada =  1.5f;
    estadoTanque.vazaoSaida   =  0.8f;

    // Criação dos mutex 
    mutexTanque = xSemaphoreCreateMutex();
    mutexModo   = xSemaphoreCreateMutex();

    // Criação dos semáforos binários 
    semEmergencia = xSemaphoreCreateBinary();
    semReset      = xSemaphoreCreateBinary();  

    // Criação das filas 
    filaLeituras = xQueueCreate( 10, sizeof(LeituraSensor)  );
    filaComandos = xQueueCreate( 10, sizeof(ComandoAtuador) );
    filaLogs     = xQueueCreate( 20, LOG_MAX                );

    if( mutexTanque    == NULL ||
        mutexModo      == NULL ||
        semEmergencia  == NULL ||
        semReset       == NULL ||   
        filaLeituras   == NULL ||
        filaComandos   == NULL ||
        filaLogs       == NULL )
    {
        printf( "Erro ao criar recursos do FreeRTOS\n" );
        return 1;
    }

    // Criação das tasks
    if( criarTask( vTaskSensores,   "Sensores",   PRIORIDADE_MEDIA ) != pdPASS ) return 1;
    if( criarTask( vTaskControle,   "Controle",   PRIORIDADE_ALTA  ) != pdPASS ) return 1;
    if( criarTask( vTaskAtuadores,  "Atuadores",  PRIORIDADE_MEDIA ) != pdPASS ) return 1;
    if( criarTask( vTaskSupervisao, "Supervisao", PRIORIDADE_ALTA  ) != pdPASS ) return 1;
    if( criarTask( vTaskEmergencia, "Emergencia", PRIORIDADE_MEDIA ) != pdPASS ) return 1;
    if( criarTask( vTaskReset,      "Reset",      PRIORIDADE_MEDIA ) != pdPASS ) return 1;  
    if( criarTask( vTaskLogger,     "Logger",     PRIORIDADE_BAIXA ) != pdPASS ) return 1;

    vTaskStartScheduler();

    printf( "Erro ao iniciar scheduler\n" );
    return 1;
}