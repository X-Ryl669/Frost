#ifndef hpp_CPP_AVLTree_CPP_hpp
#define hpp_CPP_AVLTree_CPP_hpp

// Need comparable declaration
#include "Comparable.hpp"
// Need FIFO for iterator
#include "../Container/FIFO.hpp"
// We need Bool2Type
#include "../Container/Bool2Type.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif 

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

/** The Tree namespace holds the classes for tree manipulation */
namespace Tree
{
    /** AVL implementation is here. 
        AVL is an implementation of auto balanced binary tree, providing O(log N) access to sparse nodes.
        You might prefer HashTable for some algorithm (hash provides O(1) access but consume more memory).
        AVL tree are very convenient if you need to store comparable data (key => data with key being sortable), 
        and when keys are very sparse distributed, but the tree is dense.
        Please refer to Tree class */
    namespace AVL
    {
        using namespace Comparator;

        /** Default deletion for node's objects */
        template <typename T, typename KeyType>
        struct NoDeletion
        {   inline static void deleter(T &, KeyType) {} };
        /** Pointer deletion with delete() */
        template <typename T, typename KeyType>
        struct PointerDeletion
        {   inline static void deleter(T & t, KeyType) { delete t; t = 0; } };
        /** Array deletion with [] */
        template <typename T, typename KeyType>
        struct ArrayDeletion
        {   inline static void deleter(T & t, KeyType) { delete[] t; t = 0; } };

        /** Valid for all nodes */
        struct AllNodes
        {
            /** The balanced factor */
            enum Balance { LeftTreeIsHeavier = -1, Balanced = 0, RightTreeIsHeavier = 1, Forgotten = 256 };
        };

        template <typename T, typename KeyType, typename DeleterT = NoDeletion<T, KeyType> > 
        struct Node : public AllNodes
        {
        // The direction
            enum { Left = 0, Right = 1 };
        
        // Interface
            /** Get the left node */
            inline Node*& left() { return child[Left]; } 
            /** Get the right node */
            inline Node*& right() { return child[Right]; }
            /** Get the left node */
            inline const Node*& left() const { return child[Left]; } 
            /** Get the right node */
            inline const Node*& right() const { return child[Right]; }

            /** Set the left node */
            inline void left(Node * left) { child[Left] = left; }
            /** Set the right node */
            inline void right(Node * right) { child[Right] = right; }

            /** Get the node from the given compare result */
            inline Node *& fromCompare(const Comparator::CompareType comp) { return child[comp == Comparator::Greater]; }
            /** Get the opposite node from the given compare result */
            inline Node *& fromInverseCompare(const Comparator::CompareType comp) { return child[comp != Comparator::Greater]; }

            /** Get the (un)balanced factor from the given compare result */
            static inline const Balance balanceFromCompare(const Comparator::CompareType comp) { return comp == Comparator::Greater ? RightTreeIsHeavier : LeftTreeIsHeavier; }
            /** Get the opposite (un)balanced factor from the given compare result */
            static inline const Balance balanceFromInverseCompare(const Comparator::CompareType comp) { return comp != Comparator::Greater ? RightTreeIsHeavier : LeftTreeIsHeavier; }
            /** Get the balanced factor from the given compare result, by respecting all the case */
            static inline const Balance strictBalanceFromCompare(const Comparator::CompareType comp) { return comp == Comparator::Greater ? RightTreeIsHeavier : (comp == Comparator::Equal ? Balanced : LeftTreeIsHeavier); }
 
            /** Call this to avoid deleting the data of this node */
            inline void Forget() { balance = Forgotten; }
        // Constructor
            inline Node( Node* _root, const T& _data, KeyType _key) : balance(Balanced), rootNode(_root), data(_data), key(_key) { child[Left] = child[Right] = 0; }
            inline ~Node() { if (balance != Forgotten) DeleterT::deleter(data, key); }

        // Members
            /** The current node balance factor */
            Balance                                             balance;
            /** The left subtree */
            Node*                                               child[2];
            /** The root node */
            Node*                                               rootNode;
            /** The data */
            T                                                   data;
            /** The key */
            KeyType                                             key;
        };




        /** This iterator is a facility to avoid recursive algorithm.
            It use a stack internally, so it consume some heap memory, but it gives you the exact Tree 
            representation in memory. */
        template <typename T, typename KeyType, typename DeleterT = NoDeletion<T, KeyType> > 
        class Iterator
        {
        public:
            typedef Node<T, KeyType, DeleterT>  NodeT;

