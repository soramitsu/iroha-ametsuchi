/**
 * Copyright Soramitsu Co., Ltd. 2016 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef AMETSUCHI_WSV_H
#define AMETSUCHI_WSV_H


#include <flatbuffers/flatbuffers.h>
#include <lmdb.h>
#include <unordered_map>
#include <ametsuchi/generated/commands_generated.h>
#include <ametsuchi/common.h>

namespace ametsuchi {

class WSV{

 public:
   WSV();
  ~WSV();

  void update(const flatbuffers::Vector<uint8_t> *blob);

  void init(MDB_txn* append_tx);

  /**
   * Commit appended data to database. Commit creates the latest 'checkpoint',
   * when you can not rollback.
   */
  void close_cursors();


  // WSV queries:
  AM_val accountGetAsset(const flatbuffers::String *pubKey,
                         const flatbuffers::String *ln,
                         const flatbuffers::String *dn,
                         const flatbuffers::String *an,
                         bool uncommitted = true, MDB_env* env = nullptr);



 private:
  //size_t tx_store_total;
  std::unordered_map<std::string, std::pair<MDB_dbi, MDB_cursor *>> trees_;
  MDB_txn *append_tx_;
  std::unordered_map<std::string, std::vector<uint8_t>> created_assets_;
  void read_created_assets();

  //WSV commands:
  void asset_create(const iroha::AssetCreate *command);
  void asset_add(const iroha::AssetAdd *command);
  void asset_remove(const iroha::AssetRemove *command);
  void asset_transfer(const iroha::AssetTransfer *command);
  void account_add(const iroha::AccountAdd *command);
  void account_remove(const iroha::AccountRemove *command);
  void peer_add(const iroha::PeerAdd *command);
  void peer_remove(const iroha::PeerRemove *command);
  // manipulate with account's assets using these functions
  void account_add_currency(const flatbuffers::String *acc_pub_key,
                            const iroha::Currency *c, size_t c_size);
  void account_remove_currency(const flatbuffers::String *acc_pub_key,
                                    const iroha::Currency *c);



};


}

#endif //AMETSUCHI_WSV_H
