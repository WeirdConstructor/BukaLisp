#include <mutex>
#include <vector>

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

            void free(Type *mem)
            {
                char *buf = (char *) mem;
                buf -= sizeof(Descriptor);
                Descriptor *d = (Descriptor *) buf;
                d->next = m_free;
                m_free = d;
//                std::cout << "FREE " << m_block_size << std::endl;
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

    public:
        MemoryPool()
            : m_tiny(1000,   2),
              m_small(1000, 10),
              m_medium(300, 100),
              m_large(300, 500)
        {
        }

        Type *allocate(size_t block_len)
        {
            std::lock_guard<std::mutex> lock(m_global_lock);
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

            buf = new char[block_byte_len];

            Descriptor *d = (Descriptor *) buf;
            d->size       = block_len;
            d->next       = nullptr;
            return new (buf + sizeof(Descriptor)) Type[block_len];
        }

        void free(Type *mem)
        {
            std::lock_guard<std::mutex> lock(m_global_lock);
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
            else
                delete[] ((char *) d);
        }
};
