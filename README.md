# Sprint 4 - Simulação de Tanque Industrial com FreeRTOS

## Grupo 3

### Visão Geral

Este projeto implementa a simulação de um tanque industrial crítico utilizando FreeRTOS, demonstrando conceitos de concorrência, sincronização, comunicação entre tarefas, regiões críticas, eventos assíncronos, tratamento de falhas, máquina de estados e modo seguro.

A implementação foi modularizada em múltiplos arquivos para facilitar manutenção, organização e reutilização dos componentes.

---


## Organização dos Arquivos

| Arquivo | Função |
|----------|---------|
| `exercicio_06_split4.c` | Integra o projeto ao menu de exercícios da aplicação e chama a função principal da simulação |
| `simulacao_main.c` | Inicializa os módulos, cria mutexes, semáforos, filas e tasks |
| `sensores.c` / `sensores.h` | Simulação do tanque e task de sensores |
| `controle.c` / `controle.h` | Lógica de controle e geração de comandos |
| `atuadores.c` / `atuadores.h` | Recebimento dos comandos e registro da atuação |
| `supervisao.c` / `supervisao.h` | Máquina de estados, emergência, timeout e reset |
| `logger.c` / `logger.h` | Sistema de logs |
| `configuracao.h` | Constantes, limites, tempos e prioridades |
| `main.h` | Declaração da função principal |
|`exercicio_06_split4_codigo_unificado` | Código unificado, caso precise |

---

## 1. Tasks Existentes e Responsabilidades

| Task | Responsabilidade |
|------|------------------|
| **Sensores** | Atualiza a simulação do tanque e envia leituras de nível, temperatura e pressão para a fila de leituras |
| **Controle** | Recebe leituras dos sensores, aplica a lógica de controle e envia comandos para os atuadores |
| **Atuadores** | Recebe comandos e registra no log a atuação da bomba, aquecedor, válvula e alarme |
| **Supervisão** | Monitora emergências e timeout dos sensores, ativa modo seguro e controla a recuperação do sistema |
| **Emergência** | Simula o acionamento de um botão de emergência externo após 10 segundos |
| **Reset** | Simula a confirmação do operador para retorno à operação normal |
| **Logger** | Recebe mensagens de log e imprime no console |

---

## 2. Tasks Periódicas e Tasks Bloqueadas

| Task | Tipo | Mecanismo |
|------|------|-----------|
| Sensores | Periódica | `vTaskDelayUntil()` a cada 1 segundo |
| Controle | Bloqueada | `xQueueReceive()` aguardando leituras |
| Atuadores | Bloqueada | `xQueueReceive()` aguardando comandos |
| Supervisão | Periódica e Bloqueada | `vTaskDelayUntil()` para monitoramento e `xSemaphoreTake()` ao aguardar reset |
| Emergência | Bloqueada | `vTaskDelay()` por 10 segundos e depois encerra |
| Reset | Bloqueada | Verifica o modo do sistema, usa `vTaskDelay()` e libera o reset quando necessário |
| Logger | Bloqueada | `xQueueReceive()` aguardando mensagens |

---

## 3. Filas Utilizadas

| Fila | Produtor | Consumidor | Dados Trafegados |
|------|----------|------------|------------------|
| `filaLeituras` | Sensores | Controle | `LeituraSensor` |
| `filaComandos` | Controle e Supervisão | Atuadores | `ComandoAtuador` |
| `filaLogs` | Todas as tasks | Logger | `char[200]` |

---

## 4. Evento Assíncrono Implementado

### Botão de Emergência Externo

O evento assíncrono implementado é o acionamento de um botão de emergência.

Funcionamento:

1. A task Emergência aguarda 10 segundos.
2. Libera o semáforo `semEmergencia`.
3. A task Supervisão detecta o evento.
4. O sistema entra em `MODO_SEGURO`.
5. Um comando de proteção é enviado para a fila dos atuadores.

---

## 5. Região Crítica do Sistema

### Estado do Tanque