        public:
            /** The current node pointer */
            mutable NodeT *             node;

        public:
            /** Constructor */
            inline Iterator( NodeT* _node = 0 ) : node(_node)
            {   if(node != 0 )
                {   if( node->left() != 0) nodes.Push( node->left() );
                    if( node->right()!= 0) nodes.Push( node->right()); }
            }

            /** Copy constructor */
            inline Iterator( const Iterator& iter ) : node(iter.node), nodes(iter.nodes) {}             

            // Operators
        public:
            /** Access the pointed object */
            inline T& operator *()                  { return  node->data; }
            /** Access the pointed object */
            inline const T& operator *() const      { return  node->data; }
            /** Access the pointed object */
//          inline T* operator ->()                 { return node->data; } 
            /** Access the pointed object */
//          inline const T* operator ->() const     { return node->data; } 

            /** Change the pointed object */
            inline void Mutate(const T & newData)   { node->data = newData; }
            
            /** Move to the next node */
            inline Iterator& operator ++() const
            {   if (nodes.getSize())
                {
                    node = nodes.Pop();
                    if( node->left() ) nodes.Push( node->left() );
                    if( node->right()) nodes.Push( node->right());
                }
                else node = 0;
                return *const_cast<Iterator*>( this );
            }

            /** Move to the next node before the actual advance */
            inline Iterator operator ++(int) const                  { Iterator tmp(*this); ++(*this); return tmp; }
            /** Basic iterator compare */
            inline bool operator ==( const Iterator& rhs ) const    { return(node==rhs.node); }
            /** Inverted compare */
            inline bool operator !=( const Iterator& rhs ) const    { return(node!=rhs.node); }

            /** Copy the iterator */
            inline Iterator& operator = ( const Iterator& iter ) const { node = iter.node; nodes = iter.nodes; return *const_cast< Iterator* >(this); }

            /** Check validity for this iterator */
            inline bool isValid() const { return node != 0; }

            /** Get left iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline Iterator getLeftIterator() const { return Iterator( node != 0 ? node->left() : 0 ); }
            /** Get right iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline Iterator getRightIterator() const { return Iterator( node != 0 ? node->right() : 0 ); }
            /** Get parent iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline Iterator getParentIterator() const { return Iterator( node != 0 ? node->rootNode : 0 ); }
            /** Check if this iterator node is a final node */
            inline bool isTerminal() const { return node != 0 && node->left() == 0 && node->right() == 0; }
            /** Return the node key 
                @return a pointer on the key for this node if valid, or 0 else */
            inline const KeyType * getKey() const { if (node) return &node->key; return 0; } 

        private:
            /** The FIFO stack of next nodes */
            mutable Stack::PlainOldData::FIFO< NodeT* >   nodes;
        };

        /** This iterator is a facility to avoid recursive algorithm 
            Prefer using this iterator, as it consume less memory. */
        template <typename T, typename KeyType, typename DeleterT = NoDeletion<T, KeyType> > 
        class SortedIterator
        {
        public:
            typedef Node<T, KeyType, DeleterT>  NodeT;
            struct FromSearch { };

        public:
            /** The current node pointer */
            mutable NodeT *             node;

        public:
            /** Constructor */
            inline SortedIterator( NodeT* _node = 0, const bool first = true) : node(first ? minNode(_node) : maxNode(_node)) { }
            /** Constructor based for searching (doesn't walk down) */
            inline SortedIterator( FromSearch value, NodeT* _node = 0) : node(_node) { }
            /** Copy constructor */
            inline SortedIterator( const SortedIterator& iter ) : node(iter.node) {}             

