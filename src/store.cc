#include "threadpool.h"

#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>

#include "store.grpc.pb.h"
#include "vendor.grpc.pb.h"


using vendor::Vendor;
using vendor::TransactionQuery;
using vendor::TransactionReply;

using store::ProductQuery;
using store::ProductReply;
using store::ProductQueryItem;
using store::Store;

using grpc::Channel;
using grpc::CompletionQueue;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;

class Threadpool;

class StoreImpl {
 public:
  void Run(std::string server_address, int pool_size) {
    ServerBuilder builder;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

    builder.RegisterService(&service);

    store_request_cq = builder.AddCompletionQueue();

    store_server = builder.BuildAndStart();

    std::cout << "Server listening on " << server_address << std::endl;

    // Spawning threads to handle the requests
    Threadpool threadpool(pool_size);
    threadpool.intializeThreadpool(this);
    threadpool.waitForThreads();
  }

	void setUpConnections(){
		std::ifstream vendor_address("./vendor_addresses.txt");
		std::string vendor_server;
		int j=0;
		while(vendor_address >> vendor_server ){
			//form a channel and create a Stub
			int i = vendor_server.find(";");
			vendor_server = vendor_server.substr(0, i);
			std::shared_ptr<Channel> channel = grpc::CreateChannel(vendor_server, grpc::InsecureChannelCredentials());
			std::unique_ptr<Vendor::Stub> vendor_stub = Vendor::NewStub(channel);
			vendor_stubs.push_back(std::move(vendor_stub));  //unique_ptr cannot be copied, have to be moved
			vendor_channels.push_back(channel);
			vendor_id_map[vendor_server] = j;
			j++;
		}
	}

	bool reserveProduct(std::string vendor, std::string product_name, int count){
			TransactionQuery request;
			request.set_product_name(product_name);
			request.set_count(count);

			TransactionReply reply;
			ClientContext context;
			CompletionQueue cq;
			Status status;

			std::unique_ptr<ClientAsyncResponseReader<TransactionReply> > rpc;

       // reserve
      rpc = vendor_stubs[vendor_id_map[vendor]]->AsyncreserveProduct(&context,request,&cq);
      rpc->Finish(&reply,&status,this);

			void* got_tag;
			bool ok = false;
      GPR_ASSERT(cq.Next(&got_tag, &ok));
      GPR_ASSERT(ok);
      if(got_tag == this) {
          if(status.ok()) {
              return reply.success();
          }
      }
			return false;
	}

	void releaseProduct(std::string vendor, std::string product_name, int count){
			TransactionQuery request;
			request.set_product_name(product_name);
			request.set_count(count);

			TransactionReply reply;
			ClientContext context;
			CompletionQueue cq;
			Status status;

			std::unique_ptr<ClientAsyncResponseReader<TransactionReply> > rpc;

			 // reserve
			rpc = vendor_stubs[vendor_id_map[vendor]]->AsyncreleaseProduct(&context,request,&cq);
			rpc->Finish(&reply,&status,this);

			void* got_tag;
			bool ok = false;
			GPR_ASSERT(cq.Next(&got_tag, &ok));
			GPR_ASSERT(ok);
			if(got_tag == this) {
					if(!status.ok()) {
							std::cout << "Unable to release products\n";
					}
			}

	}

	bool sellProduct(std::string vendor, std::string product_name, int count){
			TransactionQuery request;
			request.set_product_name(product_name);
			request.set_count(count);

			TransactionReply reply;
			ClientContext context;
			CompletionQueue cq;
			Status status;

			std::unique_ptr<ClientAsyncResponseReader<TransactionReply> > rpc;

			 // reserve
			rpc = vendor_stubs[vendor_id_map[vendor]]->AsyncsellProduct(&context,request,&cq);
			rpc->Finish(&reply,&status,this);

			void* got_tag;
			bool ok = false;
			GPR_ASSERT(cq.Next(&got_tag, &ok));
			GPR_ASSERT(ok);
			if(got_tag == this) {
					if(!status.ok()) {
							return reply.success();
					}
			}

	}

