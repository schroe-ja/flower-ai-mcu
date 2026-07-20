import ctypes
import os
import numpy as np
from flwr.app import ArrayRecord, Context
from flwr.serverapp import ServerApp
from flwr.serverapp.strategy import FedAvg


INPUT_NEURONS = 100
HIDDEN_NEURONS = 16
OUTPUT_NEURONS = 6
NN: list[tuple[int, ...]] = [
    (INPUT_NEURONS, HIDDEN_NEURONS),
    (HIDDEN_NEURONS,),
    (HIDDEN_NEURONS, OUTPUT_NEURONS),
    (OUTPUT_NEURONS,)
]
TOTAL_PARAMETERS = sum([int(np.prod(tup)) for tup in NN])
MIN_CLIENTS = int(os.environ.get("MIN_CLIENTS", "1"))
print(f"Total payload size is {ctypes.sizeof(ctypes.c_float)} * {TOTAL_PARAMETERS}")

app = ServerApp()


def initial_ndarrays(seed: int = 42) -> list[np.ndarray]:
    """Initializes neural network with the Glorot approach."""

    rng = np.random.default_rng(seed)

    def glorot(fan_in: int, fan_out: int) -> np.ndarray:
        limit = np.sqrt(6.0 / (fan_in + fan_out))
        return rng.uniform(-limit, limit, size=(fan_out, fan_in)).astype(np.float32)

    result = [
        glorot(INPUT_NEURONS, HIDDEN_NEURONS),
        np.zeros(HIDDEN_NEURONS, dtype=np.float32),
        glorot(HIDDEN_NEURONS, OUTPUT_NEURONS),
        np.zeros(OUTPUT_NEURONS, dtype=np.float32)
    ]
    return result


@app.main()
def main(grid, ctx: Context) -> None:
    num_rounds = int(ctx.run_config["num-server-rounds"])

    # TODO NOW
    strategy = FedAvg(
        fraction_evaluate=0,
        min_train_nodes=MIN_CLIENTS,
        min_evaluate_nodes=0,
        min_available_nodes=MIN_CLIENTS
    )

    print("About to start stategy!")
    result = strategy.start(
        grid=grid,
        initial_arrays=ArrayRecord(initial_ndarrays()),
        num_rounds=num_rounds,
    )

    print(f"Hello from server! o/ ({num_rounds} clients)")
    print(result)
