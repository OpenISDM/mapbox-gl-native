#include <mbgl/util/async_task.hpp>

#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/uv_detail.hpp>

#include <atomic>
#include <functional>

namespace mbgl {
namespace util {

class AsyncTask::Impl {
public:
    Impl(std::function<void()>&& fn)
        : async(RunLoop::getLoop(), [this] { received(); })
        , timer(RunLoop::getLoop())
        , task(std::move(fn))
        , queued(ATOMIC_FLAG_INIT) {
    }

    void maySend() {
        if (!queued.test_and_set()) {
            async.send();
        }
    }

    void received() {
        if (throttle) {
            timer.start(throttle, 0, [this] { runTask(); });
        } else {
            runTask();
        }
    }

    void runTask() {
        queued.clear();
        task();
    }

    void unref() {
        async.unref();
        timer.unref();
    }

    void setThrottle(unsigned msec) {
        throttle = msec;
    }

private:
    uv::async async;
    uv::timer timer;

    std::function<void()> task;
    std::atomic_flag queued;

    unsigned throttle = 0;
};

AsyncTask::AsyncTask(std::function<void()>&& fn)
    : impl(std::make_unique<Impl>(std::move(fn))) {
}

AsyncTask::~AsyncTask() {
}

void AsyncTask::send() {
    impl->maySend();
}

void AsyncTask::unref() {
    impl->unref();
}

void AsyncTask::setThrottle(unsigned msec) {
    impl->setThrottle(msec);
}

}
}
