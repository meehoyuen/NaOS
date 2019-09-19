#pragma once
#include "../mm/memory.hpp"
#include "common.hpp"

namespace util
{
template <typename E> class linked_list
{
  public:
    struct list_node
    {
        E element;
        list_node *prev, *next;
        list_node(E element)
            : element(element){};
    };
    struct list_info_node
    {
        char data[sizeof(E)];
        list_node *prev, *next;
        list_info_node(){};
    };

    static_assert(sizeof(list_node) == sizeof(list_info_node));

  private:
    memory::IAllocator *allocator;

    list_info_node *head, *tail;

  public:
    list_node *push_back(const E &e)
    {
        list_node *node = memory::New<list_node>(allocator, e);
        node->next = (list_node *)tail;
        tail->prev->next = node;
        node->prev = tail->prev;
        tail->prev = node;
        return node;
    }

    E pop_back()
    {
        E e = tail->prev->element;
        list_node *node = tail->prev;
        node->prev->next = (list_node *)tail;
        tail->prev = node->prev;
        memory::Delete<>(allocator, node);
        return e;
    };

    list_node *begin() const { return head->next; }

    list_node *end() const { return (list_node *)tail; }

    bool empty() const { return head->next == (list_node *)tail; }

    E back() const { return tail->prev->element; }

    E front() const { return head->next->element; }

    list_node *next(list_node *node) const { return node->next; }
    // return node if element finded else return end()
    list_node *find(const E &e)
    {
        list_node *node = begin();
        while (node != end())
        {
            if (node->element == e)
            {
                return node;
            }
            node = next(node);
        }
        return end();
    }
    // insert node before after
    list_node *insert(list_node *after, E e)
    {
        if (unlikely(after == end()))
            return end();

        list_node *node = memory::New<list_node>(allocator, e);
        auto last = after->prev;

        last->next = node;
        node->next = after;
        node->prev = last;
        after->prev = node;

        return node;
    }

    void remove(list_node *node)
    {
        if (unlikely(node == end() || node == begin()))
            return;

        node->prev->next = node->next;
        node->next->prev = node->prev;
        memory::Delete<>(allocator, node);
    }

    linked_list(memory::IAllocator *allocator)
        : allocator(allocator)
        , head(memory::New<list_info_node>(allocator))
        , tail(memory::New<list_info_node>(allocator))

    {
        head->next = (list_node *)tail;
        head->prev = nullptr;
        tail->prev = (list_node *)head;
        tail->next = nullptr;
    };

    ~linked_list()
    {
        memory::Delete<>(allocator, head);
        memory::Delete<>(allocator, tail);
    }
};
} // namespace util