            // The helpers method
        private:
            /** Move to the node of the minimum value */
            inline static NodeT * minNode(NodeT * node)
            {
                while(node && node->right())
                    node = node->right();
                return node;
            } 
            /** Move to the node of the minimum value */
            inline static NodeT * maxNode(NodeT * node)
            {
                while(node && node->left())
                    node = node->left();
                return node;
            }
            /** Go to the next node, in the sort order */
            inline bool Increment() const
            {
                // Already at end?
                if (node == 0) return false;

                if (node->left())
                {
                    // If current node has a left child, the next higher node is the 
                    // node with lowest key beneath the left child.
                    node = minNode(node->left());
                }
                else if (node->rootNode && node->rootNode->right() == node)
                {
                    // No left child, maybe the current node is a right child then
                    // the next higher node is the parent
                    node = node->rootNode;
                }
                else
                {
                    // Current node neither is right child nor has a left child.
                    // Ie it is either left child or root
                    // The next higher node is the parent of the first non-left
                    // child (ie either a right child or the root) up in the
                    // hierarchy. Root's parent is 0.
                    while(node && (node->rootNode && node->rootNode->left() == node))
                        node = node->rootNode;
                    node = node ? node->rootNode : 0;
                }
                return true;
            }            
            /** Go to the previous node, in the sort order */
            inline bool Decrement() const
            {
                // Already at end?
                if (node == 0) return false;

                if (node->right())
                {
                    // If current node has a right child, the next higher node is the 
                    // node with lowest key beneath the right child.
                    node = maxNode(node->right());
                }
                else if (node->rootNode && node->rootNode->left() == node)
                {
                    // No right child, maybe the current node is a left child then
                    // the next higher node is the parent
                    node = node->rootNode;
                }
                else
                {
                    // Current node neither is left child nor has a right child.
                    // Ie it is either right child or root
                    // The next higher node is the parent of the first non-right
                    // child (ie either a left child or the root) up in the
                    // hierarchy. Root's parent is 0.
                    while(node && (node->rootNode && node->rootNode->right() == node))
                        node = node->rootNode;
                    node = node ? node->rootNode : 0;
                }
                return true;
            }  
            
            // Operators
        public:
            /** Access the pointed object */
            inline T& operator *()                  { return  node->data; }
            /** Access the pointed object */
            inline const T& operator *() const      { return  node->data; }
            /** Access the pointed object */
//          inline T* operator ->()                 { return node->data; } 
            /** Access the pointed object */
//          inline const T* operator ->() const     { return node->data; } 

            /** Change the pointed object */
            inline void Mutate(const T & newData)   { node->data = newData; }
            
            /** Move to the next node */
            inline SortedIterator& operator ++() const
            {   
                Increment();
                return *const_cast<SortedIterator*>( this );
            }

            /** Move to the next node before the actual advance */
            inline SortedIterator operator ++(int) const                  { SortedIterator tmp(*this); ++(*this); return tmp; }
            /** Move to the previous node */
            inline SortedIterator& operator --() const
            {   
                Decrement();
                return *const_cast<SortedIterator*>( this );
            }

            /** Move to the next node before the actual advance */
            inline SortedIterator operator --(int) const                  { SortedIterator tmp(*this); --(*this); return tmp; }
            /** Basic iterator compare */
            inline bool operator ==( const SortedIterator& rhs ) const    { return(node==rhs.node); }
            /** Inverted compare */
            inline bool operator !=( const SortedIterator& rhs ) const    { return(node!=rhs.node); }

            /** Copy the iterator */
            inline SortedIterator& operator = ( const SortedIterator& iter ) const { node = iter.node; return *const_cast< SortedIterator* >(this); }

            /** Check validity for this iterator */
            inline bool isValid() const { return node != 0; }

            /** Get left iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline SortedIterator getLeftIterator() const { return SortedIterator( node != 0 ? node->left() : 0 ); }
            /** Get right iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline SortedIterator getRightIterator() const { return SortedIterator( node != 0 ? node->right() : 0 ); }
            /** Get parent iterator 
                @return valid iterator if valid, ie to be checked with isValid() */
            inline SortedIterator getParentIterator() const { return SortedIterator( node != 0 ? node->rootNode : 0 ); }
            /** Check if this iterator node is a final node */
            inline bool isTerminal() const { return node != 0 && node->left() == 0 && node->right() == 0; }
            /** Return the node key 
                @return a pointer on the key for this node if valid, or 0 else */
            inline const KeyType * getKey() const { if (node) return &node->key; return 0; } 


        };



