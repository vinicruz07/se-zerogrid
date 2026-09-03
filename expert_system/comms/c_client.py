"""
Cliente de comunicação TCP com o Simulador em C
"""

import socket
import json
import time

class CSimulatorClient:
    def __init__(self, host="127.0.0.1", port=9001):
        self.host = host
        self.port = port
        self.sock = None
        self.buffer = ""

    def connect(self, timeout=5.0, retries=10, retry_delay=1.0):
        for attempt in range(1, retries + 1):
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.settimeout(timeout)
                self.sock.connect((self.host, self.port))
                print(f"[COMMS] Conectado ao Simulador C em {self.host}:{self.port}")
                return True
            except (socket.error, ConnectionRefusedError) as e:
                print(f"[COMMS] Tentativa {attempt}/{retries} falhou: {e}. Nova tentativa em {retry_delay}s...")
                time.sleep(retry_delay)
        return False

    def receive_telemetry(self):
        """Lê e decodifica o próximo objeto JSON de telemetria do socket."""
        if not self.sock:
            return None

        while "\n" not in self.buffer:
            try:
                data = self.sock.recv(4096)
                if not data:
                    print("[COMMS] Conexão encerrada pelo Simulador C.")
                    self.close()
                    return None
                self.buffer += data.decode("utf-8", errors="replace")
            except socket.timeout:
                return None
            except socket.error as e:
                print(f"[COMMS] Erro de socket: {e}")
                self.close()
                return None

        line, self.buffer = self.buffer.split("\n", 1)
        line = line.strip()
        if not line:
            return None

        try:
            return json.loads(line)
        except json.JSONDecodeError as e:
            print(f"[COMMS] Erro ao decodificar JSON: {e} (linha: {line[:50]}...)")
            return None

    def send_command(self, cmd_string):
        """Envia um comando texto simples terminado com quebra de linha."""
        if not self.sock:
            return False
        try:
            msg = (cmd_string.strip() + "\n").encode("utf-8")
            self.sock.sendall(msg)
            return True
        except socket.error as e:
            print(f"[COMMS] Falha ao enviar comando: {e}")
            self.close()
            return False

    def set_string(self, string_id, state):
        return self.send_command(f"SET_STRING {string_id} {1 if state else 0}")

    def set_load(self, load_watts):
        return self.send_command(f"SET_LOAD {load_watts}")

    def set_irradiance(self, irradiance):
        return self.send_command(f"SET_IRRADIANCE {irradiance}")

    def close(self):
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None
