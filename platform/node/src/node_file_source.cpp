#include "node_file_source_impl.hpp"
#include <mbgl/storage/request.hpp>

#include <mbgl/util/thread.hpp>
#include <mbgl/util/exception.hpp>

#include <cassert>
#include <iostream>

namespace mbgl {

NodeFileSource::NodeFileSource(v8::Local<v8::Object> options)
    : thread(std::make_unique<util::Thread<Impl>>(
          util::ThreadContext{ "FileSource", util::ThreadType::Unknown, util::ThreadPriority::Low },
          options)) {
}

NodeFileSource::~NodeFileSource() {
    MBGL_VERIFY_THREAD(tid);
}

Request* NodeFileSource::request(const Resource& resource, uv_loop_t* l, Callback callback) {
    assert(l);

    if (!callback) {
        throw util::MisuseException("FileSource callback can't be empty");
    }

    auto req = new Request(resource, l, std::move(callback));
    thread->invoke(&Impl::add, req);
    return req;
}

void NodeFileSource::cancel(Request* req) {
    assert(req);
    req->cancel();
    thread->invoke(&Impl::cancel, req);
}

// ----- Impl -----

NodeFileSource::Impl::Impl(v8::Local<v8::Object> options_)
    : options(options_),
    loop(util::RunLoop::getLoop()) {
}

NodeFileSource::Impl::~Impl() {
    options.Reset();
}

void NodeFileSource::Impl::add(Request* req) {
    auto& request = *Nan::ObjectWrap::Unwrap<NodeFileRequest>(
        Nan::New<v8::Object>(
            pending.emplace(
                req->resource,
                NodeFileRequest::Create(req->resource)->ToObject()
            ).first->second));

    // Trigger a potentially required refresh of this Request
    update(request);

    // Add this request as an observer so that it'll get notified when something about this
    // request changes.
    request.addObserver(req);
}

void NodeFileSource::Impl::update(NodeFileRequest& request) {
    if (request.getResponse()) {
        // The observers have been notified already; send what we have to the new one as well.
    } else if (!request.inProgress) {
        // There is no request in progress, and we don't have a response yet. This means we'll have
        // to start the request ourselves.
        startRequest(request);
    } else {
        // There is a request in progress. We just have to wait.
    }
}

void NodeFileSource::Impl::startRequest(NodeFileRequest& request) {
    assert(!request.inProgress);

    std::cout << &request << std::endl;

    // TODO: actually send the callback to Node-land

    /*
    auto callback = [this, &request](std::shared_ptr<const Response> response) {
        request.inProgress = nullptr;
        request.setResponse(response);
        request.notify();
    };

    request.inProgress =
        assetContext->createRequest(request.resource, callback, loop, assetRoot);

    auto callback = Nan::GetFunction(Nan::New<v8::FunctionTemplate>(NodeFileRequest::Callback, request)).ToLocalChecked();
    callback->SetName(Nan::New("respond").ToLocalChecked());

    v8::Local<v8::Value> argv[] = { request, callback };
    Nan::MakeCallback(Nan::New(options), "request", 2, argv);
    */
}

void NodeFileSource::Impl::cancel(Request* req) {
    auto it = pending.find(req->resource);
    if (it != pending.end()) {
        // If the number of dependent requests of the NodeFileRequest drops to zero,
        // cancel the request and remove it from the pending list.
        auto& request = *Nan::ObjectWrap::Unwrap<NodeFileRequest>(
            Nan::New<v8::Object>(it->second));
        request.removeObserver(req);
        if (!request.hasObservers()) {
            pending.erase(it);
        }

        // TODO: fire JS cancel method if defined

        /*
        if (Nan::Has(Nan::New(options), Nan::New("cancel").ToLocalChecked()).FromJust()) {
            v8::Local<v8::Value> argv[] = { request };
            Nan::MakeCallback(Nan::New(options), "cancel", 1, argv);
        }
        */
    } else {
        // There is no request for this URL anymore. Likely, the request already completed
        // before we got around to process the cancelation request.
    }

    // Send a message back to the requesting thread and notify it that this request has been
    // canceled and is now safe to be deleted.
    req->destruct();
}

// ----- NodeFileRequest -----

Nan::Persistent<v8::Function> NodeFileRequest::constructor;

NAN_MODULE_INIT(NodeFileRequest::Init) {
    // Prepare constructor template
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->SetClassName(Nan::New("Request").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    constructor.Reset(tpl->GetFunction());
    Nan::Set(target, Nan::New("Request").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(NodeFileRequest::New) {
    if (info.Length() < 1 || !info[0]->IsExternal()) {
        return Nan::ThrowTypeError("Cannot create Request objects explicitly");
    }

    auto resource = reinterpret_cast<const Resource*>(info[0].As<v8::External>()->Value());
    auto req = new NodeFileRequest(*resource);
    req->Wrap(info.This());

    info.GetReturnValue().Set(info.This());
}

NodeFileRequest::~NodeFileRequest() {
    if (inProgress) {
        // inProgress->cancel();
        inProgress = nullptr;
    }
}

v8::Handle<v8::Object> NodeFileRequest::Create(const Resource& resource) {
    Nan::EscapableHandleScope scope;

    v8::Local<v8::Value> argv[] = {
        Nan::New<v8::External>(const_cast<Resource*>(&resource))
    };
    auto instance = Nan::New(constructor)->NewInstance(1, argv);

    Nan::Set(instance, Nan::New("url").ToLocalChecked(), Nan::New(resource.url).ToLocalChecked());
    Nan::Set(instance, Nan::New("kind").ToLocalChecked(), Nan::New<v8::Integer>(int(resource.kind)));

    return scope.Escape(instance);
}

NAN_METHOD(NodeFileRequest::Callback) {
    auto request = Nan::ObjectWrap::Unwrap<NodeFileRequest>(info.Data().As<v8::Object>());

    using Error = mbgl::Response::Error;

    auto resource = std::move(request->resource);
    auto response = std::make_shared<mbgl::Response>();

    if (info.Length() < 1) {
        response->error = std::make_unique<Error>(Error::Reason::NotFound);
        request->setResponse(response);
    } else if (info[0]->BooleanValue()) {
        // Store the error string.
        const Nan::Utf8String message { info[0]->ToString() };
        response->error = std::make_unique<Error>(
            Error::Reason::Other, std::string{ *message, size_t(message.length()) });
        request->setResponse(response);
    } else if (info.Length() < 2 || !info[1]->IsObject()) {
        response->error = std::make_unique<Error>(
            Error::Reason::Other, "Second argument must be a response object");
        request->setResponse(response);
    } else {
        auto res = info[1]->ToObject();

        if (Nan::Has(res, Nan::New("data").ToLocalChecked()).FromJust()) {
            auto data = Nan::Get(res, Nan::New("data").ToLocalChecked()).ToLocalChecked();
            if (node::Buffer::HasInstance(data)) {
                // TODO: is this an unnecessary copy operation?
                response->data = std::make_shared<std::string>(
                    node::Buffer::Data(data),
                    node::Buffer::Length(data)
                );
            } else {
                response->error = std::make_unique<Error>(
                    Error::Reason::Other, "Response data must be a Buffer");
            }
        }

        if (Nan::Has(res, Nan::New("modified").ToLocalChecked()).FromJust()) {
            const double modified = Nan::Get(res, Nan::New("modified").ToLocalChecked()).ToLocalChecked()->ToNumber()->Value();
            if (!std::isnan(modified)) {
                // JS timestamps are milliseconds
                response->modified = modified / 1000;
            }
        }

        if (Nan::Has(res, Nan::New("expires").ToLocalChecked()).FromJust()) {
            const double expires = Nan::Get(res, Nan::New("expires").ToLocalChecked()).ToLocalChecked()->ToNumber()->Value();
            if (!std::isnan(expires)) {
                // JS timestamps are milliseconds
                response->expires = expires / 1000;
            }
        }

        if (Nan::Has(res, Nan::New("etag").ToLocalChecked()).FromJust()) {
            auto etagHandle = Nan::Get(res, Nan::New("etag").ToLocalChecked()).ToLocalChecked();
            if (etagHandle->BooleanValue()) {
                const Nan::Utf8String etag { etagHandle->ToString() };
                response->etag = std::string { *etag, size_t(etag.length()) };
            }
        }

        request->setResponse(response);
    }

    // Notify all observers.
    request->notify();

    info.GetReturnValue().SetUndefined();
}

void NodeFileRequest::addObserver(Request* req) {
    observers.insert(req);

    if (response) {
        // We've got a response, so send the response to the requester.
        req->notify(response);
    }
}

void NodeFileRequest::removeObserver(Request* req) {
    observers.erase(req);
}

bool NodeFileRequest::hasObservers() const {
    return !observers.empty();
}

void NodeFileRequest::notify() {
    if (response) {
        for (auto req : observers) {
            req->notify(response);
        }
    }
}

void NodeFileRequest::setResponse(const std::shared_ptr<const Response>& response_) {
    response = response_;
}

const std::shared_ptr<const Response>& NodeFileRequest::getResponse() const {
    return response;
}

} // namespace mbgl
