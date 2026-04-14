## 👤 Author

**Soulcynics404**

- GitHub: [@Soulcynics404](https://github.com/Soulcynics404)
- Repository: [Sentinal-AI](https://github.com/Soulcynics404/Sentinal-AI)

---

## ⭐ Star this repo if you find it useful!

# 🛡️ SENTINEL AI — Personal Security System

![Version](https://img.shields.io/badge/version-3.2-blue)
![Language](https://img.shields.io/badge/language-C++17-orange)
![Platform](https://img.shields.io/badge/platform-Linux-green)
![License](https://img.shields.io/badge/license-MIT-yellow)

> AI-powered personal laptop security system that continuously monitors your face and auto-locks when an unauthorized person is detected.

![SENTINEL Banner](https://img.shields.io/badge/SENTINEL-AI_Security-red?style=for-the-badge&logo=shield)

---

## 🎯 Features

### Core Security
- **Real-time Face Recognition** — Uses dlib's ResNet for 128D face embeddings
- **Auto-Lock** — Locks screen when unknown face or no face detected
- **Camera Tamper Detection** — Detects covered, blurry, or dark camera
- **Multi-face Detection** — Locks when multiple unauthorized people detected

### Kill Switch System
- **3 Security Questions** — Intruders must answer all 3 correctly
- **Configurable Timeouts** — Separate timers for face verification and kill switch
- **Security Bypass** — Temporary bypass after correct kill switch answers

### Telegram Remote Control (27 commands)
- 📸 Live camera capture & screenshots
- 🔒 Remote lock/unlock
- ⏸️ Pause/resume face verification
- 🎥 Video recording (30s/60s)
- 👀 Continuous screen/camera monitoring
- ⏰ Temporary access with approval workflow
- 🔴 Remote kill switch
- 📋 Activity log viewing & export

### Activity Logging
- **CSV Activity Log** — Every security event logged with 16 data columns
- **Timestamped entries** — Millisecond precision
- **Exportable** — Download via Telegram or view in terminal
- **Auto-rotating** — Backup and clear via control panel

### Verification Popup
- Full-screen identity verification on unlock
- Live camera feed with face detection overlay
- Kill switch interface (keyboard input for answers)
- Temp access request via Telegram approval
- Countdown timer with visual progress bar

---

## 📋 Requirements

- **OS:** Linux (tested on Kali Linux, Ubuntu, Debian)
- **Camera:** USB or built-in webcam
- **C++17** compiler (GCC 8+ or Clang 7+)
- **OpenCV 4.x**
- **dlib 19.x**
- **libcurl**
- **Optional:** Telegram account (for remote control)

---

## 🚀 Quick Start

### Automatic Setup (Recommended)

```bash
git clone https://github.com/Soulcynics404/sentinel-ai.git
cd sentinel-ai
chmod +x setup.sh reset.sh
./setup.sh

## 🏗️ System Architecture

```mermaid
flowchart TB
    subgraph INPUT["📷 Input Layer"]
        CAM[Web Camera]
        CFG[sentinel.conf]
        MODELS[AI Models<br/>Shape Predictor<br/>Face Recognition ResNet<br/>Haar Cascade]
    end

    subgraph CORE["🧠 Core Engine"]
        direction TB
        FE[Face Engine<br/>dlib HOG + ResNet<br/>128D Embeddings]
        TD[Tamper Detector<br/>Blur/Dark/Cover]
        SM[Security Manager<br/>3 Security Questions<br/>Hash Verification]
    end

    subgraph ANALYSIS["🔍 Analysis Layer"]
        direction LR
        OD[Owner Detected<br/>✅ SECURE]
        UD[Unknown Detected<br/>🔴 THREAT]
        MP[Multiple People<br/>⚠️ Smart Detection]
        NF[No Face<br/>⏰ Timeout Lock]
        CT[Camera Tamper<br/>🔒 Lock]
    end

    subgraph MULTIFACE["👥 Smart Multi-Person"]
        direction TB
        MC{Owner Present?}
        POPUP[MultiPerson Popup<br/>LOCK / DISMISS / SUPPRESS]
        SUPPRESS[Suppress Mode<br/>Track Owner Only]
        IMLOCK[Immediate Lock<br/>No Owner = Threat]
    end

    subgraph RESPONSE["🔐 Response Layer"]
        LOCK[Screen Lock<br/>loginctl / i3lock / xdotool]
        VP[Verification Popup<br/>Full-screen Face Check]
        KS[Kill Switch<br/>3 Questions Challenge]
        TA[Temp Access<br/>Telegram Approval]
        EVIDENCE[Evidence Capture<br/>Photos + Video]
        ALARM[Sound Alarm]
    end

    subgraph TELEGRAM["📱 Telegram Bot"]
        direction TB
        TGPOLL[Long Polling<br/>27+ Commands]
        TGNOTIFY[Notifications<br/>Photos + Alerts]
        TGREMOTE[Remote Control<br/>Lock/Pause/Capture]
        TGMONITOR[Monitoring<br/>Screenshots/Camera]
        TGACCESS[Access Control<br/>Approve/Deny/Revoke]
    end

    subgraph LOGGING["📊 Logging Layer"]
        LOGGER[Console + File Logger<br/>4 Levels]
        CSV[CSV Activity Log<br/>16 Columns<br/>All Events Tracked]
    end

    subgraph STORAGE["💾 Storage"]
        OWNERDAT[owner_face.dat<br/>Face Embeddings]
        SECDAT[security.dat<br/>Question Hashes]
        CSVFILE[sentinel_log.csv]
        EVIDIR[evidence/<br/>Intruder Photos<br/>Recordings]
    end

    subgraph SERVICE["⚙️ System Service"]
        SYSTEMD[systemd Service<br/>Auto-start on Login]
        SETUP[setup.sh<br/>9-Step Wizard]
        RESET[reset.sh<br/>15-Option Control Panel]
    end

    CAM --> FE
    CFG --> FE
    MODELS --> FE
    CAM --> TD

    FE --> OD
    FE --> UD
    FE --> MP
    FE --> NF
    TD --> CT

    MP --> MC
    MC -->|Yes| POPUP
    MC -->|No| IMLOCK
    POPUP -->|Dismiss| OD
    POPUP -->|Suppress| SUPPRESS
    POPUP -->|Lock/Timeout| LOCK
    SUPPRESS -->|Owner Leaves| LOCK
    IMLOCK --> LOCK

    UD --> LOCK
    NF --> LOCK
    CT --> LOCK

    LOCK --> VP
    VP -->|Face OK| OD
    VP -->|Press K| KS
    VP -->|Press T| TA
    VP -->|Timeout| LOCK
    KS -->|Pass| OD
    KS -->|Fail| LOCK
    TA -->|Approved| OD
    TA -->|Denied| LOCK

    LOCK --> EVIDENCE
    LOCK --> ALARM
    LOCK --> TGNOTIFY

    TGPOLL --> TGREMOTE
    TGPOLL --> TGMONITOR
    TGPOLL --> TGACCESS
    TGREMOTE --> LOCK
    TGACCESS --> TA

    OD --> LOGGER
    UD --> LOGGER
    LOCK --> LOGGER
    VP --> LOGGER
    LOGGER --> CSV

    FE --> OWNERDAT
    SM --> SECDAT
    CSV --> CSVFILE
    EVIDENCE --> EVIDIR

    SYSTEMD --> CAM
    SETUP --> CFG
    RESET --> CFG

    style INPUT fill:#1a1a2e,stroke:#0ff,color:#fff
    style CORE fill:#16213e,stroke:#0ff,color:#fff
    style ANALYSIS fill:#0f3460,stroke:#0ff,color:#fff
    style MULTIFACE fill:#533483,stroke:#ff0,color:#fff
    style RESPONSE fill:#e94560,stroke:#fff,color:#fff
    style TELEGRAM fill:#0d47a1,stroke:#0ff,color:#fff
    style LOGGING fill:#1b5e20,stroke:#0f0,color:#fff
    style STORAGE fill:#4a148c,stroke:#e040fb,color:#fff
    style SERVICE fill:#37474f,stroke:#90a4ae,color:#fff
```