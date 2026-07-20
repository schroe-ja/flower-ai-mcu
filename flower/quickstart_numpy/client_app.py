import ctypes
import socket
import signal
import sys
import threading
import time
import os
import numpy as np
from flwr.app import ArrayRecord, Context, Message, MetricRecord, RecordDict
from flwr.clientapp import ClientApp
from quickstart_numpy.server_app import NN, TOTAL_PARAMETERS

HOST = "0.0.0.0"
app = ClientApp()

class ESP32Connection:
    def __init__(self, port: int):
        self.port = port
        # protects self.sock / self.srv
        self.state_lock = threading.Lock()
        # serializes a full send+recv round
        self.io_lock = threading.Lock()
        self.sock: socket.socket | None = None
        self.srv: socket.socket | None = None
        self.shutdown_event = threading.Event()
        self._server_thread: threading.Thread | None = None

    def ensure_server_running(self):
        with self.state_lock:
            if self._server_thread is not None:
                return
            self._server_thread = threading.Thread(
                target=self._accept_loop, daemon=True
            )
            self._server_thread.start()

    def _accept_loop(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        with self.state_lock:
            self.srv = srv
        try:
            srv.bind((HOST, self.port))
            srv.listen(1)
            srv.settimeout(1.0)
            print(f"[port {self.port}] Listening on {HOST}:{self.port} for ESP32...")

            while not self.shutdown_event.is_set():
                try:
                    sock, addr = srv.accept()
                except socket.timeout:
                    continue
                except OSError:
                    break

                print(f"[port {self.port}] ESP32 connected from {addr}")
                with self.state_lock:
                    if self.sock is not None:
                        try:
                            self.sock.close()
                        except OSError:
                            pass
                    self.sock = sock
        except Exception as e:
            print(f"[port {self.port}] TCP server error on {HOST}:{self.port}: {e}")
        finally:
            srv.close()
            with self.state_lock:
                if self.srv is srv:
                    self.srv = None
            print(f"[port {self.port}] Closed TCP server")

    def get_sock(self) -> socket.socket:
        while True:
            with self.state_lock:
                if self.sock is not None:
                    return self.sock
            time.sleep(0.2)

    def run_round(self, ndarrays: list[np.ndarray]) -> list[np.ndarray]:
        print("getting lock")
        sock = self.get_sock()
        print("done.")
        with self.io_lock:
            print(f"[port {self.port}] Sending weights...")
            send_weights(sock, ndarrays)
            print(f"[port {self.port}]   done sending")
            print(f"[port {self.port}] Receiving updates...")
            updates = recv_weights(sock)
            print(f"[port {self.port}]   done receiving")
            time.sleep(2)
        return updates

    def close(self):
        self.shutdown_event.set()
        with self.state_lock:
            for s in (self.sock, self.srv):
                if s is None:
                    continue
                try:
                    s.close()
                except OSError:
                    pass
            self.sock = None
            self.srv = None


class ConnectionRegistry:
    def __init__(self):
        self._lock = threading.Lock()
        self._connections: dict[int, ESP32Connection] = {}

    def get(self, port: int) -> ESP32Connection:
        with self._lock:
            conn = self._connections.get(port)
            if conn is None:
                conn = ESP32Connection(port)
                self._connections[port] = conn
            return conn

    def close_all(self):
        with self._lock:
            conns = list(self._connections.values())
        for conn in conns:
            conn.close()


_registry = ConnectionRegistry()


def handle_signal(signum, frame):
    print("Running handle_signal")
    _registry.close_all()
    sys.exit(0)


signal.signal(signal.SIGINT, handle_signal)
signal.signal(signal.SIGTERM, handle_signal)


def ndarrays_from_flat(flat: np.ndarray) -> list[np.ndarray]:
    result = []
    offset = 0
    for shape in NN:
        count = int(np.prod(shape))
        chunk = flat[offset:offset + count].reshape(shape)
        result.append(chunk)
        offset += count
    return result


def send_weights(sock: socket.socket, ndarrays: list[np.ndarray]):
    flat = np.concatenate([a.astype("<f4").ravel() for a in ndarrays])
    sock.sendall(flat.tobytes())


def recv_all(sock: socket.socket, size: int) -> bytes:
    buf = bytearray()
    while len(buf) < size:
        chunk = sock.recv(size - len(buf))
        if not chunk:
            raise ConnectionError("ESP32 closed connection mid-read!")
        buf.extend(chunk)
    return bytes(buf)


def recv_weights(sock: socket.socket) -> list[np.ndarray]:
    leng = TOTAL_PARAMETERS * ctypes.sizeof(ctypes.c_float)
    payload = recv_all(sock, leng)
    flat = np.frombuffer(payload, dtype="<f4").copy()
    return ndarrays_from_flat(flat)


@app.train()
def train(msg: Message, ctx: Context) -> Message:
    print("Running train()...")
    port = int(ctx.node_config.get("port", 8081))
    conn = _registry.get(port)
    conn.ensure_server_running()

    ndarrays = msg.content["arrays"].to_numpy_ndarrays()
    updates = conn.run_round(ndarrays)

    model_record = ArrayRecord(updates)
    metric_record = MetricRecord({"num-examples": 1})
    content = RecordDict({"arrays": model_record, "metrics": metric_record})
    return Message(content=content, reply_to=msg)


@app.evaluate()
def evaluate(msg: Message, ctx: Context) -> Message:
    print("Running evaluate()...")
    port = int(ctx.node_config.get("port", 8081))
    conn = _registry.get(port)
    conn.ensure_server_running()

    ndarrays = msg.content["arrays"].to_numpy_ndarrays()
    conn.run_round(ndarrays)  # matches original: evaluate() ignores the updates

    metric_record = MetricRecord({
        "random_metric": np.random.rand(3).tolist(),
        "num-examples": 1,
    })
    content = RecordDict({"metrics": metric_record})
    return Message(content=content, reply_to=msg)
