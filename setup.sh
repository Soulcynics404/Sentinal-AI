#!/bin/bash
#!/bin/bash
# ═══════════════════════════════════════════════════════
# SENTINEL AI — Interactive Setup
#
# Author:  Soulcynics404
# GitHub:  https://github.com/Soulcynics404
# Repo:    https://github.com/Soulcynics404/Sentinal-AI
# License: MIT
# ═══════════════════════════════════════════════════════
# SENTINEL Setup — Interactive Configuration

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SENTINEL_DIR="$(cd "$(dirname "\$0")" && pwd)"
MODEL_DIR="${SENTINEL_DIR}/models"
DATA_DIR="${SENTINEL_DIR}/data"
EVIDENCE_DIR="${DATA_DIR}/evidence"
CONF_FILE="${SENTINEL_DIR}/sentinel.conf"
LOG_FILE="${SENTINEL_DIR}/setup.log"

clear
echo -e "${CYAN}"
cat << 'BANNER'
  ╔═══════════════════════════════════════════════════════════╗
  ║   ____  _____ _   _ _____ ___ _   _ _____ _               ║
  ║  / ___|| ____| \ | |_   _|_ _| \ | | ____| |              ║
  ║  \___ \|  _| |  \| | | |  | ||  \| |  _| | |              ║
  ║   ___) | |___| |\  | | |  | || |\  | |___| |___           ║
  ║  |____/|_____|_| \_| |_| |___|_| \_|_____|_____|          ║
  ║                                                           ║
  ║         Personal AI Security System — Setup v3.0          ║
  ╚═══════════════════════════════════════════════════════════╝
BANNER
echo -e "${NC}"

echo -e "${BOLD}This setup will:${NC}"
echo "  1. Install all dependencies"
echo "  2. Download AI face recognition models"
echo "  3. Build SENTINEL from source"
echo "  4. Detect your camera"
echo "  5. Configure ALL security settings"
echo "  6. (Optional) Setup Telegram remote control"
echo "  7. Enroll your face (owner)"
echo "  8. Setup security questions (kill switch)"
echo "  9. Install auto-start service"
echo ""
read -p "$(echo -e ${YELLOW}Press ENTER to begin...${NC})"
echo ""

mkdir -p "$MODEL_DIR" "$DATA_DIR" "$EVIDENCE_DIR"

# ═══════════════════════════════════════
# Step 1: Dependencies
# ═══════════════════════════════════════
echo -e "${GREEN}${BOLD}[1/9] Installing dependencies...${NC}"
sudo apt update -y >> "$LOG_FILE" 2>&1 || true

sudo apt install -y \
    build-essential cmake pkg-config wget bzip2 git curl \
    libcurl4-openssl-dev libopencv-dev libopencv-contrib-dev \
    libx11-dev libopenblas-dev liblapack-dev libpng-dev libjpeg-dev \
    libsqlite3-dev i3lock scrot xdotool v4l-utils wmctrl \
    sox alsa-utils python3 \
    >> "$LOG_FILE" 2>&1 || true

echo -e "${GREEN}  ✓ Dependencies installed${NC}"

# Check dlib
if ! ldconfig -p 2>/dev/null | grep -q libdlib; then
    echo -e "${YELLOW}  Building dlib from source (10-20 min)...${NC}"
    DLIB_VERSION="19.24.6"
    DLIB_BUILD="/tmp/sentinel_dlib_build"
    rm -rf "$DLIB_BUILD" && mkdir -p "$DLIB_BUILD" && cd "$DLIB_BUILD"
    wget -q --show-progress "https://github.com/davisking/dlib/archive/refs/tags/v${DLIB_VERSION}.tar.gz" -O dlib.tar.gz
    tar xzf dlib.tar.gz && cd "dlib-${DLIB_VERSION}" && mkdir -p build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_AVX_INSTRUCTIONS=ON -DBUILD_SHARED_LIBS=ON >> "$LOG_FILE" 2>&1
    cmake --build . --config Release -- -j$(nproc) >> "$LOG_FILE" 2>&1
    sudo make install >> "$LOG_FILE" 2>&1 && sudo ldconfig
    echo -e "${GREEN}  ✓ dlib compiled${NC}"
    cd "$SENTINEL_DIR"
else
    echo -e "${GREEN}  ✓ dlib already installed${NC}"
fi

