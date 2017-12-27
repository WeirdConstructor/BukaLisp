#include <mutex>
#include <vector>
#include <sstream>

template<typename Type>
class MemoryPool
{
    private:
        std::mutex m_global_lock;


        struct Descriptor
        {
            size_t      size;
            Descriptor *next;
        };

        struct SegmentGroup
        {
            size_t              m_segment_size;
            size_t              m_block_size;
            std::vector<char *> m_segments;
            Descriptor         *m_free;

            SegmentGroup(size_t segment_size, size_t block_size)
                : m_segment_size(segment_size), m_block_size(block_size), m_free(nullptr)
            {
            }

            size_t count_free()
            {
                size_t block_byte_size =
                    (sizeof(Descriptor) + m_block_size * sizeof(Type));
                Descriptor *d = m_free;
                size_t c = 0;
                while (d)
                {
                    c++;
                    d = d->next;
                }
                return c * block_byte_size;
            }

            void free(Type *mem)
            {
                char *buf = (char *) mem;
                buf -= sizeof(Descriptor);
                Descriptor *d = (Descriptor *) buf;
                d->next = m_free;
                m_free = d;
//                std::cout << "FREE " << m_block_size << std::endl;
            }

            size_t allocated_bytes()
            {
                size_t block_byte_size =
                    (sizeof(Descriptor) + m_block_size * sizeof(Type));
                return m_segments.size() * block_byte_size * m_segment_size;
            }

            void write_statistics(std::ostream &ss)
            {
                size_t free_bytes  = count_free();
                size_t alloc_bytes = allocated_bytes();

                if (alloc_bytes > 0)
                    ss << "[" << m_segment_size << "/" << m_block_size << "] "
                       << ((100 * free_bytes) / alloc_bytes) << "% "
                       << "(" << free_bytes << "," << alloc_bytes << ")";
            }

            Type *allocate()
            {
                if (!m_free)
                {
//                    std::cout << "ALLOC SEGMENT " << (m_segment_size * m_block_size) << std::endl;
                    size_t block_byte_size =
                        (sizeof(Descriptor) + m_block_size * sizeof(Type));

                    char *seg = new char[block_byte_size * m_segment_size];
                    m_segments.push_back(seg);

                    for (size_t i = 0; i < m_segment_size; i++)
                    {
                        Descriptor *d = (Descriptor *) seg;
                        d->size = m_block_size;
                        d->next = m_free;
                        m_free = d;

                        seg += block_byte_size;
                    }
                }

                Descriptor *d = m_free;
                m_free = m_free->next;
                d->next = nullptr;


                return new (((char *) d) + sizeof(Descriptor)) Type[m_block_size];
            }

            ~SegmentGroup()
            {
                for (auto seg : m_segments)
                    delete[] seg;
            }
        };

        SegmentGroup    m_tiny;
        SegmentGroup    m_small;
        SegmentGroup    m_medium;
        SegmentGroup    m_large;
        SegmentGroup    m_big;
        SegmentGroup    m_huge;

    public:
        MemoryPool()
//            : m_tiny(10,   2),
//              m_small(10, 10),
//              m_medium(10, 25),
//              m_large(10,  50),
//              m_big(10,   100),
//              m_huge(10, 1000)
            : m_tiny(100,   2),
              m_small(100, 10),
              m_medium(50, 100),
              m_large(50, 500),
              m_big(10,  1500),
              m_huge(10, 4000)
//            : m_tiny(1000,   2),
//              m_small(1000, 10),
//              m_medium(300, 100),
//              m_large(300, 500),
//              m_big(300,  1500),
//              m_huge(100, 4000)
        {
        }

        std::string statistics()
        {
            std::stringstream ss;
            m_tiny.write_statistics(  ss); ss << endl;
            m_small.write_statistics( ss); ss << endl;
            m_medium.write_statistics(ss); ss << endl;
            m_large.write_statistics( ss); ss << endl;
            m_big.write_statistics(   ss); ss << endl;
            m_huge.write_statistics(  ss); ss << endl;
            return ss.str();
        }

        Type *allocate(size_t block_len)
        {
            // XXX TESTING:
#if TEST_DISABLE_MEM_POOL_INTERNAL
            return new Type[block_len];
#endif


//            std::lock_guard<std::mutex> lock(m_global_lock);
            size_t type_block_byte_len = sizeof(Type) * block_len;
            size_t block_byte_len      = sizeof(Descriptor) + type_block_byte_len;
            char *buf = nullptr;
            size_t segment_idx = 0;

            if (block_len <= m_tiny.m_block_size)
            {
                return m_tiny.allocate();
            }
            else if (block_len <= m_small.m_block_size)
            {
                return m_small.allocate();
            }
            else if (block_len <= m_medium.m_block_size)
            {
                return m_medium.allocate();
            }
            else if (block_len <= m_large.m_block_size)
            {
                return m_large.allocate();
            }
            else if (block_len <= m_big.m_block_size)
            {
                return m_big.allocate();
            }
            else if (block_len <= m_huge.m_block_size)
            {
                return m_huge.allocate();
            }

            buf = new char[block_byte_len];

            Descriptor *d = (Descriptor *) buf;
            d->size       = block_len;
            d->next       = nullptr;
            return new (buf + sizeof(Descriptor)) Type[block_len];
        }

        void free(Type *mem)
        {
#if TEST_DISABLE_MEM_POOL_INTERNAL
            delete[] mem;
            return;
#endif
//            std::lock_guard<std::mutex> lock(m_global_lock);
            Descriptor *d =
                (Descriptor *) (((char *) mem) - sizeof(Descriptor));

            if (d->size == m_tiny.m_block_size)
            {
                m_tiny.free(mem);
            }
            else if (d->size == m_small.m_block_size)
            {
                m_small.free(mem);
            }
            else if (d->size == m_medium.m_block_size)
            {
                m_medium.free(mem);
            }
            else if (d->size == m_large.m_block_size)
            {
                m_large.free(mem);
            }
            else if (d->size == m_big.m_block_size)
            {
                m_big.free(mem);
            }
            else if (d->size == m_huge.m_block_size)
            {
                m_huge.free(mem);
            }
            else
                delete[] ((char *) d);
        }
};
