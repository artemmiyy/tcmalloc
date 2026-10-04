// Copyright 2026 The TCMalloc Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Checks that each thread owns at most one ThreadCache, including the thread
// that allocated before tcmalloc's thread-specific data was initialized.

#include <stddef.h>
#include <stdlib.h>

#include <optional>
#include <thread>  // NOLINT(build/c++11)

#include "gtest/gtest.h"
#include "tcmalloc/malloc_extension.h"

namespace tcmalloc {
namespace {

size_t ThreadCacheCount() {
  std::optional<size_t> count =
      MallocExtension::GetNumericProperty("tcmalloc.thread_cache_count");
  return count.value_or(~size_t{0});
}

void AllocateSomething() {
  void* volatile p = malloc(1);
  free(p);
}

// The main thread allocated (from static initializers and TCMallocGuard)
// before ThreadCache::InitTSD() ran.  It should still own a single cache.
TEST(ThreadCacheCountTest, MainThreadOwnsOneCache) {
  AllocateSomething();
  EXPECT_EQ(ThreadCacheCount(), 1);
}

// MarkThreadIdle() releases the calling thread's cache.  If the main thread
// has no other cache, none should remain.
TEST(ThreadCacheCountTest, MarkThreadIdleReleasesAllMainThreadCaches) {
  AllocateSomething();
  MallocExtension::MarkThreadIdle();
  // ThreadCacheCount() must not allocate, or it would recreate a cache.
  EXPECT_EQ(ThreadCacheCount(), 0);
}

// Control: a thread started after initialization gets exactly one cache, and
// it goes away when the thread exits.
TEST(ThreadCacheCountTest, ChildThreadCacheIsCountedAndDestroyed) {
  AllocateSomething();
  const size_t before = ThreadCacheCount();

  size_t during = 0;
  std::thread t([&]() {
    AllocateSomething();
    during = ThreadCacheCount();
  });
  t.join();

  EXPECT_EQ(during, before + 1);
  EXPECT_EQ(ThreadCacheCount(), before);
}

}  // namespace
}  // namespace tcmalloc
