
# SE-ZeroGrid - Retrofit Inteligente com Sistema Especialista

Sistema para manter sistemas fotovoltaicos com inversores antigos (sem suporte nativo a injeção zero) operando em **Zero Grid**, através do chaveamento inteligente e dinâmico de strings fotovoltaicas por um **Sistema Especialista**.

---

## Arquitetura do Sistema

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

## Regras do Sistema Especialista

1. **Regra 1 (Proteção contra Injeção - Corte de Strings):**
   - **Condição:** $P_{grid} > 30\text{ W}$ (geração solar supera o consumo e está injetando energia na rede).
   - **Ação:** Desconecta uma string ativa que respeite o lockout temporal para anular a injeção.
2. **Regra 2 (Aproveitamento Solar - Reconexão Segura):**
   - **Condição:** $P_{grid} < 0$ (importando energia) E $|P_{grid}| > P_{string\_candidata} + \text{Histerese}$ (margem de segurança de 250 W).
   - **Ação:** Reconecta a string com segurança sem causar violação do zero grid.
3. **Regra 3 (Estabilidade Temporal e Lockout):**
   - Respeita um tempo mínimo de 2 segundos entre comutações consecutivas de uma mesma string, evitando desgaste de contatores e oscilações (*chattering*).

---

## Pré-requisitos do Sistema

O ecossistema SE-ZeroGrid depende de três ambientes:
- **C (Compilador GCC / MinGW)**: para o simulador físico em tempo real.
- **Node.js & npm (v18+)**: para o servidor Express e dashboard interativo.
- **Python 3 (3.10+)**: para o motor de inferência e regras do sistema especialista.

> [!TIP]
> **Ainda não possui as linguagens instaladas no seu PC?**  
> Execute primeiro o script de instalação automática. Ele verifica o que está faltando na máquina e realiza a instalação sem você precisar configurar nada manualmente:
> ```powershell
> .\scripts\install_prerequisites.ps1
> ```
> *(Também acessível pelo atalho `.\scripts\install_all.ps1` no Windows ou `./scripts/install_prerequisites.sh` no Linux/macOS).*

---

## Como Executar

### Opção 1: Script Automático (Recomendado - Windows)

Após garantir os pré-requisitos instalados:

```powershell
# Para iniciar todos os 3 serviços integrados
.\scripts\run_all.ps1

# Para encerrar todos os serviços
.\scripts\stop_all.ps1
```

Acesse o painel em seu navegador: **`http://localhost:3000`**


### Opção 2: Execução Manual dos 3 Serviços

1. **Simulador em C:**
   ```powershell
   cd simulator
   gcc -Wall -O2 -Iinclude src/main.c src/pv_system.c src/ipc_socket.c -lws2_32 -o simulator.exe
   .\simulator.exe
   ```

2. **Dashboard em Node.js:**
   ```powershell
   cd dashboard
   npm install
   node server.js
   ```
   Acesse no navegador: **`http://localhost:3000`**

3. **Sistema Especialista em Python:**
   ```powershell
   cd expert_system
   python main.py
   ```
