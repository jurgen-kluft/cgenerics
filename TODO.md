# xgenerics

## btree

``https://code.google.com/archive/p/cpp-btree/``

## Implementations

## UnitTests


## trie-tree map

```c++
void (*ttmap_hash_fn)(const void* key, u64& high, u64& low);
struct ttmap_value
{
    tmap_value(void const* key, void*& value);
    tmap_value(void const* key, tmap_hash_fn key_hasher, void*& value);

    void            remove();

    operator        bool ()       { return m_key != nullptr; }
    operator        bool () const { return m_key != nullptr; }

    ttmap_value&     operator = (const void* value);

private:
    ttmap*           m_tmap;
    void const*     m_key;
    void *&         m_value;
};

struct ttmap_idxer
{
    u64     m_hash[2];
    u32     m_rootsize;

};

class ttmap
{
public:
    ttmap(u32 sizeofkey);

    ttmap_value  operator [] (const void* key);
    ttmap_cvalue operator [] (const void* key) const;

protected:
    // highest bit is used to identify between a node and item
    struct ttnode
    {
        u32 elements[4];
    };
    struct ttitem
    {
        void const* m_key;
        void const* m_value;
    };

    u32         m_rootsize;
    ttnode*     m_roottable;

    friend struct ttmap_value;
    void insert(const void* key, const void* value);
    void remove(const void* key);
};

```