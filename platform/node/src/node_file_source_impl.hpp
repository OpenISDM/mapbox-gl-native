#pragma once

#include "node_file_source.hpp"

#include <set>
#include <unordered_map>

namespace mbgl {

class RequestBase;

class NodeFileRequest : private util::noncopyable, public Nan::ObjectWrap {
public:
    static Nan::Persistent<v8::Function> constructor;

    static NAN_MODULE_INIT(Init);
    static NAN_METHOD(New);

    static v8::Handle<v8::Object> Create(const mbgl::Resource&);

    static NAN_METHOD(Callback);

    const Resource resource;
    RequestBase* inProgress = nullptr;

    inline NodeFileRequest(const Resource& resource_)
        : resource(resource_) {}
    ~NodeFileRequest();

    // Observer accessors.
    void addObserver(Request*);
    void removeObserver(Request*);
    bool hasObservers() const;

    // Updates/gets the response of this request object.
    void setResponse(const std::shared_ptr<const Response>&);
    const std::shared_ptr<const Response>& getResponse() const;

    // Notifies all observers.
    void notify();


private:
    // Stores a set of all observing Request objects.
    std::set<Request*> observers;

    // The current response data. We're storing it because we can satisfy requests for the same
    // resource directly by returning this response object. We also need it to create conditional
    // HTTP requests, and to check whether new responses we got changed any data.
    std::shared_ptr<const Response> response;
};

class NodeFileSource::Impl {
public:
    Impl(v8::Local<v8::Object> options_);
    ~Impl();

    void add(Request*);
    void cancel(Request*);

private:
    void update(NodeFileRequest&);
    void startRequest(NodeFileRequest&);

    std::unordered_map<Resource, Nan::Persistent<v8::Object>, Resource::Hash> pending;
    Nan::Persistent<v8::Object> options;

    uv_loop_t* const loop;
};

}
