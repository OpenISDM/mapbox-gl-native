#pragma once

#include <mbgl/storage/file_source.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wnested-anon-types"
#include <nan.h>
#pragma GCC diagnostic pop

namespace mbgl {

namespace util {
template <typename T> class Thread;
}

class NodeFileSource : public FileSource {
public:
    NodeFileSource(v8::Local<v8::Object>);
    ~NodeFileSource() override;

    // FileSource API
    Request* request(const Resource&, uv_loop_t*, Callback) override;
    void cancel(Request*) override;

public:
    class Impl;

private:
    const std::unique_ptr<util::Thread<Impl>> thread;
};

}