        /** The Tree class with a configurable compare policy.
            
            This class is very simply to use (despite the "seemingly complex" declaration).
            An AVL tree is a tree that allow you to access a data from a key very efficiently.
            <br>
            For example, if you need to build a reverse dictionary of phone number to name, you'll 
            define a :
            @code
            typedef String Name;
            typedef uint32 Number
            AVL::Tree<Name, Number>   reverseDictionary;
            @endcode

            Then, let's say you want to build a reverse dictionary of phone number to a entity object,
            and the tree must store the entities in memory, the phone number being a virtual class:
            @code
            // If PhoneNumber doesn't provide a < operator and a == operator, you must write a Comparator
            struct PhoneNumber; 
            struct Entity; // Not constructible
            typedef AVL::Tree<Entity *, PhoneNumber, Comparator::DefaultComparator, AVL::PointerDeletion<Entity *, PhoneNumber> > ReverseDictionaryT;
            @endcode
              
            Then, using the tree is very simple:
            @code
            // From the example above
            Entity * entity = [...];
            // Insert an entity for phone number +334324T5
            ReverseDictionaryT reverseDict;
            reverseDict.insertObject(entity, PhoneNumber("+334324T5"));
            // Ok, let's search for this object
            ReverseDictionaryT::IterT iter = reverseDict.searchFor(PhoneNumber("+334324T5"));
            if (iter.isValid()) 
            {
                Entity * ent = *iter;
                // Do what you want to entity
            }
            // Let's say you want to print a list of the directory (the list will be sorted if you use a SortedIterT)
            ReverseDictionaryT::SortedIterT iter = reverseDict.getFirstSortedIterator(); // or reverseDict[0] 
            while (iter.isValid())
            {
                Entity * ent = *iter;
                // Do what you want to entity

                ++iter; // Notice the preincrement, it's faster
            }

            // Ok, Entity stop paying his bills, let's delete from the dictionary
            reverseDict.Delete(PhoneNumber("+334324T5")); // Or, if you already had an iterator from a previous search: reverseDict.Delete(iter)
            // Please notice that the previous call will actually delete the entity for the key +334324T5, because the policy is to PointerDeletion
            // If you had to store a key to array, that need delete[] call, you'll like use ArrayDeletion etc...
 
            // As usual with other containers, you have a getSize() method, and a Clear() method
            @endcode
        */
        template < typename T, typename KeyType, class Policy = Comparator::DefaultComparator, typename DeleterT = NoDeletion<T, KeyType> > 
        class Tree
        {   
            // Members
        private:
            typedef Node<T, KeyType, DeleterT>                  NodeT;
            typedef typename NodeT::Balance                     ResultT;
            typedef Comparator::Comparable<KeyType, Policy>     CompareT;

            /** Root node */
            NodeT *     root;
            /** Size */
            uint32      size;

        private:
            /** Possible result of a tree operation */
            enum OperationResult { Ok = 1, NeedReBalancing = 0, Invalid = -1 };
            
        protected:
            /** Used for insertion */
            enum { NoRelativeParent =  0, };

            // Interface which still keep the tree balanced
        public:
            /** Used when searching */
            typedef Iterator<T, KeyType, DeleterT>              IterT;
            /** Used when iterating in a sorted way */
            typedef SortedIterator<T, KeyType, DeleterT>        SortedIterT;
            
            /** Insert an object in the tree with its key. The tree takes ownership of data
                This method use an iterative algorithm, so it support very large trees

                @return true on success, false if already exists
            */
            inline bool insertObject( const T& obj, KeyType key)
            {
                /** We need to use an external method as the root might get updated */
                if (insertInTree(&root, obj, key) == Invalid) return false;
                size++; return true;
            }

            /** Delete an object from the tree. Data itself is deleted. 
                Iterators held on this tree are invalidated
                @return false if not found */
            inline bool Delete( KeyType key )
            {
                return deleteInTree(&root, key) != Invalid;
            }

            /** Delete an object from the tree. Data itself is deleted. 
                @return false if not found */
            inline bool Delete( const IterT & iter )
            {
                // Check invariant 
                if(!iter.isValid()) return false;

                // If we are working with the root node, then handle the case correctly
                if (iter.node->rootNode == 0 || iter.node->rootNode->rootNode == 0)
                {   // We can use the other function for this (as the search will directly give the correct result)
                    return deleteInTree(&root, iter.node->key) != Invalid;
                }
                // Else perform the search from the root itself
                else 
                {
                    // We need to walk up the tree until we find a parent whose balance is Balanced
                    NodeT * current = iter.node->rootNode->rootNode;
                    while (current && current->balance != AllNodes::Balanced)
                    {   // Simply go up
                        current = current->rootNode;
                    }
                    // Not found, so call the other algorithm to sort this out
                    if (!current || !current->rootNode) return deleteInTree(&root, iter.node->key) != Invalid;
                    
                    // Need to find the right root node address then
                    NodeT ** newRoot = current->rootNode->left() == current ? &current->rootNode->left() : &current->rootNode->right();
                    return deleteInTree(newRoot, iter.node->key) != Invalid;
                }
            }


