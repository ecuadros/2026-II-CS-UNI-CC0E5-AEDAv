#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include <utility>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralTraits.h"
#include "GeneralContainer.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type = T;
    using Node       = BinaryTreeNode<T>;
    using NodePtr    = Node *;
    using Parent     = GeneralNode<T>;          // (el esqueleto decia GeneralNode<MySelf>)
    using Parent::Parent;                       // hereda GeneralNode(value, ref)

    NodePtr m_pChildLeft  = nullptr;
    NodePtr m_pChildRight = nullptr;
    NodePtr m_pParent     = nullptr;            
};

template <typename T>
class BinaryTreeForwardIterator : public GeneralIterator<BinaryTreeForwardIterator<T>, BinaryTreeNode<T>> {
public:
    using Node   = BinaryTreeNode<T>;
    using Parent = GeneralIterator<BinaryTreeForwardIterator<T>, Node>;
    using Parent::Parent;

    BinaryTreeForwardIterator &operator++() {
        Node *cur = Parent::m_ptr;
        if (!cur) return *this;
        // Caso trivial
        if (cur->m_pChildRight) {
            Node *nxt = cur->m_pChildRight;
            while (nxt->m_pChildLeft) {
                nxt = nxt->m_pChildLeft;
            }
            Parent::m_ptr = nxt;
            return *this;
        }
        // no existe mi hijo derecho, toca subir
        Node *ch  = cur;
        Node *par = cur->m_pParent;
        while (par && par->m_pChildRight == ch) {   
            ch  = par;
            par = par->m_pParent;
        }
        Parent::m_ptr = par;                               
        return *this;
    }
};

template <typename T>
class BinaryTreeBackwardIterator : public GeneralIterator<BinaryTreeBackwardIterator<T>, BinaryTreeNode<T>> {
public:
    using Node   = BinaryTreeNode<T>;
    using Parent = GeneralIterator<BinaryTreeBackwardIterator<T>, Node>;
    using Parent::Parent;

    BinaryTreeBackwardIterator &operator++() {
        Node *cur = Parent::m_ptr;
        if (!cur) return *this;                     
        // Caso trivial
        if (cur->m_pChildLeft) {
            Node *prv = cur->m_pChildLeft;
            while (prv->m_pChildRight) {
                prv = prv->m_pChildRight;
            }
            Parent::m_ptr = prv;
            return *this;
        }
        // no tengo hijo izquierdo, toca subir
        Node *ch  = cur;
        Node *par = cur->m_pParent;
        while (par && par->m_pChildLeft == ch) {    
            ch  = par;
            par = par->m_pParent;
        }
        Parent::m_ptr = par;                               
        return *this;
    }
};

template <typename T, typename _Compare>
struct BinaryTreeTraits : public DefaultTraits<T, _Compare> {
    using Node             = BinaryTreeNode<T>;
    using ForwardIterator  = BinaryTreeForwardIterator<T>;
    using BackwardIterator = BinaryTreeBackwardIterator<T>;
};

template <typename T>
struct BinaryTreeAscTraits  : public BinaryTreeTraits<T, std::less<T>> {};

template <typename T>
struct BinaryTreeDescTraits : public BinaryTreeTraits<T, std::greater<T>> {};

template <typename Traits>
class BinaryTree : public ReversibleContainer<Traits> {
public:
    using value_type       = typename Traits::value_type;
    using Node             = typename Traits::Node;
    using NodePtr          = Node *;
    using Compare          = typename Traits::Compare;
    using ForwardIterator  = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
private:
    using GeneralContainer<Traits>::m_mutex;
    NodePtr m_root = nullptr;
    
    void internal_insert(NodePtr &node, NodePtr parent, const value_type &value, Ref ref) {
        if (!node) {
            node = new Node(value, ref);
            node->m_pParent = parent;               
            return;
        }
        if (Compare()(node->getValue(), value)) {
            internal_insert(node->m_pChildRight, node, value, ref);
        }
        else {
            internal_insert(node->m_pChildLeft, node, value, ref);
        }
    }

    void internal_clear(NodePtr node) {
        if (!node) return;                      
        internal_clear(node->m_pChildLeft);
        internal_clear(node->m_pChildRight);
        delete node;
    }
    
    NodePtr internal_copy(NodePtr node, NodePtr par) {
        if (!node) return nullptr;
        NodePtr cur = new Node(node->getValue(), node->getRef());
        cur->m_pParent = par;
        cur->m_pChildLeft = internal_copy(node->m_pChildLeft, cur);
        cur->m_pChildRight = internal_copy(node->m_pChildRight, cur);
        return cur;
    }

    NodePtr Leftmost(NodePtr node) const {
        while (node && node->m_pChildLeft)
            node = node->m_pChildLeft;
        return node;
    }

    NodePtr Rightmost(NodePtr node) const {
        while (node && node->m_pChildRight)
            node = node->m_pChildRight;
        return node;
    }

public:
    BinaryTree() = default;
    BinaryTree(const BinaryTree &other) {
        lock_guard<mutex> lock(other.m_mutex);
        m_root = internal_copy(other.m_root, nullptr);
    }
    
    BinaryTree(BinaryTree &&other) noexcept {
        lock_guard<mutex> lock(other.m_mutex);
        m_root = std::exchange(other.m_root, nullptr);
    }

    BinaryTree &operator=(const BinaryTree &other) {
        if (this == &other) {
            return *this;
        }
        scoped_lock lock(m_mutex, other.m_mutex);
        internal_clear(m_root);
        m_root = internal_copy(other.m_root, nullptr);
        return *this;
    }

    BinaryTree &operator=(BinaryTree &&other) noexcept {
        if (this == &other) {                           
            return *this;
        }
        scoped_lock lock(m_mutex, other.m_mutex);   
        internal_clear(m_root);                     
        m_root = std::exchange(other.m_root, nullptr);
        return *this;
    }

    virtual ~BinaryTree() { clear(); }

    bool empty() const {
        lock_guard<mutex> lock(m_mutex);
        return m_root == nullptr;
    }

    void push_back(const value_type &value, Ref ref) override {
        lock_guard<mutex> lock(m_mutex);
        internal_insert(m_root, nullptr, value, ref);
    } 
    void clear() override {
        lock_guard<mutex> lock(m_mutex);        
        internal_clear(m_root);
        m_root = nullptr;                       
    }

    ForwardIterator  begin()  const override { return ForwardIterator(Leftmost(m_root)); }
    ForwardIterator  end()    const override { return ForwardIterator(nullptr); }
    BackwardIterator rbegin() const override { return BackwardIterator(Rightmost(m_root)); }
    BackwardIterator rend()   const override { return BackwardIterator(nullptr); }
};

template <typename T>
class AscBinaryTree : public BinaryTree<BinaryTreeAscTraits<T>> {};    

template <typename T>
class DescBinaryTree : public BinaryTree<BinaryTreeDescTraits<T>> {};  

#endif // __BINARYTREE_H__
