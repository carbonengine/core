// Copyright © 2026 CCP ehf.

#include "gtest/gtest.h"
#include "CcpPairingHeap.h"

TEST(PairingHeap, RemoveNonRootNode)
{
    PairingHeap<int> heap;
    heap.insert(10);
    auto* node = heap.insert(5);
    heap.insert(20);

    int val = heap.remove(node);

    EXPECT_EQ(val, 5);
    EXPECT_EQ(heap.size(), 2u);
    EXPECT_EQ(heap.find_min(), 10);
}

TEST(PairingHeap, RemoveRootNode)
{
    PairingHeap<int> heap;
    auto* root = heap.insert(1);
    heap.insert(3);
    heap.insert(2);

    int val = heap.remove(root);

    EXPECT_EQ(val, 1);
    EXPECT_EQ(heap.size(), 2u);
    EXPECT_EQ(heap.find_min(), 2);
}

TEST(PairingHeap, RemoveAllNodesOneByOne)
{
    PairingHeap<int> heap;
    auto* a = heap.insert(3);
    auto* b = heap.insert(1);
    auto* c = heap.insert(2);

    EXPECT_EQ(heap.remove(b), 1);
    EXPECT_EQ(heap.remove(c), 2);
    EXPECT_EQ(heap.remove(a), 3);
    EXPECT_TRUE(heap.is_empty());
}