            /** Forget a key (this doesn't delete the object) 
                @return false if not found */
            inline bool Forget( KeyType key ) 
            {
                return deleteInTree(&root, key, false) != Invalid;
            }

            /** Empty the tree and delete all objects */
            inline void Clear()
            {
                if (root)   deleteTree(root);
                root = 0; size = 0;
            }

            /** Check this tree correctness 
                @warning This method is recursive, so for (very) large tree, please avoid using it */
            inline bool checkTree()
            {
                return checkTree(root);
            }
            inline bool checkTree(NodeT * node)
            {
                if (!node) return true;
                CompareT        key(node->key);
                if (node->left() && node->left()->rootNode != node && key.Compare(node->left()->key) != Comparator::Less) 
                    return false; 
                if (node->right() && node->right()->rootNode != node && key.Compare(node->left()->key) != Comparator::Greater) 
                    return false;
                if (!node->right() && !node->left())
                {
                    if (node->balance != AllNodes::Balanced)
                        return false;
                } else
                {
                    if (!node->right() && node->balance != AllNodes::LeftTreeIsHeavier)
                        return false;
                    if (!node->left() && node->balance != AllNodes::RightTreeIsHeavier)
                        return false;
                }
                return checkTree(node->left()) && checkTree(node->right());
            }

            // Construction and destruction
        public:
            /** Default constructor */
            Tree() : root(0), size(0) {}
            /** Destructor */
            virtual ~Tree() { Clear(); }
            
            // Helpers functions
        private:
            /** Simple rotate the node 
                A 2-point rotation looks like this, where B is the root where the rotation must happen, and E or below is where the new node was inserted. 
                @verbatim
                     B                       D
                    / \         ==>         / \ 
                   A   D                   B   E
                      / \                 / \
                     C   E               A   C
                @endverbatim
            */
            NodeT * rotate2(NodeT ** parent, typename CompareT::Result result)
            {
                NodeT *B, *C, *D, *E;
                NodeT *Bparent = (*parent)->rootNode;
                B = *parent;
                if (result == Comparator::Greater)
                {
                    D = B->right();
                    C = D->left();
                    E = D->right();

                    *parent = D;
                
                    D->left(B);
                    B->right(C);
                } else
                {
                    D = B->left();
                    C = D->right();
                    E = D->left();

                    *parent = D;
                
                    D->right(B);
                    B->left(C);
                }
                // Fix the parent nodes
                if (C) C->rootNode = B;
                B->rootNode = D;
                D->rootNode = Bparent;
                B->balance = AllNodes::Balanced;
                D->balance = AllNodes::Balanced;
                return E;
            }

            /** Double rotation for the given node 
                The 3-point rotation looks like this, where B is the top of the path needing rotation, and the insertion point 
                is D, or at-or-below one for C and E, depending on the value of "third".

                @verbatim
                     B                        D
                    / \         ==>         /   \ 
                   A   F                   B     F
                      / \                 / \   / \
                     D   G               A   C E   G
                    / \
                   C   E
                @endverbatim
                

            */
            NodeT * rotate3(NodeT ** parent, typename CompareT::Result result, AllNodes::Balance third)
            {
                NodeT *B, *F, *D, *C, *E;
                NodeT *Bparent = (*parent)->rootNode;
                B = *parent;

                if (result == Comparator::Greater)
                {
                    F = B->right();
                    D = F->left();
                    C = D->left();
                    E = D->right();

                    *parent = D;
                
                    D->left(B);
                    D->right(F);
                    B->right(C);
                    F->left(E);
                } else
                {
                    F = B->left();
                    D = F->right();
                    C = D->right();
                    E = D->left();

                    *parent = D;
                
                    D->right(B);
                    D->left(F);
                    B->left(C);
                    F->right(E);
                }

                // Fix the parent nodes
                if (F) F->rootNode = D;
                if (C) C->rootNode = B;
                if (E) E->rootNode = F; 
                B->rootNode = D;
                D->rootNode = Bparent;

                D->balance = B->balance = F->balance = AllNodes::Balanced;

                if (third == AllNodes::Balanced) return 0;
                else if (third == D->balanceFromCompare(result)) 
                {
                    /* E holds the insertion so B is unbalanced */ 
                    B->balance = B->balanceFromInverseCompare(result);
                    return E;
                } else 
                {
                    /* C holds the insertion so F is unbalanced */
                    F->balance = F->balanceFromCompare(result);
                    return C;
                }
            }            