  void handleRequests() {
    // Spawn a new CallData instance to serve new clients.
    new RequestHandler(&service, store_request_cq.get(), this);
    void* tag;  // uniquely identifies a request.
    bool ok;
    while (true) {
      GPR_ASSERT(store_request_cq->Next(&tag, &ok));
      GPR_ASSERT(ok);
      static_cast<RequestHandler*>(tag)->processRequest();
    }
  }

 private:
  class RequestHandler {
   public:
    RequestHandler(Store::AsyncService* service, ServerCompletionQueue* cq, StoreImpl* store)
        : service(service), store_request_cq(cq), response_writer(&store_server_context), finish(false), store(store) {
        service->RequestbuyProducts(&store_server_context, &request, &response_writer, store_request_cq, store_request_cq,
                                    this);
    }

    void processRequest() {
    	ProductReply reply;

      if (!finish) {
        new RequestHandler(service, store_request_cq, store);

        //reply = store->getProductBids(request.product_name());
				//loop through each query and check if all can be reserved

				std::map<std::string, std::pair<std::string, int>> reserved;
				bool allReserved=true;
				int num_queries = request.query_items_size();
				for (int i = 0; i < num_queries; i++) {
						ProductQueryItem query = request.query_items(i);
						if (store->reserveProduct(query.vendor_id(), query.product_name(), query.count())) {
							std::cout << "Reserved " << query.product_name() << "\n" ;
							reserved[query.vendor_id()] = std::make_pair(query.product_name(), query.count());
						} else {
							std::cout << "Not Reserved " << query.product_name() << "\n" ;
							allReserved = false;
							break;
						}
				}

				if (allReserved) {
					bool allSold=true;
					for (int i = 0; i < num_queries; i++) {
							ProductQueryItem query = request.query_items(i);
							if (store->sellProduct(query.vendor_id(), query.product_name(), query.count())) {
								std::cout << "Sold " << query.product_name() << "\n" ;
							} else {
								std::cout << "Unable to sell " << query.product_name() << "\n" ;
								allSold = false;
								break;
							}
					}
					if (allSold) {
						reply.set_success(true);
					} else {
						reply.set_success(false);
					}
					//if we are not able to sell everything, what to do?
				} else {
					if(reserved.size()>0){
						//release the items
						for (auto entry: reserved) {
							store->releaseProduct(entry.first, entry.second.first, entry.second.second);
						}
					}
					reply.set_success(false);
				}

        finish = true;
        response_writer.Finish(reply, Status::OK, this);

      } else {
        delete this;
      }
    }

   private:
    Store::AsyncService* service;
    ServerCompletionQueue* store_request_cq;
    ServerContext store_server_context;
    ProductQuery request;
    ProductReply reply;
    StoreImpl* store;

    ServerAsyncResponseWriter<ProductReply> response_writer;
    bool finish;
  };

  std::unique_ptr<ServerCompletionQueue> store_request_cq;
  Store::AsyncService service;
  std::unique_ptr<Server> store_server;
  std::vector<std::unique_ptr<Vendor::Stub>> vendor_stubs;
  std::vector<std::shared_ptr<Channel>> vendor_channels;
	std::map<std::string, int> vendor_id_map;
};

Threadpool::Threadpool(int pool_size): pool_size(pool_size){}

void Threadpool::intializeThreadpool(StoreImpl *store){
  for (auto i = 0; i < 5; i++) {
    std::thread t(&StoreImpl::handleRequests, store);
    this->threads.push_back(std::move(t));
  }
}

void Threadpool::waitForThreads(){
  for (auto i = 0; i < 5; i++) {
    this->threads[i].join();
  }
}

int main(int argc, char** argv) {
  if(argc < 2){
    std::cout << "Incorrect number of arguments" << std::endl;
  }

  std::string server_address = argv[1];
  int pool_size = atoi(argv[2]);

  StoreImpl storeObj;
  storeObj.setUpConnections();
  storeObj.Run(server_address, pool_size);

  return 0;
}