# ═══════════════════════════════════════
# Step 2: Models
# ═══════════════════════════════════════
echo -e "${GREEN}${BOLD}[2/9] Downloading AI models...${NC}"
cd "$MODEL_DIR"
[ ! -f "shape_predictor_68_face_landmarks.dat" ] && {
    wget -q --show-progress "http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2"
    bzip2 -d shape_predictor_68_face_landmarks.dat.bz2; } || echo -e "${GREEN}  ✓ Shape predictor exists${NC}"
[ ! -f "dlib_face_recognition_resnet_model_v1.dat" ] && {
    wget -q --show-progress "http://dlib.net/files/dlib_face_recognition_resnet_model_v1.dat.bz2"
    bzip2 -d dlib_face_recognition_resnet_model_v1.dat.bz2; } || echo -e "${GREEN}  ✓ Face rec model exists${NC}"
[ ! -f "haarcascade_frontalface_default.xml" ] && {
    wget -q "https://raw.githubusercontent.com/opencv/opencv/master/data/haarcascades/haarcascade_frontalface_default.xml"; }
echo -e "${GREEN}  ✓ All models ready${NC}"
cd "$SENTINEL_DIR"

# ═══════════════════════════════════════
# Step 3: Build
# ═══════════════════════════════════════
echo -e "${GREEN}${BOLD}[3/9] Building SENTINEL...${NC}"
rm -rf build && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release >> "$LOG_FILE" 2>&1
cmake --build . -- -j$(nproc) >> "$LOG_FILE" 2>&1
if [ -f "./sentinel" ]; then
    cp sentinel "$SENTINEL_DIR/sentinel"
    echo -e "${GREEN}  ✓ Build successful${NC}"
else
    echo -e "${RED}  ✗ Build failed! Check setup.log${NC}"
    exit 1
fi
cd "$SENTINEL_DIR"

# ═══════════════════════════════════════
# Step 4: Camera Detection
# ═══════════════════════════════════════
echo -e "${GREEN}${BOLD}[4/9] Detecting camera...${NC}"
CAMERA_INDEX=-1
for i in 0 1 2 3 4; do
    RESULT=$(python3 -c "
import cv2
cap = cv2.VideoCapture($i)
if cap.isOpened():
    ret, f = cap.read()
    cap.release()
    print('OK' if ret else 'FAIL')
else:
    print('FAIL')
" 2>/dev/null)
    if [ "$RESULT" = "OK" ]; then
        CAMERA_INDEX=$i
        echo -e "${GREEN}  ✓ Camera found at index $i${NC}"
        break
    fi
done
if [ "$CAMERA_INDEX" = "-1" ]; then
    echo -e "${RED}  ✗ No camera found!${NC}"
    echo -e "${YELLOW}  Enter camera index manually: ${NC}"
    read -p "  Camera index: " CAMERA_INDEX
fi

# ═══════════════════════════════════════
# Step 5: Security Configuration
# ═══════════════════════════════════════
echo ""
echo -e "${GREEN}${BOLD}[5/9] Security Configuration${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"

# Face threshold
echo ""
echo -e "${BOLD}Face recognition strictness:${NC}"
echo "  1) Strict  (0.35) — May lock you occasionally"
echo "  2) Normal  (0.45) — Recommended"
echo "  3) Relaxed (0.55) — Fewer false locks"
read -p "  Choose [1/2/3, default: 3]: " STRICT_CHOICE
case "$STRICT_CHOICE" in
    1) FACE_THRESHOLD=0.35 ;;
    2) FACE_THRESHOLD=0.45 ;;
    *) FACE_THRESHOLD=0.55 ;;
esac

# No-face lock timer
echo ""
echo -e "${BOLD}Seconds without seeing your face before auto-lock:${NC}"
echo "  (Recommended: 30-60 seconds)"
read -p "  Enter seconds [default: 45]: " NOFACE_SECONDS
NOFACE_SECONDS=${NOFACE_SECONDS:-45}

# Unknown frames before lock
echo ""
echo -e "${BOLD}Unknown face frames before locking:${NC}"
echo "  (Higher = more tolerant, Lower = faster lock)"
read -p "  Enter frames [default: 5]: " UNK_FRAMES
UNK_FRAMES=${UNK_FRAMES:-5}

# Verification popup timeout
echo ""
echo -e "${BOLD}Face verification popup timeout (seconds):${NC}"
echo "  Time given to verify face after unlock"
read -p "  Enter seconds [default: 30]: " VERIFY_TIMEOUT
VERIFY_TIMEOUT=${VERIFY_TIMEOUT:-30}

