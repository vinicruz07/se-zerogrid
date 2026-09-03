# Implementação do Projeto SE-ZeroGrid (Simulador C, Sistema Especialista Python e Dashboard Node/EJS)

Este documento descreve a arquitetura e o plano de implementação completo para o projeto **se-zerogrid**, cujo objetivo é manter um sistema fotovoltaico com inversores legados (sem algoritmo nativo de zero grid) operando em **Zero Grid**, chaveando dinamicamente strings fotovoltaicas através de um **Sistema Especialista**.

---

## 1. Arquitetura do Sistema

O sistema será composto por 3 módulos principais comunicando-se localmente via Sockets TCP e WebSockets:

```
+-------------------------------------------------------------------------------+
|                             SIMULADOR FÍSICO EM C                             |
|  - 4 Strings FV com acionamento independente (ON/OFF)                         |
|  - Carga local simulada (P_load) e Irradiação Solar ajustáveis                |
|  - Medidor de Ponto de Conexão com a Rede (PAC): P_grid = P_pv - P_load       |
|  - Servidor TCP Winsock (porta 9001): broadcast de telemetria & cmd parser    |
+------------------------------------+------------------------------------------+
                                     | TCP (Porta 9001)
                  +------------------+------------------+
                  |                                     |
                  v                                     v
+-----------------------------------+ +-----------------------------------------+
|    SISTEMA ESPECIALISTA (PYTHON)   | |      DASHBOARD INTERATIVO (NODE.JS)     |
| - Conexão TCP com o simulador C   | | - Express + EJS (SSR com layout rico)   |
| - Motor de Regras e Inferência:   | | - Cliente TCP conectado ao Simulador C  |
|   * Regra 1: Corte por injeção    | | - Servidor WebSocket para o frontend    |
|   * Regra 2: Reconexão segura     | | - Gráficos Chart.js em tempo real       |
|   * Histerese & Lockout anti-chatt| | - Controles de carga, sol e strings     |
+-----------------------------------+ +-----------------------------------------+
```

---

## 2. Componentes e Alterações Propostas

### 2.1. Simulador Físico em C (`simulator/`)

O simulador em C modelará o comportamento elétrico discreto ($\Delta t = 500\text{ ms}$) e disponibilizará um servidor TCP Winsock nativo em Windows.

#### [NEW] [pv_system.h](file:///c:/Users/Rambor/se-zerogrid/simulator/include/pv_system.h)
- Estrutura `StringFV`: `id`, `potencia_nominal` (ex: 2500W por string, 10kW total), `potencia_atual`, `estado` (1 = ON, 0 = OFF).
- Estrutura `MedidoresPAC`:
  - `p_load`: Potência da carga consumidora (W)
  - `p_pv`: Potência solar total gerada pelas strings ativas (W)
  - `p_grid`: Potência na rede ($P_{grid} = P_{pv} - P_{load}$).
    - Se $P_{grid} > 0$: Excedente/Injeção (violação do zero grid).
    - Se $P_{grid} < 0$: Importação da rede pública.
    - Se $P_{grid} == 0$: Ponto ideal zero-grid.
- Protótipos de cálculo de balanço, chaveamento e perturbação de carga/sol.

#### [NEW] [ipc_socket.h](file:///c:/Users/Rambor/se-zerogrid/simulator/include/ipc_socket.h) & [ipc_socket.c](file:///c:/Users/Rambor/se-zerogrid/simulator/src/ipc_socket.c)
- Servidor TCP non-blocking utilizando `winsock2.h` na porta `9001`.
- Suporte a múltiplos clientes conectados simultaneamente (Node.js e Python).
- Função para transmitir broadcast de telemetria em formato JSON:
  `{"timestamp": 1234, "p_load": 4000, "p_pv": 5000, "p_grid": 1000, "irradiance": 1000, "strings": [1, 1, 0, 0]}`
- Função para interpretar comandos de entrada:
  - `SET_STRING <id> <0|1>`
  - `SET_LOAD <watts>`
  - `SET_IRRADIANCE <w_m2>`

#### [NEW] [pv_system.c](file:///c:/Users/Rambor/se-zerogrid/simulator/src/pv_system.c)
- Implementação física dos cálculos de geração de cada string baseada na irradiação solar e status.
- Manutenção do histórico e transições de estado.

#### [MODIFY] [main.c](file:///c:/Users/Rambor/se-zerogrid/simulator/src/main.c)
- Ponto de entrada do simulador: inicialização do Winsock, configuração de 4 strings de 2.5 kW, carga inicial (3.0 kW).
- Loop temporal periódico ($\Delta t = 500\text{ ms}$):
  1. Processa comandos de rede recebidos dos clientes.
  2. Atualiza estado elétrico dos sensores e medidor PAC.
  3. Envia telemetria via TCP.
  4. Exibe logs formatados no terminal.

#### [NEW] [Makefile](file:///c:/Users/Rambor/se-zerogrid/simulator/Makefile)
- Script de compilação com `gcc -O2 src/*.c -Iinclude -lws2_32 -o simulator.exe`.

---

### 2.2. Sistema Especialista em Python (`expert_system/`)

O sistema especialista atuará como cérebro de controle em malha fechada para garantir que $P_{grid} \le 0$ sempre.

#### [NEW] [config.py](file:///c:/Users/Rambor/se-zerogrid/expert_system/config.py)
- Configurações de conexão: host `127.0.0.1`, porta `9001`.
- Parâmetros das regras:
  - `GRID_TOLERANCE_MARGIN`: 50 W (margem de tolerância antes de considerar injeção).
  - `HYSTERESIS_MARGIN`: 200 W (margem para evitar ligar uma string que causaria injeção imediata).
  - `SWITCHING_LOCKOUT_SECS`: 2.0 s (tempo mínimo entre chaveamentos da mesma string para evitar *chattering*).

