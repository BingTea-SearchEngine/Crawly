#!/bin/bash

PROCESS_NAME="Crawly"

if [ -z "$PROCESS_NAME" ]; then
    echo "Usage: $0 <process_name>"
    exit 1
fi

# Color codes
CYAN_BOLD="\033[1;36m"
YELLOW="\033[0;33m"
RESET="\033[0m"

echo -e "🔍 Monitoring process '$PROCESS_NAME' every 60 seconds..."

while true; do
    # Get number of input files and add 1
    NUM_FILES=$(find ~/index/input -type f | wc -l)
    echo -e "📂 ${CYAN_BOLD}Number of input files: $NUM_FILES${RESET}"

    # Show disk usage in yellow
    echo -e "${YELLOW}$(df -h ~)${RESET}"

    # Check if process is running
    if pgrep -x "$PROCESS_NAME" > /dev/null; then
        echo "$(date): ✅ Process '$PROCESS_NAME' is running."
    else
        MAX_NUM=$(find ~/index/input -type f -name "*.parsed" \
            | sed -E 's|.*/([0-9]+)\.parsed$|\1|' \
            | sort -n | tail -n 1)

        # If no files matched, default to 0
        if [[ -z "$MAX_NUM" ]]; then
            ARG=1
        else
            ARG=$((MAX_NUM + 1))
        fi
        echo "$(date): ❌ Process '$PROCESS_NAME' is NOT running."
        echo "🔄 Restarting '$PROCESS_NAME' with argument $ARG..."

        nohup ./build/Crawly -a $FRONTIER_IP -p $FRONTIER_PORT -o /home/wbjin/index/input -s $ARG > ~/CrawlerLog.txt 2>&1 &
    fi

    sleep 60
done

