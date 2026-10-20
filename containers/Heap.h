#ifndef __HEAP_H__
#define __HEAP_H__
#include "vector.h"
#include <stdexcept>
#include "../traits.h"
#include "GeneralNode.h"
#include "../container.h"
#include <mutex>
#include "../foreach.h"
#include <utility>

//
template <typename T, typename Comparator>
struct HeapTraits : public DefaultTraits<T, Comparator> {
    using Node = GeneralNode<T>;
};

template <typename Traits>
class Heap {
    
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using Compare           = typename Traits::Compare;

    //

     std::ostream& write(std::ostream& os) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        os << "[";
        bool first = true;

        for (auto it = begin(); it != end(); ++it) {
            if (!first)
                os << ", ";

            os << *it;
            first = false;
        }

        return os << "]";
    }
    //
    std::istream& read(std::istream& is) {
         std::lock_guard<std::mutex> lock(m_mutex);
    m_data.read(is);

  RebuildHeap();

    return is;
}





    
private:
    //Vector<Node> m_data;
    Vector<VectorAscTraits<value_type>> m_data;
    mutable std::mutex m_mutex;
    decltype(auto) begin() { return m_data.begin(); }
    
    decltype(auto) end()   { return m_data.end();   }
    decltype(auto) begin() const {
    return m_data.begin();
}

decltype(auto) end() const {
    return m_data.end();
}

//
public:
    void push(const value_type &value, Ref ref) {
        //m_data.push_back(Node(value));
         std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    private:

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (Compare()(m_data[index].getValue(), m_data[parentIndex].getValue())) {
                std::swap(m_data[index], m_data[parentIndex]);
                index = parentIndex;
            } else {
                break;
            }
        }
    }
    //
    void HeapifyDown(size_t index) {
        while(true){

        
        size_t left = 2 * index + 1;
        size_t right = 2 * index + 2;

        if (left >= m_data.size())
        return;

    size_t selected = left;

    if (right < m_data.size() &&
        Compare()(m_data[right].getValue(),
              m_data[left].getValue())) {
                selected = right;
            }
    if (!Compare()(m_data[selected].getValue(),
               m_data[index].getValue())) {
                return;
          }


    std::swap(m_data[index], m_data[selected]);
        index = selected;
    }
}
   void RebuildHeap() {
        for (size_t contador = m_data.size() / 2;
             contador > 0;
             --contador) {
            HeapifyDown(contador - 1);
        }
    }

//ApplyFunction
public:
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::lock_guard<std::mutex> lock(m_mutex);

        ::ApplyFunction(
            begin(),
            end(),
            func,
            std::forward<Args>(args)...
        );

        RebuildHeap();
    }





public:
    Node pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_data.empty())
        
            throw std::out_of_range("No se puede extraer de un heap vacio");
        //
        Node extracted = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        HeapifyDown(0);
        return extracted;
    }

};
//
template <typename T>
class MinHeap : public Heap<HeapTraits<T, std::less<T>>> {
};
//
template <typename T>
class MaxHeap : public Heap<HeapTraits<T, std::greater<T>>> {
};



#endif // __HEAP_H__