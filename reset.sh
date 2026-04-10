#!/bin/bash
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

SENTINEL_DIR="$(cd "$(dirname "\$0")" && pwd)"
CONF_FILE="${SENTINEL_DIR}/sentinel.conf"
CSV_FILE="${SENTINEL_DIR}/data/sentinel_log.csv"

echo -e "${CYAN}"
echo "╔═══════════════════════════════════════════════════╗"
echo "║          SENTINEL — Control Panel v3.2            ║"
echo "╚═══════════════════════════════════════════════════╝"
echo -e "${NC}"

echo "  1)  Re-enroll face"
echo "  2)  Reset security questions"
echo "  3)  Delete evidence photos"
echo "  4)  Change timers & thresholds"
echo "  5)  Configure Telegram"
echo "  6)  Toggle sound alarm"
echo "  7)  Toggle evidence saving"
echo "  8)  Disable auto-start service"
echo "  9)  Enable auto-start service"
echo "  10) View current config"
echo "  11) View logs"
echo "  12) FULL RESET (keep models)"
echo "  13) COMPLETE UNINSTALL"
echo "  14) View last 30 CSV activity log entries"
echo "  15) Clear CSV activity log"
echo "  0)  Cancel"
echo ""
read -p "Choose [0-15]: " CHOICE

# Helper to update config value
update_conf() {
    local key="\$1" val="\$2"
    if grep -q "^${key}=" "$CONF_FILE" 2>/dev/null; then
        sed -i "s|^${key}=.*|${key}=${val}|" "$CONF_FILE"
    else
        echo "${key}=${val}" >> "$CONF_FILE"
    fi
    echo -e "${GREEN}  ✓ ${key} = ${val}${NC}"
}

