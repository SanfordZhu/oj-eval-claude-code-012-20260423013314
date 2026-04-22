/**
 * implement a container like std::linked_hashmap
 */
#ifndef SJTU_LINKEDHASHMAP_HPP
#define SJTU_LINKEDHASHMAP_HPP

// only for std::equal_to<T> and std::hash<T>
#include <functional>
#include <cstddef>
#include "utility.hpp"
#include "exceptions.hpp"

namespace sjtu {
    /**
     * In linked_hashmap, iteration ordering is differ from map,
     * which is the order in which keys were inserted into the map.
     * You should maintain a doubly-linked list running through all
     * of its entries to keep the correct iteration order.
     *
     * Note that insertion order is not affected if a key is re-inserted
     * into the map.
     */

template<
    class Key,
    class T,
    class Hash = std::hash<Key>,
    class Equal = std::equal_to<Key>
> class linked_hashmap {
public:
    typedef pair<const Key, T> value_type;
private:
    struct Node {
        value_type kv;
        Node *prev, *next;      // list order
        Node *bucket_next;      // bucket chain
        Node(const Key &k, const T &v) : kv(k, v), prev(nullptr), next(nullptr), bucket_next(nullptr) {}
        Node(const Key &k) : kv(k, T()), prev(nullptr), next(nullptr), bucket_next(nullptr) {}
    };

    Node **buckets = nullptr;
    size_t bucket_count = 0;
    size_t sz = 0;
    Node *first = nullptr;
    Node *last = nullptr;

    Hash hasher;
    Equal equal;

    static constexpr double LOAD_FACTOR = 0.75;

    size_t index_for(const Key &key, size_t bc) const {
        size_t h = static_cast<size_t>(hasher(key));
        return bc ? (h % bc) : 0;
    }

    void init_buckets(size_t initial) {
        bucket_count = initial ? initial : 16;
        buckets = new Node*[bucket_count];
        for (size_t i = 0; i < bucket_count; ++i) buckets[i] = nullptr;
        first = last = nullptr;
        sz = 0;
    }

    void destroy_all() {
        // delete all nodes
        Node *cur = first;
        while (cur) {
            Node *nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        first = last = nullptr;
        if (buckets) { delete [] buckets; buckets = nullptr; }
        bucket_count = 0;
        sz = 0;
    }

    void rehash(size_t new_count) {
        if (new_count < 16) new_count = 16;
        Node **new_buckets = new Node*[new_count];
        for (size_t i = 0; i < new_count; ++i) new_buckets[i] = nullptr;
        for (Node *cur = first; cur; cur = cur->next) {
            size_t idx = index_for(cur->kv.first, new_count);
            cur->bucket_next = new_buckets[idx];
            new_buckets[idx] = cur;
        }
        delete [] buckets;
        buckets = new_buckets;
        bucket_count = new_count;
    }

    Node *find_node(const Key &key) const {
        if (bucket_count == 0) return nullptr;
        size_t idx = index_for(key, bucket_count);
        Node *cur = buckets[idx];
        while (cur) {
            if (equal(cur->kv.first, key)) return cur;
            cur = cur->bucket_next;
        }
        return nullptr;
    }

    void unlink_from_bucket(Node *node) {
        size_t idx = index_for(node->kv.first, bucket_count);
        Node *cur = buckets[idx];
        Node *prevb = nullptr;
        while (cur) {
            if (cur == node) {
                if (prevb) prevb->bucket_next = cur->bucket_next;
                else buckets[idx] = cur->bucket_next;
                break;
            }
            prevb = cur;
            cur = cur->bucket_next;
        }
    }

    void append_to_list(Node *node) {
        node->next = nullptr;
        node->prev = last;
        if (last) last->next = node; else first = node;
        last = node;
    }

    void push_into_bucket(Node *node) {
        size_t idx = index_for(node->kv.first, bucket_count);
        node->bucket_next = buckets[idx];
        buckets[idx] = node;
    }

public:

    class const_iterator;
    class iterator {
    private:
        linked_hashmap *container = nullptr;
        Node *cur = nullptr; // nullptr means end()
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename linked_hashmap::value_type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::output_iterator_tag;

        iterator() {}
        iterator(linked_hashmap *c, Node *n) : container(c), cur(n) {}
        iterator(const iterator &other) = default;

        iterator operator++(int) {
            if (!container || !cur) throw invalid_iterator();
            iterator tmp(*this);
            cur = cur->next;
            return tmp;
        }
        iterator & operator++() {
            if (!container || !cur) throw invalid_iterator();
            cur = cur->next;
            return *this;
        }
        iterator operator--(int) {
            if (!container) throw invalid_iterator();
            iterator tmp(*this);
            if (!cur) {
                if (!container->last) throw invalid_iterator();
                cur = container->last;
            } else {
                if (cur == container->first) throw invalid_iterator();
                cur = cur->prev;
            }
            return tmp;
        }
        iterator & operator--() {
            if (!container) throw invalid_iterator();
            if (!cur) {
                if (!container->last) throw invalid_iterator();
                cur = container->last;
            } else {
                if (cur == container->first) throw invalid_iterator();
                cur = cur->prev;
            }
            return *this;
        }

        value_type & operator*() const {
            if (!container || !cur) throw invalid_iterator();
            return cur->kv;
        }
        bool operator==(const iterator &rhs) const {
            return container == rhs.container && cur == rhs.cur;
        }
        bool operator==(const const_iterator &rhs) const { return container == rhs.container && cur == rhs.cur; }
        bool operator!=(const iterator &rhs) const { return !(*this == rhs); }
        bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

        value_type* operator->() const noexcept { return &const_cast<iterator*>(this)->operator*(); }

        friend class linked_hashmap;
        friend class const_iterator;
    };

    class const_iterator {
    private:
        const linked_hashmap *container = nullptr;
        const Node *cur = nullptr;
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = typename linked_hashmap::value_type;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::output_iterator_tag;

        const_iterator() {}
        const_iterator(const linked_hashmap *c, const Node *n) : container(c), cur(n) {}
        const_iterator(const const_iterator &other) = default;
        const_iterator(const iterator &other) : container(other.container), cur(other.cur) {}

        const_iterator operator++(int) {
            if (!container || !cur) throw invalid_iterator();
            const_iterator tmp(*this);
            cur = cur->next;
            return tmp;
        }
        const_iterator & operator++() {
            if (!container || !cur) throw invalid_iterator();
            cur = cur->next;
            return *this;
        }
        const_iterator operator--(int) {
            if (!container) throw invalid_iterator();
            const_iterator tmp(*this);
            if (!cur) {
                if (!container->last) throw invalid_iterator();
                cur = container->last;
            } else {
                if (cur == container->first) throw invalid_iterator();
                cur = cur->prev;
            }
            return tmp;
        }
        const_iterator & operator--() {
            if (!container) throw invalid_iterator();
            if (!cur) {
                if (!container->last) throw invalid_iterator();
                cur = container->last;
            } else {
                if (cur == container->first) throw invalid_iterator();
                cur = cur->prev;
            }
            return *this;
        }

        reference operator*() const {
            if (!container || !cur) throw invalid_iterator();
            return cur->kv;
        }
        bool operator==(const const_iterator &rhs) const { return container == rhs.container && cur == rhs.cur; }
        bool operator==(const iterator &rhs) const { return container == rhs.container && cur == rhs.cur; }
        bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
        bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

        pointer operator->() const noexcept { return &operator*(); }

        friend class linked_hashmap;
    };

    // iterator cross comparisons

    linked_hashmap() { init_buckets(16); }
    linked_hashmap(const linked_hashmap &other) {
        init_buckets(other.bucket_count);
        for (Node *cur = other.first; cur; cur = cur->next) {
            Node *node = new Node(cur->kv.first, cur->kv.second);
            append_to_list(node);
            push_into_bucket(node);
            ++sz;
        }
    }

    linked_hashmap & operator=(const linked_hashmap &other) {
        if (this == &other) return *this;
        destroy_all();
        init_buckets(other.bucket_count);
        for (Node *cur = other.first; cur; cur = cur->next) {
            Node *node = new Node(cur->kv.first, cur->kv.second);
            append_to_list(node);
            push_into_bucket(node);
            ++sz;
        }
        return *this;
    }

    ~linked_hashmap() { destroy_all(); }

    T & at(const Key &key) {
        Node *n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->kv.second;
    }
    const T & at(const Key &key) const {
        Node *n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->kv.second;
    }

    T & operator[](const Key &key) {
        Node *n = find_node(key);
        if (n) return n->kv.second;
        if ((sz + 1) > static_cast<size_t>(bucket_count * LOAD_FACTOR)) {
            rehash(bucket_count * 2);
        }
        Node *node = new Node(key);
        append_to_list(node);
        push_into_bucket(node);
        ++sz;
        return node->kv.second;
    }

    const T & operator[](const Key &key) const {
        Node *n = find_node(key);
        if (!n) throw index_out_of_bound();
        return n->kv.second;
    }

    iterator begin() { return iterator(this, first); }
    const_iterator cbegin() const { return const_iterator(this, first); }

    iterator end() { return iterator(this, nullptr); }
    const_iterator cend() const { return const_iterator(this, nullptr); }

    bool empty() const { return sz == 0; }
    size_t size() const { return sz; }

    void clear() {
        Node *cur = first;
        while (cur) {
            Node *nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        first = last = nullptr;
        for (size_t i = 0; i < bucket_count; ++i) buckets[i] = nullptr;
        sz = 0;
    }

    pair<iterator, bool> insert(const value_type &value) {
        Node *exist = find_node(value.first);
        if (exist) return pair<iterator, bool>(iterator(this, exist), false);
        if ((sz + 1) > static_cast<size_t>(bucket_count * LOAD_FACTOR)) {
            rehash(bucket_count * 2);
        }
        Node *node = new Node(value.first, value.second);
        append_to_list(node);
        push_into_bucket(node);
        ++sz;
        return pair<iterator, bool>(iterator(this, node), true);
    }

    void erase(iterator pos) {
        if (pos.container != this || pos.cur == nullptr) throw invalid_iterator();
        Node *node = pos.cur;
        // unlink from list
        if (node->prev) node->prev->next = node->next; else first = node->next;
        if (node->next) node->next->prev = node->prev; else last = node->prev;
        // unlink from bucket chain
        unlink_from_bucket(node);
        delete node;
        --sz;
    }

    size_t count(const Key &key) const { return find_node(key) ? 1u : 0u; }

    iterator find(const Key &key) {
        Node *n = find_node(key);
        return n ? iterator(this, n) : end();
    }
    const_iterator find(const Key &key) const {
        Node *n = find_node(key);
        return n ? const_iterator(this, n) : cend();
    }
};

}

#endif