# Kill switch timeout
echo ""
echo -e "${BOLD}Kill switch timeout (seconds):${NC}"
echo "  Time given to answer security questions"
read -p "  Enter seconds [default: 120]: " KILLSWITCH_TIMEOUT
KILLSWITCH_TIMEOUT=${KILLSWITCH_TIMEOUT:-120}

# Security bypass duration
echo ""
echo -e "${BOLD}Security bypass duration after kill switch (minutes):${NC}"
echo "  How long face verification is disabled after correct answers"
read -p "  Enter minutes [default: 30]: " BYPASS_MINUTES
BYPASS_MINUTES=${BYPASS_MINUTES:-30}

# Lock cooldown
echo ""
echo -e "${BOLD}Lock cooldown (seconds between re-locks):${NC}"
read -p "  Enter seconds [default: 3]: " LOCK_COOLDOWN
LOCK_COOLDOWN=${LOCK_COOLDOWN:-3}

# Sound alarm
echo ""
read -p "$(echo -e ${BOLD}Enable alarm sound on intrusion? [y/N]: ${NC})" SOUND_CHOICE
SOUND_ALARM=false
[[ "$SOUND_CHOICE" =~ ^[yY] ]] && SOUND_ALARM=true

# Save evidence
echo ""
read -p "$(echo -e ${BOLD}Save intruder photos as evidence? [Y/n]: ${NC})" EVIDENCE_CHOICE
SAVE_EVIDENCE=true
[[ "$EVIDENCE_CHOICE" =~ ^[nN] ]] && SAVE_EVIDENCE=false

# Tamper detection
echo ""
echo -e "${BOLD}Camera tamper detection:${NC}"
echo "  Detects if someone covers or blocks camera"
read -p "  Tamper frames before lock [default: 30]: " TAMPER_FRAMES
TAMPER_FRAMES=${TAMPER_FRAMES:-30}

echo -e "${GREEN}  ✓ Security settings configured${NC}"

# ═══════════════════════════════════════
# Step 6: Telegram (Optional)
# ═══════════════════════════════════════
echo ""
echo -e "${GREEN}${BOLD}[6/9] Telegram Remote Control (Optional)${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"
echo ""
echo "Telegram lets you:"
echo "  • Receive intruder photos on your phone"
echo "  • Lock/unlock your laptop remotely"
echo "  • Turn face verification ON/OFF"
echo "  • Capture camera/screenshot remotely"
echo ""
read -p "$(echo -e ${BOLD}Setup Telegram? [y/N]: ${NC})" TG_CHOICE

TELEGRAM_ENABLED=false
TELEGRAM_TOKEN=""
TELEGRAM_CHAT_ID=""

if [[ "$TG_CHOICE" =~ ^[yY] ]]; then
    echo ""
    echo -e "${CYAN}Steps:${NC}"
    echo "  1. Open Telegram → search @BotFather"
    echo "  2. Send /newbot → follow instructions"
    echo "  3. Copy the bot token"
    echo ""
    read -p "  Bot Token: " TELEGRAM_TOKEN
    echo ""
    echo "  4. Start a chat with your bot"
    echo "  5. Send any message to it"
    echo "  6. Visit: https://api.telegram.org/bot${TELEGRAM_TOKEN}/getUpdates"
    echo "  7. Find your chat_id"
    echo ""
    read -p "  Chat ID: " TELEGRAM_CHAT_ID

    if [ -n "$TELEGRAM_TOKEN" ] && [ -n "$TELEGRAM_CHAT_ID" ]; then
        TELEGRAM_ENABLED=true
        echo -e "  Sending test message..."
        curl -s -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendMessage" \
            -d "chat_id=${TELEGRAM_CHAT_ID}" \
            -d "text=🛡️ SENTINEL connected! Use /help to see commands." > /dev/null 2>&1
        echo -e "${GREEN}  ✓ Telegram configured — check your phone!${NC}"
    fi
fi

# ═══════════════════════════════════════
# Generate sentinel.conf
# ═══════════════════════════════════════
cat > "$CONF_FILE" << CONFEOF
# ═══════════════════════════════════════
# SENTINEL Configuration v3.0 Phase 3
# Generated: $(date)
# ═══════════════════════════════════════

# Camera
camera_index=${CAMERA_INDEX}
frame_skip=3
show_preview=false

# Face Recognition
face_threshold=${FACE_THRESHOLD}
enrollment_samples=20
consecutive_unknown_frames=${UNK_FRAMES}
max_allowed_faces=1