Recurso compartilhado:

```c
EstadoTanque estadoTanque;
```

Protegido por:

```c
mutexTanque
```

No código atual, a task Sensores é responsável por atualizar o `estadoTanque`. O mutex foi utilizado para garantir acesso seguro ao recurso compartilhado e permitir expansão futura do sistema, caso outras tasks passem a acessar diretamente o estado do tanque.

### Estado Operacional do Sistema

Recursos compartilhados:

```c
modoAtual
causaFalha
ultimaLeituraSensorTick
```

Protegidos por:

```c
mutexModo
```

Essas variáveis representam o estado global da aplicação. O `modoAtual` é usado por Controle, Supervisão e Reset. A variável `ultimaLeituraSensorTick` é atualizada por Sensores e lida pela Supervisão. A variável `causaFalha` é atualizada principalmente pela Supervisão.

---

## 6. Mecanismos de Proteção Utilizados

### Mutexes

| Mutex | Função |
|---------|---------|
| `mutexTanque` | Protege o estado do tanque |
| `mutexModo` | Protege o estado operacional do sistema |

### Semáforos Binários

| Semáforo | Função |
|------------|---------|
| `semEmergencia` | Sinaliza ocorrência de emergência |
| `semReset` | Sinaliza confirmação de reset |

### Filas

As filas são utilizadas para comunicação segura entre tasks, evitando que as tasks acessem diretamente os mesmos dados durante a troca de mensagens.

---

## 7. Possíveis Condições de Corrida e Como Foram Evitadas

### Estado do Tanque

Poderia ocorrer condição de corrida caso mais de uma task acessasse diretamente:

```c
estadoTanque
```

Para evitar esse problema, o acesso ao tanque é protegido por:

```c
mutexTanque
```

No código atual, a atualização ocorre na task Sensores, mas o uso do mutex torna o recurso seguro para acessos futuros.

### Estado do Sistema

Poderia ocorrer condição de corrida nas variáveis:

```c
modoAtual
causaFalha
ultimaLeituraSensorTick
```

Essas variáveis são compartilhadas entre diferentes tasks.

Para evitar leituras e escritas inconsistentes, foi utilizado:

```c
mutexModo
```

---

## 8. Máquina de Estados

O sistema possui quatro estados:

| Estado | Descrição |
|----------|-----------|
| `MODO_NORMAL` | Operação normal |
| `MODO_ALERTA` | Alguma variável ultrapassou limite de alerta |
| `MODO_SEGURO` | Emergência ou falha detectada |
| `MODO_AGUARDANDO_RESET` | Sistema aguardando confirmação do operador |

Fluxo de estados:

```text
MODO_NORMAL
     ↕
MODO_ALERTA
     ↓
MODO_SEGURO
     ↓
MODO_AGUARDANDO_RESET
     ↓
MODO_NORMAL
```

A task Controle alterna entre `MODO_NORMAL` e `MODO_ALERTA`, de acordo com os limites operacionais. A task Supervisão é responsável por colocar o sistema em `MODO_SEGURO` em caso de emergência ou timeout dos sensores. Após isso, o sistema entra em `MODO_AGUARDANDO_RESET` e só retorna ao `MODO_NORMAL` após confirmação da task Reset.

---

## 9. Limites de Alerta e Críticos

### Nível

| Limite | Valor |
|---------|---------|
| Mínimo | 20% |
| Alerta | 80% |
| Crítico | 95% |

### Temperatura

| Limite | Valor |
|---------|---------|
| Mínimo | 15°C |
| Alerta | 70°C |
| Crítico | 90°C |

### Pressão

| Limite | Valor |
|---------|---------|
| Alerta | 3 bar |
| Crítico | 5 bar |

### Timeout dos Sensores

| Parâmetro | Valor |
|------------|---------|
| `SENSOR_TIMEOUT_TICKS` | 5 segundos |

---

## 10. Ativação do Modo Seguro e Retorno Controlado

### Ativação do Modo Seguro

