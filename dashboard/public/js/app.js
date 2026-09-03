// SE-ZeroGrid - Frontend Application Logic
document.addEventListener('DOMContentLoaded', () => {
  // Elementos do DOM
  const textSimStatus = document.getElementById('text-sim-status');
  const badgeSim = document.getElementById('badge-sim');
  const zeroGridBanner = document.getElementById('zero-grid-banner');
  const statusBadgeText = document.getElementById('status-badge-text');
  const bannerDescription = document.getElementById('banner-description');
  const pacFlowValue = document.getElementById('pac-flow-value');
  const pacFlowDirection = document.getElementById('pac-flow-direction');

  const valPPv = document.getElementById('val-p-pv');
  const barPPv = document.getElementById('bar-p-pv');
  const activeStringsCount = document.getElementById('active-strings-count');

  const valPLoad = document.getElementById('val-p-load');
  const barPLoad = document.getElementById('bar-p-load');

  const valIrradiance = document.getElementById('val-irradiance');
  const barIrradiance = document.getElementById('bar-irradiance');
  const sunStatus = document.getElementById('sun-status');

  const stringsGridContainer = document.getElementById('strings-grid-container');
  const btnMasterMode = document.getElementById('btn-master-mode');
  const btnModeText = document.getElementById('btn-mode-text');
  const selectNumStrings = document.getElementById('select-num-strings');
  const badgeMode = document.getElementById('badge-mode');
  const textMode = document.getElementById('text-mode');

  const sliderLoad = document.getElementById('slider-load');
  const displayLoadVal = document.getElementById('display-load-val');
  const sliderIrradiance = document.getElementById('slider-irradiance');
  const displayIrradianceVal = document.getElementById('display-irradiance-val');
  const logStream = document.getElementById('log-stream');
  const btnClearLogs = document.getElementById('btn-clear-logs');

  let currentControlMode = 'AUTO';
  let previousStringStates = {};
  let previousZeroGridStatus = null;

  // 1. Configuração do Gráfico Chart.js em Tempo Real
  const ctx = document.getElementById('realtimeChart').getContext('2d');
  const maxDataPoints = 35;
  const chartLabels = [];
  const solarData = [];
  const loadData = [];
  const gridData = [];

  const realtimeChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: chartLabels,
      datasets: [
        {
          label: 'Geração Solar (W)',
          data: solarData,
          borderColor: '#f59e0b',
          backgroundColor: 'rgba(245, 158, 11, 0.12)',
          borderWidth: 2,
          fill: true,
          tension: 0.3,
          pointRadius: 0
        },
        {
          label: 'Demanda Carga (W)',
          data: loadData,
          borderColor: '#818cf8',
          backgroundColor: 'rgba(129, 140, 248, 0.1)',
          borderWidth: 2,
          fill: true,
          tension: 0.3,
          pointRadius: 0
        },
        {
          label: 'Rede / PAC (W)',
          data: gridData,
          borderColor: '#10b981',
          borderWidth: 2.5,
          fill: false,
          tension: 0.2,
          pointRadius: 0
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      animation: false,
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: 'rgba(18, 24, 38, 0.95)',
          titleColor: '#f8fafc',
          bodyColor: '#94a3b8',
          borderColor: 'rgba(255, 255, 255, 0.1)',
          borderWidth: 1,
          padding: 10
        }
      },
      scales: {
        x: {
          grid: { color: 'rgba(255, 255, 255, 0.04)' },
          ticks: { color: '#64748b', maxTicksLimit: 8 }
        },
        y: {
          grid: { color: 'rgba(255, 255, 255, 0.05)' },
          ticks: {
            color: '#94a3b8',
            callback: (v) => `${v} W`
          }
        }
      }
    }
  });

  // 2. Conexão WebSocket com o Backend Node.js
  let ws;
  function initWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${window.location.host}`);

    ws.onopen = () => {
      addLog('system', 'Conectado ao servidor de telemetria via WebSocket.');
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        if (msg.type === 'sim_status') {
          updateSimulatorStatus(msg.connected);
        } else if (msg.type === 'telemetry') {
          updateTelemetry(msg.data);
        }
      } catch (e) {
        console.error('Erro ao processar mensagem WS:', e);
      }
    };

    ws.onclose = () => {
      updateSimulatorStatus(false);
      setTimeout(initWebSocket, 2000);
    };

    ws.onerror = () => {
      ws.close();
    };
  }

  function updateSimulatorStatus(connected) {
    if (connected) {
      textSimStatus.textContent = 'Simulador C Ativo';
      badgeSim.querySelector('.status-dot').className = 'status-dot online';
    } else {
      textSimStatus.textContent = 'Aguardando Simulador C...';
      badgeSim.querySelector('.status-dot').className = 'status-dot offline';
    }
  }

  function updateModeUI(mode) {
    currentControlMode = mode;
    if (mode === 'AUTO') {
      btnMasterMode.className = 'btn-mode-toggle mode-auto';
      btnMasterMode.dataset.mode = 'AUTO';
      btnMasterMode.querySelector('.mode-icon').textContent = '🤖';
      btnModeText.textContent = 'Modo: AUTOMÁTICO (IA)';
      if (badgeMode) {
        badgeMode.querySelector('.status-dot').className = 'status-dot mode-dot online';
        textMode.textContent = 'Modo: AUTOMÁTICO (IA)';
      }
    } else {
      btnMasterMode.className = 'btn-mode-toggle mode-manual';
      btnMasterMode.dataset.mode = 'MANUAL';
      btnMasterMode.querySelector('.mode-icon').textContent = '✋';
      btnModeText.textContent = 'Modo: MANUAL (Usuário)';
      if (badgeMode) {
        badgeMode.querySelector('.status-dot').className = 'status-dot mode-dot manual';
        textMode.textContent = 'Modo: MANUAL (Usuário)';
      }
    }
  }

  function renderStringsGrid(strings) {
    stringsGridContainer.innerHTML = '';
    strings.forEach((str) => {
      const card = document.createElement('div');
      card.id = `string-card-${str.id}`;
      card.className = `string-card ${str.state === 1 ? 'active' : 'inactive'}`;
      card.innerHTML = `
        <div class="string-header">
          <span class="string-tag">String #${str.id + 1}</span>
          <span class="string-badge ${str.state === 1 ? 'badge-on' : 'badge-off'}" id="string-badge-${str.id}">
            ${str.state === 1 ? 'LIGADA' : 'CORTADA'}
          </span>
        </div>
        <div class="string-power">
          <span class="str-power-val" id="string-val-${str.id}">${Math.round(str.p_actual)}</span>
          <span class="str-power-unit">W</span>
        </div>
        <div class="string-nominal">Nominal: ${Math.round(str.p_nominal)} W</div>
        <div class="string-action">
          <button type="button" class="btn-toggle-string" id="btn-toggle-${str.id}" data-id="${str.id}" data-state="${str.state}">
            ${str.state === 1 ? 'Desligar' : 'Ligar'}
          </button>
        </div>
      `;
      stringsGridContainer.appendChild(card);
    });
  }

  function updateTelemetry(data) {
    const pGrid = data.p_grid;
    const pLoad = data.p_load;
    const pPv = data.p_pv;
    const irradiance = data.irradiance;
    const isViolation = pGrid > 50;

    // Atualiza Modo de Controle se recebido na telemetria
    if (data.control_mode && data.control_mode !== currentControlMode) {
      updateModeUI(data.control_mode);
    }

    // Atualiza Banner PAC / Zero Grid
    if (isViolation) {
      zeroGridBanner.className = 'zero-grid-banner violation';
      statusBadgeText.className = 'banner-badge badge-violation';
      statusBadgeText.textContent = 'ALERTA: VIOLAÇÃO DE ZERO-GRID (INJEÇÃO)';
      bannerDescription.textContent = currentControlMode === 'AUTO'
        ? `Excedente solar de +${Math.round(pGrid)}W detectado! A IA atuará no corte de strings.`
        : `Excedente solar de +${Math.round(pGrid)}W detectado! Modo MANUAL ativo: desligue strings manualmente para conter a injeção.`;
      pacFlowValue.className = 'metric-big-value val-negative';
      pacFlowValue.innerHTML = `+${Math.round(pGrid)} <small>W</small>`;
      pacFlowDirection.textContent = 'Injetando na concessionária (NÃO PERMITIDO)';
    } else {
      zeroGridBanner.className = 'zero-grid-banner ok';
      statusBadgeText.className = 'banner-badge badge-ok';
      statusBadgeText.textContent = 'CONFORME: ZERO-GRID RESPEITADO';
      pacFlowValue.className = 'metric-big-value val-positive';
      pacFlowValue.innerHTML = `${Math.round(pGrid)} <small>W</small>`;

      if (pGrid < -50) {
        bannerDescription.textContent = `Consumo atendido com complementação da concessionária (${Math.abs(Math.round(pGrid))} W importados).`;
        pacFlowDirection.textContent = 'Importando da rede pública';
      } else {
        bannerDescription.textContent = 'Ponto ideal de autoconsumo: Geração atende a carga com injeção nula na rede.';
        pacFlowDirection.textContent = 'Balanço neutro com a concessionária';
      }
    }

    // Registra mudança de status no log
    if (previousZeroGridStatus !== null && previousZeroGridStatus !== data.zero_grid_status) {
      if (data.zero_grid_status === 'VIOLATION') {
        addLog('cut', `⚠️ Violação de Zero-Grid detectada! Injeção no PAC: +${Math.round(pGrid)}W`);
      } else {
        addLog('reconnect', `✅ Zero-Grid restabelecido! PAC operando em segurança: ${Math.round(pGrid)}W`);
      }
    }
    previousZeroGridStatus = data.zero_grid_status;

    // Atualiza Cartões Numéricos
    const maxPvCapacity = data.num_strings * 2500;
    valPPv.innerHTML = `${Math.round(pPv)} <small>W</small>`;
    barPPv.style.width = `${Math.min(100, (pPv / Math.max(1, maxPvCapacity)) * 100)}%`;

    valPLoad.innerHTML = `${Math.round(pLoad)} <small>W</small>`;
    barPLoad.style.width = `${Math.min(100, (pLoad / 12000) * 100)}%`;

    valIrradiance.innerHTML = `${Math.round(irradiance)} <small>W/m²</small>`;
    barIrradiance.style.width = `${Math.min(100, (irradiance / 1200) * 100)}%`;
    sunStatus.textContent = irradiance >= 800 ? 'Sol Pleno' : (irradiance >= 400 ? 'Parcial' : 'Baixo');

    // Atualiza texto de capacidade
    const cardMetaSolar = document.querySelector('.card-solar .card-meta span:first-child');
    if (cardMetaSolar) {
      cardMetaSolar.textContent = `Capacidade: ${(maxPvCapacity / 1000).toFixed(1)} kWp`;
    }

    // Sincroniza seletor de número de strings se divergente
    if (selectNumStrings && parseInt(selectNumStrings.value, 10) !== data.num_strings) {
      selectNumStrings.value = data.num_strings;
    }

    // Verifica se a quantidade de cards de strings na tela precisa ser reconstruída
    if (stringsGridContainer.children.length !== data.strings.length) {
      renderStringsGrid(data.strings);
    }

    // Atualiza Strings individualmente
    let activeCount = 0;
    data.strings.forEach((str) => {
      if (str.state === 1) activeCount++;

      const card = document.getElementById(`string-card-${str.id}`);
      const badge = document.getElementById(`string-badge-${str.id}`);
      const val = document.getElementById(`string-val-${str.id}`);
      const btn = document.getElementById(`btn-toggle-${str.id}`);

      if (card && badge && val && btn) {
        val.textContent = Math.round(str.p_actual);

        if (str.state === 1) {
          card.className = 'string-card active';
          badge.className = 'string-badge badge-on';
          badge.textContent = 'LIGADA';
          btn.textContent = 'Desligar';
          btn.dataset.state = '1';
        } else {
          card.className = 'string-card inactive';
          badge.className = 'string-badge badge-off';
          badge.textContent = 'CORTADA';
          btn.textContent = 'Ligar';
          btn.dataset.state = '0';
        }
      }

      // Log de comutação
      if (previousStringStates[str.id] !== undefined && previousStringStates[str.id] !== str.state) {
        const actor = (currentControlMode === 'AUTO') ? 'IA/AÇÃO' : 'USUÁRIO/MANUAL';
        if (str.state === 0) {
          addLog('cut', `[${actor}] String #${str.id + 1} CORTADA (-${Math.round(str.p_nominal)}W).`);
        } else {
          addLog('reconnect', `[${actor}] String #${str.id + 1} RELIGADA (+${Math.round(str.p_nominal)}W).`);
        }
      }
      previousStringStates[str.id] = str.state;
    });

    activeStringsCount.textContent = `${activeCount}/${data.num_strings} Strings Ativas`;

    // Atualiza Gráfico
    const nowStr = new Date().toLocaleTimeString('pt-BR', { hour12: false, minute: '2-digit', second: '2-digit' });
    chartLabels.push(nowStr);
    solarData.push(Math.round(pPv));
    loadData.push(Math.round(pLoad));
    gridData.push(Math.round(pGrid));

    if (chartLabels.length > maxDataPoints) {
      chartLabels.shift();
      solarData.shift();
      loadData.shift();
      gridData.shift();
    }

    realtimeChart.update();
  }

  // Helper para adicionar entradas no log
  function addLog(type, text) {
    const timeStr = new Date().toLocaleTimeString('pt-BR', { hour12: false });
    const div = document.createElement('div');
    div.className = `log-entry ${type}`;
    div.innerHTML = `<span class="log-time">${timeStr}</span><span class="log-text">${text}</span>`;
    logStream.appendChild(div);
    logStream.scrollTop = logStream.scrollHeight;
  }

  // 3. Handlers de Controles Interativos

  // Alternância de Modo Automático / Manual para os módulos
  if (btnMasterMode) {
    btnMasterMode.addEventListener('click', () => {
      const nextMode = (currentControlMode === 'AUTO') ? 'MANUAL' : 'AUTO';
      updateModeUI(nextMode);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'set_mode', mode: nextMode }));
        addLog('system', `[MODO] Sistema alterado para ${nextMode === 'AUTO' ? 'AUTOMÁTICO (IA ativa)' : 'MANUAL (Controle do Usuário)'}.`);
      }
    });
  }

  // Seleção de Quantidade de Strings
  if (selectNumStrings) {
    selectNumStrings.addEventListener('change', (e) => {
      const val = parseInt(e.target.value, 10);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'set_num_strings', value: val }));
        addLog('system', `[CONFIG] Quantidade de strings alterada para ${val} módulos.`);
      }
    });
  }

  // Sliders de Carga e Irradiação
  sliderLoad.addEventListener('input', (e) => {
    const val = parseInt(e.target.value, 10);
    displayLoadVal.textContent = `${val} W`;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ action: 'set_load', value: val }));
    }
  });

  sliderIrradiance.addEventListener('input', (e) => {
    const val = parseInt(e.target.value, 10);
    displayIrradianceVal.textContent = `${val} W/m²`;
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ action: 'set_irradiance', value: val }));
    }
  });

  document.querySelectorAll('.btn-preset').forEach((btn) => {
    btn.addEventListener('click', () => {
      const type = btn.dataset.type;
      const val = parseInt(btn.dataset.val, 10);

      if (type === 'load') {
        sliderLoad.value = val;
        displayLoadVal.textContent = `${val} W`;
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ action: 'set_load', value: val }));
          addLog('info', `[SIMULADOR] Carga alterada para ${val}W via preset.`);
        }
      } else if (type === 'sun') {
        sliderIrradiance.value = val;
        displayIrradianceVal.textContent = `${val} W/m²`;
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ action: 'set_irradiance', value: val }));
          addLog('info', `[SIMULADOR] Irradiação alterada para ${val}W/m² via preset.`);
        }
      }
    });
  });

  // Handler de Botões de Chaveamento de Strings (Respeitado no modo manual)
  stringsGridContainer.addEventListener('click', (e) => {
    if (e.target.classList.contains('btn-toggle-string')) {
      const btn = e.target;
      const id = parseInt(btn.dataset.id, 10);
      const currentState = parseInt(btn.dataset.state, 10);
      const newState = currentState === 1 ? 0 : 1;

      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'set_string', id: id, state: newState }));
        addLog('info', `[COMANDO] String #${id + 1} alterada para ${newState === 1 ? 'LIGADA' : 'CORTADA'}.`);
      }
    }
  });

  // Limpar Logs
  btnClearLogs.addEventListener('click', () => {
    logStream.innerHTML = '';
  });

  // Inicia conexão WebSocket
  initWebSocket();
});
