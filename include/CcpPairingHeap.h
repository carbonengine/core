// Copyright © 2006 CCP ehf.

/*
	*************************************************************************

	CcpPairingHeap.h

	Created:   Dec 2006
	OS:        Win32
	Project:   Util

	Description:
		A template-based two-pass pairing heap implementation.
		Use where an efficient priority queue is required.
		see http://en.wikipedia.org/wiki/Pairing_heap
		http://www.cise.ufl.edu/~sahni/dsaaj/enrich/c13/pairing.htm

	Usage:
		similar to many stl classes.

	Dependencies:

	(c) CCP 2006

	*************************************************************************
*/

#ifndef _PH_H_
#define _PH_H_


#include <vector>


//Declarations

template <class T>
class PairingHeap;

// A node in a multiway tree.  This is a tree in which each node can have an
// arbitrary number of children.  For efficiency, the children are linked
// in a sibling list, and the node only stores a pointer to its leftmost
// child.  If node->prev->child == node, then prev is a node's parent and
// node is its leftmost child.
template <class T>
class MultiwayNode
{
	friend class PairingHeap<T>;
	MultiwayNode(const T & item);
	void AssertLink() const;

	bool IsRoot() const;
	bool IsLeftChild() const;

	void Unlink();
	void LinkChild(MultiwayNode<T> *c);
	MultiwayNode<T> *UnlinkChild();

	operator T () const {return item;}

	T item;
	MultiwayNode<T> *prev, *sibling, *child;
};


template <class T>
class PairingHeap
{
public:
	PairingHeap();
	PairingHeap(const PairingHeap<T> &other);
	~PairingHeap();

	PairingHeap<T> &operator=(const PairingHeap<T> &other);

	typedef MultiwayNode<T> *nodeptr_t;
	nodeptr_t insert(const T &item);
	void decrease_key(nodeptr_t node);

	bool is_empty() const;
	size_t size() const;


	T &find_min() const;
	T remove_min();
	T remove(nodeptr_t node);
	void clear();
	void merge(const PairingHeap &other);

	void get_stats(int &nNodes, int &maxDepth, float &avgDepth);



private:
	static nodeptr_t CompareLink(nodeptr_t a, nodeptr_t b);
	static nodeptr_t DetachChildren(nodeptr_t node);
	static void Delete(nodeptr_t node);
	static void RecStats(nodeptr_t node, nodeptr_t parent, int depth, int &maxDepth, int &nFound, float &weighedDepth);
	static nodeptr_t CloneNode(nodeptr_t node, nodeptr_t prev);

	nodeptr_t root;
	size_t nElements;
};

// Class member definitions

template <class T>
MultiwayNode<T>::MultiwayNode(const T & i) :
	item(i)
{
	prev = sibling = child = 0;
}

template <class T>
void MultiwayNode<T>::AssertLink() const
{
	if( prev )
	{
		CCP_ASSERT( prev->child == this || prev->sibling == this );
	}
	if( sibling )
	{
		CCP_ASSERT( sibling->prev == this );
	}
	if( child )
	{
		CCP_ASSERT( child->prev == this );
	}
}


template <class T>
void MultiwayNode<T>::Unlink()
{
	AssertLink();
	MultiwayNode<T> *p = prev, *s = sibling;
	if (p) {
		if (p->child == this)
			p->child = s;
		else
			p->sibling = s;
		prev = 0;
	}
	if (s) {
		s->prev = p;
		sibling = 0;
	}
	AssertLink();
	if (p)
		p->AssertLink();
	if (s)
		s->AssertLink();
}

template <class T>
void MultiwayNode<T>::LinkChild(MultiwayNode<T> *c)
{
	AssertLink();
	CCP_ASSERT(c && c->IsRoot());
	c->AssertLink();

	MultiwayNode<T> *oldC = child;
	c->sibling = oldC;
	c->prev = this;
	child = c;
	if (oldC)
		oldC->prev = c;

	AssertLink();
	c->AssertLink();
}


template <class T>
MultiwayNode<T> *MultiwayNode<T>::UnlinkChild()
{
	AssertLink();
	MultiwayNode<T> *c = child;
	if (c) {
		child = c->sibling;
		if (child)
			child->prev = this;
		c->prev = c->sibling = 0;
	}
	AssertLink();
	return c;
}


template <class T>
bool MultiwayNode<T>::IsRoot() const
{
	return !prev && !sibling;
}


template <class T>
bool MultiwayNode<T>::IsLeftChild() const
{
	return prev && prev->child == this;
}

//PairingHeap member functions

template <class T>
PairingHeap<T>::PairingHeap()
{
	root = 0;
	nElements = 0;
}


template <class T>
PairingHeap<T>::PairingHeap(const PairingHeap<T> &other)
{
	root = CloneNode(other.root, 0);
	nElements = other.nElements;
}


template <class T>
PairingHeap<T>::~PairingHeap()
{
	clear();
}


template <class T>
PairingHeap<T> &PairingHeap<T>::operator = (const PairingHeap<T> &other)
{
	clear();
	root = CloneNode(other.root, 0);
	nElements = other.nElements;
	return *this;
}