            /** Rebalance after an insertion */
            OperationResult rebalancePath(NodeT* current, CompareT & keyToCheck)
            {
                typename CompareT::Result result = current ? keyToCheck.Compare(current->key) : Comparator::Equal;
                while (current && result != Comparator::Equal)
                {
                    current->balance = current->balanceFromCompare(result);
                    current = current->fromCompare(result);
                    result = keyToCheck.Compare(current->key);
                }
                return Ok;
            }


            /** Rebalance after an insertion */
            OperationResult rebalanceAfterInsert(NodeT** parent, CompareT & keyToCheck)
            {
                NodeT * current = *parent;
                typename CompareT::Result first, second;
                if (current->balance != AllNodes::Balanced)
                {
                    first = keyToCheck.Compare(current->key);
                    NodeT * next = current->fromCompare(first); 
                    if (!next) return Invalid;
                    second = keyToCheck.Compare(next->key);
                    bool biggerFirst = first == Comparator::Greater;
                    bool biggerSecond = second == Comparator::Greater;

                    if (current->balance != current->balanceFromCompare(first))
                    {   // Follow the shorter path
                        current->balance = AllNodes::Balanced;
                        current = next;
                    }
                    else if (biggerFirst == biggerSecond)
                    {   // Two point rotate
                        current = rotate2(parent, first);
                    } else
                    {   // Double rotation
                        current = next->fromCompare(second);
                        AllNodes::Balance third;
                        second = keyToCheck.Compare(current->key);
                        third = current->strictBalanceFromCompare(second);
                        current = rotate3(parent, first, third);
                    }
                }
                return rebalancePath(current, keyToCheck);
            }

            /** Insert an object in the tree */
            OperationResult insertInTree( NodeT** parent, const T& obj, KeyType key)
            {
                CompareT        keyToCheck(key);

                // Find where to insert the node
                // A parent node exists, need to select the right subtree
                NodeT * current = *parent;
                NodeT ** balancer = parent;
                NodeT ** previousRoot = parent;
                typename CompareT::Result compareResult = Comparator::Greater;
                while (current && compareResult != Comparator::Equal)
                {
                    typename CompareT::Result compareResult = keyToCheck.Compare(current->key);
                    if (current->balance != AllNodes::Balanced) balancer = parent;

                    previousRoot = parent;
                    parent = &current->fromCompare(compareResult);
                    current = *parent;
                }

                // Already exist, so reject the insert
                if (current) return Invalid;

                // Create the node
                current = new NodeT(*previousRoot, obj, key);
                *parent = current;

                return rebalanceAfterInsert(balancer, keyToCheck);
            }
            
            static inline void SwapAndDelete(NodeT ** targetParent, NodeT ** parent, typename CompareT::Result result, const bool shouldDeleteNode)
            {
                bool same = targetParent == parent;
                
                NodeT * newTarget = *targetParent;
                NodeT * current = *parent;

                NodeT * rootNode = newTarget->rootNode;
                *targetParent = current;
                int currentOnLeft = -1;
                if (current->rootNode) currentOnLeft = current->rootNode->left() == current;
                *parent = current->fromInverseCompare(result);
                
                current->left(newTarget->left());
                current->right(newTarget->right());
                current->balance = newTarget->balance;
                if (currentOnLeft != -1)
                {
                    if (currentOnLeft)
                    {
                        if (current->rootNode->left()) current->rootNode->left()->rootNode = current->rootNode;
                    }
                    else if (current->rootNode->right()) current->rootNode->right()->rootNode = current->rootNode;
                }
                current->rootNode = rootNode;
                if (current->left()) current->left()->rootNode = current;
                if (current->right()) current->right()->rootNode = current;
                
                if (same && *parent) (*parent)->rootNode = rootNode;
                if (!shouldDeleteNode && newTarget) newTarget->Forget();

                delete newTarget;
            }