#### [NEW] [rules.py](file:///c:/Users/Rambor/se-zerogrid/expert_system/core/rules.py)
- **Regra 1 (Proteção contra Injeção - Corte)**:
  - Condição: $P_{grid} > \text{GRID\_TOLERANCE\_MARGIN}$.
  - Ação: Encontrar a menor string ativa cuja desativação reduza a injeção ou desligar a primeira string ativa disponível.
- **Regra 2 (Aproveitamento Solar - Reconexão Segura)**:
  - Condição: $P_{grid} < 0$ (importando energia) E $|P_{grid}| > (\text{Potência Estimada da String Candidata} + \text{HYSTERESIS\_MARGIN})$.
  - Ação: Religar a string com segurança sem correr risco de ultrapassar zero grid.
- **Regra 3 (Estabilidade Temporal)**:
  - Respeitar o tempo de lockout de cada string antes de emitir nova comutação.

#### [NEW] [engine.py](file:///c:/Users/Rambor/se-zerogrid/expert_system/core/engine.py)
- Classe `InferenceEngine` que recebe a telemetria, atualiza o grafo de fatos do sistema especialista e avalia o conjunto de regras.

#### [NEW] [main.py](file:///c:/Users/Rambor/se-zerogrid/expert_system/main.py)
- Conecta ao simulador C, recebe o stream JSON, executa o ciclo de inferência a cada leitura e envia os comandos de atuação de volta ao simulador com logs coloridos e explicativos de cada decisão tomada.

---

### 2.3. Dashboard Web em Node.js com Express e EJS (`dashboard/`)

#### [NEW] [package.json](file:///c:/Users/Rambor/se-zerogrid/dashboard/package.json)
- Dependências: `express`, `ejs`, `ws`.

#### [NEW] [server.js](file:///c:/Users/Rambor/se-zerogrid/dashboard/server.js)
- Servidor Express na porta `3000`.
- Cliente TCP conectado ao simulador C na porta `9001`.
- Servidor WebSocket que retransmite dados instantâneos para os navegadores conectados e recebe comandos do dashboard para repassar ao simulador C.
- Rotas HTTP para renderização da página principal e APIs REST de comando.

#### [NEW] [views/index.ejs](file:///c:/Users/Rambor/se-zerogrid/dashboard/views/index.ejs)
- Interface rica e moderna (Dark Mode premium com glassmorphism):
  - **Banner de Status Zero-Grid**: Indicador visual dinâmico (Verde quando em conformidade com $P_{grid} \le 0$, Vermelho pulsante quando houver injeção $P_{grid} > 0$).
  - **Cartões de Telemetria PAC**:
    - Potência Solar Gerada ($P_{pv}$)
    - Demanda da Carga ($P_{load}$)
    - Balanço da Rede ($P_{grid}$) com indicação de importação / injeção
  - **Gráfico Temporal em Tempo Real**: Curvas interativas com Chart.js exibindo Solar, Carga e Rede.
  - **Arranjo Fotovoltaico (Strings)**: Card individual para cada uma das 4 strings, exibindo potência individual, status (Ligada/Desligada) e interruptor manual.
  - **Painel de Controle de Simulação**:
    - Controles interativos para variar a carga local ($P_{load}$) e a irradiação solar.
    - Chave para ativar/desativar o Sistema Especialista Automático ou operar em modo manual para testes.
    - Console de logs em tempo real recebendo justificativas da IA.

#### [NEW] [public/css/style.css](file:///c:/Users/Rambor/se-zerogrid/dashboard/public/css/style.css) & [public/js/app.js](file:///c:/Users/Rambor/se-zerogrid/dashboard/public/js/app.js)
- Estilos e interatividade WebSocket + Chart.js.

---

### 2.4. Orquestração e Execução dos 3 Serviços

#### [NEW] [scripts/run_all.ps1](file:///c:/Users/Rambor/se-zerogrid/scripts/run_all.ps1) & [scripts/run_local.sh](file:///c:/Users/Rambor/se-zerogrid/scripts/run_local.sh)
- Script de automação que:
  1. Compila o simulador C via `gcc`.
  2. Instala dependências do Node.js (`npm install`).
  3. Inicia o Simulador C (`simulator.exe`).
  4. Inicia o Dashboard Node.js (`server.js`).
  5. Inicia o Sistema Especialista Python (`main.py`).
  6. Abre o navegador na porta `3000`.

---

## 3. Plano de Verificação

### Testes e Validação
1. **Compilação e Execução do C**:
   - Compilar o simulador com `gcc` e `-lws2_32`.
   - Executar e verificar abertura da porta TCP `9001` e emissão periódica do pacote JSON.
2. **Validação do Sistema Especialista Python**:
   - Rodar o script Python e verificar recepção do fluxo JSON.
   - Forçar aumento da geração ou queda da carga para disparar a Regra de Corte de String.
   - Observar se o comando é enviado ao C e se $P_{grid}$ retorna para $\le 0$.
   - Aumentar a carga e observar se a Regra de Reconexão é acionada respeitando a histerese.
3. **Validação do Dashboard Web**:
   - Subir o servidor Node.js com Express e EJS.
   - Acessar `http://localhost:3000` via subagente de browser para verificar:
     - Renderização perfeita do layout EJS.
     - Atualização dos gráficos em tempo real via WebSocket.
     - Interatividade dos botões de controle de carga e status das strings.
4. **Captura Visual**:
   - Registrar screenshot do Dashboard operando com o sistema em equilíbrio zero-grid.
