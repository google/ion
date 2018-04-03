/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "ion/base/readwritelock.h"

#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/threadspawner.h"
#include "ion/port/barrier.h"
#include "ion/port/timer.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// Helper struct for testing a ReadWriteLock under contention.
struct ReadWriteHelper {
  ReadWriteHelper()
      : read_lock(&lock),
        write_lock(&lock),
        read_barrier(3U),
        write_barrier(2U),
        reader_count(0),
        writer_count(0),
        test_writer_count(0) {}

  bool DoRead() {
    read_barrier.Wait();
    {
      ReadGuard reader(&read_lock);
      EXPECT_EQ(test_writer_count, writer_count);
      ++reader_count;
      read_barrier.Wait();
      read_barrier.Wait();
    }
    read_barrier.Wait();
    return true;
  }

  bool DoWrite() {
    write_barrier.Wait();
    const bool ret = DoWriteInner();
    write_barrier.Wait();
    return ret;
  }

  bool DoWriteInner() {
    {
      WriteGuard writer(&write_lock);
      ++writer_count;
      write_barrier.Wait();
      write_barrier.Wait();
    }
    return true;
  }

  ReadWriteLock lock;
  ReadLock read_lock;
  WriteLock write_lock;
  port::Barrier read_barrier;
  port::Barrier write_barrier;
  std::atomic<int> reader_count;
  std::atomic<int> writer_count;
  int test_writer_count;
};

}  // anonymous namespace

TEST(ReadWriteLock, BasicUsage) {
  // Test that basic functions do not block on a single caller.
  ReadWriteLock lock;
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_EQ(0, lock.GetWriterCount());

  lock.LockForRead();
  EXPECT_EQ(1, lock.GetReaderCount());
  lock.UnlockForRead();
  EXPECT_EQ(0, lock.GetReaderCount());

  lock.LockForWrite();
  EXPECT_EQ(1, lock.GetWriterCount());
  lock.UnlockForWrite();
  EXPECT_EQ(0, lock.GetWriterCount());
}

TEST(ReadWriteLock, ReadLock) {
  ReadWriteLock lock;
  ReadLock reader(&lock);
  EXPECT_FALSE(reader.IsLocked());
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_EQ(0, lock.GetWriterCount());

  reader.Lock();
  EXPECT_EQ(1, lock.GetReaderCount());
  EXPECT_TRUE(reader.IsLocked());
  reader.Unlock();
  EXPECT_EQ(0, lock.GetReaderCount());

  // Try the same with a LockGuard.
  {
    ReadGuard guard(&reader);
    EXPECT_EQ(1, lock.GetReaderCount());
    EXPECT_TRUE(reader.IsLocked());
    EXPECT_EQ(0, lock.GetWriterCount());
  }
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_EQ(0, lock.GetWriterCount());
  EXPECT_FALSE(reader.IsLocked());

  // Can have multiple readers at once, even in the same thread.
  {
    ReadGuard guard1(&reader);
    EXPECT_EQ(1, lock.GetReaderCount());
    EXPECT_TRUE(reader.IsLocked());
    EXPECT_EQ(0, lock.GetWriterCount());
    ReadGuard guard2(&reader);
    EXPECT_EQ(2, lock.GetReaderCount());
    EXPECT_TRUE(reader.IsLocked());
    EXPECT_EQ(0, lock.GetWriterCount());
  }
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_EQ(0, lock.GetWriterCount());
}

TEST(ReadWriteLock, WriteLock) {
  ReadWriteLock lock;
  WriteLock writer(&lock);
  EXPECT_FALSE(writer.IsLocked());
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_EQ(0, lock.GetWriterCount());

  writer.Lock();
  EXPECT_EQ(1, lock.GetWriterCount());
  writer.Unlock();
  EXPECT_EQ(0, lock.GetWriterCount());

  // Try the same with a LockGuard.
  {
    WriteGuard guard(&writer);
    EXPECT_EQ(1, lock.GetWriterCount());
    EXPECT_TRUE(writer.IsLocked());
    EXPECT_EQ(0, lock.GetReaderCount());
  }
  EXPECT_EQ(0, lock.GetWriterCount());
  EXPECT_EQ(0, lock.GetReaderCount());
  EXPECT_FALSE(writer.IsLocked());
}

