#!/bin/bash
SESSION_NAME="mva"
SETUP="source ../.venv/bin/activate"

COMMANDS=(
	"flower-superlink --insecure"
	"flower-supernode --insecure --superlink 127.0.0.1:9092 --clientappio-api-address 0.0.0.0:9094 --node-config 'port=8081'"
	"flower-supernode --insecure --superlink 127.0.0.1:9092 --clientappio-api-address 0.0.0.0:9095 --node-config 'port=8082'"
	# "flower-supernode --insecure --superlink 127.0.0.1:9092"
	"source ../esp-idf/export.sh && cd ../esp && make -p /dev/ttyUSB0 m"
	"source ../esp-idf/export.sh && cd ../esp && make -p /dev/ttyUSB1 m"
	# "killall -9 flwr-simulation && flwr run ../flower/ local --stream"
	"killall -9 flwr-simulation && flwr run ../flower/ embedded --stream"
)
	# "killall -9 flwr-simulation && flwr run ../flower/ local --stream"
	# "killall -9 flwr-simulation && flwr run ../flower/ local --stream --federation-config 'options.num-supernodes=1'"

if tmux has-session -t "$SESSION_NAME" 2>/dev/null; then
	echo "Session '$SESSION_NAME' already exists."
	echo "Attaching to existing session..."
	sleep 1
	tmux attach-session -t "$SESSION_NAME"
	exit 0
fi

echo "Creating new tmux session: $SESSION_NAME"

# Force the initial session window to start with bash instead of your default fish shell
tmux new-session -d -s "$SESSION_NAME" "bash"

for i in "${!COMMANDS[@]}"; do
	# For subsequent panes, explicitly split using 'bash' as the target shell
	if [ "$i" -gt 0 ]; then
		tmux split-window -t "$SESSION_NAME" "bash"
	fi

	# Target the specific pane index ($i) to ensure keys go to the right place
	# We send the environment setup first, then the ROS2 command
	tmux send-keys -t "$SESSION_NAME.$i" "$SETUP" C-m
	tmux send-keys -t "$SESSION_NAME.$i" "${COMMANDS[$i]}" C-m

	tmux select-layout -t "$SESSION_NAME" tiled
done

tmux attach-session -t "$SESSION_NAME"