            /** Rebalance after an insertion */
            NodeT** rebalanceAfterDelete(NodeT** parent, CompareT & keyToCheck, NodeT ** targetParent)
            {
                NodeT * newTarget = *targetParent;

                while (1)
                {
                    NodeT * current = *parent;
                    typename CompareT::Result compareResult = keyToCheck.Compare(current->key);
                    NodeT * next = current->fromCompare(compareResult);
                    if (!next) break;

                    if (current->balance == AllNodes::Balanced)
                        current->balance = current->balanceFromInverseCompare(compareResult);
                    else if (current->balance == current->balanceFromCompare(compareResult))
                        current->balance = AllNodes::Balanced;
                    else
                    {
                        NodeT * invertNext = current->fromInverseCompare(compareResult);
                        typename CompareT::Result invertResult = compareResult == Comparator::Greater ? Comparator::Less : Comparator::Greater;
                        if (invertNext->balance == current->balanceFromCompare(compareResult))
                        {   
                            NodeT * nextInvertNext = invertNext->fromCompare(compareResult);
                            rotate3(parent, invertResult, nextInvertNext->balance);
                        } else if (invertNext->balance == AllNodes::Balanced)
                        {
                            rotate2(parent, invertResult);
                            current->balance = current->balanceFromInverseCompare(compareResult);
                            (*parent)->balance = current->balanceFromCompare(compareResult);
                        } else rotate2(parent, invertResult);

                        if (current == newTarget)
                            targetParent = &(*parent)->fromCompare(compareResult);
                    }

                    parent = &current->fromCompare(compareResult);
                }
                
                return targetParent;
            }

            /** Delete a object with the matching key in the tree 
                This method is iterative
            */
            OperationResult deleteInTree( NodeT** parent, KeyType key, const bool callDeleter = true )
            {
                // Check parameters
                if (parent == 0 || *parent == 0)  return Invalid;

                CompareT keyToFind(key);

                NodeT * current = *parent;
                NodeT ** targetParent = 0;
                NodeT ** balanced = parent;

                typename CompareT::Result compareResult = Comparator::Equal;
                while (current)
                {
                    compareResult = keyToFind.Compare(current->key);
                    if (compareResult == Comparator::Equal)
                    {   // Ok, we have found it
                        targetParent = parent;
                    }
                    // Then need to find with node to swap this one (or continue searching)
                    NodeT * next = current->fromCompare(compareResult);
                    if (!next) break;

                    // Check the balance to determine which node to balance with once deleted
                    NodeT * invertNext = current->fromInverseCompare(compareResult);
                    if (current->balance == AllNodes::Balanced
                        || (current->balance == current->balanceFromInverseCompare(compareResult) && invertNext->balance == AllNodes::Balanced))
                        balanced = parent;

                    parent = &current->fromCompare(compareResult);
                    current = *parent;
                }

                // Not found ?
                if (!targetParent) return Invalid;

                targetParent = rebalanceAfterDelete(balanced, keyToFind, targetParent);
                SwapAndDelete(targetParent, parent, compareResult, callDeleter);
                
                --size;
                return Ok;
            }           


            /** Delete the entire tree */
            void deleteTree(NodeT* node)
            {
                NodeT * current = node;
                while (current)
                {
                    if (current->left()) { current = current->left(); continue; }
                    if (current->right()) { current = current->right(); continue; }
                    NodeT * previous = current->rootNode;
                    delete current;
                    if (previous && previous->left() == current) previous->left(0);
                    if (previous && previous->right() == current) previous->right(0);
                    current = previous;
                }
            }

            /** Get the given node's root node pointer */
            inline  NodeT** getRootOf(NodeT* node)
            {
                // If the node is already root, or the tree as no node
                if (root == 0 || node == 0 || node->rootNode == 0) return 0;
                // If the root node of the given node is the tree root node, return our root node
                else if (node->rootNode->rootNode == 0)  return &root;
                // Else check if the root node is the left node of the root node, and return the left node
                else if (node->rootNode == node->rootNode->rootNode->left())    return &node->rootNode->rootNode->left();
                // Else it is the right node
                return &node->rootNode->rootNode->right();              
            }



            // Accessors
        public:
            /** Get tree height (or size) */
            inline uint32 getSize() const { return (uint32)size; }
            
            /** Check if the tree is empty */
            inline bool isEmpty() const { return root == 0; }
            
