#!/bin/bash

SESSION="rf_deployment"
SERVERS=("rf01" "rf02" "rf03" "rf04" "rf05")
SLEEP_TIME=2

# Check if the tmux session already exists
tmux has-session -t "$SESSION" 2>/dev/null

if [ $? -eq 0 ]; then
    echo "Warning: Tmux session '$SESSION' is already running."
    read -p "Do you want to [k]ill the existing session or [c]ancel? (k/c): " choice
    case "$choice" in
        k|K ) 
            echo "Killing existing session..."
            tmux kill-session -t "$SESSION"
            ;;
        * ) 
            echo "Execution cancelled. Use 'tmux attach -t $SESSION' to see the existing windows."
            exit 1
            ;;
    esac
fi

# Start the new fresh session
tmux new-session -d -s "$SESSION" -n "RF_Monitors"

for i in "${!SERVERS[@]}"; do
    TARGET=${SERVERS[$i]}
    VAL1=$((10 + i))
    VAL3=$((0 + i))
    
    # Combined command: Init -> Sleep -> ELF
    CMD="ssh -t $TARGET 'bin/init.sh && sleep $SLEEP_TIME && bin/rfdcbabies.elf $VAL1 9 $VAL3'"

    if [ $i -ne 0 ]; then
        tmux split-window -t "$SESSION"
        tmux select-layout -t "$SESSION" tiled
    fi

    tmux send-keys -t "$SESSION.$i" "$CMD" C-m
done

# Finalize layout and attach
tmux select-layout -t "$SESSION" tiled
tmux attach-session -t "$SESSION"