# Auto-lock (seconds without owner face)
noface_lock_seconds=${NOFACE_SECONDS}

# Lock cooldown
lock_cooldown_seconds=${LOCK_COOLDOWN}

# Evidence
save_evidence=${SAVE_EVIDENCE}

# Camera tamper
tamper_blur_threshold=5.0
tamper_dark_threshold=5.0
tamper_frames_before_lock=${TAMPER_FRAMES}

# Sound
sound_alarm=${SOUND_ALARM}

# Verification popup (seconds)
verification_timeout=${VERIFY_TIMEOUT}

# Kill switch (seconds for security questions)
killswitch_timeout=${KILLSWITCH_TIMEOUT}

# Bypass duration after kill switch (minutes)
security_disable_minutes=${BYPASS_MINUTES}

# Telegram
telegram_enabled=${TELEGRAM_ENABLED}
telegram_bot_token=${TELEGRAM_TOKEN}
telegram_chat_id=${TELEGRAM_CHAT_ID}
CONFEOF

echo -e "${GREEN}  ✓ sentinel.conf generated${NC}"

# ═══════════════════════════════════════
# Step 7: Enroll Face
# ═══════════════════════════════════════
echo ""
echo -e "${GREEN}${BOLD}[7/9] Face Enrollment${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"
echo ""
echo "Look at camera. Move head slowly."
read -p "$(echo -e ${YELLOW}Press ENTER when ready...${NC})"

./sentinel --enroll --camera "$CAMERA_INDEX"
if [ $? -eq 0 ]; then
    echo -e "${GREEN}  ✓ Face enrolled!${NC}"
else
    echo -e "${RED}  ✗ Failed! Re-run: ./sentinel --enroll${NC}"
fi

# ═══════════════════════════════════════
# Step 8: Security Questions
# ═══════════════════════════════════════
echo ""
echo -e "${GREEN}${BOLD}[8/9] Security Questions (Kill Switch)${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"
echo ""
read -p "$(echo -e ${BOLD}Setup security questions now? [Y/n]: ${NC})" SEC_CHOICE
if [[ ! "$SEC_CHOICE" =~ ^[nN] ]]; then
    ./sentinel --setup-security
fi

# ═══════════════════════════════════════
# Step 9: Auto-start
# ═══════════════════════════════════════
echo ""
echo -e "${GREEN}${BOLD}[9/9] Auto-start Service${NC}"
echo -e "${CYAN}═══════════════════════════════════════${NC}"
read -p "$(echo -e ${BOLD}Enable auto-start on login? [Y/n]: ${NC})" AUTO_CHOICE

if [[ ! "$AUTO_CHOICE" =~ ^[nN] ]]; then
    CURRENT_USER=$(whoami)
    sudo bash -c "cat > /etc/systemd/system/sentinel.service" << SVCEOF
[Unit]
Description=SENTINEL AI Security System
After=graphical.target

[Service]
Type=simple
User=${CURRENT_USER}
Environment=DISPLAY=:0
Environment=XAUTHORITY=/home/${CURRENT_USER}/.Xauthority
WorkingDirectory=${SENTINEL_DIR}
ExecStart=${SENTINEL_DIR}/sentinel --guard
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical.target
SVCEOF
    sudo systemctl daemon-reload
    sudo systemctl enable sentinel.service
    sudo systemctl start sentinel.service
    echo -e "${GREEN}  ✓ Auto-start enabled${NC}"
fi

# ═══════════════════════════════════════
# Done
# ═══════════════════════════════════════
echo ""
echo -e "${CYAN}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${GREEN}${BOLD}         SENTINEL Setup Complete!                      ${NC}${CYAN}║${NC}"
echo -e "${CYAN}╠═══════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}  Camera:       index ${CAMERA_INDEX}                                ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  Threshold:    ${FACE_THRESHOLD}                                   ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  No-face lock: ${NOFACE_SECONDS}s                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  Verify time:  ${VERIFY_TIMEOUT}s face / ${KILLSWITCH_TIMEOUT}s kill switch            ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  Telegram:     $([ "$TELEGRAM_ENABLED" = "true" ] && echo "ENABLED" || echo "disabled")                                ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                       ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}./sentinel --guard --preview${NC}  (start with view)      ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}./sentinel --test${NC}             (test camera)   t       ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}./sentinel --status${NC}           (show config)   t       ${CYAN}║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════╝${NC}"