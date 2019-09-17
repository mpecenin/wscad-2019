// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: schedule.proto

#include "schedule.pb.h"
#include "schedule.grpc.pb.h"

#include <grpc++/impl/codegen/async_stream.h>
#include <grpc++/impl/codegen/async_unary_call.h>
#include <grpc++/impl/codegen/channel_interface.h>
#include <grpc++/impl/codegen/client_unary_call.h>
#include <grpc++/impl/codegen/method_handler_impl.h>
#include <grpc++/impl/codegen/rpc_service_method.h>
#include <grpc++/impl/codegen/service_type.h>
#include <grpc++/impl/codegen/sync_stream.h>

static const char* ScheduleService_method_names[] = {
  "/ScheduleService/init",
  "/ScheduleService/step",
  "/ScheduleService/reset",
  "/ScheduleService/render",
  "/ScheduleService/close",
};

std::unique_ptr< ScheduleService::Stub> ScheduleService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  std::unique_ptr< ScheduleService::Stub> stub(new ScheduleService::Stub(channel));
  return stub;
}

ScheduleService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_init_(ScheduleService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_step_(ScheduleService_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_reset_(ScheduleService_method_names[2], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_render_(ScheduleService_method_names[3], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_close_(ScheduleService_method_names[4], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status ScheduleService::Stub::init(::grpc::ClientContext* context, const ::ScheduleInitRequest& request, ::ScheduleInitResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_init_, context, request, response);
}

::grpc::ClientAsyncResponseReader< ::ScheduleInitResponse>* ScheduleService::Stub::AsyncinitRaw(::grpc::ClientContext* context, const ::ScheduleInitRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleInitResponse>::Create(channel_.get(), cq, rpcmethod_init_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::ScheduleInitResponse>* ScheduleService::Stub::PrepareAsyncinitRaw(::grpc::ClientContext* context, const ::ScheduleInitRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleInitResponse>::Create(channel_.get(), cq, rpcmethod_init_, context, request, false);
}

::grpc::Status ScheduleService::Stub::step(::grpc::ClientContext* context, const ::ScheduleStepRequest& request, ::ScheduleStepResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_step_, context, request, response);
}

::grpc::ClientAsyncResponseReader< ::ScheduleStepResponse>* ScheduleService::Stub::AsyncstepRaw(::grpc::ClientContext* context, const ::ScheduleStepRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleStepResponse>::Create(channel_.get(), cq, rpcmethod_step_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::ScheduleStepResponse>* ScheduleService::Stub::PrepareAsyncstepRaw(::grpc::ClientContext* context, const ::ScheduleStepRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleStepResponse>::Create(channel_.get(), cq, rpcmethod_step_, context, request, false);
}

::grpc::Status ScheduleService::Stub::reset(::grpc::ClientContext* context, const ::ScheduleResetRequest& request, ::ScheduleResetResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_reset_, context, request, response);
}

::grpc::ClientAsyncResponseReader< ::ScheduleResetResponse>* ScheduleService::Stub::AsyncresetRaw(::grpc::ClientContext* context, const ::ScheduleResetRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleResetResponse>::Create(channel_.get(), cq, rpcmethod_reset_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::ScheduleResetResponse>* ScheduleService::Stub::PrepareAsyncresetRaw(::grpc::ClientContext* context, const ::ScheduleResetRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleResetResponse>::Create(channel_.get(), cq, rpcmethod_reset_, context, request, false);
}

::grpc::Status ScheduleService::Stub::render(::grpc::ClientContext* context, const ::ScheduleRenderRequest& request, ::ScheduleRenderResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_render_, context, request, response);
}

::grpc::ClientAsyncResponseReader< ::ScheduleRenderResponse>* ScheduleService::Stub::AsyncrenderRaw(::grpc::ClientContext* context, const ::ScheduleRenderRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleRenderResponse>::Create(channel_.get(), cq, rpcmethod_render_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::ScheduleRenderResponse>* ScheduleService::Stub::PrepareAsyncrenderRaw(::grpc::ClientContext* context, const ::ScheduleRenderRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleRenderResponse>::Create(channel_.get(), cq, rpcmethod_render_, context, request, false);
}

::grpc::Status ScheduleService::Stub::close(::grpc::ClientContext* context, const ::ScheduleCloseRequest& request, ::ScheduleCloseResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_close_, context, request, response);
}

::grpc::ClientAsyncResponseReader< ::ScheduleCloseResponse>* ScheduleService::Stub::AsynccloseRaw(::grpc::ClientContext* context, const ::ScheduleCloseRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleCloseResponse>::Create(channel_.get(), cq, rpcmethod_close_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::ScheduleCloseResponse>* ScheduleService::Stub::PrepareAsynccloseRaw(::grpc::ClientContext* context, const ::ScheduleCloseRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::ScheduleCloseResponse>::Create(channel_.get(), cq, rpcmethod_close_, context, request, false);
}

ScheduleService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ScheduleService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ScheduleService::Service, ::ScheduleInitRequest, ::ScheduleInitResponse>(
          std::mem_fn(&ScheduleService::Service::init), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ScheduleService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ScheduleService::Service, ::ScheduleStepRequest, ::ScheduleStepResponse>(
          std::mem_fn(&ScheduleService::Service::step), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ScheduleService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ScheduleService::Service, ::ScheduleResetRequest, ::ScheduleResetResponse>(
          std::mem_fn(&ScheduleService::Service::reset), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ScheduleService_method_names[3],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ScheduleService::Service, ::ScheduleRenderRequest, ::ScheduleRenderResponse>(
          std::mem_fn(&ScheduleService::Service::render), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      ScheduleService_method_names[4],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< ScheduleService::Service, ::ScheduleCloseRequest, ::ScheduleCloseResponse>(
          std::mem_fn(&ScheduleService::Service::close), this)));
}

ScheduleService::Service::~Service() {
}

::grpc::Status ScheduleService::Service::init(::grpc::ServerContext* context, const ::ScheduleInitRequest* request, ::ScheduleInitResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ScheduleService::Service::step(::grpc::ServerContext* context, const ::ScheduleStepRequest* request, ::ScheduleStepResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ScheduleService::Service::reset(::grpc::ServerContext* context, const ::ScheduleResetRequest* request, ::ScheduleResetResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ScheduleService::Service::render(::grpc::ServerContext* context, const ::ScheduleRenderRequest* request, ::ScheduleRenderResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status ScheduleService::Service::close(::grpc::ServerContext* context, const ::ScheduleCloseRequest* request, ::ScheduleCloseResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

