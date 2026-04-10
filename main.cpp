/*
 * SENTINEL v3.2 Phase 3.2
 * Features: Face Recognition, Kill Switch, Telegram Bot, Temp Access,
 *           Video Recording, Continuous Monitoring, Auto-start,
 *           CSV Activity Log
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <cstdio>

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/dnn.h>
#include <dlib/opencv.h>
#include <dlib/image_io.h>
#include <dlib/serialize.h>

#include <curl/curl.h>

namespace fs = std::filesystem;

template <template <int,template<typename>class,int,typename> class block,int N,template<typename>class BN,typename SUBNET>
using residual = dlib::add_prev1<block<N,BN,1,dlib::tag1<SUBNET>>>;
template <template <int,template<typename>class,int,typename> class block,int N,template<typename>class BN,typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<2,2,2,2,dlib::skip1<dlib::tag2<block<N,BN,2,dlib::tag1<SUBNET>>>>>>;
template <int N,template<typename> class BN,int stride,typename SUBNET>
using block = BN<dlib::con<N,3,3,1,1,dlib::relu<BN<dlib::con<N,3,3,stride,stride,SUBNET>>>>>;
template <int N,typename SUBNET> using ares = dlib::relu<residual<block,N,dlib::affine,SUBNET>>;
template <int N,typename SUBNET> using ares_down = dlib::relu<residual_down<block,N,dlib::affine,SUBNET>>;
template <typename SUBNET> using alevel0 = ares_down<256,SUBNET>;
template <typename SUBNET> using alevel1 = ares<256,ares<256,ares_down<256,SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128,ares<128,ares_down<128,SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64,ares<64,ares<64,ares_down<64,SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32,ares<32,ares<32,SUBNET>>>;
using anet_type = dlib::loss_metric<dlib::fc_no_bias<128,dlib::avg_pool_everything<
    alevel0<alevel1<alevel2<alevel3<alevel4<dlib::max_pool<3,3,2,2,
    dlib::relu<dlib::affine<dlib::con<32,7,7,2,2,dlib::input_rgb_image_sized<150>
    >>>>>>>>>>>>;

namespace Color {
    const std::string RED="\033[0;31m",GREEN="\033[0;32m",YELLOW="\033[1;33m";
    const std::string CYAN="\033[0;36m",MAGENTA="\033[0;35m",BOLD="\033[1m",NC="\033[0m";
}

struct Config {
    std::string base_dir,model_dir,data_dir,evidence_dir;
    std::string log_file,owner_face_file,config_file,security_file;
    std::string csv_log_file;  // NEW: CSV activity log path
    std::string shape_predictor_path,face_rec_model_path,haar_cascade_path;
    int camera_index=1,frame_skip=3;
    bool show_preview=false;
    double face_recognition_threshold=0.55;
    int enrollment_samples=20,consecutive_unknown_frames=5,max_allowed_faces=1;
    double lock_cooldown_seconds=3.0;
    bool save_evidence=true;
    int noface_lock_seconds=45;
    double tamper_blur_threshold=5.0,tamper_dark_threshold=5.0;
    int tamper_frames_before_lock=30;
    bool telegram_enabled=false;
    std::string telegram_bot_token="",telegram_chat_id="";
    bool sound_alarm=false;
    int verification_timeout=30;
    int killswitch_timeout=120;
    int security_disable_minutes=30;

    void init(const std::string& exe) {
        base_dir=fs::path(exe).parent_path().string();
        if(base_dir.empty())base_dir=".";
        model_dir=base_dir+"/models";data_dir=base_dir+"/data";
        evidence_dir=data_dir+"/evidence";log_file=base_dir+"/sentinel.log";
        owner_face_file=data_dir+"/owner_face.dat";config_file=base_dir+"/sentinel.conf";
        security_file=data_dir+"/security.dat";
        csv_log_file=data_dir+"/sentinel_log.csv";  // NEW
        shape_predictor_path=model_dir+"/shape_predictor_68_face_landmarks.dat";
        face_rec_model_path=model_dir+"/dlib_face_recognition_resnet_model_v1.dat";
        haar_cascade_path=model_dir+"/haarcascade_frontalface_default.xml";
    }

    void load() {
        if(!fs::exists(config_file))return;
        std::ifstream f(config_file);std::string line;
        while(std::getline(f,line)){
            if(line.empty()||line[0]=='#')continue;
            auto eq=line.find('=');if(eq==std::string::npos)continue;
            std::string k=line.substr(0,eq),v=line.substr(eq+1);
            k.erase(0,k.find_first_not_of(" \t"));k.erase(k.find_last_not_of(" \t")+1);
            v.erase(0,v.find_first_not_of(" \t"));v.erase(v.find_last_not_of(" \t")+1);
            if(k=="camera_index")camera_index=std::stoi(v);
            else if(k=="frame_skip")frame_skip=std::stoi(v);
            else if(k=="show_preview")show_preview=(v=="true");
            else if(k=="face_threshold")face_recognition_threshold=std::stod(v);
            else if(k=="enrollment_samples")enrollment_samples=std::stoi(v);
            else if(k=="consecutive_unknown_frames")consecutive_unknown_frames=std::stoi(v);
            else if(k=="max_allowed_faces")max_allowed_faces=std::stoi(v);
            else if(k=="lock_cooldown_seconds")lock_cooldown_seconds=std::stod(v);
            else if(k=="save_evidence")save_evidence=(v=="true");
            else if(k=="noface_lock_seconds")noface_lock_seconds=std::stoi(v);
            else if(k=="tamper_blur_threshold")tamper_blur_threshold=std::stod(v);
            else if(k=="tamper_dark_threshold")tamper_dark_threshold=std::stod(v);
            else if(k=="tamper_frames_before_lock")tamper_frames_before_lock=std::stoi(v);
            else if(k=="telegram_enabled")telegram_enabled=(v=="true");
            else if(k=="telegram_bot_token")telegram_bot_token=v;
            else if(k=="telegram_chat_id")telegram_chat_id=v;
            else if(k=="sound_alarm")sound_alarm=(v=="true");
            else if(k=="verification_timeout")verification_timeout=std::stoi(v);
            else if(k=="killswitch_timeout")killswitch_timeout=std::stoi(v);
            else if(k=="security_disable_minutes")security_disable_minutes=std::stoi(v);
        }
    }
};

static Config g_config;
static std::atomic<bool> g_running{true};

// ═══════════════════════════════════════
// Logger (unchanged)
// ═══════════════════════════════════════
class Logger {
public:
    enum Level{INFO,WARN,ERR,SECURITY};
    static Logger& inst(){static Logger l;return l;}
    void init(const std::string& p){std::lock_guard<std::mutex>g(mtx);fi.open(p,std::ios::app);}
    void log(Level lv,const std::string& msg){
        std::lock_guard<std::mutex>g(mtx);
        auto now=std::chrono::system_clock::now();auto t=std::chrono::system_clock::to_time_t(now);
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())%1000;
        std::tm buf;localtime_r(&t,&buf);std::ostringstream ts;
        ts<<std::put_time(&buf,"%Y-%m-%d %H:%M:%S")<<'.'<<std::setfill('0')<<std::setw(3)<<ms.count();
        std::string lvs,col;
        switch(lv){case INFO:lvs="INFO    ";col=Color::GREEN;break;case WARN:lvs="WARNING ";col=Color::YELLOW;break;
                   case ERR:lvs="ERROR   ";col=Color::RED;break;case SECURITY:lvs="SECURITY";col=Color::MAGENTA;break;}
        std::string line="["+ts.str()+"] ["+lvs+"] "+msg;
        std::cout<<col<<line<<Color::NC<<std::endl;
        if(fi.is_open()){fi<<line<<std::endl;fi.flush();}
    }
    void info(const std::string&m){log(INFO,m);}void warn(const std::string&m){log(WARN,m);}
    void error(const std::string&m){log(ERR,m);}void security(const std::string&m){log(SECURITY,m);}
private:Logger()=default;std::mutex mtx;std::ofstream fi;
};
#define LOG_INFO(m) Logger::inst().info(m)
#define LOG_WARN(m) Logger::inst().warn(m)
#define LOG_ERROR(m) Logger::inst().error(m)
#define LOG_SECURITY(m) Logger::inst().security(m)

// ═══════════════════════════════════════
// NEW: CSV Activity Logger
// ═══════════════════════════════════════
class CSVLogger {
public:
    static CSVLogger& inst() { static CSVLogger l; return l; }

    void init(const std::string& path) {
        std::lock_guard<std::mutex> g(mtx);
        filepath = path;
        bool exists = fs::exists(path) && fs::file_size(path) > 0;
        file.open(path, std::ios::app);
        if (!exists && file.is_open()) {
            file << "timestamp,event_type,result,face_distance,face_count,"
                 << "threat_reason,verification_method,verification_result,"
                 << "evidence_path,telegram_sent,temp_access_granted_to,"
                 << "temp_access_duration_min,session_duration_sec,camera_index,"
                 << "ip_address,system_uptime\n";
            file.flush();
        }
        cached_ip = getIPAddress();
        LOG_INFO("CSV Activity Log initialized: " + path);
    }

    void log(const std::string& event_type,
             const std::string& result = "",
             double face_distance = -1.0,
             int face_count = -1,
             const std::string& threat_reason = "",
             const std::string& verification_method = "",
             const std::string& verification_result = "",
             const std::string& evidence_path = "",
             bool telegram_sent = false,
             const std::string& temp_access_granted_to = "",
             int temp_access_duration_min = -1,
             double session_duration_sec = -1.0) {
        std::lock_guard<std::mutex> g(mtx);
        if (!file.is_open()) return;

        file << getTimestamp() << ","
             << esc(event_type) << ","
             << esc(result) << ","
             << (face_distance >= 0 ? std::to_string(face_distance).substr(0, 6) : "") << ","
             << (face_count >= 0 ? std::to_string(face_count) : "") << ","
             << esc(threat_reason) << ","
             << esc(verification_method) << ","
             << esc(verification_result) << ","
             << esc(evidence_path) << ","
             << (telegram_sent ? "true" : "false") << ","
             << esc(temp_access_granted_to) << ","
             << (temp_access_duration_min >= 0 ? std::to_string(temp_access_duration_min) : "") << ","
             << (session_duration_sec >= 0 ? std::to_string((int)session_duration_sec) : "") << ","
             << g_config.camera_index << ","
             << cached_ip << ","
             << getSystemUptime() << "\n";
        file.flush();
    }

    std::string getLastNFormatted(int n) const {
        std::lock_guard<std::mutex> g(mtx);
        std::ifstream f(filepath);
        if (!f.is_open()) return "No CSV log found";
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(f, line)) lines.push_back(line);

        std::string result;
        int start = std::max(1, (int)lines.size() - n);
        for (int i = start; i < (int)lines.size(); i++) {
            std::istringstream iss(lines[i]);
            std::string ts, event, res, fd, fc, threat;
            std::getline(iss, ts, ',');
            std::getline(iss, event, ',');
            std::getline(iss, res, ',');
            std::getline(iss, fd, ',');
            std::getline(iss, fc, ',');
            std::getline(iss, threat, ',');

            std::string time_short = ts.length() > 11 ? ts.substr(11) : ts;
            result += "[" + time_short + "] " + event;
            if (!res.empty()) result += " (" + res + ")";
            if (!threat.empty()) result += " - " + threat;
            result += "\n";
        }
        if (result.empty()) result = "No entries yet";
        return result;
    }

    std::string getFilePath() const { return filepath; }

    int getEntryCount() const {
        std::lock_guard<std::mutex> g(mtx);
        std::ifstream f(filepath);
        int count = 0;
        std::string line;
        while (std::getline(f, line)) count++;
        return std::max(0, count - 1);
    }

private:
    CSVLogger() = default;
    mutable std::mutex mtx;
    std::ofstream file;
    std::string filepath;
    std::string cached_ip;

    static std::string getIPAddress() {
        char buf[256] = {0};
        std::string result;
        FILE* pipe = popen("hostname -I 2>/dev/null", "r");
        if (pipe) {
            if (fgets(buf, sizeof(buf), pipe)) result = buf;
            pclose(pipe);
        }
        result.erase(0, result.find_first_not_of(" \t\n\r"));
        auto sp = result.find(' ');
        if (sp != std::string::npos) result = result.substr(0, sp);
        result.erase(result.find_last_not_of(" \t\n\r") + 1);
        return result.empty() ? "unknown" : result;
    }

    static std::string getSystemUptime() {
        std::ifstream f("/proc/uptime");
        std::string up;
        if (f.is_open()) f >> up;
        return up.empty() ? "0" : up;
    }

    static std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::tm buf;
        localtime_r(&t, &buf);
        std::ostringstream oss;
        oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << "."
            << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    static std::string esc(const std::string& s) {
        if (s.empty()) return "";
        if (s.find(',') != std::string::npos || s.find('"') != std::string::npos ||
            s.find('\n') != std::string::npos) {
            std::string e = "\"";
            for (char c : s) {
                if (c == '"') e += "\"\"";
                else e += c;
            }
            e += "\"";
            return e;
        }
        return s;
    }
};

#define CSV_LOG(...) CSVLogger::inst().log(__VA_ARGS__)

// ═══════════════════════════════════════
// Telegram (unchanged)
// ═══════════════════════════════════════
class Telegram {
public:
    static size_t wCb(void*,size_t s,size_t n,void*){return s*n;}
    static void sendMessage(const std::string&text){
        if(!g_config.telegram_enabled)return;
        std::thread([text](){CURL*c=curl_easy_init();if(!c)return;
            char*esc=curl_easy_escape(c,text.c_str(),0);
            std::string url="https://api.telegram.org/bot"+g_config.telegram_bot_token+"/sendMessage";
            std::string post="chat_id="+g_config.telegram_chat_id+"&text="+std::string(esc)+"&parse_mode=HTML";
            curl_free(esc);curl_easy_setopt(c,CURLOPT_URL,url.c_str());
            curl_easy_setopt(c,CURLOPT_POSTFIELDS,post.c_str());curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wCb);
            curl_easy_setopt(c,CURLOPT_TIMEOUT,10L);curl_easy_perform(c);curl_easy_cleanup(c);
        }).detach();
    }
    static void sendPhoto(const std::string&path,const std::string&caption){
        if(!g_config.telegram_enabled)return;
        std::thread([path,caption](){CURL*c=curl_easy_init();if(!c)return;
            std::string url="https://api.telegram.org/bot"+g_config.telegram_bot_token+"/sendPhoto";
            curl_mime*mime=curl_mime_init(c);curl_mimepart*p=curl_mime_addpart(mime);
            curl_mime_name(p,"chat_id");curl_mime_data(p,g_config.telegram_chat_id.c_str(),CURL_ZERO_TERMINATED);
            p=curl_mime_addpart(mime);curl_mime_name(p,"photo");curl_mime_filedata(p,path.c_str());
            p=curl_mime_addpart(mime);curl_mime_name(p,"caption");curl_mime_data(p,caption.c_str(),CURL_ZERO_TERMINATED);
            curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_MIMEPOST,mime);
            curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wCb);curl_easy_setopt(c,CURLOPT_TIMEOUT,30L);
            curl_easy_perform(c);curl_mime_free(mime);curl_easy_cleanup(c);
        }).detach();
    }
    static void sendDocument(const std::string&path,const std::string&caption){
        if(!g_config.telegram_enabled)return;
        std::thread([path,caption](){CURL*c=curl_easy_init();if(!c)return;
            std::string url="https://api.telegram.org/bot"+g_config.telegram_bot_token+"/sendDocument";
            curl_mime*mime=curl_mime_init(c);curl_mimepart*p=curl_mime_addpart(mime);
            curl_mime_name(p,"chat_id");curl_mime_data(p,g_config.telegram_chat_id.c_str(),CURL_ZERO_TERMINATED);
            p=curl_mime_addpart(mime);curl_mime_name(p,"document");curl_mime_filedata(p,path.c_str());
            if(!caption.empty()){p=curl_mime_addpart(mime);curl_mime_name(p,"caption");
                curl_mime_data(p,caption.c_str(),CURL_ZERO_TERMINATED);}
            curl_easy_setopt(c,CURLOPT_URL,url.c_str());curl_easy_setopt(c,CURLOPT_MIMEPOST,mime);
            curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wCb);curl_easy_setopt(c,CURLOPT_TIMEOUT,60L);
            curl_easy_perform(c);curl_mime_free(mime);curl_easy_cleanup(c);
        }).detach();
    }
};

namespace SoundAlarm {
    void play() {
        if(!g_config.sound_alarm) return;
        std::thread([](){
            system("paplay /usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga 2>/dev/null");
        }).detach();
    }
}

namespace SystemAction {
    void lockScreen() {
        LOG_SECURITY(">>> LOCKING <<<");
        if(system("loginctl lock-session 2>/dev/null") == 0) {
            LOG_INFO("Locked loginctl");
            return;
        }
        if(system("xdg-screensaver lock 2>/dev/null") == 0) return;
        if(system("i3lock -c 1a1a2e -e -f 2>/dev/null") == 0) return;
        system("xdotool key super+l 2>/dev/null");
    }
    // ═══════════════════════════════════════
// (Continuing from Part 1 — SystemAction::saveEvidence)
// ═══════════════════════════════════════
    std::string saveEvidence(const cv::Mat& frame, const std::string& dir) {
        auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm buf;
        localtime_r(&t, &buf);
        std::ostringstream o;
        o << dir << "/intruder_" << std::put_time(&buf, "%Y%m%d_%H%M%S") << ".jpg";
        std::string path = o.str();
        if(cv::imwrite(path, frame)) {
            LOG_SECURITY("Evidence:" + path);
            CSV_LOG("EVIDENCE_SAVED", "success", -1, -1, "", "", "", path, g_config.telegram_enabled);
            return path;
        }
        return "";
    }
}

// ═══════════════════════════════════════
// Telegram Bot — Remote Control with /logs and /exportlog
// ═══════════════════════════════════════
class TelegramBot {
public:
    static std::atomic<bool> guard_paused;
    static std::mutex frame_mutex;
    static cv::Mat shared_frame;

    static std::atomic<bool> temp_access_active;
    static std::atomic<int> temp_access_minutes;
    static std::chrono::steady_clock::time_point temp_access_until;
    static std::atomic<bool> temp_access_pending;

    static std::atomic<bool> remote_killswitch;

    static std::atomic<bool> monitor_screenshots;
    static std::atomic<bool> monitor_camera;
    static std::atomic<int> monitor_interval;

    static std::atomic<bool> recording_active;
    static std::atomic<int> recording_seconds;

    static void updateFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(frame_mutex);
        frame.copyTo(shared_frame);
    }

    static cv::Mat getFrame() {
        std::lock_guard<std::mutex> lock(frame_mutex);
        return shared_frame.clone();
    }

    static size_t writeData(void* ptr, size_t size, size_t nmemb, std::string* data) {
        data->append((char*)ptr, size * nmemb);
        return size * nmemb;
    }

    static std::string apiCall(const std::string& method, const std::string& params = "") {
        CURL* c = curl_easy_init();
        if (!c) return "";
        std::string url = "https://api.telegram.org/bot" + g_config.telegram_bot_token + "/" + method;
        if (!params.empty()) url += "?" + params;
        std::string response;
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 35L);
        curl_easy_perform(c);
        curl_easy_cleanup(c);
        return response;
    }

    static void sendMsg(const std::string& text) {
        CURL* c = curl_easy_init();
        if (!c) return;
        char* esc = curl_easy_escape(c, text.c_str(), 0);
        std::string url = "https://api.telegram.org/bot" + g_config.telegram_bot_token + "/sendMessage";
        std::string post = "chat_id=" + g_config.telegram_chat_id + "&text=" + std::string(esc) + "&parse_mode=HTML";
        curl_free(esc);
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, post.c_str());
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, Telegram::wCb);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 10L);
        curl_easy_perform(c);
        curl_easy_cleanup(c);
    }

    static void sendPhotoFile(const std::string& path, const std::string& caption = "") {
        CURL* c = curl_easy_init();
        if (!c) return;
        std::string url = "https://api.telegram.org/bot" + g_config.telegram_bot_token + "/sendPhoto";
        curl_mime* mime = curl_mime_init(c);
        curl_mimepart* p = curl_mime_addpart(mime);
        curl_mime_name(p, "chat_id");
        curl_mime_data(p, g_config.telegram_chat_id.c_str(), CURL_ZERO_TERMINATED);
        p = curl_mime_addpart(mime);
        curl_mime_name(p, "photo");
        curl_mime_filedata(p, path.c_str());
        if (!caption.empty()) {
            p = curl_mime_addpart(mime);
            curl_mime_name(p, "caption");
            curl_mime_data(p, caption.c_str(), CURL_ZERO_TERMINATED);
        }
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, Telegram::wCb);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
        curl_easy_perform(c);
        curl_mime_free(mime);
        curl_easy_cleanup(c);
    }

    static void sendDocFile(const std::string& path, const std::string& caption = "") {
        CURL* c = curl_easy_init();
        if (!c) return;
        std::string url = "https://api.telegram.org/bot" + g_config.telegram_bot_token + "/sendDocument";
        curl_mime* mime = curl_mime_init(c);
        curl_mimepart* p = curl_mime_addpart(mime);
        curl_mime_name(p, "chat_id");
        curl_mime_data(p, g_config.telegram_chat_id.c_str(), CURL_ZERO_TERMINATED);
        p = curl_mime_addpart(mime);
        curl_mime_name(p, "document");
        curl_mime_filedata(p, path.c_str());
        if (!caption.empty()) {
            p = curl_mime_addpart(mime);
            curl_mime_name(p, "caption");
            curl_mime_data(p, caption.c_str(), CURL_ZERO_TERMINATED);
        }
        curl_easy_setopt(c, CURLOPT_URL, url.c_str());
        curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, Telegram::wCb);
        curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);
        curl_easy_perform(c);
        curl_mime_free(mime);
        curl_easy_cleanup(c);
    }

    static void recordVideo(int seconds) {
        recording_active = true;
        std::string path = g_config.evidence_dir + "/recording.avi";
        LOG_INFO("Recording " + std::to_string(seconds) + "s video...");
        sendMsg("Recording " + std::to_string(seconds) + "s video...");

        cv::Mat first = getFrame();
        if (first.empty()) {
            sendMsg("No frame available for recording");
            recording_active = false;
            return;
        }

        cv::VideoWriter writer(path, cv::VideoWriter::fourcc('M','J','P','G'),
                               10, first.size());
        if (!writer.isOpened()) {
            sendMsg("Failed to create video file");
            recording_active = false;
            return;
        }

        auto start = std::chrono::steady_clock::now();
        while (g_running && recording_active) {
            double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= seconds) break;

            cv::Mat frame = getFrame();
            if (!frame.empty()) {
                writer.write(frame);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        writer.release();
        recording_active = false;

        LOG_INFO("Recording complete: " + path);
        Telegram::sendDocument(path, "Video recording (" + std::to_string(seconds) + "s)");
    }

    static void monitorScreenshots(int interval) {
        LOG_INFO("Screenshot monitoring started — every " + std::to_string(interval) + "s");
        while (g_running && monitor_screenshots) {
            std::string path = g_config.evidence_dir + "/monitor_screen.png";
            std::string cmd = "scrot " + path + " 2>/dev/null";
            if (system(cmd.c_str()) == 0) {
                sendPhotoFile(path, "Screen monitor (" + std::to_string(interval) + "s interval)");
            }
            for (int i = 0; i < interval * 10 && monitor_screenshots && g_running; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        LOG_INFO("Screenshot monitoring stopped");
        sendMsg("Screenshot monitoring stopped");
    }

    static void monitorCamera(int interval) {
        LOG_INFO("Camera monitoring started — every " + std::to_string(interval) + "s");
        while (g_running && monitor_camera) {
            cv::Mat frame = getFrame();
            if (!frame.empty()) {
                std::string path = g_config.evidence_dir + "/monitor_cam.jpg";
                cv::imwrite(path, frame);
                sendPhotoFile(path, "Camera monitor (" + std::to_string(interval) + "s interval)");
            }
            for (int i = 0; i < interval * 10 && monitor_camera && g_running; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        LOG_INFO("Camera monitoring stopped");
        sendMsg("Camera monitoring stopped");
    }

    static void pollLoop(int camIdx) {
        if (!g_config.telegram_enabled) return;
        LOG_INFO("Telegram bot started — polling for commands");

        long last_update_id = 0;

        while (g_running) {
            std::string params = "offset=" + std::to_string(last_update_id + 1) + "&timeout=30";
            std::string response = apiCall("getUpdates", params);

            if (response.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }

            size_t searchPos = 0;
            while (true) {
                auto uidPos = response.find("\"update_id\"", searchPos);
                if (uidPos == std::string::npos) break;

                auto colonPos = response.find(":", uidPos);
                auto commaPos = response.find(",", colonPos);
                std::string uidStr = response.substr(colonPos + 1, commaPos - colonPos - 1);
                long uid = std::stol(uidStr);
                if (uid > last_update_id) last_update_id = uid;

                auto chatIdPos = response.find("\"chat\"", uidPos);
                std::string senderChatId;
                if (chatIdPos != std::string::npos) {
                    auto idPos = response.find("\"id\"", chatIdPos);
                    if (idPos != std::string::npos && idPos < chatIdPos + 200) {
                        auto c1 = response.find(":", idPos);
                        auto c2 = response.find_first_of(",}", c1);
                        senderChatId = response.substr(c1 + 1, c2 - c1 - 1);
                        senderChatId.erase(0, senderChatId.find_first_not_of(" "));
                        senderChatId.erase(senderChatId.find_last_not_of(" ") + 1);
                    }
                }

                if (senderChatId != g_config.telegram_chat_id) {
                    searchPos = uidPos + 20;
                    continue;
                }

                auto textPos = response.find("\"text\"", uidPos);
                std::string msgText;
                if (textPos != std::string::npos && textPos < uidPos + 500) {
                    auto t1 = response.find("\"", textPos + 6);
                    auto t2 = response.find("\"", t1 + 1);
                    if (t1 != std::string::npos && t2 != std::string::npos)
                        msgText = response.substr(t1 + 1, t2 - t1 - 1);
                }

                if (!msgText.empty()) {
                    processCommand(msgText, camIdx);
                }

                searchPos = uidPos + 20;
            }
        }
        LOG_INFO("Telegram bot stopped");
    }

    static void processCommand(const std::string& cmd, int camIdx) {
        LOG_INFO("Telegram command: " + cmd);
        CSV_LOG("TELEGRAM_COMMAND", cmd);

        if (cmd == "/help") {
            sendMsg(
                "\xF0\x9F\x9B\xA1 <b>SENTINEL v3.2 Commands</b>\n\n"
                "<b>Basic:</b>\n"
                "/status — Current status\n"
                "/lock — Lock laptop now\n"
                "/pause — Pause face verification\n"
                "/resume — Resume face verification\n\n"
                "<b>Capture:</b>\n"
                "/capture — Camera photo\n"
                "/screenshot — Screen capture\n"
                "/evidence — Latest intruder photo\n"
                "/record — Record 30s video\n"
                "/record60 — Record 60s video\n\n"
                "<b>Monitoring:</b>\n"
                "/watchscreen5 — Screenshots every 5s\n"
                "/watchscreen10 — Screenshots every 10s\n"
                "/watchscreen15 — Screenshots every 15s\n"
                "/watchscreen20 — Screenshots every 20s\n"
                "/watchscreen35 — Screenshots every 35s\n"
                "/watchscreen60 — Screenshots every 60s\n"
                "/watchcam5 — Camera every 5s\n"
                "/watchcam10 — Camera every 10s\n"
                "/watchcam15 — Camera every 15s\n"
                "/watchcam20 — Camera every 20s\n"
                "/watchcam35 — Camera every 35s\n"
                "/watchcam60 — Camera every 60s\n"
                "/stopwatch — Stop all monitoring\n\n"
                "<b>Access:</b>\n"
                "/killswitch — Activate kill switch\n"
                "/tempaccess5 — 5 min temp access\n"
                "/tempaccess15 — 15 min temp access\n"
                "/tempaccess30 — 30 min temp access\n"
                "/tempaccess60 — 60 min temp access\n"
                "/revoke — Revoke temp access NOW\n"
                "/approve — Approve pending temp access\n"
                "/deny — Deny pending temp access\n\n"
                "<b>Logs:</b>\n"
                "/logs — Last 20 activity log entries\n"
                "/exportlog — Download full CSV log\n\n"
                "/help — This help"
            );
        }
        else if (cmd == "/status") {
            std::string paused = guard_paused ? "PAUSED" : "ACTIVE";
            std::string tempStr = "None";
            if (temp_access_active) {
                auto remaining = std::chrono::duration<double>(
                    temp_access_until - std::chrono::steady_clock::now()).count();
                int mins = std::max(0, (int)(remaining / 60));
                tempStr = "ACTIVE (" + std::to_string(mins) + "m left)";
            }
            std::string monStr = "OFF";
            if (monitor_screenshots) monStr = "Screenshots every " + std::to_string(monitor_interval.load()) + "s";
            else if (monitor_camera) monStr = "Camera every " + std::to_string(monitor_interval.load()) + "s";

            int csvEntries = CSVLogger::inst().getEntryCount();

            sendMsg(
                "\xF0\x9F\x9B\xA1 <b>SENTINEL v3.2 Status</b>\n\n"
                "Guard: " + paused + "\n"
                "Camera: " + std::to_string(g_config.camera_index) + "\n"
                "No-face lock: " + std::to_string(g_config.noface_lock_seconds) + "s\n"
                "Verify timeout: " + std::to_string(g_config.verification_timeout) + "s\n"
                "KillSwitch timeout: " + std::to_string(g_config.killswitch_timeout) + "s\n"
                "Temp access: " + tempStr + "\n"
                "Monitoring: " + monStr + "\n"
                "Recording: " + std::string(recording_active ? "YES" : "No") + "\n"
                "CSV Log entries: " + std::to_string(csvEntries)
            );
        }
        else if (cmd == "/lock") {
            sendMsg("\xF0\x9F\x94\x92 Locking laptop...");
            CSV_LOG("REMOTE_LOCK", "telegram");
            SystemAction::lockScreen();
        }
        else if (cmd == "/pause") {
            guard_paused = true;
            sendMsg("\xE2\xB8\x8F Face verification PAUSED\nUse /resume to re-enable");
            LOG_SECURITY("Telegram: Guard PAUSED by owner");
            CSV_LOG("REMOTE_PAUSE", "telegram");
        }
        else if (cmd == "/resume") {
            guard_paused = false;
            sendMsg("\xE2\x96\xB6 Face verification RESUMED");
            LOG_SECURITY("Telegram: Guard RESUMED by owner");
            CSV_LOG("REMOTE_RESUME", "telegram");
        }
        else if (cmd == "/capture") {
            cv::Mat frame = getFrame();
            if (!frame.empty()) {
                std::string path = g_config.evidence_dir + "/telegram_capture.jpg";
                cv::imwrite(path, frame);
                sendPhotoFile(path, "Live camera capture");
            } else {
                sendMsg("No frame available");
            }
        }
        else if (cmd == "/screenshot") {
            std::string path = g_config.evidence_dir + "/telegram_screenshot.png";
            std::string scrotCmd = "scrot " + path + " 2>/dev/null";
            if (system(scrotCmd.c_str()) == 0) {
                sendPhotoFile(path, "Screenshot");
            } else {
                sendMsg("Screenshot failed — is scrot installed?");
            }
        }
        else if (cmd == "/evidence") {
            std::string latest;
            std::filesystem::file_time_type latestTime;
            bool found = false;
            if (fs::exists(g_config.evidence_dir)) {
                for (auto& p : fs::directory_iterator(g_config.evidence_dir)) {
                    if (p.path().extension() == ".jpg" &&
                        p.path().filename().string().find("intruder") != std::string::npos) {
                        if (!found || p.last_write_time() > latestTime) {
                            latest = p.path().string();
                            latestTime = p.last_write_time();
                            found = true;
                        }
                    }
                }
            }
            if (found) sendPhotoFile(latest, "Latest intruder evidence");
            else sendMsg("No intruder evidence found");
        }
        else if (cmd == "/record") {
            if (recording_active) { sendMsg("Already recording!"); return; }
            std::thread(recordVideo, 30).detach();
        }
        else if (cmd == "/record60") {
            if (recording_active) { sendMsg("Already recording!"); return; }
            std::thread(recordVideo, 60).detach();
        }
        else if (cmd == "/killswitch") {
            remote_killswitch = true;
            sendMsg("\xF0\x9F\x94\xB4 <b>KILL SWITCH ACTIVATED</b>\n\n"
                    "Laptop will be locked.\n"
                    "Person must answer security questions to unlock.\n"
                    "Use /revoke to cancel temp access if any.");
            SystemAction::lockScreen();
            LOG_SECURITY("Telegram: KILL SWITCH activated remotely");
            CSV_LOG("REMOTE_KILLSWITCH", "telegram", -1, -1, "Remote activation");
        }
        else if (cmd == "/tempaccess" || cmd == "/tempaccess5" || cmd == "/tempaccess15" ||
                 cmd == "/tempaccess30" || cmd == "/tempaccess60") {
            int mins = 0;
            if (cmd == "/tempaccess5") mins = 5;
            else if (cmd == "/tempaccess15") mins = 15;
            else if (cmd == "/tempaccess30") mins = 30;
            else if (cmd == "/tempaccess60") mins = 60;
            else {
                sendMsg("Use /tempaccess5, /tempaccess15, /tempaccess30, or /tempaccess60");
                return;
            }
            temp_access_active = true;
            temp_access_minutes = mins;
            temp_access_until = std::chrono::steady_clock::now() + std::chrono::minutes(mins);
            guard_paused = true;
            sendMsg("\xE2\x9C\x85 <b>Temp access GRANTED</b>\n"
                    "Duration: " + std::to_string(mins) + " minutes\n"
                    "Face verification: PAUSED\n\n"
                    "Use /revoke to end early\n"
                    "Auto-expires in " + std::to_string(mins) + "m");
            LOG_SECURITY("Temp access granted: " + std::to_string(mins) + " minutes");
            CSV_LOG("TEMP_ACCESS_APPROVED", "granted", -1, -1, "", "", "",
                    "", true, "telegram_owner", mins);
        }
        else if (cmd == "/approve") {
            if (temp_access_pending) {
                temp_access_pending = false;
                temp_access_active = true;
                temp_access_until = std::chrono::steady_clock::now() +
                    std::chrono::minutes(temp_access_minutes.load());
                guard_paused = true;
                sendMsg("\xE2\x9C\x85 Access APPROVED for " +
                        std::to_string(temp_access_minutes.load()) + " minutes");
                LOG_SECURITY("Temp access APPROVED via Telegram");
                CSV_LOG("TEMP_ACCESS_APPROVED", "approved", -1, -1, "", "", "",
                        "", true, "popup_requester", temp_access_minutes.load());
            } else {
                sendMsg("No pending access request");
            }
        }
        else if (cmd == "/deny") {
            if (temp_access_pending) {
                temp_access_pending = false;
                sendMsg("\xE2\x9D\x8C Access DENIED — locking laptop");
                SystemAction::lockScreen();
                LOG_SECURITY("Temp access DENIED via Telegram");
                CSV_LOG("TEMP_ACCESS_DENIED", "denied", -1, -1, "", "", "",
                        "", true);
            } else {
                sendMsg("No pending access request");
            }
        }
        else if (cmd == "/revoke") {
            temp_access_active = false;
            temp_access_pending = false;
            guard_paused = false;
            sendMsg("\xF0\x9F\x94\x92 <b>Access REVOKED</b>\n"
                    "Face verification: RESUMED\n"
                    "Locking laptop NOW");
            SystemAction::lockScreen();
            LOG_SECURITY("Temp access REVOKED — locked");
            CSV_LOG("TEMP_ACCESS_REVOKED", "revoked", -1, -1, "", "", "",
                    "", true);
        }
        // NEW: /logs command — send last 20 CSV entries
        else if (cmd == "/logs") {
            std::string logs = CSVLogger::inst().getLastNFormatted(20);
            if (logs.length() > 4000) {
                logs = logs.substr(logs.length() - 4000);
                logs = "...(truncated)\n" + logs;
            }
            sendMsg("\xF0\x9F\x93\x8B <b>Last 20 Activity Logs:</b>\n\n<pre>" + logs + "</pre>");
        }
        // NEW: /exportlog command — send CSV file as document
        else if (cmd == "/exportlog") {
            std::string csvPath = CSVLogger::inst().getFilePath();
            if (fs::exists(csvPath) && fs::file_size(csvPath) > 0) {
                int entries = CSVLogger::inst().getEntryCount();
                sendDocFile(csvPath, "SENTINEL Activity Log (" + std::to_string(entries) + " entries)");
                sendMsg("\xF0\x9F\x93\x84 CSV log exported (" + std::to_string(entries) + " entries)");
            } else {
                sendMsg("No CSV log file found or empty");
            }
        }
        // CONTINUOUS MONITORING - SCREENSHOTS
        else if (cmd.find("/watchscreen") == 0) {
            int interval = 0;
            if (cmd == "/watchscreen5") interval = 5;
            else if (cmd == "/watchscreen10") interval = 10;
            else if (cmd == "/watchscreen15") interval = 15;
            else if (cmd == "/watchscreen20") interval = 20;
            else if (cmd == "/watchscreen35") interval = 35;
            else if (cmd == "/watchscreen60") interval = 60;
            if (interval > 0) {
                monitor_screenshots = true;
                monitor_camera = false;
                monitor_interval = interval;
                std::thread(monitorScreenshots, interval).detach();
                sendMsg("Screen monitoring started — every " + std::to_string(interval) + "s\n"
                        "Use /stopwatch to stop");
            }
        }
        // CONTINUOUS MONITORING - CAMERA
        else if (cmd.find("/watchcam") == 0) {
            int interval = 0;
            if (cmd == "/watchcam5") interval = 5;
            else if (cmd == "/watchcam10") interval = 10;
            else if (cmd == "/watchcam15") interval = 15;
            else if (cmd == "/watchcam20") interval = 20;
            else if (cmd == "/watchcam35") interval = 35;
            else if (cmd == "/watchcam60") interval = 60;
            if (interval > 0) {
                monitor_camera = true;
                monitor_screenshots = false;
                monitor_interval = interval;
                std::thread(monitorCamera, interval).detach();
                sendMsg("Camera monitoring started — every " + std::to_string(interval) + "s\n"
                        "Use /stopwatch to stop");
            }
        }
        else if (cmd == "/stopwatch") {
            monitor_screenshots = false;
            monitor_camera = false;
            sendMsg("All monitoring stopped");
        }
        else {
            sendMsg("Unknown command. Use /help");
        }
    }
};

// Static member initialization
std::atomic<bool> TelegramBot::guard_paused{false};
std::mutex TelegramBot::frame_mutex;
cv::Mat TelegramBot::shared_frame;
std::atomic<bool> TelegramBot::temp_access_active{false};
std::atomic<int> TelegramBot::temp_access_minutes{0};
std::chrono::steady_clock::time_point TelegramBot::temp_access_until;
std::atomic<bool> TelegramBot::temp_access_pending{false};
std::atomic<bool> TelegramBot::remote_killswitch{false};
std::atomic<bool> TelegramBot::monitor_screenshots{false};
std::atomic<bool> TelegramBot::monitor_camera{false};
std::atomic<int> TelegramBot::monitor_interval{10};
std::atomic<bool> TelegramBot::recording_active{false};
std::atomic<int> TelegramBot::recording_seconds{30};

// ═══════════════════════════════════════
// END OF PART 2
// Part 3 starts with TamperDetector
// ═══════════════════════════════════════
// ═══════════════════════════════════════
// (Continuing from Part 2)
// ═══════════════════════════════════════

class TamperDetector {
public:
    enum TamperType{NONE,COVERED,BLURRY,TOO_DARK};
    struct Result{TamperType type;double blur_score,brightness;std::string desc;};
    Result check(const cv::Mat&frame){
        Result r;r.type=NONE;r.blur_score=0;r.brightness=0;
        if(frame.empty()){r.type=COVERED;r.desc="No frame";return r;}
        cv::Mat gray;cv::cvtColor(frame,gray,cv::COLOR_BGR2GRAY);
        r.brightness=cv::mean(gray)[0];
        if(r.brightness<g_config.tamper_dark_threshold){r.type=TOO_DARK;r.desc="Covered("+std::to_string((int)r.brightness)+")";return r;}
        cv::Mat lap;cv::Laplacian(gray,lap,CV_64F);cv::Scalar mu,sigma;cv::meanStdDev(lap,mu,sigma);
        r.blur_score=sigma[0]*sigma[0];
        if(r.blur_score<g_config.tamper_blur_threshold){r.type=BLURRY;r.desc="Blurry("+std::to_string((int)r.blur_score)+")";return r;}
        r.desc="OK";return r;}
};

class Overlay {
public:
    static void drawFaceBox(cv::Mat&frame,const dlib::rectangle&face,const std::string&label,bool is_owner){
        cv::Scalar color=is_owner?cv::Scalar(0,255,0):cv::Scalar(0,0,255);
        int l=std::max(0,(int)face.left()),t=std::max(0,(int)face.top());
        int r=std::min(frame.cols-1,(int)face.right()),b=std::min(frame.rows-1,(int)face.bottom());
        cv::rectangle(frame,{l,t},{r,b},color,is_owner?2:3);int c=20;
        cv::line(frame,{l,t},{l+c,t},color,3);cv::line(frame,{l,t},{l,t+c},color,3);
        cv::line(frame,{r,t},{r-c,t},color,3);cv::line(frame,{r,t},{r,t+c},color,3);
        cv::line(frame,{l,b},{l+c,b},color,3);cv::line(frame,{l,b},{l,b-c},color,3);
        cv::line(frame,{r,b},{r-c,b},color,3);cv::line(frame,{r,b},{r,b-c},color,3);
        int bl=0;cv::Size ts=cv::getTextSize(label,cv::FONT_HERSHEY_SIMPLEX,0.55,2,&bl);
        int lt2=std::max(0,t-ts.height-14);
        cv::rectangle(frame,{l,lt2},{l+ts.width+10,t},color,cv::FILLED);
        cv::putText(frame,label,{l+5,t-7},cv::FONT_HERSHEY_SIMPLEX,0.55,{255,255,255},2);}
    static void drawStatusBar(cv::Mat&frame,const std::string&status,cv::Scalar color,double fps,int faces){
        int w=frame.cols,h=frame.rows;cv::Mat ov=frame.clone();
        cv::rectangle(ov,{0,0},{w,55},{0,0,0},cv::FILLED);cv::addWeighted(ov,0.7,frame,0.3,0,frame);
        cv::putText(frame,"SENTINEL v3.2",{10,22},cv::FONT_HERSHEY_SIMPLEX,0.6,{0,255,255},2);
        cv::putText(frame,status,{180,22},cv::FONT_HERSHEY_SIMPLEX,0.55,color,2);
        cv::putText(frame,"FPS:"+std::to_string((int)fps),{w-100,22},cv::FONT_HERSHEY_SIMPLEX,0.45,{200,200,200},1);
        if(color==cv::Scalar(0,0,255)){cv::Mat bot=frame.clone();
            cv::rectangle(bot,{0,h-30},{w,h},{0,0,180},cv::FILLED);cv::addWeighted(bot,0.6,frame,0.4,0,frame);
            cv::putText(frame,"! SECURITY ALERT !",{w/2-100,h-8},cv::FONT_HERSHEY_SIMPLEX,0.6,{255,255,255},2);}}
    static void drawScanline(cv::Mat&frame,int fc){int y=(fc*3)%frame.rows;cv::line(frame,{0,y},{frame.cols,y},{0,255,0},1);}
};

class SecurityManager {
public:
    struct QA{std::string question;unsigned long answer_hash;};
    static unsigned long hashStr(const std::string&s){
        unsigned long h=5381;std::string low=s;
        std::transform(low.begin(),low.end(),low.begin(),::tolower);
        low.erase(0,low.find_first_not_of(" \t"));low.erase(low.find_last_not_of(" \t")+1);
        for(char c:low)h=((h<<5)+h)+c;return h;}
    bool hasQuestions()const{return m_qa.size()>=3;}
    const std::vector<QA>&getQuestions()const{return m_qa;}
    bool verify(int idx,const std::string&ans){
        if(idx<0||idx>=(int)m_qa.size())return false;return hashStr(ans)==m_qa[idx].answer_hash;}
    void setup(){
        m_qa.clear();
        std::cout<<Color::CYAN<<"\n=== Security Questions Setup (Kill Switch) ===\n"<<Color::NC;
        std::cout<<"If an intruder fails face recognition, they must answer ALL 3.\n\n";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
        std::string sugg[3]={"What is your pet's name?","What city were you born in?","What is your favorite movie?"};
        for(int i=0;i<3;i++){QA qa;
            std::cout<<Color::YELLOW<<"Q"<<(i+1)<<" (suggestion: "<<sugg[i]<<")\n"<<Color::NC;
            std::cout<<"  Your question: ";std::getline(std::cin,qa.question);
            std::string ans;std::cout<<"  Your answer: ";std::getline(std::cin,ans);
            qa.answer_hash=hashStr(ans);m_qa.push_back(qa);
            std::cout<<Color::GREEN<<"  Saved!\n\n"<<Color::NC;}
        save();std::cout<<Color::GREEN<<"All 3 security questions saved!\n"<<Color::NC;}
    void save(){std::ofstream f(g_config.security_file);for(auto&qa:m_qa)f<<qa.question<<"\n"<<qa.answer_hash<<"\n";}
    void load(){if(!fs::exists(g_config.security_file))return;std::ifstream f(g_config.security_file);std::string line;m_qa.clear();
        while(std::getline(f,line)){QA qa;qa.question=line;if(!std::getline(f,line))break;qa.answer_hash=std::stoul(line);m_qa.push_back(qa);}
        if(m_qa.size()>=3)LOG_INFO("  [OK] Security questions loaded");}
private:std::vector<QA>m_qa;
};

class FaceEngine {
public:
    bool init(){
        LOG_INFO("Initializing Face Engine...");
        try{m_det=dlib::get_frontal_face_detector();LOG_INFO("  [OK] HOG");}catch(const std::exception&e){LOG_ERROR(e.what());return false;}
        if(!fs::exists(g_config.shape_predictor_path)){LOG_ERROR("Missing SP");return false;}
        try{dlib::deserialize(g_config.shape_predictor_path)>>m_sp;LOG_INFO("  [OK] SP");}catch(const std::exception&e){LOG_ERROR(e.what());return false;}
        if(!fs::exists(g_config.face_rec_model_path)){LOG_ERROR("Missing net");return false;}
        try{dlib::deserialize(g_config.face_rec_model_path)>>m_net;LOG_INFO("  [OK] Net");}catch(const std::exception&e){LOG_ERROR(e.what());return false;}
        if(fs::exists(g_config.haar_cascade_path)){m_haar.load(g_config.haar_cascade_path);LOG_INFO("  [OK] Haar");}
        if(fs::exists(g_config.owner_face_file))loadOwner();
        LOG_INFO("Face Engine ready");return true;}
    enum Result{NO_FACE,OWNER_DETECTED,UNKNOWN_DETECTED,MULTIPLE_PEOPLE};
    struct FaceInfo{dlib::rectangle rect;double distance;bool is_owner;std::string label;};
    struct Analysis{Result result;int face_count;double best_distance;std::vector<FaceInfo>faces;};
    Analysis analyze(const cv::Mat&frame){
        Analysis a;a.result=NO_FACE;a.face_count=0;a.best_distance=999.0;
        if(frame.empty())return a;dlib::cv_image<dlib::bgr_pixel>img(frame);
        auto dets=m_det(img);a.face_count=(int)dets.size();if(dets.empty())return a;
        for(const auto&d:dets){FaceInfo fi;fi.rect=d;fi.distance=999.0;fi.is_owner=false;
            if(!m_owner.empty()){auto shape=m_sp(img,d);dlib::matrix<dlib::rgb_pixel>chip;
                dlib::extract_image_chip(img,dlib::get_face_chip_details(shape,150,0.25),chip);
                std::vector<dlib::matrix<dlib::rgb_pixel>>chips={std::move(chip)};auto descs=m_net(chips);
                double minD=999.0;for(const auto&od:m_owner){double dist=dlib::length(descs[0]-od);if(dist<minD)minD=dist;}
                fi.distance=minD;fi.is_owner=(minD<g_config.face_recognition_threshold);
                fi.label=fi.is_owner?"OWNER("+std::to_string(minD).substr(0,4)+")":"UNKNOWN("+std::to_string(minD).substr(0,4)+")";}
            else fi.label="UNKNOWN";
            a.faces.push_back(fi);if(fi.distance<a.best_distance)a.best_distance=fi.distance;}
        if(a.face_count>g_config.max_allowed_faces)a.result=MULTIPLE_PEOPLE;
        else{bool hasO=false,hasU=false;for(auto&f:a.faces){if(f.is_owner)hasO=true;else hasU=true;}
             if(hasU)a.result=UNKNOWN_DETECTED;else if(hasO)a.result=OWNER_DETECTED;}
        return a;}
    bool enroll(int camIdx){
        LOG_INFO("Enrollment — "+std::to_string(g_config.enrollment_samples)+" samples");
        cv::VideoCapture cap(camIdx);if(!cap.isOpened()){LOG_ERROR("Camera failed");return false;}
        cap.set(cv::CAP_PROP_FRAME_WIDTH,640);cap.set(cv::CAP_PROP_FRAME_HEIGHT,480);
        std::vector<dlib::matrix<float,0,1>>descs;int captured=0,fc=0;
        while(captured<g_config.enrollment_samples&&g_running){
            cv::Mat frame;cap>>frame;if(frame.empty())continue;fc++;
            dlib::cv_image<dlib::bgr_pixel>img(frame);auto dets=m_det(img);cv::Mat disp=frame.clone();
            if(dets.size()==1){auto shape=m_sp(img,dets[0]);dlib::matrix<dlib::rgb_pixel>chip;
                dlib::extract_image_chip(img,dlib::get_face_chip_details(shape,150,0.25),chip);
                std::vector<dlib::matrix<dlib::rgb_pixel>>chips={std::move(chip)};auto fd=m_net(chips);
                if(fc%10==0){descs.push_back(fd[0]);captured++;LOG_INFO("  Sample "+std::to_string(captured)+"/"+std::to_string(g_config.enrollment_samples));}
                Overlay::drawFaceBox(disp,dets[0],"ENROLL "+std::to_string(captured)+"/"+std::to_string(g_config.enrollment_samples),true);
                double prog=(double)captured/g_config.enrollment_samples;
                cv::rectangle(disp,{10,disp.rows-30},{10+(int)(prog*(disp.cols-20)),disp.rows-15},{0,255,0},cv::FILLED);
                cv::rectangle(disp,{10,disp.rows-30},{disp.cols-10,disp.rows-15},{255,255,255},1);
            }else if(dets.empty())cv::putText(disp,"No face — look at camera",{10,30},cv::FONT_HERSHEY_SIMPLEX,0.7,{0,0,255},2);
            else cv::putText(disp,"Multiple faces!",{10,30},cv::FONT_HERSHEY_SIMPLEX,0.6,{0,0,255},2);
            cv::imshow("SENTINEL - Enrollment",disp);int key=cv::waitKey(30);
            if(key=='q'||key==27){cap.release();cv::destroyAllWindows();return false;}}
        cap.release();cv::destroyAllWindows();if(descs.empty())return false;
        m_owner=descs;saveOwner();LOG_INFO("Enrolled "+std::to_string(descs.size())+" samples");return true;}
    bool isEnrolled()const{return !m_owner.empty();}
    void reset(){m_owner.clear();if(fs::exists(g_config.owner_face_file))fs::remove(g_config.owner_face_file);}
private:
    dlib::frontal_face_detector m_det;dlib::shape_predictor m_sp;anet_type m_net;cv::CascadeClassifier m_haar;
    std::vector<dlib::matrix<float,0,1>>m_owner;
    void saveOwner(){try{dlib::serialize(g_config.owner_face_file)<<m_owner;}catch(...){};}
    void loadOwner(){try{dlib::deserialize(g_config.owner_face_file)>>m_owner;
        LOG_INFO("  [OK] Owner("+std::to_string(m_owner.size())+")");}catch(...){m_owner.clear();}}
};

// ═══════════════════════════════════════
// VERIFICATION POPUP with CSV Logging
// ═══════════════════════════════════════
class VerificationPopup {
public:
    enum PopupResult { OWNER_VERIFIED, KILLSWITCH_PASSED, FAILED_LOCKED, TEMP_ACCESS_GRANTED };

    static PopupResult show(FaceEngine& engine, SecurityManager& secMgr, int camIdx) {
        LOG_SECURITY("=== VERIFICATION POPUP ===");

        cv::VideoCapture cap(camIdx);
        if (!cap.isOpened()) { LOG_ERROR("Camera failed for verify"); return FAILED_LOCKED; }
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

        std::string winName = "SENTINEL - IDENTITY VERIFICATION";
        cv::namedWindow(winName, cv::WINDOW_NORMAL);
        cv::setWindowProperty(winName, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);
        system("wmctrl -r 'SENTINEL' -b add,above 2>/dev/null");

        auto startTime = std::chrono::steady_clock::now();
        auto sessionStart = std::chrono::steady_clock::now();
        int currentTimeout = g_config.verification_timeout;
        int fc = 0, owner_confirm = 0;
        bool inKillSwitch = false;
        bool waitingApproval = false;
        int currentQ = 0;
        std::string typed = "";

        FaceEngine::Analysis lastA;
        lastA.result = FaceEngine::NO_FACE;
        lastA.face_count = 0;
        lastA.best_distance = 999.0;

        while (g_running) {
            cv::Mat frame;
            cap >> frame;
            if (frame.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(50)); continue; }
            fc++;

            double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - startTime).count();
            int remaining = currentTimeout - (int)elapsed;

            double sessionDuration = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - sessionStart).count();

            // Check if temp access was approved via telegram
            if (TelegramBot::temp_access_active && !waitingApproval) {
                LOG_SECURITY("Temp access approved — granting access");
                CSV_LOG("TEMP_ACCESS_APPROVED", "granted_during_verify", -1, -1,
                        "", "telegram", "approved", "", true, "telegram_owner",
                        TelegramBot::temp_access_minutes.load(), sessionDuration);
                cap.release(); cv::destroyWindow(winName);
                return TEMP_ACCESS_GRANTED;
            }

            if (fc % 3 == 0 && !inKillSwitch && !waitingApproval) {
                lastA = engine.analyze(frame);
            }

            if (lastA.result == FaceEngine::OWNER_DETECTED && !inKillSwitch && !waitingApproval) {
                owner_confirm++;
                if (owner_confirm >= 10) {
                    LOG_SECURITY("Owner VERIFIED by face!");
                    CSV_LOG("VERIFICATION_SUCCESS", "owner_verified",
                            lastA.best_distance, lastA.face_count,
                            "", "face_recognition", "success",
                            "", g_config.telegram_enabled, "", -1, sessionDuration);
                    if (g_config.telegram_enabled) {
                        std::string path = g_config.evidence_dir + "/owner_verified.jpg";
                        cv::imwrite(path, frame);
                        Telegram::sendPhoto(path, "\xE2\x9C\x85 Owner verified by face recognition");
                    }
                    cap.release(); cv::destroyWindow(winName);
                    return OWNER_VERIFIED;
                }
            } else if (!inKillSwitch && !waitingApproval) {
                owner_confirm = std::max(0, owner_confirm - 1);
            }

            cv::Mat display(768, 1366, CV_8UC3, cv::Scalar(20, 20, 35));

            cv::rectangle(display, {0, 0}, {display.cols, 70}, {30, 30, 50}, cv::FILLED);
            cv::putText(display, "SENTINEL - IDENTITY VERIFICATION",
                {display.cols / 2 - 280, 45}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {0, 255, 255}, 2);

            cv::Mat camSmall;
            cv::resize(frame, camSmall, cv::Size(400, 300));
            int cx = display.cols / 2 - 200, cy = 90;
            if (cx >= 0 && cy >= 0 && cx + 400 <= display.cols && cy + 300 <= display.rows)
                camSmall.copyTo(display(cv::Rect(cx, cy, 400, 300)));

            for (const auto& fi : lastA.faces) {
                double sx = 400.0 / frame.cols, sy = 300.0 / frame.rows;
                cv::Scalar col = fi.is_owner ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
                cv::rectangle(display,
                    {cx + (int)(fi.rect.left() * sx), cy + (int)(fi.rect.top() * sy)},
                    {cx + (int)(fi.rect.right() * sx), cy + (int)(fi.rect.bottom() * sy)}, col, 2);
            }

            cv::Scalar bCol = (lastA.result == FaceEngine::OWNER_DETECTED) ?
                cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
            cv::rectangle(display, {cx - 3, cy - 3}, {cx + 403, cy + 303}, bCol, 3);

            if (waitingApproval) {
                cv::putText(display, "WAITING FOR OWNER APPROVAL...",
                    {cx - 130, cy + 350}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0, 165, 255}, 2);
                cv::putText(display, "Request sent via Telegram",
                    {cx - 60, cy + 390}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {150, 150, 150}, 1);
                cv::putText(display, "Owner must send /approve or /deny",
                    {cx - 100, cy + 420}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {150, 150, 150}, 1);

                if (TelegramBot::temp_access_active) {
                    CSV_LOG("TEMP_ACCESS_APPROVED", "approved_via_popup", -1, -1,
                            "", "telegram", "approved", "", true, "popup_requester",
                            TelegramBot::temp_access_minutes.load(), sessionDuration);
                    cap.release(); cv::destroyWindow(winName);
                    return TEMP_ACCESS_GRANTED;
                }
                if (!TelegramBot::temp_access_pending && !TelegramBot::temp_access_active) {
                    LOG_SECURITY("Temp access denied — locking");
                    CSV_LOG("TEMP_ACCESS_DENIED", "denied_via_popup", -1, -1,
                            "", "telegram", "denied", "", true, "", -1, sessionDuration);
                    cap.release(); cv::destroyWindow(winName);
                    return FAILED_LOCKED;
                }
            }
            else if (!inKillSwitch) {
                std::string stText;
                cv::Scalar stCol;
                if (lastA.result == FaceEngine::OWNER_DETECTED) {
                    stText = "Verifying owner... (" + std::to_string(owner_confirm) + "/10)";
                    stCol = {0, 255, 0};
                    double p = owner_confirm / 10.0;
                    cv::rectangle(display, {cx, cy + 310}, {cx + (int)(400 * p), cy + 325}, {0, 255, 0}, cv::FILLED);
                    cv::rectangle(display, {cx, cy + 310}, {cx + 400, cy + 325}, {255, 255, 255}, 1);
                } else if (lastA.result == FaceEngine::UNKNOWN_DETECTED) {
                    stText = "UNKNOWN FACE — Access Denied";
                    stCol = {0, 0, 255};
                } else {
                    stText = "Look at the camera to verify identity...";
                    stCol = {200, 200, 200};
                }
                cv::putText(display, stText, {cx - 50, cy + 360}, cv::FONT_HERSHEY_SIMPLEX, 0.7, stCol, 2);

                if (secMgr.hasQuestions()) {
                    cv::putText(display, "Press 'K' for Kill Switch | 'T' for Temp Access Request",
                        {cx - 200, cy + 400}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {150, 150, 150}, 1);
                }
            } else {
                cv::putText(display, "KILL SWITCH ACTIVE — Answer Security Questions",
                    {cx - 180, cy + 340}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 165, 255}, 2);

                if (currentQ < (int)secMgr.getQuestions().size()) {
                    auto& qa = secMgr.getQuestions()[currentQ];
                    cv::putText(display, "Question " + std::to_string(currentQ + 1) + " of 3:",
                        {100, cy + 380}, cv::FONT_HERSHEY_SIMPLEX, 0.65, {0, 165, 255}, 2);
                    cv::putText(display, qa.question,
                        {100, cy + 415}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {255, 255, 255}, 2);
                    cv::rectangle(display, {100, cy + 435}, {display.cols - 100, cy + 475}, {40, 40, 60}, cv::FILLED);
                    cv::rectangle(display, {100, cy + 435}, {display.cols - 100, cy + 475}, {0, 255, 255}, 2);
                    cv::putText(display, "> " + typed + "_",
                        {115, cy + 463}, cv::FONT_HERSHEY_SIMPLEX, 0.6, {0, 255, 255}, 1);
                    for (int i = 0; i < 3; i++) {
                        cv::Scalar dotCol = (i < currentQ) ? cv::Scalar(0, 255, 0) :
                                           (i == currentQ) ? cv::Scalar(0, 165, 255) : cv::Scalar(80, 80, 80);
                        cv::circle(display, {display.cols / 2 - 30 + i * 30, cy + 500}, 8, dotCol, cv::FILLED);
                    }
                    cv::putText(display, "ENTER = Submit | BACKSPACE = Delete | ESC = Back",
                        {100, cy + 540}, cv::FONT_HERSHEY_SIMPLEX, 0.45, {120, 120, 120}, 1);
                }
            }

            // Timer bar
            cv::Scalar timerCol = (remaining < 15) ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 255);
            double timerProg = std::max(0.0, (double)remaining / currentTimeout);
            cv::rectangle(display, {0, display.rows - 50}, {display.cols, display.rows}, {15, 15, 25}, cv::FILLED);
            cv::rectangle(display, {10, display.rows - 40},
                {10 + (int)((display.cols - 20) * timerProg), display.rows - 20}, timerCol, cv::FILLED);
            std::string timerText = "Time: " + std::to_string(std::max(0, remaining)) + "s";
            cv::putText(display, timerText, {display.cols / 2 - 40, display.rows - 5},
                cv::FONT_HERSHEY_SIMPLEX, 0.5, {200, 200, 200}, 1);

            if (remaining <= 0 && !waitingApproval) {
                LOG_SECURITY("Verification TIMEOUT — locking");
                CSV_LOG("VERIFICATION_FAILED", "timeout", lastA.best_distance,
                        lastA.face_count, "Verification timeout",
                        inKillSwitch ? "killswitch" : "face_recognition",
                        "timeout", "", g_config.telegram_enabled, "", -1, sessionDuration);
                cap.release(); cv::destroyWindow(winName);
                return FAILED_LOCKED;
            }

            cv::imshow(winName, display);
            cv::setWindowProperty(winName, cv::WND_PROP_FULLSCREEN, cv::WINDOW_FULLSCREEN);

            int key = cv::waitKey(30);

            if (!inKillSwitch && !waitingApproval) {
                if ((key == 'k' || key == 'K') && secMgr.hasQuestions()) {
                    inKillSwitch = true;
                    currentQ = 0;
                    typed = "";
                    startTime = std::chrono::steady_clock::now();
                    currentTimeout = g_config.killswitch_timeout;
                    LOG_SECURITY("Kill switch activated");
                    CSV_LOG("KILLSWITCH_ACTIVATED", "popup", lastA.best_distance,
                            lastA.face_count, "", "killswitch", "started",
                            "", g_config.telegram_enabled, "", -1, sessionDuration);
                    if (g_config.telegram_enabled) {
                        std::string path = g_config.evidence_dir + "/killswitch_intruder.jpg";
                        cv::imwrite(path, frame);
                        Telegram::sendPhoto(path,
                            "\xF0\x9F\x94\xB4 <b>KILL SWITCH ACTIVATED</b>\n"
                            "Someone is answering security questions\n"
                            "Use /revoke to lock immediately");
                    }
                }
                else if ((key == 't' || key == 'T') && g_config.telegram_enabled) {
                    waitingApproval = true;
                    TelegramBot::temp_access_pending = true;
                    TelegramBot::temp_access_minutes = g_config.security_disable_minutes;
                    startTime = std::chrono::steady_clock::now();
                    currentTimeout = 300;
                    std::string path = g_config.evidence_dir + "/access_request.jpg";
                    cv::imwrite(path, frame);
                    Telegram::sendPhoto(path,
                        "\xF0\x9F\x94\x94 <b>ACCESS REQUEST</b>\n\n"
                        "Someone is requesting temporary access.\n"
                        "Duration: " + std::to_string(g_config.security_disable_minutes) + " minutes\n\n"
                        "Send /approve to grant access\n"
                        "Send /deny to reject and lock");
                    LOG_SECURITY("Temp access requested via popup — waiting for Telegram approval");
                    CSV_LOG("TEMP_ACCESS_REQUESTED", "popup_request", lastA.best_distance,
                            lastA.face_count, "", "telegram", "pending",
                            path, true, "unknown_person",
                            g_config.security_disable_minutes, sessionDuration);
                }
            } else if (inKillSwitch) {
                if (key == 13 || key == 10) {
                    if (secMgr.verify(currentQ, typed)) {
                        LOG_INFO("Q" + std::to_string(currentQ + 1) + " CORRECT");
                        currentQ++; typed = "";
                        if (currentQ >= 3) {
                            LOG_SECURITY("Kill switch: ALL CORRECT");
                            CSV_LOG("KILLSWITCH_PASSED", "all_correct", lastA.best_distance,
                                    lastA.face_count, "", "killswitch", "passed",
                                    "", g_config.telegram_enabled, "", 
                                    g_config.security_disable_minutes, sessionDuration);
                            if (g_config.telegram_enabled) {
                                Telegram::sendMessage(
                                    "\xE2\x9A\xA0 <b>Kill Switch PASSED</b>\n"
                                    "All 3 questions answered correctly\n"
                                    "Bypass: " + std::to_string(g_config.security_disable_minutes) + " min");
                            }
                            cap.release(); cv::destroyWindow(winName);
                            return KILLSWITCH_PASSED;
                        }
                    } else {
                        LOG_SECURITY("Kill switch: WRONG ANSWER — locking");
                        CSV_LOG("KILLSWITCH_FAILED", "wrong_answer", lastA.best_distance,
                                lastA.face_count, "Wrong answer at Q" + std::to_string(currentQ + 1),
                                "killswitch", "failed",
                                "", g_config.telegram_enabled, "", -1, sessionDuration);
                        if (g_config.telegram_enabled) {
                            Telegram::sendMessage(
                                "\xE2\x9D\x8C <b>Kill Switch FAILED</b>\n"
                                "Wrong answer — laptop locked");
                        }
                        cap.release(); cv::destroyWindow(winName);
                        return FAILED_LOCKED;
                    }
                } else if (key == 27) {
                    inKillSwitch = false;
                    startTime = std::chrono::steady_clock::now();
                    currentTimeout = g_config.verification_timeout;
                    typed = "";
                } else if (key == 8 || key == 127) {
                    if (!typed.empty()) typed.pop_back();
                } else if (key >= 32 && key <= 126) {
                    typed += (char)key;
                }
            }
        }
        cap.release(); cv::destroyWindow(winName);
        return FAILED_LOCKED;
    }
};

// ═══════════════════════════════════════
// END OF PART 3
// Part 4 starts with Guard class
// ═══════════════════════════════════════
// ═══════════════════════════════════════
// (Continuing from Part 3)
// Guard Mode — All features integrated with CSV Logging
// ═══════════════════════════════════════
class Guard {
public:
    Guard(FaceEngine& e, SecurityManager& s) : m_engine(e), m_secMgr(s) {}

    void run(int camIdx) {
        LOG_SECURITY("==================================");
        LOG_SECURITY("  SENTINEL Guard Mode ACTIVATED");
        LOG_SECURITY("==================================");

        auto guardStartTime = std::chrono::steady_clock::now();
        CSV_LOG("GUARD_START", "activated", -1, -1, "", "", "", "",
                g_config.telegram_enabled);

        if (!m_engine.isEnrolled()) { LOG_ERROR("No owner! --enroll first"); return; }

        cv::VideoCapture cap(camIdx);
        if (!cap.isOpened()) { LOG_ERROR("Camera failed"); return; }
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        cap.set(cv::CAP_PROP_FPS, 30);
        LOG_INFO("Monitoring. Ctrl+C to stop.");

        // Startup alert with photo
        if (g_config.telegram_enabled) {
            cv::Mat startupFrame;
            for (int i = 0; i < 10; i++) { cap >> startupFrame; }
            if (!startupFrame.empty()) {
                std::string path = g_config.evidence_dir + "/startup_capture.jpg";
                cv::imwrite(path, startupFrame);
                auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::tm buf; localtime_r(&t, &buf); std::ostringstream o;
                o << "\xF0\x9F\x9A\xA8 <b>SENTINEL STARTED</b>\n\n"
                  << "Laptop login detected\n"
                  << "Time: " << std::put_time(&buf, "%Y-%m-%d %H:%M:%S") << "\n"
                  << "Guard: ACTIVE\n"
                  << "Telegram: CONNECTED\n\n"
                  << "Use /help for commands";
                Telegram::sendPhoto(path, o.str());
                LOG_INFO("Startup alert sent via Telegram");
            } else {
                Telegram::sendMessage("\xF0\x9F\x9A\xA8 SENTINEL: Guard activated");
            }
        }

        // Start Telegram bot in background
        std::thread telegramThread;
        if (g_config.telegram_enabled) {
            telegramThread = std::thread(TelegramBot::pollLoop, camIdx);
            telegramThread.detach();
        }

        int fc = 0, unk_streak = 0, tamper_streak = 0;
        auto last_lock = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        auto last_face_seen = std::chrono::steady_clock::now();
        auto fps_start = std::chrono::steady_clock::now();
        int fps_fc = 0; double fps = 0.0;

        bool intruder_mode = false;
        bool security_bypassed = false;
        auto bypass_until = std::chrono::steady_clock::now();

        FaceEngine::Analysis lastA;
        lastA.result = FaceEngine::NO_FACE; lastA.face_count = 0; lastA.best_distance = 999.0;
        TamperDetector tamper;

        while (g_running) {
            cv::Mat frame; cap >> frame;
            if (frame.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); continue; }
            fc++; fps_fc++;

            // Share frame with Telegram bot for /capture and /record
            if (g_config.telegram_enabled && fc % 5 == 0) {
                TelegramBot::updateFrame(frame);
            }

            auto now = std::chrono::steady_clock::now();
            double fps_el = std::chrono::duration<double>(now - fps_start).count();
            if (fps_el >= 1.0) { fps = fps_fc / fps_el; fps_fc = 0; fps_start = now; }

            double guardSessionDuration = std::chrono::duration<double>(
                now - guardStartTime).count();

            // CHECK REMOTE KILL SWITCH
            if (TelegramBot::remote_killswitch) {
                TelegramBot::remote_killswitch = false;
                intruder_mode = true;
                doLockAndVerify(frame, cap, camIdx, "Remote kill switch", last_lock,
                    intruder_mode, security_bypassed, bypass_until, guardStartTime);
                last_face_seen = std::chrono::steady_clock::now();
                continue;
            }

            // CHECK TEMP ACCESS EXPIRY
            if (TelegramBot::temp_access_active) {
                if (now >= TelegramBot::temp_access_until) {
                    TelegramBot::temp_access_active = false;
                    TelegramBot::guard_paused = false;
                    LOG_SECURITY("Temp access expired — resuming guard");
                    CSV_LOG("TEMP_ACCESS_EXPIRED", "expired", -1, -1,
                            "Timer expired", "", "", "",
                            g_config.telegram_enabled, "", -1, guardSessionDuration);
                    if (g_config.telegram_enabled) {
                        Telegram::sendMessage(
                            "\xE2\x8F\xB0 <b>Temp access EXPIRED</b>\n"
                            "Face verification resumed\n"
                            "Locking laptop...");
                    }
                    SystemAction::lockScreen();
                    last_face_seen = std::chrono::steady_clock::now();
                } else {
                    // Temp access still active — skip face check
                    if (g_config.show_preview) {
                        cv::Mat disp = frame.clone();
                        int mins = std::max(0, (int)(std::chrono::duration<double>(
                            TelegramBot::temp_access_until - now).count() / 60));
                        Overlay::drawStatusBar(disp, "TEMP ACCESS(" + std::to_string(mins) + "m)",
                            {0, 165, 255}, fps, 0);
                        cv::imshow("SENTINEL - Guard", disp);
                        if (cv::waitKey(1) == 'q') { g_running = false; break; }
                    }
                    last_face_seen = now;
                    continue;
                }
            }

            // TELEGRAM PAUSE CHECK
            if (TelegramBot::guard_paused) {
                if (g_config.show_preview) {
                    cv::Mat disp = frame.clone();
                    Overlay::drawStatusBar(disp, "PAUSED (Telegram)", {0, 165, 255}, fps, 0);
                    cv::imshow("SENTINEL - Guard", disp);
                    if (cv::waitKey(1) == 'q') { g_running = false; break; }
                }
                last_face_seen = std::chrono::steady_clock::now();
                continue;
            }

            // Security bypass check
            if (security_bypassed) {
                if (now >= bypass_until) {
                    security_bypassed = false;
                    LOG_INFO("Security bypass expired — resuming monitoring");
                    if (g_config.telegram_enabled) {
                        Telegram::sendMessage("\xE2\x8F\xB0 Security bypass expired — guard resumed");
                    }
                } else {
                    if (g_config.show_preview) {
                        cv::Mat disp = frame.clone();
                        int mins = (int)std::chrono::duration<double>(bypass_until - now).count() / 60;
                        Overlay::drawStatusBar(disp, "BYPASS(" + std::to_string(mins) + "m)",
                            {0, 165, 255}, fps, 0);
                        cv::imshow("SENTINEL - Guard", disp);
                        if (cv::waitKey(1) == 'q') { g_running = false; break; }
                    }
                    continue;
                }
            }

            // Tamper check
            auto tr = tamper.check(frame);
            if (tr.type != TamperDetector::NONE) {
                tamper_streak++;
                if (tamper_streak >= g_config.tamper_frames_before_lock) {
                    CSV_LOG("CAMERA_TAMPER", tr.desc, -1, -1,
                            "Camera tampered: " + tr.desc, "", "", "",
                            g_config.telegram_enabled, "", -1, guardSessionDuration);
                    intruder_mode = true;
                    doLockAndVerify(frame, cap, camIdx, "Camera tampered", last_lock,
                        intruder_mode, security_bypassed, bypass_until, guardStartTime);
                    tamper_streak = 0; last_face_seen = std::chrono::steady_clock::now();
                }
                if (g_config.show_preview) {
                    cv::Mat disp = frame.clone();
                    Overlay::drawStatusBar(disp, "TAMPER", {0, 0, 255}, fps, 0);
                    cv::imshow("SENTINEL - Guard", disp);
                    if (cv::waitKey(1) == 'q') { g_running = false; break; }
                }
                continue;
            }
            tamper_streak = 0;

            // Face analysis
            bool analyzed = false;
            if (fc % g_config.frame_skip == 0) { lastA = m_engine.analyze(frame); analyzed = true; }

            if (analyzed) {
                switch (lastA.result) {
                    case FaceEngine::OWNER_DETECTED:
                        unk_streak = 0; last_face_seen = now;
                        if (intruder_mode) {
                            LOG_SECURITY("Owner face detected — clearing intruder mode");
                            intruder_mode = false;
                            CSV_LOG("OWNER_VERIFIED", "intruder_mode_cleared",
                                    lastA.best_distance, lastA.face_count,
                                    "", "face_recognition", "owner_detected",
                                    "", g_config.telegram_enabled, "", -1, guardSessionDuration);
                            if (g_config.telegram_enabled) {
                                Telegram::sendMessage("\xE2\x9C\x85 Owner detected — intruder mode cleared");
                            }
                        }
                        if (fc % (g_config.frame_skip * 30) == 0) {
                            LOG_INFO("Owner(" + std::to_string(lastA.best_distance).substr(0, 5) + ")");
                            // Periodic owner log (every ~90 frames analyzed)
                            CSV_LOG("OWNER_VERIFIED", "periodic_check",
                                    lastA.best_distance, lastA.face_count,
                                    "", "face_recognition", "owner_present",
                                    "", false, "", -1, guardSessionDuration);
                        }
                        break;

                    case FaceEngine::UNKNOWN_DETECTED:
                        unk_streak++; last_face_seen = now;
                        LOG_WARN("Unknown(" + std::to_string(unk_streak) + "/" +
                            std::to_string(g_config.consecutive_unknown_frames) + ")");
                        CSV_LOG("UNKNOWN_DETECTED", "streak_" + std::to_string(unk_streak),
                                lastA.best_distance, lastA.face_count,
                                "Unknown face detected", "face_recognition", "unknown",
                                "", g_config.telegram_enabled, "", -1, guardSessionDuration);
                        if (unk_streak >= g_config.consecutive_unknown_frames) {
                            intruder_mode = true;
                            doLockAndVerify(frame, cap, camIdx, "Unknown person", last_lock,
                                intruder_mode, security_bypassed, bypass_until, guardStartTime);
                            unk_streak = 0; last_face_seen = std::chrono::steady_clock::now();
                        }
                        break;

                    case FaceEngine::MULTIPLE_PEOPLE:
                        CSV_LOG("MULTIPLE_FACES", "detected",
                                lastA.best_distance, lastA.face_count,
                                "Multiple people detected", "face_recognition", "multiple",
                                "", g_config.telegram_enabled, "", -1, guardSessionDuration);
                        intruder_mode = true;
                        doLockAndVerify(frame, cap, camIdx, "Multiple people", last_lock,
                            intruder_mode, security_bypassed, bypass_until, guardStartTime);
                        unk_streak = 0;
                        break;

                    case FaceEngine::NO_FACE: {
                        unk_streak = 0;
                        double noface_el = std::chrono::duration<double>(now - last_face_seen).count();
                        if (noface_el >= g_config.noface_lock_seconds) {
                            CSV_LOG("NO_FACE_TIMEOUT", "timeout",
                                    -1, 0, "No face for " + std::to_string((int)noface_el) + "s",
                                    "", "", "", g_config.telegram_enabled,
                                    "", -1, guardSessionDuration);
                            doLockAndVerify(frame, cap, camIdx,
                                "No face " + std::to_string((int)noface_el) + "s",
                                last_lock, intruder_mode, security_bypassed,
                                bypass_until, guardStartTime);
                            last_face_seen = std::chrono::steady_clock::now();
                        }
                        break;
                    }
                }
            }

            if (g_config.show_preview) {
                cv::Mat disp = frame.clone();
                for (const auto& fi : lastA.faces)
                    Overlay::drawFaceBox(disp, fi.rect, fi.label, fi.is_owner);
                std::string status; cv::Scalar color;
                if (intruder_mode) { status = "INTRUDER MODE"; color = {0, 0, 255}; }
                else switch (lastA.result) {
                    case FaceEngine::OWNER_DETECTED:
                        status = "SECURE"; color = {0, 255, 0}; break;
                    case FaceEngine::UNKNOWN_DETECTED:
                        status = "THREAT[" + std::to_string(unk_streak) + "]";
                        color = {0, 0, 255}; break;
                    case FaceEngine::MULTIPLE_PEOPLE:
                        status = "MULTI"; color = {0, 0, 255}; break;
                    default: {
                        double el = std::chrono::duration<double>(now - last_face_seen).count();
                        int rem = g_config.noface_lock_seconds - (int)el;
                        status = "NO FACE(" + std::to_string(std::max(0, rem)) + "s)";
                        color = (rem < 5) ? cv::Scalar(0, 0, 255) : cv::Scalar(128, 128, 128);
                        break;
                    }
                }
                Overlay::drawStatusBar(disp, status, color, fps, lastA.face_count);
                Overlay::drawScanline(disp, fc);
                cv::imshow("SENTINEL - Guard", disp);
                if (cv::waitKey(1) == 'q') { g_running = false; break; }
            }
        }
        cap.release();
        if (g_config.show_preview) cv::destroyAllWindows();

        double totalDuration = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - guardStartTime).count();
        CSV_LOG("GUARD_STOP", "deactivated", -1, -1, "", "", "", "",
                g_config.telegram_enabled, "", -1, totalDuration);
        LOG_SECURITY("Guard DEACTIVATED");
        if (g_config.telegram_enabled) {
            Telegram::sendMessage("\xF0\x9F\x94\xB4 SENTINEL Guard DEACTIVATED");
        }
    }

private:
    FaceEngine& m_engine;
    SecurityManager& m_secMgr;

    void doLockAndVerify(const cv::Mat& frame, cv::VideoCapture& cap, int camIdx,
                         const std::string& reason,
                         std::chrono::steady_clock::time_point& last_lock,
                         bool& intruder_mode, bool& security_bypassed,
                         std::chrono::steady_clock::time_point& bypass_until,
                         const std::chrono::steady_clock::time_point& guardStartTime) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - last_lock).count() < g_config.lock_cooldown_seconds)
            return;

        double guardSessionDuration = std::chrono::duration<double>(
            now - guardStartTime).count();

        LOG_SECURITY("THREAT: " + reason);
        SoundAlarm::play();

        std::string evidence;
        if (g_config.save_evidence)
            evidence = SystemAction::saveEvidence(frame, g_config.evidence_dir);

        CSV_LOG("SCREEN_LOCKED", "threat_detected", -1, -1,
                reason, "", "", evidence, g_config.telegram_enabled,
                "", -1, guardSessionDuration);

        if (g_config.telegram_enabled) {
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm buf; localtime_r(&t, &buf); std::ostringstream o;
            o << "\xF0\x9F\x9A\xA8 <b>SENTINEL ALERT</b>\n\n"
              << "Threat: " << reason << "\n"
              << "Time: " << std::put_time(&buf, "%H:%M:%S") << "\n"
              << "Action: LOCKED"
              << (intruder_mode ? "\nMode: INTRUDER" : "");
            if (!evidence.empty()) Telegram::sendPhoto(evidence, o.str());
            else Telegram::sendMessage(o.str());
        }

        cap.release();
        if (g_config.show_preview) cv::destroyAllWindows();
        SystemAction::lockScreen();
        last_lock = std::chrono::steady_clock::now();

        LOG_INFO("Waiting for unlock...");
        std::this_thread::sleep_for(std::chrono::seconds(3));

        LOG_SECURITY("Unlocked — verification required" +
            std::string(intruder_mode ? " [INTRUDER MODE]" : ""));

        auto result = VerificationPopup::show(m_engine, m_secMgr, camIdx);

        switch (result) {
            case VerificationPopup::OWNER_VERIFIED:
                LOG_SECURITY("OWNER confirmed — intruder mode cleared");
                intruder_mode = false;
                break;
            case VerificationPopup::KILLSWITCH_PASSED:
                LOG_SECURITY("Kill switch passed — bypass " +
                    std::to_string(g_config.security_disable_minutes) + " min");
                intruder_mode = false;
                security_bypassed = true;
                bypass_until = std::chrono::steady_clock::now() +
                    std::chrono::minutes(g_config.security_disable_minutes);
                break;
            case VerificationPopup::TEMP_ACCESS_GRANTED:
                LOG_SECURITY("Temp access granted via Telegram");
                intruder_mode = false;
                break;
            case VerificationPopup::FAILED_LOCKED:
                LOG_SECURITY("Verification FAILED — re-locking!");
                CSV_LOG("VERIFICATION_FAILED", "relocked", -1, -1,
                        "Failed verification after " + reason,
                        "", "failed", "", g_config.telegram_enabled,
                        "", -1, guardSessionDuration);
                if (g_config.telegram_enabled) {
                    Telegram::sendMessage(
                        "\xE2\x9D\x8C <b>Verification FAILED</b>\n"
                        "Intruder could not verify — laptop re-locked");
                }
                SystemAction::lockScreen();
                std::this_thread::sleep_for(std::chrono::seconds(3));
                break;
        }

        cap.open(camIdx);
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    }
};

// ═══════════════════════════════════════
// Test Mode (unchanged)
// ═══════════════════════════════════════
void runTest(FaceEngine& engine, int camIdx) {
    LOG_INFO("Test Mode — 'q' to quit");
    cv::VideoCapture cap(camIdx);
    if (!cap.isOpened()) { LOG_ERROR("Camera failed"); return; }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    int fc = 0, fps_fc = 0; double fps = 0.0;
    auto fps_start = std::chrono::steady_clock::now();
    FaceEngine::Analysis lastA;
    lastA.result = FaceEngine::NO_FACE;
    lastA.face_count = 0;
    lastA.best_distance = 999.0;
    TamperDetector tamper;

    while (g_running) {
        cv::Mat frame; cap >> frame;
        if (frame.empty()) continue;
        fc++; fps_fc++;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - fps_start).count() >= 1.0) {
            fps = fps_fc / std::chrono::duration<double>(now - fps_start).count();
            fps_fc = 0; fps_start = now;
        }
        auto tr = tamper.check(frame);
        if (fc % 3 == 0 && tr.type == TamperDetector::NONE)
            lastA = engine.analyze(frame);

        cv::Mat disp = frame.clone();
        for (const auto& fi : lastA.faces)
            Overlay::drawFaceBox(disp, fi.rect, fi.label, fi.is_owner);

        std::string status; cv::Scalar color;
        if (tr.type != TamperDetector::NONE) {
            status = "TAMPER: " + tr.desc; color = {0, 128, 255};
        } else switch (lastA.result) {
            case FaceEngine::OWNER_DETECTED:
                status = "OWNER(" + std::to_string(lastA.best_distance).substr(0, 5) + ")";
                color = {0, 255, 0}; break;
            case FaceEngine::UNKNOWN_DETECTED:
                status = "UNKNOWN"; color = {0, 0, 255}; break;
            case FaceEngine::MULTIPLE_PEOPLE:
                status = "MULTI(" + std::to_string(lastA.face_count) + ")";
                color = {0, 165, 255}; break;
            default:
                status = "Scanning..."; color = {128, 128, 128}; break;
        }
        Overlay::drawStatusBar(disp, status, color, fps, lastA.face_count);
        Overlay::drawScanline(disp, fc);
        cv::imshow("SENTINEL - Test (q quit)", disp);
        if (cv::waitKey(30) == 'q') break;
    }
    cap.release(); cv::destroyAllWindows();
}

// ═══════════════════════════════════════
// Signal, Banner, Help, Status (updated with CSV info)
// ═══════════════════════════════════════
void sigHandler(int) { g_running = false; }

void banner() {
    std::cout << Color::CYAN << R"(
  ╔═════════════════════════════════════════════════════╗
  ║  SENTINEL v3.2 Phase 3.2                            ║
  ║  [Face|Lock|Tamper|Popup|KillSwitch|Telegram|CSV]   ║
  ╚═════════════════════════════════════════════════════╝
)" << Color::NC << std::endl;
}

void help() {
    banner();
    std::cout << Color::BOLD << "COMMANDS:\n" << Color::NC
        << "  --enroll            Enroll face\n"
        << "  --guard             Start guard\n"
        << "  --test              Test detection\n"
        << "  --setup-security    Setup kill switch questions\n"
        << "  --reset             Delete face data\n"
        << "  --status            Show config\n"
        << "  --help              This help\n\n"
        << Color::BOLD << "OPTIONS:\n" << Color::NC
        << "  --camera N          Camera index\n"
        << "  --threshold N       Face threshold\n"
        << "  --preview           Show camera\n"
        << "  --timer N           No-face lock seconds\n"
        << "  --no-sound          Disable alarm\n\n"
        << Color::BOLD << "TELEGRAM COMMANDS:\n" << Color::NC
        << "  /help               Show all commands\n"
        << "  /status             Guard status\n"
        << "  /lock               Lock laptop\n"
        << "  /pause              Pause face check\n"
        << "  /resume             Resume face check\n"
        << "  /capture            Live camera photo\n"
        << "  /screenshot         Screen capture\n"
        << "  /evidence           Latest intruder photo\n"
        << "  /record             Record 30s video\n"
        << "  /record60           Record 60s video\n"
        << "  /killswitch         Activate kill switch\n"
        << "  /tempaccess5-60     Grant temp access\n"
        << "  /revoke             Revoke access\n"
        << "  /approve /deny      Handle access requests\n"
        << "  /watchscreen5-60    Monitor screenshots\n"
        << "  /watchcam5-60       Monitor camera\n"
        << "  /stopwatch          Stop monitoring\n"
        << "  /logs               Last 20 CSV log entries\n"
        << "  /exportlog          Download full CSV log\n\n";
}

void status(SecurityManager& sec) {
    banner();
    std::cout << Color::BOLD << "Configuration:\n" << Color::NC
        << "  Camera:            " << g_config.camera_index << "\n"
        << "  Threshold:         " << g_config.face_recognition_threshold << "\n"
        << "  No-face lock:      " << g_config.noface_lock_seconds << "s\n"
        << "  Verify timeout:    " << g_config.verification_timeout << "s\n"
        << "  KillSwitch timeout:" << g_config.killswitch_timeout << "s\n"
        << "  Bypass duration:   " << g_config.security_disable_minutes << " min\n"
        << "  Evidence:          " << (g_config.save_evidence ? "ON" : "OFF") << "\n"
        << "  Sound:             " << (g_config.sound_alarm ? "ON" : "OFF") << "\n"
        << "  Telegram:          " << (g_config.telegram_enabled ? "ON" : "OFF") << "\n"
        << "  Enrolled:          " << (fs::exists(g_config.owner_face_file) ? "YES" : "NO") << "\n"
        << "  Security Q's:      " << (sec.hasQuestions() ? "SET" : "NOT SET") << "\n";

    // CSV Log info
    std::string csvPath = g_config.csv_log_file;
    int csvEntries = 0;
    if (fs::exists(csvPath)) {
        std::ifstream f(csvPath);
        std::string line;
        while (std::getline(f, line)) csvEntries++;
        csvEntries = std::max(0, csvEntries - 1);
    }
    std::cout << "  CSV Log:           " << csvPath << " (" << csvEntries << " entries)\n";

    int ev = 0;
    if (fs::exists(g_config.evidence_dir))
        for (auto& p : fs::directory_iterator(g_config.evidence_dir))
            if (p.path().extension() == ".jpg") ev++;
    std::cout << "  Evidence files:    " << ev << "\n\n";
}

// ═══════════════════════════════════════
// Main (updated with CSVLogger init)
// ═══════════════════════════════════════
int main(int argc, char* argv[]) {
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    g_config.init(argv[0]);
    g_config.load();
    Logger::inst().init(g_config.log_file);
    CSVLogger::inst().init(g_config.csv_log_file);  // NEW: Init CSV Logger
    curl_global_init(CURL_GLOBAL_ALL);

    SecurityManager secMgr;
    secMgr.load();

    if (argc < 2) { help(); curl_global_cleanup(); return 0; }

    std::string cmd;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--enroll" || a == "--guard" || a == "--test" || a == "--reset" ||
            a == "--help" || a == "--status" || a == "--setup-security") cmd = a;
        else if (a == "--camera" && i + 1 < argc) g_config.camera_index = std::stoi(argv[++i]);
        else if (a == "--threshold" && i + 1 < argc)
            g_config.face_recognition_threshold = std::stod(argv[++i]);
        else if (a == "--preview") g_config.show_preview = true;
        else if (a == "--no-sound") g_config.sound_alarm = false;
        else if (a == "--timer" && i + 1 < argc)
            g_config.noface_lock_seconds = std::stoi(argv[++i]);
        else { std::cerr << Color::RED << "Unknown: " << a << Color::NC << "\n"; return 1; }
    }

    if (cmd == "--help" || cmd.empty()) { help(); curl_global_cleanup(); return 0; }
    if (cmd == "--status") { status(secMgr); curl_global_cleanup(); return 0; }
    banner();

    if (cmd == "--setup-security") { secMgr.setup(); curl_global_cleanup(); return 0; }
    if (cmd == "--reset") {
        if (fs::exists(g_config.owner_face_file)) {
            fs::remove(g_config.owner_face_file);
            LOG_INFO("Face data deleted.");
        }
        if (fs::exists(g_config.security_file)) {
            fs::remove(g_config.security_file);
            LOG_INFO("Security questions deleted.");
        }
        curl_global_cleanup(); return 0;
    }

    if (!fs::exists(g_config.shape_predictor_path) ||
        !fs::exists(g_config.face_rec_model_path)) {
        LOG_ERROR("Models missing! Run setup.sh");
        curl_global_cleanup(); return 1;
    }

    fs::create_directories(g_config.data_dir);
    fs::create_directories(g_config.evidence_dir);

    FaceEngine engine;
    if (!engine.init()) { curl_global_cleanup(); return 1; }

    if (cmd == "--enroll") {
        if (engine.isEnrolled()) {
            std::cout << Color::YELLOW << "Re-enroll? (y/n): " << Color::NC;
            char c; std::cin >> c;
            if (c != 'y' && c != 'Y') { curl_global_cleanup(); return 0; }
            engine.reset();
        }
        if (engine.enroll(g_config.camera_index))
            LOG_INFO("Enrolled! Run: ./sentinel --guard");
        else { LOG_ERROR("Enrollment failed!"); curl_global_cleanup(); return 1; }
    }
    else if (cmd == "--guard") {
        Guard guard(engine, secMgr);
        guard.run(g_config.camera_index);
    }
    else if (cmd == "--test") {
        runTest(engine, g_config.camera_index);
    }

    LOG_INFO("SENTINEL shutdown.");
    curl_global_cleanup();
    return 0;
}