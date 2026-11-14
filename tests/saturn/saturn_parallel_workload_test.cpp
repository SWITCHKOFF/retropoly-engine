// SPDX-License-Identifier: MIT
// RetroPolyEngine — tests/saturn/saturn_parallel_workload_test.cpp
//
// S25–26: детерминизм single/dual, fallback и бюджет shared-памяти.
#include "core/math/mat4.h"
#include "hal/saturn/saturn_memory.h"
#include "hal/saturn/saturn_parallel_workload.h"
#include "hal/saturn/slave_worker.h"
#include "tests/saturn/fake_saturn_bridge.h"

#include <gtest/gtest.h>

namespace {

Mat4 make_mvp()
{
    Mat4 view;
    Mat4 projection;
    Mat4 mvp;
    mat4_look_at(&view, vec3_make(0, fixed_from_int(3), fixed_from_int(7)), vec3_make(0, 0, 0),
                 vec3_make(0, FIXED_ONE, 0));
    const fixed_t fov = fixed_mul(FIXED_PI, fixed_div(fixed_from_int(70), fixed_from_int(180)));
    mat4_perspective(&projection, fov, fixed_div(fixed_from_int(320), fixed_from_int(224)),
                     fixed_from_int(1), fixed_from_int(100));
    mat4_multiply(&mvp, &projection, &view);
    return mvp;
}

class ParallelWorkloadTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        fake_saturn::reset();
        saturn::slave_worker_test_reset();
        saturn::parallel_workload_init(nullptr);
    }

    void TearDown() override
    {
        saturn::parallel_workload_shutdown();
        saturn::slave_worker_test_reset();
        fake_saturn::reset();
    }

    void init_worker()
    {
        fake_saturn::slave_available = true;
        fake_saturn::slave_execute_posted = true;
        ASSERT_TRUE(saturn::slave_worker_init());
    }
};

}  // namespace

TEST_F(ParallelWorkloadTest, FrameUsesSlaveAndIsDeterministic)
{
    init_worker();
    const Mat4 mvp = make_mvp();

    const saturn::ParallelFrameResult first = saturn::parallel_workload_run_frame(&mvp, 20u);
    ASSERT_TRUE(first.ok);
    EXPECT_TRUE(first.used_slave);
    EXPECT_FALSE(first.fallback);
    EXPECT_EQ(first.master_triangles, saturn::PARALLEL_TRIANGLES_PER_CPU);
    EXPECT_EQ(first.slave_triangles, saturn::PARALLEL_TRIANGLES_PER_CPU);
    EXPECT_NE(first.hash, 0u);
    EXPECT_EQ(saturn::slave_worker_stats().slave_jobs, 1u);

    const saturn::ParallelFrameResult second = saturn::parallel_workload_run_frame(&mvp, 20u);
    ASSERT_TRUE(second.ok);
    EXPECT_EQ(first.hash, second.hash);
}

TEST_F(ParallelWorkloadTest, FallbackWhenSlaveUnavailable)
{
    const Mat4 mvp = make_mvp();
    const uint32_t fallback_before = saturn::slave_worker_stats().fallback_frames;

    const saturn::ParallelFrameResult result = saturn::parallel_workload_run_frame(&mvp, 20u);
    ASSERT_TRUE(result.ok);
    EXPECT_FALSE(result.used_slave);
    EXPECT_TRUE(result.fallback);
    EXPECT_EQ(result.slave_triangles, 0u);
    EXPECT_NE(result.hash, 0u);
    EXPECT_EQ(saturn::slave_worker_stats().fallback_frames, fallback_before + 1u);
}

TEST_F(ParallelWorkloadTest, SingleAndDualHashesMatch)
{
    init_worker();
    const Mat4 mvp = make_mvp();

    const saturn::ParallelBenchResult bench = saturn::parallel_workload_benchmark(&mvp, 3u);
    EXPECT_TRUE(bench.ok);
    EXPECT_TRUE(bench.hashes_match);
    EXPECT_TRUE(bench.slave_used);
    EXPECT_EQ(bench.iterations, 3u);
    EXPECT_EQ(bench.single_hash, bench.dual_hash);
    EXPECT_GT(bench.single_us, 0u);
    EXPECT_GT(bench.dual_us, 0u);
}

TEST_F(ParallelWorkloadTest, TimeoutFallbackHashMatchesSingleCpu)
{
    const Mat4 mvp = make_mvp();

    // Baseline: Slave недоступен, обе половины считает Master.
    const saturn::ParallelFrameResult baseline = saturn::parallel_workload_run_frame(&mvp, 20u);
    ASSERT_TRUE(baseline.ok);
    ASSERT_TRUE(baseline.fallback);

    // Slave «завис»: job поставлен, но не исполняется; Master ловит timeout
    // и пересчитывает вторую половину в fallback-буфер.
    init_worker();
    fake_saturn::slave_execute_posted = false;
    const saturn::ParallelFrameResult fallback = saturn::parallel_workload_run_frame(&mvp, 5u);
    ASSERT_TRUE(fallback.ok);
    EXPECT_TRUE(fallback.fallback);
    EXPECT_FALSE(fallback.used_slave);
    EXPECT_EQ(fallback.hash, baseline.hash);
    EXPECT_EQ(saturn::slave_worker_state(), saturn::SLAVE_WORKER_FAILED);
}

TEST_F(ParallelWorkloadTest, InvalidMatrixIsRejected)
{
    const saturn::ParallelFrameResult result = saturn::parallel_workload_run_frame(nullptr, 20u);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.hash, 0u);
}

TEST_F(ParallelWorkloadTest, SharedBudgetIsRespected)
{
    EXPECT_LE(saturn::parallel_workload_shared_bytes(), saturn::PARALLEL_WORK_BYTES);
    EXPECT_EQ(saturn::parallel_workload_shared_bytes(),
              saturn::parallel_workload_shared_used_bytes());
}