case "$CHOICE" in
    1)
        echo -e "${YELLOW}Re-enrolling face...${NC}"
        rm -f "$SENTINEL_DIR/data/owner_face.dat"
        cd "$SENTINEL_DIR"
        ./sentinel --enroll
        ;;
    2)
        echo -e "${YELLOW}Resetting security questions...${NC}"
        rm -f "$SENTINEL_DIR/data/security.dat"
        cd "$SENTINEL_DIR"
        ./sentinel --setup-security
        ;;
    3)
        rm -rf "$SENTINEL_DIR/data/evidence/"
        mkdir -p "$SENTINEL_DIR/data/evidence"
        echo -e "${GREEN}✓ Evidence deleted${NC}"
        ;;
    4)
        echo ""
        echo -e "${BOLD}Current values:${NC}"
        grep -E "^(noface_lock|verification_timeout|killswitch_timeout|security_disable|face_threshold|consecutive_unknown|lock_cooldown|tamper_frames)" "$CONF_FILE" 2>/dev/null
        echo ""

        echo -e "${BOLD}What to change?${NC}"
        echo "  a) No-face lock timer (seconds)"
        echo "  b) Face verification timeout (seconds)"
        echo "  c) Kill switch timeout (seconds)"
        echo "  d) Security bypass duration (minutes)"
        echo "  e) Face recognition threshold"
        echo "  f) Unknown frames before lock"
        echo "  g) Lock cooldown (seconds)"
        echo "  h) Tamper frames before lock"
        echo "  i) Change ALL"
        echo ""
        read -p "  Choose [a-i]: " TIMER_CHOICE

        case "$TIMER_CHOICE" in
            a)
                read -p "  No-face lock seconds [current: $(grep noface_lock_seconds "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "noface_lock_seconds" "$VAL"
                ;;
            b)
                read -p "  Verification timeout seconds [current: $(grep verification_timeout "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "verification_timeout" "$VAL"
                ;;
            c)
                read -p "  Kill switch timeout seconds [current: $(grep killswitch_timeout "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "killswitch_timeout" "$VAL"
                ;;
            d)
                read -p "  Bypass minutes [current: $(grep security_disable_minutes "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "security_disable_minutes" "$VAL"
                ;;
            e)
                read -p "  Face threshold (0.35=strict, 0.45=normal, 0.55=relaxed) [current: $(grep face_threshold "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "face_threshold" "$VAL"
                ;;
            f)
                read -p "  Unknown frames [current: $(grep consecutive_unknown_frames "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "consecutive_unknown_frames" "$VAL"
                ;;
            g)
                read -p "  Lock cooldown seconds [current: $(grep lock_cooldown_seconds "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "lock_cooldown_seconds" "$VAL"
                ;;
            h)
                read -p "  Tamper frames [current: $(grep tamper_frames_before_lock "$CONF_FILE" | cut -d= -f2)]: " VAL
                [ -n "$VAL" ] && update_conf "tamper_frames_before_lock" "$VAL"
                ;;
            i)
                read -p "  No-face lock seconds: " V; [ -n "$V" ] && update_conf "noface_lock_seconds" "$V"
                read -p "  Verification timeout: " V; [ -n "$V" ] && update_conf "verification_timeout" "$V"
                read -p "  Kill switch timeout: " V; [ -n "$V" ] && update_conf "killswitch_timeout" "$V"
                read -p "  Bypass minutes: " V; [ -n "$V" ] && update_conf "security_disable_minutes" "$V"
                read -p "  Face threshold: " V; [ -n "$V" ] && update_conf "face_threshold" "$V"
                read -p "  Unknown frames: " V; [ -n "$V" ] && update_conf "consecutive_unknown_frames" "$V"
                read -p "  Lock cooldown: " V; [ -n "$V" ] && update_conf "lock_cooldown_seconds" "$V"
                read -p "  Tamper frames: " V; [ -n "$V" ] && update_conf "tamper_frames_before_lock" "$V"
                ;;
        esac
        echo -e "${YELLOW}  Restart sentinel for changes to take effect${NC}"
        ;;
    5)
        echo ""
        read -p "  Enable Telegram? [y/n]: " TG
        if [[ "$TG" =~ ^[yY] ]]; then
            read -p "  Bot Token: " TOKEN
            read -p "  Chat ID: " CHATID
            update_conf "telegram_enabled" "true"
            update_conf "telegram_bot_token" "$TOKEN"
            update_conf "telegram_chat_id" "$CHATID"
            echo -e "${GREEN}  ✓ Telegram configured${NC}"
        else
            update_conf "telegram_enabled" "false"
            echo -e "${GREEN}  ✓ Telegram disabled${NC}"
        fi
        ;;
    6)
        CURRENT=$(grep "^sound_alarm=" "$CONF_FILE" | cut -d= -f2)
        if [ "$CURRENT" = "true" ]; then
            update_conf "sound_alarm" "false"
            echo -e "${GREEN}  Sound alarm: OFF${NC}"
        else
            update_conf "sound_alarm" "true"
            echo -e "${GREEN}  Sound alarm: ON${NC}"
        fi
        ;;
    7)
        CURRENT=$(grep "^save_evidence=" "$CONF_FILE" | cut -d= -f2)
        if [ "$CURRENT" = "true" ]; then
            update_conf "save_evidence" "false"
            echo -e "${GREEN}  Evidence saving: OFF${NC}"
        else
            update_conf "save_evidence" "true"
            echo -e "${GREEN}  Evidence saving: ON${NC}"
        fi
        ;;
    8)
        sudo systemctl stop sentinel.service 2>/dev/null
        sudo systemctl disable sentinel.service 2>/dev/null
        echo -e "${GREEN}✓ Auto-start disabled${NC}"
        ;;
    9)
        sudo systemctl enable sentinel.service 2>/dev/null
        sudo systemctl start sentinel.service 2>/dev/null
        echo -e "${GREEN}✓ Auto-start enabled${NC}"
        ;;
    10)
        echo ""
        cd "$SENTINEL_DIR"
        ./sentinel --status
        ;;
    11)
        echo ""
        tail -50 "$SENTINEL_DIR/sentinel.log"
        ;;
    12)
        echo -e "${RED}This deletes ALL data except models.${NC}"
        read -p "Type YES to confirm: " CONFIRM
        if [ "$CONFIRM" = "YES" ]; then
            sudo systemctl stop sentinel.service 2>/dev/null
            sudo systemctl disable sentinel.service 2>/dev/null
            rm -f "$SENTINEL_DIR/data/owner_face.dat"
            rm -f "$SENTINEL_DIR/data/security.dat"
            rm -f "$SENTINEL_DIR/data/sentinel_log.csv"
            rm -rf "$SENTINEL_DIR/data/evidence/"
            rm -f "$SENTINEL_DIR/sentinel.conf"
            rm -f "$SENTINEL_DIR/sentinel.log"
            rm -f "$SENTINEL_DIR/setup.log"
            mkdir -p "$SENTINEL_DIR/data/evidence"
            echo -e "${GREEN}✓ Full reset done. Run ./setup.sh to start fresh${NC}"
        fi
        ;;
    13)
        echo -e "${RED}${BOLD}COMPLETE UNINSTALL — removes EVERYTHING${NC}"
        read -p "Type UNINSTALL to confirm: " CONFIRM
        if [ "$CONFIRM" = "UNINSTALL" ]; then
            sudo systemctl stop sentinel.service 2>/dev/null
            sudo systemctl disable sentinel.service 2>/dev/null
            sudo rm -f /etc/systemd/system/sentinel.service
            sudo systemctl daemon-reload
            rm -rf "$SENTINEL_DIR/data"
            rm -rf "$SENTINEL_DIR/build"
            rm -rf "$SENTINEL_DIR/models"
            rm -f "$SENTINEL_DIR/sentinel"
            rm -f "$SENTINEL_DIR/sentinel.conf"
            rm -f "$SENTINEL_DIR/sentinel.log"
            rm -f "$SENTINEL_DIR/setup.log"
            echo -e "${GREEN}✓ SENTINEL completely uninstalled${NC}"
            echo -e "${YELLOW}  Delete ~/sentinel/ folder manually to remove scripts${NC}"
        fi
        ;;
    14)
        echo ""
        echo -e "${BOLD}Last 30 CSV Activity Log Entries:${NC}"
        echo -e "${CYAN}════════════════════════════════════════════════════════${NC}"
        if [ -f "$CSV_FILE" ]; then
            # Print header
            HEAD=$(head -1 "$CSV_FILE")
            echo -e "${YELLOW}${HEAD}${NC}"
            echo "────────────────────────────────────────────────────────"
            # Print last 30 entries (skip header)
            tail -n +2 "$CSV_FILE" | tail -30 | while IFS=',' read -r ts event result fd fc threat vmethod vresult epath tgsent tagranted tadur sdur camidx ip uptime; do
                # Color code by event type
                case "$event" in
                    *OWNER*|*GUARD_START*|*VERIFICATION_SUCCESS*|*KILLSWITCH_PASSED*)
                        COLOR="${GREEN}" ;;
                    *UNKNOWN*|*TAMPER*|*FAILED*|*MULTIPLE*|*KILLSWITCH_FAILED*)
                        COLOR="${RED}" ;;
                    *TEMP_ACCESS*|*REMOTE*|*TELEGRAM*)
                        COLOR="${YELLOW}" ;;
                    *)
                        COLOR="${NC}" ;;
                esac
                # Extract time part from timestamp
                TIME_PART="${ts:11}"
                printf "${COLOR}[%-12s] %-25s %-20s %s${NC}\n" "$TIME_PART" "$event" "$result" "$threat"
            done
            echo ""
            TOTAL=$(( $(wc -l < "$CSV_FILE") - 1 ))
            echo -e "${BOLD}Total entries: ${TOTAL}${NC}"
        else
            echo -e "${RED}No CSV log file found at: ${CSV_FILE}${NC}"
        fi
        echo ""
        ;;
    15)
        echo ""
        echo -e "${YELLOW}Clear CSV Activity Log${NC}"
        if [ -f "$CSV_FILE" ]; then
            TOTAL=$(( $(wc -l < "$CSV_FILE") - 1 ))
            echo -e "  Current entries: ${BOLD}${TOTAL}${NC}"
            read -p "  Type CLEAR to confirm: " CONFIRM
            if [ "$CONFIRM" = "CLEAR" ]; then
                # Backup old log
                BACKUP="${CSV_FILE}.backup_$(date +%Y%m%d_%H%M%S)"
                cp "$CSV_FILE" "$BACKUP"
                echo -e "${GREEN}  ✓ Backup saved: ${BACKUP}${NC}"
                # Create fresh CSV with headers
                head -1 "$CSV_FILE" > "${CSV_FILE}.tmp"
                mv "${CSV_FILE}.tmp" "$CSV_FILE"
                echo -e "${GREEN}  ✓ CSV log cleared (headers preserved)${NC}"
            else
                echo "  Cancelled."
            fi
        else
            echo -e "${RED}  No CSV log file found${NC}"
        fi
        ;;
    *)
        echo "Cancelled."
        ;;
esac