            /** Get iterator on first object */
            inline IterT getFirstIterator() { return IterT(root); }
            /** Get iterator on first object */
            inline const IterT getFirstIterator() const { return IterT(root); }
            
            /** Get iterator on last object */
            inline IterT getLastIterator() { return IterT(0); }
            /** Get iterator on last object */
            inline const IterT getLastIterator() const { return IterT(0); }

            /** Get iterator on first object */
            inline SortedIterT getFirstSortedIterator() { return SortedIterT(root, true); }
            /** Get iterator on first object */
            inline const SortedIterT getFirstSortedIterator() const { return SortedIterT(root, true); }
            
            /** Get iterator on last object */
            inline SortedIterT getLastSortedIterator() { return SortedIterT(root, false); }
            /** Get iterator on last object */
            inline const SortedIterT getLastSortedIterator() const { return SortedIterT(root, false); }

            
            /** Get the data at i-th position, in the key sort order
                @return an iterator that should be checked with isValid() 
                @warning This method is very, very slow as every node is accessed from one head to i-th position */
            inline const IterT getIterAt(const uint32 index) const
            {
                if (index > size) return IterT(0);
                // Faster to select by the end of list
                IterT iter(root);
                // This loop is based on the ++ operator of the iter
                for (uint32 i = 0; i < index; i++, ++iter); 
                return iter;
            }

            /** Get the data at i-th position, in the key sort order
                @return an iterator that should be checked with isValid() 
                @warning This method is very, very slow as every node is accessed from one head to i-th position */
            inline const SortedIterT operator [] (const uint32 index) const
            {
                if (index > size) return SortedIterT(0);
                // Faster to select by the end of list
                if (index > (size>>1))
                {
                    SortedIterT iter(root, false);
                    // This loop is based on the -- operator of the iter
                    for (uint32 i = size - 1; i > index; i--, --iter); 
                    return iter;
                }
                else
                {
                    SortedIterT iter(root, true);
                    // This loop is based on the ++ operator of the iter
                    for (uint32 i = 0; i < index; i++, ++iter); 
                    return iter;
                }
            }

            
            /** Search for the given key in the tree
                @return an iterator that should be checked with isValid() */
            inline IterT searchFor(KeyType key) const
            {
                if (root == 0) return getLastIterator();
                NodeT * node = root;
                NodeT * best = 0;
                CompareT keyToLookFor(key);
                while (node != 0)
                {
                    typename CompareT::Result result = keyToLookFor.Compare(node->key);
                    // Found ?
                    if (result == Comparator::Equal)    { best = node; break; }
                    // No, let's select the correct subtree based on comparison
                    else if (result == Comparator::Less)
                    {   node = node->left(); }
                    else // Less
                    {   node = node->right();   }
                }
                return IterT(best != 0 ? best : 0);               
            }
            /** Search for the given key in the tree
                @return an iterator that should be checked with isValid() */
            inline SortedIterT searchForFirst(KeyType key) const
            {
                if (root == 0) return getLastSortedIterator();
                NodeT * node = root;
                NodeT * best = 0;
                CompareT keyToLookFor(key);
                while (node != 0)
                {
                    typename CompareT::Result result = keyToLookFor.Compare(node->key);
                    // Found ?
                    if (result == Comparator::Equal)    { best = node; break; }
                    // No, let's select the correct subtree based on comparison
                    else if (result == Comparator::Less)
                    {   node = node->left(); }
                    else // Less
                    {   node = node->right();   }
                }
                NodeT * resultNode =  best != 0 ? best : 0;
                typename SortedIterT::FromSearch searchNode;
                SortedIterT iter(searchNode, resultNode);
                return iter;               
            }
            
        };

        /** Simple struct used to simplify type definitions */
        template < typename T, typename KeyType, class Policy = Comparator::DefaultComparator> 
        struct PointerTree
        {
            /** The tree definition for a pointer based Tree storage */
            typedef Tree<T, KeyType, Policy, PointerDeletion<T, KeyType> > Type;
        };

        /** Simple struct used to simplify type definitions */
        template < typename T, typename KeyType, class Policy = Comparator::DefaultComparator> 
        struct ArrayTree
        {
            /** The tree definition for a pointer based Tree storage */
            typedef Tree<T, KeyType, Policy, ArrayDeletion<T, KeyType> > Type;
        };
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  
