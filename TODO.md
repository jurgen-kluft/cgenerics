# xgenerics

## btree

``https://code.google.com/archive/p/cpp-btree/``

## Implementations

## UnitTests


## trie-tree map

```c++
class xalloc;
class ttmap;

void (*ttmap_hash_fn)(const void* key, u64& high, u64& low);

struct ttmap_value
{
    tmap_value(void const* key, void*& value);

    void            remove();

    ttmap_value&    operator =  (const void* value);

    bool            operator == (ttmap_value const& iter) const;
    bool            operator != (ttmap_value const& iter) const;

    bool            empty() const { return m_key == nullptr; }
    const void*     key() const   { return m_key; }
    const void*     value() const { return m_value; }

private:
    ttmap*          m_tmap;
    void const*     m_key;
    void *&         m_value;
};

struct ttmap_idxer
{
    u64     m_hash[2];
    u32     m_rootsize;
    s32     m_depth;

    void    reset();
    s32     next();
};

class ttmap
{
public:
    ttmap(xalloc* allocator, u32 sizeofkey);
    ttmap(xalloc* allocator, u32 sizeofkey, tmap_hash_fn key_hasher);

    ttmap_value  operator [] (const void* key);
    ttmap_cvalue operator [] (const void* key) const;

protected:
    // highest bit is used to identify between a node and item
    struct node
    {
        u32 elements[4];
    };
    struct item
    {
        void const* m_key;
        void const* m_value;
    };

    xalloc*     m_allocator;
    u32         m_rootsize;
    u32*        m_roottable;

    friend struct ttmap_value;
    void insert(const void* key, const void* value);
    void remove(const void* key);
};

```