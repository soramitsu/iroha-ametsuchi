#pragma once

#include "block_index.h"
#include <cpp_redis/cpp_redis>

namespace ametsuchi{
namespace block_index{

class BlockIndexRedis: public BlockIndex{
 public:
  BlockIndexRedis(std::string host, size_t port): host_(host), port_(port){
    client_.connect(host, port);
  }

  ~BlockIndexRedis();

  bool add_blockhash_blockid(std::string blockhash, size_t blockid);
  size_t get_blockid_by_blockhash(std::string blockhash);


 private:
  cpp_redis::redis_client client_;
  std::string host_;
  size_t port_;
};

}
}