TEST(ReadWriteLock, ReadersBlockWriters) {
  // Test that readers block a writer from entering, but that readers don't
  // block other readers.
  ReadWriteHelper helper;
  ThreadSpawner r1("Reader 1", std::bind(&ReadWriteHelper::DoRead, &helper));
  ThreadSpawner r2("Reader 2", std::bind(&ReadWriteHelper::DoRead, &helper));

  // Pause readers inside their locks.
  helper.read_barrier.Wait();
  helper.read_barrier.Wait();
  // At this point both readers should have obtained their locks.
  EXPECT_EQ(2, helper.lock.GetReaderCount());

  // Start the writer and let it try to obtain a lock.
  ThreadSpawner w1("Writer 1", std::bind(&ReadWriteHelper::DoWrite, &helper));
  helper.write_barrier.Wait();
  // Give the writer thread ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  // Since the readers are still holding the lock, the writer can't enter.
  EXPECT_EQ(0, helper.writer_count);

  // Let the readers exit, which should allow the writer to obtain the lock
  // and write its string.
  helper.read_barrier.Wait();
  helper.read_barrier.Wait();
  // Give the writer thread ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  EXPECT_EQ(1, helper.writer_count);

  // Let the writer exit.
  helper.write_barrier.Wait();
  helper.write_barrier.Wait();
  helper.write_barrier.Wait();
}

TEST(ReadWriteLock, WritersBlockReaders) {
  // Test that writers block readers from entering.
  ReadWriteHelper helper;
  helper.test_writer_count = 1;

  // Start the writer and let obtain the lock.
  ThreadSpawner w1("Writer 1", std::bind(&ReadWriteHelper::DoWrite, &helper));
  helper.write_barrier.Wait();
  helper.write_barrier.Wait();
  EXPECT_EQ(1, helper.writer_count);

  // Now spawn readers and let them try to lock
  ThreadSpawner r1("Reader 1", std::bind(&ReadWriteHelper::DoRead, &helper));
  ThreadSpawner r2("Reader 2", std::bind(&ReadWriteHelper::DoRead, &helper));
  helper.read_barrier.Wait();
  // Give the reader threads ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  EXPECT_EQ(0, helper.reader_count);

  // Let the writer continue.
  helper.write_barrier.Wait();
  helper.write_barrier.Wait();

  // Give the reader threads ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  // Both readers should have their locks.
  EXPECT_EQ(2, helper.reader_count);

  helper.read_barrier.Wait();
  helper.read_barrier.Wait();
  helper.read_barrier.Wait();
}

TEST(ReadWriteLock, WritersBlockWriters) {
  // Test that writers block each other from entering.
  ReadWriteHelper helper;

  // Start the writer and let obtain the lock.
  ThreadSpawner w1("Writer 1",
                   std::bind(&ReadWriteHelper::DoWriteInner, &helper));
  ThreadSpawner w2("Writer 2",
                   std::bind(&ReadWriteHelper::DoWriteInner, &helper));
  helper.write_barrier.Wait();
  // Give the writer threads ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  // Only one thread should have succeeded in obtaining the lock.
  EXPECT_EQ(1, helper.writer_count);
  // Let the first thread exit.
  helper.write_barrier.Wait();

  // Give the writer threads ample chances to proceed.
  port::Timer::SleepNSeconds(1);
  // The second thread should have obtained the lock.
  EXPECT_EQ(2, helper.writer_count);

  helper.write_barrier.Wait();
  helper.write_barrier.Wait();
}

}  // namespace base
}  // namespace ion
