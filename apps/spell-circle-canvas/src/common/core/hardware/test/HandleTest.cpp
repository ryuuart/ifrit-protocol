// Generation-checked handles: what names a resource, what stops naming
// one, and what a freed slot does to the name that used to hold it. No
// device is created here — a handle table is arithmetic over indices, so
// the claims stand on any machine.

#include <sigilcore/hardware/Handle.h>

#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

using namespace sigil::core::hardware;

namespace {

// A handle names one kind of resource and cannot be passed as another —
// decided by the compiler, so it is asserted where the compiler reads it.
static_assert(!std::is_convertible_v<TextureHandle, BufferHandle>);
static_assert(!std::is_convertible_v<FenceHandle, TextureHandle>);

}  // namespace

TEST(HardwareHandle, TheNullHandleNamesNothing) {
  HandleTable<int, TextureHandle> table;
  EXPECT_FALSE(table.contains(TextureHandle{}));
  EXPECT_EQ(table.find(TextureHandle{}), nullptr);
  EXPECT_EQ(table.release(TextureHandle{}), 0);
}

TEST(HardwareHandle, AReusedSlotRejectsTheHandleThatUsedToNameIt) {
  HandleTable<int, TextureHandle> table;
  const TextureHandle first = table.allocate(1);
  ASSERT_TRUE(table.contains(first));
  EXPECT_EQ(table.release(first), 1);
  EXPECT_FALSE(table.contains(first));

  const TextureHandle second = table.allocate(2);
  EXPECT_EQ(second.index, first.index) << "the freed slot is reused";
  EXPECT_NE(second.generation, first.generation);
  EXPECT_NE(second, first);
  EXPECT_FALSE(table.contains(first)) << "the stale handle stays stale";
  EXPECT_TRUE(table.contains(second));
  EXPECT_EQ(*table.find(second), 2);
  EXPECT_EQ(table.find(first), nullptr);
  EXPECT_EQ(table.release(first), 0) << "a stale release releases nothing";
  EXPECT_TRUE(table.contains(second));
}

TEST(HardwareHandle, DrainingHandsBackEveryValueAndStalesEveryHandle) {
  HandleTable<int, BufferHandle> table;
  const BufferHandle a = table.allocate(1);
  const BufferHandle b = table.allocate(2);
  const std::vector<int> drained = table.drain();
  EXPECT_EQ(drained, (std::vector<int>{1, 2}));
  EXPECT_EQ(table.size(), 0u);
  EXPECT_FALSE(table.contains(a));
  EXPECT_FALSE(table.contains(b));
}
