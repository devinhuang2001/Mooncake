#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "client_service.h"

namespace mooncake {
namespace {

TEST(AsyncMemoryBatchGetTest, EmptyOperationIsImmediatelyComplete) {
    // This unit test does not need RDMA or a Store Master. Data-plane
    // integration tests exercise actual transfers on RDMA hardware.
    auto op = std::make_shared<AsyncMemoryBatchGet>(
        nullptr, std::vector<AsyncMemoryBatchGet::Entry>{});
    EXPECT_TRUE(op->ready());
    EXPECT_TRUE(op->wait().empty());
    EXPECT_TRUE(op->ready());
}

TEST(AsyncMemoryBatchGetTest, TerminalPerKeyErrorsRemainAligned) {
    std::vector<AsyncMemoryBatchGet::Entry> entries;
    AsyncMemoryBatchGet::Entry first;
    first.key = "missing";
    first.result = static_cast<int64_t>(ErrorCode::OBJECT_NOT_FOUND);
    entries.emplace_back(std::move(first));

    AsyncMemoryBatchGet::Entry second;
    second.key = "failed";
    second.result = static_cast<int64_t>(ErrorCode::TRANSFER_FAIL);
    entries.emplace_back(std::move(second));

    auto op = std::make_shared<AsyncMemoryBatchGet>(nullptr,
                                                    std::move(entries));
    const auto expected = std::vector<int64_t>{
        static_cast<int64_t>(ErrorCode::OBJECT_NOT_FOUND),
        static_cast<int64_t>(ErrorCode::TRANSFER_FAIL)};
    EXPECT_TRUE(op->ready());
    EXPECT_EQ(op->wait(), expected);
    EXPECT_EQ(op->wait(), expected);
}

}  // namespace
}  // namespace mooncake
