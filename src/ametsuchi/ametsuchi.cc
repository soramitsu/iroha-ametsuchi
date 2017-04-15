/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
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

#include <ametsuchi/ametsuchi.h>
#include <ametsuchi/generated/transaction_generated.h>

// static auto console = spdlog::stdout_color_mt("ametsuchi");


namespace ametsuchi {


Ametsuchi::Ametsuchi(const std::string &db_folder)
    : path_(db_folder), tree(AMETSUCHI_BLOCK_SIZE), tx_store(), wsv() {
  // initialize database:
  // create folder, create all handles and btrees
  // in case of any errors print error to stdout and exit
  init();
}


Ametsuchi::~Ametsuchi() {
  abort_append_tx();

  tx_store.close_dbi(env);
  wsv.close_dbi(env);
  mdb_env_close(env);
}


merkle::hash_t Ametsuchi::append(const flatbuffers::Vector<uint8_t> *blob) {
  auto tx = flatbuffers::GetRoot<iroha::Transaction>(blob->data());

  MDB_val c_key, c_val;
  int res;

  // 1. Append to TX_store
  tx_store.append(blob);
  // 2. Update WSV
  wsv.update(blob);

  merkle::hash_t h;
  assert(tx->hash()->size() == merkle::HASH_LEN);
  std::copy(tx->hash()->begin(), tx->hash()->end(), &h[0]);
  tree.push(h);
  return tree.root();
}

merkle::hash_t Ametsuchi::append(
    const std::vector<flatbuffers::Vector<uint8_t> *> &batch) {
  for (auto t : batch) {
    append(t);

    auto tx = flatbuffers::GetRoot<iroha::Transaction>(t);

    merkle::hash_t h;
    assert(tx->hash()->size() == merkle::HASH_LEN);
    std::copy(tx->hash()->begin(), tx->hash()->end(), &h[0]);

    tree.push(h);
  }

  return tree.root();
}


void Ametsuchi::commit() {
  // commit old transaction
  tx_store.close_cursors();
  wsv.close_cursors();
  mdb_txn_commit(append_tx_);
  mdb_env_stat(env, &mst);

  // create new append transaction
  init_append_tx();
}


void Ametsuchi::rollback() {
  abort_append_tx();
  init_append_tx();
}


void Ametsuchi::abort_append_tx() {
  tx_store.close_cursors();
  wsv.close_cursors();
  if (append_tx_) mdb_txn_abort(append_tx_);
}


void Ametsuchi::init() {
  int res;

  // create database directory
  if ((res = mkdir(path_.c_str(), 0700))) {
    if (res == EEXIST) {
      console->debug("folder with database exists");
    } else {
      AMETSUCHI_CRITICAL(res, EACCES);
      AMETSUCHI_CRITICAL(res, ELOOP);
      AMETSUCHI_CRITICAL(res, EMLINK);
      AMETSUCHI_CRITICAL(res, ENAMETOOLONG);
      AMETSUCHI_CRITICAL(res, ENOENT);
      AMETSUCHI_CRITICAL(res, ENOSPC);
      AMETSUCHI_CRITICAL(res, ENOTDIR);
      AMETSUCHI_CRITICAL(res, EROFS);
    }
  }

  // create environment
  if ((res = mdb_env_create(&env))) {
    AMETSUCHI_CRITICAL(res, MDB_VERSION_MISMATCH);
    AMETSUCHI_CRITICAL(res, MDB_INVALID);
    AMETSUCHI_CRITICAL(res, ENOENT);
    AMETSUCHI_CRITICAL(res, EACCES);
    AMETSUCHI_CRITICAL(res, EAGAIN);
  }

  // set maximum mmap size. Must be multiple of OS page size (4 KB).
  // max size of the database (!!!)
  if ((res = mdb_env_set_mapsize(env, AMETSUCHI_MAX_DB_SIZE))) {
    AMETSUCHI_CRITICAL(res, EINVAL);
  }

  AMETSUCHI_TREES_TOTAL = wsv.get_trees_total() + tx_store.get_trees_total();
  // set number of databases in single file
  if ((res = mdb_env_set_maxdbs(env, AMETSUCHI_TREES_TOTAL))) {
    AMETSUCHI_CRITICAL(res, EINVAL);
  }

  // create database environment
  if ((res = mdb_env_open(env, path_.c_str(), MDB_FIXEDMAP, 0700))) {
    AMETSUCHI_CRITICAL(res, MDB_VERSION_MISMATCH);
    AMETSUCHI_CRITICAL(res, MDB_INVALID);
    AMETSUCHI_CRITICAL(res, ENOENT);
    AMETSUCHI_CRITICAL(res, EACCES);
    AMETSUCHI_CRITICAL(res, EAGAIN);
  }

  // stats about db
  mdb_env_stat(env, &mst);

  // initialize
  init_append_tx();
}


void Ametsuchi::init_append_tx() {
  int res;

  // begin "append" transaction
  if ((res = mdb_txn_begin(env, NULL, 0, &append_tx_))) {
    AMETSUCHI_CRITICAL(res, MDB_PANIC);
    AMETSUCHI_CRITICAL(res, MDB_MAP_RESIZED);
    AMETSUCHI_CRITICAL(res, MDB_READERS_FULL);
    AMETSUCHI_CRITICAL(res, ENOMEM);
  }
  // Create database instances for each tree, open cursors for each tree, save
  // them in map for tx_store and wsv
  tx_store.init(append_tx_);
  wsv.init(append_tx_);
  // stats about db
  mdb_env_stat(env, &mst);
}


std::vector<AM_val> Ametsuchi::accountGetAllAssets(
    const flatbuffers::String *pubKey, bool uncommitted) {
  return wsv.accountGetAllAssets(pubKey, uncommitted, env);
}


AM_val Ametsuchi::accountGetAsset(const flatbuffers::String *pubKey,
                                  const flatbuffers::String *ln,
                                  const flatbuffers::String *dn,
                                  const flatbuffers::String *cn,
                                  bool uncommitted) {
  return wsv.accountGetAsset(pubKey, ln, dn, cn, uncommitted, env);
}


merkle::hash_t Ametsuchi::merkle_root() { return tree.root(); }

std::vector<AM_val> Ametsuchi::getAssetTxByCreator(
    const flatbuffers::String *pubKey, bool uncommitted) {
  return tx_store.getAssetTxByCreator(pubKey, uncommitted, env);
}
std::vector<AM_val> Ametsuchi::getAccountTxByCreator(
    const flatbuffers::String *pubKey, bool uncommitted) {
  return tx_store.getAccountTxByCreator(pubKey, uncommitted, env);
}
std::vector<AM_val> Ametsuchi::getPeerTxByCreator(
    const flatbuffers::String *pubKey, bool uncommitted) {
  return tx_store.getPeerTxByCreator(pubKey, uncommitted, env);
}
std::vector<AM_val> Ametsuchi::getAssetTxBySender(
    const flatbuffers::String *senderKey, bool uncommitted) {
  return tx_store.getAssetTxBySender(senderKey, uncommitted, env);
}
std::vector<AM_val> Ametsuchi::getAssetTxByReceiver(
    const flatbuffers::String *receiverKey, bool uncommitted) {
  return tx_store.getAssetTxByReceiver(receiverKey, uncommitted, env);
}

}  // namespace ametsuchi