O modo seguro é ativado quando ocorre:

- Emergência externa.
- Timeout de sensor superior a 5 segundos.

Ao entrar em modo seguro é enviado um comando contendo:

- Bomba desligada.
- Aquecedor desligado.
- Válvula aberta.
- Alarme ligado.

### Retorno Controlado

Fluxo:

1. Supervisão detecta a falha.
2. Sistema entra em `MODO_SEGURO`.
3. Supervisão muda o estado para `MODO_AGUARDANDO_RESET`.
4. A task Reset detecta esse estado.
5. A task Reset aguarda 5 segundos, simulando a verificação do operador.
6. A task Reset libera `semReset`.
7. Supervisão recebe o reset e retorna para `MODO_NORMAL`.

---

## 11. Prioridades das Tasks

| Task | Prioridade |
|--------|-----------|
| Controle | Alta (3) |
| Supervisão | Alta (3) |
| Sensores | Média (2) |
| Atuadores | Média (2) |
| Emergência | Média (2) |
| Reset | Média (2) |
| Logger | Baixa (1) |

### Justificativa

As tasks Controle e Supervisão possuem prioridade alta porque são responsáveis pelas decisões operacionais e pela segurança do sistema.

Sensores, Atuadores, Emergência e Reset possuem prioridade média porque participam diretamente da operação do processo, mas não executam a lógica principal de decisão.

Logger possui prioridade baixa para que a impressão de mensagens no console não interfira nas tasks críticas do sistema.

---

## Como Compilar e Executar

1. Adicione os arquivos `.c` e `.h` ao projeto FreeRTOS.
2. Certifique-se de que os includes do FreeRTOS estejam configurados corretamente.
3. Compile o projeto normalmente no ambiente utilizado.
4. Execute a função principal da simulação:

```c
exercicio_06_split4_main();
```

---

## Como Verificar o Funcionamento

Durante a execução, o terminal deve exibir logs semelhantes a:

```text
Projeto - Simulacao de tanque industrial
Sprint 4 - Concorrencia, Sincronizacao, Eventos e Modo Seguro
[Tick 1] [CONTROLE] [DECISAO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF
[Tick 1] [ATUADOR] [COMANDO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF motivo=-
[Tick 1] [SENSORES] [LEITURA] nivel=50.7 temp=25.3 pressao=2.27
[Tick 1001] [CONTROLE] [DECISAO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF
[Tick 1001] [ATUADOR] [COMANDO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF motivo=-
[Tick 1001] [SENSORES] [LEITURA] nivel=51.4 temp=25.6 pressao=2.28
[Tick 2001] [CONTROLE] [DECISAO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF
[Tick 2001] [ATUADOR] [COMANDO] bomba=OFF aquecedor=ON valvula=FECHADA alarme=OFF motivo=-
[Tick 2001] [SENSORES] [LEITURA] nivel=52.1 temp=25.9 pressao=2.30
[Tick 3001] [SUPERVISAO] [FALHA_SENSOR] timeout - sem leituras
[Tick 3001] [SUPERVISAO] [MODO_SEGURO] causa=falha de sensor - timeout
[Tick 3001] [SUPERVISAO] [AGUARDANDO_RESET] sistema bloqueado aguardando reset
[Tick 3001] [ATUADOR] [COMANDO] bomba=OFF aquecedor=OFF valvula=ABERTA alarme=ON motivo=MODO_SEGURO
```

Esses logs comprovam:

- leitura periódica dos sensores;
- comunicação por filas;
- decisão da task Controle;
- atuação simulada;
- evento assíncrono de emergência;
- entrada em modo seguro;
- retorno controlado após reset.

---

## Resultado Esperado

O projeto deve demonstrar o funcionamento integrado de:

- tasks periódicas;
- tasks bloqueadas;
- filas de comunicação;
- mutexes;
- semáforos binários;
- tratamento de eventos assíncronos;
- proteção contra condição de corrida;
- máquina de estados;
- modo seguro;
- recuperação controlada do sistema.
