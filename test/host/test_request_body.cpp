#include <unity.h>
#include <vector>
#include <string>
#include "domain/request_body.h"

namespace
{
    // Mirrors webpage.cpp's collectBody: a calloc'd total+1 buffer filled chunk
    // by chunk, clamped through writableChunk. Returns what getInput() would
    // read back, plus how many bytes were refused.
    struct Chunk
    {
        size_t index;
        size_t len;
    };

    struct Collected
    {
        std::string body;
        size_t dropped;
    };

    Collected collect(size_t total, const std::vector<Chunk> &chunks, char fill = 'x')
    {
        std::vector<char> buffer(total + 1, '\0');
        Collected result{"", 0};
        for (const Chunk &chunk : chunks)
        {
            const size_t writable =
                http_body::writableChunk(chunk.index, chunk.len, total);
            result.dropped += chunk.len - writable;
            for (size_t i = 0; i < writable; i++)
            {
                buffer[chunk.index + i] = fill;
            }
        }
        // Exactly how getInput() turns _tempObject into a String.
        result.body = std::string(buffer.data());
        return result;
    }
}

void test_body_chunk_fitting_the_declared_length_is_kept_whole()
{
    const Collected collected = collect(10, {{0, 10}});
    TEST_ASSERT_EQUAL_UINT(0, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(10, collected.body.size());
}

void test_body_split_across_chunks_is_reassembled_completely()
{
    // The split a real ESP32 produced for a 2662 byte body: lwip coalesced the
    // client's 400 byte writes into segments of its own choosing, so the
    // handler must cope with arbitrary boundaries.
    const Collected collected =
        collect(2662, {{0, 800}, {800, 1436}, {2236, 164}, {2400, 262}});
    TEST_ASSERT_EQUAL_UINT(0, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(2662, collected.body.size());
}

void test_body_longer_than_content_length_is_clamped_to_the_buffer()
{
    // The overflow probe: declare 10 bytes, then push 4000.
    const Collected collected = collect(10, {{0, 4000}});
    TEST_ASSERT_EQUAL_UINT(3990, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(10, collected.body.size());
}

void test_body_chunk_starting_past_the_declared_end_is_dropped()
{
    const Collected collected = collect(10, {{0, 10}, {10, 1460}, {1470, 1460}});
    TEST_ASSERT_EQUAL_UINT(2920, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(10, collected.body.size());
}

void test_final_body_chunk_overshooting_is_trimmed()
{
    const Collected collected = collect(1000, {{0, 900}, {900, 500}});
    TEST_ASSERT_EQUAL_UINT(400, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(1000, collected.body.size());
}

void test_zero_length_body_writes_nothing()
{
    const Collected collected = collect(0, {{0, 0}, {0, 1460}});
    TEST_ASSERT_EQUAL_UINT(1460, collected.dropped);
    TEST_ASSERT_EQUAL_UINT(0, collected.body.size());
}

void test_writable_chunk_never_reaches_the_terminating_byte()
{
    // Whatever the peer claims, an accepted chunk always ends at or before
    // `total`, so the calloc'd trailing NUL of a total+1 buffer survives and
    // getInput()'s String construction cannot run away. Chunks starting at or
    // past the declared end are refused outright.
    for (size_t total = 0; total <= 64; total++)
    {
        for (size_t index = 0; index <= total + 8; index++)
        {
            const size_t writable = http_body::writableChunk(index, 4096, total);
            if (index >= total)
            {
                TEST_ASSERT_EQUAL_UINT(0, writable);
                continue;
            }
            TEST_ASSERT_TRUE(writable > 0);
            TEST_ASSERT_TRUE(index + writable <= total);
        }
    }
}
