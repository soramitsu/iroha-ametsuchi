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

#include <gtest/gtest.h>
#include <wsv.h>
#include <manager.h>
#include <pqxx/pqxx>
#include <cpp_redis/cpp_redis>

void test_backend(const std::string &backend) {
  auto wsv_ = wsv::Manager::instance().make_WSV(backend);
  wsv_->add_account(0, "Ivan");
  ASSERT_EQ(wsv_->get_account_by_id(0), "Ivan");
  wsv_->add_domain(0, "RU", 0);
  wsv_->add_asset(0, "USD", 0);
  ASSERT_EQ(wsv_->get_balance_by_account_id_asset_id(0, 0), 0);
  wsv_->add_balance(0, 0, 100);
  ASSERT_EQ(wsv_->get_balance_by_account_id_asset_id(0, 0), 100);
  wsv_->add_balance(0, 0, 100);
  ASSERT_EQ(wsv_->get_balance_by_account_id_asset_id(0, 0), 200);
}

TEST(wsv_test, postgres_test) {
  test_backend("Postgres");
  const auto drop =
    "DROP TABLE IF EXISTS domain_has_account;\n"
      "DROP TABLE IF EXISTS account_has_asset;\n"
      "DROP TABLE IF EXISTS asset;\n"
      "DROP TABLE IF EXISTS domain;\n"
      "DROP TABLE IF EXISTS signatory;\n"
      "DROP TABLE IF EXISTS account;";

  pqxx::connection connection;
  pqxx::work txn(connection);
  txn.exec(drop);
  txn.commit();
  connection.disconnect();
}

TEST(wsv_test, redis_test) {
  test_backend("Redis");
  cpp_redis::redis_client client;
  client.connect();
  client.flushall();
  client.sync_commit();
  client.disconnect();
}