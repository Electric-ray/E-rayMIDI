// AAudioRecover.h
//
// AAudio 출력 스트림 하나를 "끊겨도 스스로 되살리는" 헬퍼. 모든 엔진 브리지가 공유한다.
//
// 왜 필요한가:
//   블루투스 통화(A2DP -> SCO -> A2DP)나 이어폰 탈착처럼 출력 경로가 바뀌면 AAudio는 스트림을
//   AAUDIO_STREAM_STATE_DISCONNECTED 로 만들고 데이터 콜백을 영영 멈춘다. 앱은 계속 "재생 중"인데
//   소리만 안 나고, 다시 연결하거나 앱을 껐다 켜기 전에는 돌아오지 않는다.
//   (에러 콜백을 등록하지 않으면 앱이 이 사실을 알 방법도 없다.)
//
// 동작:
//   - onError(): 데이터 콜백 스레드에서 불리는 AAudio 에러 콜백. DISCONNECTED 면 복구 요청만 걸고 즉시
//     반환한다 (콜백 안에서 스트림을 닫으면 교착한다).
//   - 감시 스레드(arm 이후): 500ms마다 확인해서
//       * 상태가 DISCONNECTED 이거나
//       * STARTED 인데 2초 넘게 AAudioStream_getFramesRead 가 늘지 않으면(조용히 멈춘 경우)
//     스트림을 닫고 같은 조건으로 다시 열어 start 한다 (재시작 간격 3초 이상).
//   - 데이터 콜백은 링버퍼 pop만 하므로, 새 스트림이 뜨면 렌더 스레드는 아무 일도 없었던 듯 이어진다.
//
// 사용:
//   static eray::AAudioRecover s_recover;
//   빌더:      AAudioStreamBuilder_setErrorCallback(b, eray::AAudioRecover::onError, &s_recover);
//   start 직후: s_recover.arm(&s_stream, [](AAudioStream** o) { return openSameWay(o); });
//   stop/term:  s_recover.disarm();   // 반드시 s_stream 을 requestStop/close 하기 "전에"
#pragma once

#include <aaudio/AAudio.h>
#include <android/log.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace eray {

class AAudioRecover {
public:
    // 새 스트림을 같은 조건으로 연다 (에러 콜백 등록 포함). 성공하면 AAUDIO_OK + *out 채움.
    using Opener = std::function<aaudio_result_t(AAudioStream** out)>;

    ~AAudioRecover() { disarm(); }

    static void onError(AAudioStream*, void* userData, aaudio_result_t error) {
        auto* self = static_cast<AAudioRecover*>(userData);
        __android_log_print(ANDROID_LOG_WARN, "AAudioRecover", "stream error: %s", AAudio_convertResultToText(error));
        if (self && error == AAUDIO_ERROR_DISCONNECTED) self->requestRestart();
    }

    // stream: 브리지가 가진 스트림 포인터(복구 시 이 값을 갈아끼운다). 이 시점에 이미 start 된 상태여야 한다.
    void arm(AAudioStream** stream, Opener opener) {
        disarm();
        m_ref = stream;
        m_opener = std::move(opener);
        m_restartReq = false;
        m_armed = true;
        m_thread = std::thread([this] { loop(); });
    }

    // 감시 스레드를 멈추고 합류한다. 이후 브리지가 스트림을 마음대로 stop/close 해도 안전하다.
    void disarm() {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_armed = false;
        }
        m_cv.notify_all();
        if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id()) m_thread.join();
    }

    // Kotlin 쪽에서 "지금 한번 다시 열어라"(예: 통화가 끝나 오디오 포커스를 되찾았을 때)
    void requestRestart() {
        {
            std::lock_guard<std::mutex> lk(m_mtx);
            m_restartReq = true;
        }
        m_cv.notify_all();
    }

private:
    using Clock = std::chrono::steady_clock;

    void loop() {
        int64_t lastFrames = -1;
        auto lastAdvance = Clock::now();
        auto lastRestart = Clock::now() - std::chrono::seconds(10);
        std::unique_lock<std::mutex> lk(m_mtx);
        while (m_armed) {
            m_cv.wait_for(lk, std::chrono::milliseconds(500), [this] { return !m_armed || m_restartReq; });
            if (!m_armed) break;
            const auto now = Clock::now();
            bool restart = m_restartReq;
            m_restartReq = false;

            AAudioStream* s = m_ref ? *m_ref : nullptr;
            if (!restart && s) {
                const aaudio_stream_state_t st = AAudioStream_getState(s);
                if (st == AAUDIO_STREAM_STATE_DISCONNECTED) {
                    restart = true;
                } else if (st == AAUDIO_STREAM_STATE_STARTED) {
                    const int64_t fr = AAudioStream_getFramesRead(s);
                    if (fr != lastFrames) {
                        lastFrames = fr;
                        lastAdvance = now;
                    } else if (now - lastAdvance > std::chrono::seconds(2) &&
                               now - lastRestart > std::chrono::seconds(3)) {
                        __android_log_print(ANDROID_LOG_WARN, "AAudioRecover", "stream stalled (framesRead=%lld)", (long long)fr);
                        restart = true;
                    }
                } else {
                    lastAdvance = now;   // STARTING / PAUSED / STOPPED 등은 정상 전이 구간
                }
            }
            if (!restart) continue;

            lk.unlock();
            const bool ok = restartStream();
            lk.lock();
            lastRestart = Clock::now();
            lastAdvance = lastRestart;
            lastFrames = -1;
            if (!ok) m_restartReq = true;   // 실패하면 다음 주기에 다시
        }
    }

    bool restartStream() {
        if (!m_ref) return false;
        // 오래된 스트림 폐기 (DISCONNECTED 상태에서도 close 가능; 이 스레드는 데이터 콜백 스레드가 아니다)
        if (AAudioStream* old = *m_ref) {
            *m_ref = nullptr;
            AAudioStream_close(old);
        }
        for (int attempt = 0; attempt < 5; ++attempt) {
            {
                std::lock_guard<std::mutex> lk(m_mtx);
                if (!m_armed) return true;
            }
            AAudioStream* ns = nullptr;
            const aaudio_result_t r = m_opener ? m_opener(&ns) : AAUDIO_ERROR_INVALID_STATE;
            if (r == AAUDIO_OK && ns) {
                if (AAudioStream_requestStart(ns) == AAUDIO_OK) {
                    *m_ref = ns;
                    __android_log_print(ANDROID_LOG_INFO, "AAudioRecover", "stream restarted @ %d Hz",
                                        AAudioStream_getSampleRate(ns));
                    return true;
                }
                AAudioStream_close(ns);
            }
            __android_log_print(ANDROID_LOG_WARN, "AAudioRecover", "reopen attempt %d failed (%s)", attempt + 1,
                                AAudio_convertResultToText(r));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        return false;
    }

    std::mutex m_mtx;
    std::condition_variable m_cv;
    bool m_armed = false;
    bool m_restartReq = false;
    AAudioStream** m_ref = nullptr;
    Opener m_opener;
    std::thread m_thread;
};

} // namespace eray
