const express = require('express');
const http = require('http');
const path = require('path');
const net = require('net');
const { WebSocketServer, WebSocket } = require('ws');

const app = express();
const server = http.createServer(app);
const wss = new WebSocketServer({ server });

const HTTP_PORT = process.env.PORT || 3000;
const SIM_HOST = process.env.SIM_HOST || '127.0.0.1';
const SIM_PORT = parseInt(process.env.SIM_PORT || '9001', 10);

// Configuração Express & EJS
app.set('view engine', 'ejs');
app.set('views', path.join(__dirname, 'views'));
app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());

// Estado atual em memória
let lastTelemetry = {
  timestamp: 0,
  p_load: 4200,
  p_pv: 10000,
  p_grid: 5800,
  irradiance: 1000,
  num_strings: 4,
  strings: [
    { id: 0, p_nominal: 2500, p_actual: 2500, state: 1 },
    { id: 1, p_nominal: 2500, p_actual: 2500, state: 1 },
    { id: 2, p_nominal: 2500, p_actual: 2500, state: 1 },
    { id: 3, p_nominal: 2500, p_actual: 2500, state: 1 }
  ],
  zero_grid_status: 'VIOLATION'
};

let simulatorConnected = false;
let simClient = null;
let simBuffer = '';

// Conexão TCP com o Simulador em C
function connectToSimulator() {
  if (simClient) {
    simClient.destroy();
    simClient = null;
  }

  console.log(`[DASHBOARD] Conectando ao simulador C em ${SIM_HOST}:${SIM_PORT}...`);
  simClient = new net.Socket();

  simClient.connect(SIM_PORT, SIM_HOST, () => {
    console.log(`[DASHBOARD] Conectado ao Simulador C com sucesso!`);
    simulatorConnected = true;
    broadcastWs({ type: 'sim_status', connected: true });
  });

  simClient.on('data', (data) => {
    simBuffer += data.toString();
    const lines = simBuffer.split('\n');
    simBuffer = lines.pop(); // Mantém pedaço incompleto no buffer

    for (const line of lines) {
      const trimmed = line.trim();
      if (!trimmed) continue;
      try {
        const telemetry = JSON.parse(trimmed);
        lastTelemetry = telemetry;
        // Retransmite via WebSocket para todos os navegadores abertos
        broadcastWs({ type: 'telemetry', data: telemetry });
      } catch (err) {
        // Ignora pacotes quebrados
      }
    }
  });

  simClient.on('close', () => {
    console.log(`[DASHBOARD] Conexão com o Simulador C perdida. Reconectando em 2s...`);
    simulatorConnected = false;
    simClient = null;
    broadcastWs({ type: 'sim_status', connected: false });
    setTimeout(connectToSimulator, 2000);
  });

  simClient.on('error', (err) => {
    // console.log(`[DASHBOARD - TCP] ${err.message}`);
    // O evento 'close' será chamado em seguida para tentar reconectar
  });
}

function sendCommandToSimulator(cmd) {
  if (simClient && simulatorConnected) {
    simClient.write(cmd.trim() + '\n');
    return true;
  }
  return false;
}

// WebSocket broadcast helper
function broadcastWs(msgObj) {
  const jsonStr = JSON.stringify(msgObj);
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(jsonStr);
    }
  });
}

// WebSocket connection handler
wss.on('connection', (ws) => {
  // Envia estado inicial imediatamente ao conectar
  ws.send(JSON.stringify({ type: 'sim_status', connected: simulatorConnected }));
  ws.send(JSON.stringify({ type: 'telemetry', data: lastTelemetry }));

  ws.on('message', (message) => {
    try {
      const payload = JSON.parse(message.toString());
      if (payload.action === 'set_string') {
        sendCommandToSimulator(`SET_STRING ${payload.id} ${payload.state ? 1 : 0}`);
      } else if (payload.action === 'set_load') {
        sendCommandToSimulator(`SET_LOAD ${payload.value}`);
      } else if (payload.action === 'set_irradiance') {
        sendCommandToSimulator(`SET_IRRADIANCE ${payload.value}`);
      } else if (payload.action === 'set_mode') {
        sendCommandToSimulator(`SET_MODE ${payload.mode}`);
      } else if (payload.action === 'set_num_strings') {
        sendCommandToSimulator(`SET_NUM_STRINGS ${payload.value}`);
      }
    } catch (err) {
      console.error('[WS] Erro ao processar mensagem do cliente:', err);
    }
  });
});

// Rotas HTTP
app.get('/', (req, res) => {
  res.render('index', {
    telemetry: lastTelemetry,
    connected: simulatorConnected
  });
});

app.get('/api/telemetry', (req, res) => {
  res.json({
    simulatorConnected,
    telemetry: lastTelemetry
  });
});

app.post('/api/control/string', (req, res) => {
  const { id, state } = req.body;
  if (typeof id === 'number' && (state === 0 || state === 1)) {
    const ok = sendCommandToSimulator(`SET_STRING ${id} ${state}`);
    return res.json({ success: ok, command: `SET_STRING ${id} ${state}` });
  }
  res.status(400).json({ error: 'Parâmetros inválidos. Use id (number) e state (0 ou 1).' });
});

app.post('/api/control/load', (req, res) => {
  const { load } = req.body;
  if (typeof load === 'number' && load >= 0) {
    const ok = sendCommandToSimulator(`SET_LOAD ${load}`);
    return res.json({ success: ok, command: `SET_LOAD ${load}` });
  }
  res.status(400).json({ error: 'Parâmetro load inválido.' });
});

app.post('/api/control/irradiance', (req, res) => {
  const { irradiance } = req.body;
  if (typeof irradiance === 'number' && irradiance >= 0) {
    const ok = sendCommandToSimulator(`SET_IRRADIANCE ${irradiance}`);
    return res.json({ success: ok, command: `SET_IRRADIANCE ${irradiance}` });
  }
  res.status(400).json({ error: 'Parâmetro irradiance inválido.' });
});

app.post('/api/control/mode', (req, res) => {
  const { mode } = req.body;
  if (mode === 'AUTO' || mode === 'MANUAL') {
    const ok = sendCommandToSimulator(`SET_MODE ${mode}`);
    return res.json({ success: ok, command: `SET_MODE ${mode}` });
  }
  res.status(400).json({ error: 'Modo inválido. Use "AUTO" ou "MANUAL".' });
});

app.post('/api/control/num_strings', (req, res) => {
  const { num_strings } = req.body;
  const n = parseInt(num_strings, 10);
  if (!isNaN(n) && n >= 1 && n <= 16) {
    const ok = sendCommandToSimulator(`SET_NUM_STRINGS ${n}`);
    return res.json({ success: ok, command: `SET_NUM_STRINGS ${n}` });
  }
  res.status(400).json({ error: 'Número de strings inválido. Deve ser entre 1 e 16.' });
});

// Inicialização do servidor HTTP e da conexão TCP
server.listen(HTTP_PORT, () => {
  console.log(`=======================================================`);
  console.log(`   SE-ZEROGRID - DASHBOARD WEB EM NODE.JS / EXPRESS    `);
  console.log(`=======================================================`);
  console.log(`[DASHBOARD] Servidor HTTP/EJS rodando em: http://localhost:${HTTP_PORT}`);
  connectToSimulator();
});
