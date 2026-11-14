// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_slave_worker_test.cpp
//
// S25–26: SPSC queue, handshake, bounded wait, fallback и restart.
#include "hal/saturn/slave_worker.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

#include <stdint.h>

namespace {

void increment_job(void* argument)
{
    if (argument != nullptr) {
        ++(*static_cast<uint32_t*>(argument));
    }
}

class SlaveWorkerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        saturn::slave_worker_test_reset();
    }

    void TearDown() override
    {
        saturn::slave_worker_test_reset();
        fake_saturn::reset();
    }

    void init_ready()
    {
        fake_saturn::slave_available = true;
        fake_saturn::slave_execute_posted = true;
        ASSERT_TRUE(saturn::slave_worker_init());
        ASSERT_TRUE(saturn::slave_worker_available());
    }
};

}  // namespace

TEST_F(SlaveWorkerTest, UnavailableSlaveIsSafeFallback)
{
    fake_saturn::slave_available = false;

    EXPECT_FALSE(saturn::slave_worker_init());
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_UNAVAILABLE);
    EXPECT_EQ(saturn::slave_worker_last_diag(), saturn::SLAVE_WORKER_DIAG_UNAVAILABLE);

    uint32_t value = 0u;
    EXPECT_FALSE(saturn::slave_worker_dispatch(increment_job, &value));
    EXPECT_EQ(value, 0u);
    EXPECT_FALSE(saturn::slave_worker_wait(5u));
    saturn::slave_worker_shutdown();  // no-op
}

TEST_F(SlaveWorkerTest, HandshakeDispatchAndWait)
{
    init_ready();
    EXPECT_EQ(saturn::slave_worker_stats().start_attempts, 1u);

    uint32_t value = 0u;
    saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
    EXPECT_TRUE(saturn::slave_worker_dispatch(increment_job, &value, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_OK);
    EXPECT_EQ(value, 1u);  // fake Slave исполняет синхронно
    EXPECT_TRUE(saturn::slave_worker_wait(20u, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_OK);

    const saturn::SlaveWorkerStats& stats = saturn::slave_worker_stats();
    EXPECT_EQ(stats.submitted, 1u);
    EXPECT_EQ(stats.completed, 1u);
    EXPECT_EQ(stats.slave_jobs, 1u);
    EXPECT_EQ(saturn::slave_worker_queue_depth(), 0u);
}

TEST_F(SlaveWorkerTest, QueueOverflowIsExplicitFalse)
{
    init_ready();
    fake_saturn::slave_execute_posted = false;  // job'ы копятся в очереди

    for (uint32_t i = 0u; i < saturn::SLAVE_JOB_QUEUE_CAPACITY; ++i) {
        saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
        EXPECT_TRUE(saturn::slave_worker_dispatch(increment_job, nullptr, &diag));
        EXPECT_EQ(diag, saturn::SLAVE_WORKER_OK);
    }

    saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
    EXPECT_FALSE(saturn::slave_worker_dispatch(increment_job, nullptr, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_QUEUE_FULL);
    EXPECT_EQ(saturn::slave_worker_stats().queue_overflows, 1u);
    EXPECT_EQ(saturn::slave_worker_queue_depth(), saturn::SLAVE_JOB_QUEUE_CAPACITY);

    // Разгружаем очередь и проверяем, что worker остался READY.
    fake_saturn::run_posted_callbacks();
    EXPECT_TRUE(saturn::slave_worker_wait(20u));
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_READY);
}

TEST_F(SlaveWorkerTest, WaitTimeoutMarksFailedAndStopsRetries)
{
    init_ready();
    fake_saturn::slave_execute_posted = false;

    uint32_t value = 0u;
    ASSERT_TRUE(saturn::slave_worker_dispatch(increment_job, &value));

    saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
    EXPECT_FALSE(saturn::slave_worker_wait(5u, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_WAIT_TIMEOUT);
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_FAILED);
    EXPECT_GE(saturn::slave_worker_stats().timeouts, 1u);

    EXPECT_FALSE(saturn::slave_worker_dispatch(increment_job, &value, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_ALREADY_FAILED);
    EXPECT_FALSE(saturn::slave_worker_wait(5u, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_ALREADY_FAILED);
}

TEST_F(SlaveWorkerTest, PostFailureMarksFailed)
{
    init_ready();
    fake_saturn::slave_post_result = false;

    saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
    EXPECT_FALSE(saturn::slave_worker_dispatch(increment_job, nullptr, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_POST_FAILED);
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_FAILED);
    EXPECT_EQ(saturn::slave_worker_stats().post_failures, 1u);
}

TEST_F(SlaveWorkerTest, StopAndRestartLifecycle)
{
    init_ready();

    uint32_t value = 0u;
    ASSERT_TRUE(saturn::slave_worker_dispatch(increment_job, &value));
    ASSERT_TRUE(saturn::slave_worker_wait(20u));

    saturn::slave_worker_shutdown();
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_STOPPED);

    EXPECT_TRUE(saturn::slave_worker_restart());
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_READY);
    EXPECT_EQ(saturn::slave_worker_stats().restarts, 1u);

    ASSERT_TRUE(saturn::slave_worker_dispatch(increment_job, &value));
    EXPECT_TRUE(saturn::slave_worker_wait(20u));
    EXPECT_EQ(value, 2u);

    saturn::slave_worker_shutdown();
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_STOPPED);
}

TEST_F(SlaveWorkerTest, NullJobIsRejected)
{
    init_ready();
    saturn::SlaveWorkerDiag diag = saturn::SLAVE_WORKER_COUNT;
    EXPECT_FALSE(saturn::slave_worker_dispatch(nullptr, nullptr, &diag));
    EXPECT_EQ(diag, saturn::SLAVE_WORKER_NULL_JOB);
}

TEST_F(SlaveWorkerTest, HangFaultLeadsToBoundedTimeout)
{
    init_ready();
    fake_saturn::slave_execute_posted = false;

    saturn::slave_worker_set_fault(saturn::SLAVE_FAULT_HANG_NEXT);
    EXPECT_EQ(saturn::slave_worker_fault(), saturn::SLAVE_FAULT_HANG_NEXT);

    ASSERT_TRUE(saturn::slave_worker_dispatch(increment_job, nullptr));
    EXPECT_EQ(saturn::slave_worker_fault(), saturn::SLAVE_FAULT_NONE);
    // Callback намеренно не исполняем: fault_flags уже выставлен.
    EXPECT_FALSE(saturn::slave_worker_wait(5u));
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_FAILED);
}

TEST_F(SlaveWorkerTest, FloodProbeDetectsQueueOverflow)
{
    init_ready();
    fake_saturn::slave_execute_posted = false;

    EXPECT_EQ(saturn::slave_worker_debug_flood_probe(), 1u);
    EXPECT_EQ(saturn::slave_worker_stats().queue_overflows, 1u);
}

TEST_F(SlaveWorkerTest, StateAndDiagnosticsStrings)
{
    EXPECT_STREQ(saturn::slave_worker_state_string(saturn::SLAVE_WORKER_READY), "ready");
    EXPECT_STREQ(saturn::slave_worker_state_string(saturn::SLAVE_WORKER_FAILED), "failed");
    EXPECT_STREQ(saturn::slave_worker_diag_string(saturn::SLAVE_WORKER_QUEUE_FULL), "queue full");
    EXPECT_STREQ(saturn::slave_worker_diag_string(saturn::SLAVE_WORKER_WAIT_TIMEOUT),
                 "wait timeout");
    EXPECT_GT(saturn::slave_worker_shared_bytes(), 64u);
}