template <class T>
void PairingHeap<T>::merge(const PairingHeap<T> &other)
{
	if (other.is_empty())
		return;
	nodeptr_t clone = CloneNode(other.root, 0);
	if (root)
		root = CompareLink(root, clone);
	else
		root = clone;
	nElements += other.nElements;
}


template <class T>
typename PairingHeap<T>::nodeptr_t PairingHeap<T>::insert(const T &item)
{
	nodeptr_t node = new MultiwayNode<T>(item);
	if (root)
		root = CompareLink(root, node);
	else
		root = node;
	++nElements;
	return node;
}


template <class T>
bool PairingHeap<T>::is_empty() const
{
	return !root;
}


template <class T>
void PairingHeap<T>::decrease_key(nodeptr_t node)
{
	CCP_ASSERT(node);
	CCP_ASSERT(!is_empty());
	if (node == root)
		return;
	node->Unlink();
	root = CompareLink(root, node);
}


template <class T>
T &PairingHeap<T>::find_min() const
{
	_ASSERTE(!is_empty());
	return *root;
}


template <class T>
T PairingHeap<T>::remove_min()
{
	CCP_ASSERT(!is_empty());
	MultiwayNode<T> *node = root;
	root = DetachChildren(node);
	T result = *node;
	delete node;
	nElements--;
	return result;
}


template <class T>
T PairingHeap<T>::remove(nodeptr_t node)
{
	CCP_ASSERT(node);
	CCP_ASSERT(!is_empty());
	node->Unlink;
	nodeptr_t children = DetachChildren(node);
	if (node != root)
		root = CompareLink(root, children);
	else
		root = children;
	T result = *node;
	delete node;
	nElements--;
	return result;
}


template <class T>
void PairingHeap<T>::clear()
{
	if (root)
		Delete(root);
	root = 0;
	nElements = 0;
}


template <class T>
void PairingHeap<T>::get_stats(int &nNodes, int &maxDepth, float &avgDepth)
{
	nNodes = 0;
	maxDepth = 0;
	avgDepth = 0;
	if (root)
		RecStats(root, 0, 0, maxDepth, nNodes, avgDepth);
	if (nNodes)
		avgDepth /= nNodes;
	_ASSERTE(nNodes == nElements);
}


template <class T>
typename PairingHeap<T>::nodeptr_t PairingHeap<T>::CompareLink(nodeptr_t a, nodeptr_t b)
{
	CCP_ASSERT(a && a->IsRoot()); //root nodes
	CCP_ASSERT(b && b->IsRoot());

	//make a point to lower or equal node
	//Note, this is the only place where comparison of the "item" takes place!
	//(ok, we assert it in the stats function too!)
	if (b->item < a->item) {
		nodeptr_t tmp = a;
		a = b;
		b = tmp;
	}

	//link b in as a's child
	a->LinkChild(b);
	return a;
}


template<class T>
typename PairingHeap<T>::nodeptr_t PairingHeap<T>::DetachChildren(nodeptr_t node)
{
	CCP_ASSERT(node);
	std::vector<MultiwayNode<T> *> vec;
	vec.reserve(5);
	nodeptr_t child;
	do {
		child = node->UnlinkChild();
		if (child)
			vec.push_back(child);
	} while(child);
	if (!vec.size())
		return 0; //no children

	//first pass.  merge pairs left to right
	size_t i;
	for(i = 0; i+1 < vec.size(); i+=2)
		vec[i] = CompareLink(vec[i], vec[i+1]);

	//second pass.  Merge everything from right to left
	if (i >= vec.size())
		i -= 2;
	child = vec[i];
	while (i>0) {
		i-= 2;
		child = CompareLink(child, vec[i]);
	}
	return child;
}


template<class T>
void PairingHeap<T>::Delete(nodeptr_t node)
{
	CCP_ASSERT(node);
	if (node->sibling)
		Delete(node->sibling);
	if (node->child)
		Delete(node->child);
	delete node;
}


template<class T>
void PairingHeap<T>::RecStats(nodeptr_t node, nodeptr_t parent, int depth, int &maxDepth, int &nFound, float &weighedDepth)
{
	_ASSERTE(node);
	if (parent)
		_ASSERTE(! (node->item < parent->item));  //ensure the heap rule
	nFound ++;
	weighedDepth += depth;
	if (depth > maxDepth)
		maxDepth = depth;
	if (node->sibling)
		RecStats(node->sibling, parent, depth, maxDepth, nFound, weighedDepth);
	if (node->child)
		RecStats(node->child, node, depth+1, maxDepth, nFound, weighedDepth);
}


template <class T>
typename PairingHeap<T>::nodeptr_t PairingHeap<T>::CloneNode(nodeptr_t node, nodeptr_t prev)
{
	if (!node)
		return 0;
	nodeptr_t n = new MultiwayNode<T>(*node);
	n->prev = prev;
	n->sibling = CloneNode(node->sibling, n);
	n->child = CloneNode(node->child, n);
	return n;
}


#endif // _PH_H_
