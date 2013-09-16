

#ifndef NotCopyable

// Copy constructor
template <class U, unsigned int pow2, class SearchPolicy>
ChainedList<U, pow2, SearchPolicy>::ChainedList(const ChainedList & copy)
{
    unsigned int i;
    mbUseBlocks = copy.mbUseBlocks;
    mbIntegrity = copy.mbIntegrity;
    mlNumberOfNodes = copy.mlNumberOfNodes;
    mlNumberOfBlocks = copy.mlNumberOfBlocks;

    if (mbUseBlocks)
    {
        // If copy is empty
        if (copy.mpxBlock == NULL)
        {
            mpxFirst = mpxLast = mpxCurrent = NULL;
            mpxBlock = mpxBLast = NULL;
            return;
        }

        // Must copy block contents
        LinearBlock<ChainedBS> * pBlock = new LinearBlock<ChainedBS>;
        LinearBlock<ChainedBS> * copyBlock = copy.mpxBlock;

        if (pBlock == NULL) return;

        // Set first block info
        mpxBlock = pBlock;
        mpxBlock->spxPrevious = NULL;
        mpxBlock->spxNext = NULL;
        mpxBLast = pBlock;
        mpxBlock->soUsed = copy.mpxBlock->soUsed;

        // Create blocks
        for (i=1; i < mlNumberOfBlocks; i++)
        {
            if (!pBlock->CreateNewBlock()) return;
            pBlock->soUsed = copyBlock->soUsed;
            pBlock = pBlock->spxNext;
            copyBlock = copyBlock->spxNext;
            mpxBLast = pBlock;
        }

        // Link nodes
        Node * pNode = mpxBlock->GetData();

        mpxFirst = pNode;
        mpxFirst->mpxPrevious = NULL;
        mpxCurrent = NULL;
        pBlock = mpxBlock;

        for (i=0; i < mlNumberOfBlocks; i++)
        {
            pNode = pBlock->GetData();
            for (unsigned int j=0; j < ChainedBS; j++)
            {
                pNode->mpxPrevious = mpxCurrent;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;
                mpxCurrent = pNode;
                pNode ++;
            }
            pBlock = pBlock->spxNext;
        }

        // Fill with data
        Node * copyNode = copy.mpxFirst;
        pNode = mpxFirst;

        for (i=0; i < mlNumberOfNodes; i++)
        {
            if (pNode == NULL || copyNode == NULL) return;
            pNode->Set(*copyNode->mpuElement);
            pNode = pNode->mpxNext;
            copyNode = copyNode->mpxNext;
        }


        if (pNode!=NULL) mpxLast = pNode->mpxPrevious; else mpxLast = mpxCurrent;
        mpxCurrent = pNode;
        mpxLast->mpxNext = NULL;
        mbIntegrity = true;
    }
    else
    {
        // Define temporary variables
        Node * pNode0 = NULL;
        Node * pNode = NULL;
        Node * pCopy =  copy.mpxFirst;
        for (unsigned int i = 0; i < mlNumberOfNodes; i++)
        {
            pNode = new Node;
            // Test creation
            if (pNode == NULL) return;
            if (i==0) mpxFirst = pNode;
            pNode->mpxPrevious = pNode0;
            if (pNode->mpxPrevious != NULL) pNode->mpxPrevious->mpxNext = pNode;

            // Test data validity
            if (pCopy == NULL) return;
            // And copy it
            pNode->Set(*pCopy->mpuElement);
            pCopy = pCopy->mpxNext;
            pNode0 = pNode;
        }

        // That's ok, copy pointers infos
        mpxLast = pNode;
        mpxCurrent = mpxLast;
        mpxBlock = mpxBLast = NULL;

    }
}

template <class U, unsigned int pow2, class SearchPolicy>
ChainedList<U, pow2, SearchPolicy>& ChainedList<U, pow2, SearchPolicy>::operator =(const ChainedList & copy)
{
    if (&copy == this) return (*this);  // Test a=a

    if (mpxBlock != NULL)
        if (!mpxBlock->DeleteAll()) return (*this);

    unsigned int i;
    if (!mbUseBlocks)
    {
        Node * pNode = mpxFirst;

        // Must we delete nodes ?;
        if (mpxFirst!=NULL)
        {
            // We must clean node before anything else;
            while (pNode!=NULL)
            {
                mpxFirst = pNode->mpxNext;
                delete pNode;
                pNode = mpxFirst;
            }
        }

    }

    mbUseBlocks = copy.mbUseBlocks;
    mbIntegrity = copy.mbIntegrity;
    mlNumberOfNodes = copy.mlNumberOfNodes;
    mlNumberOfBlocks = copy.mlNumberOfBlocks;

    if (mbUseBlocks)
    {
        // If copy is empty
        if (copy.mpxBlock == NULL)
        {
            mpxFirst = mpxLast = mpxCurrent = NULL;
            mpxBlock = mpxBLast = NULL;
            return (*this);
        }

        // Must copy block contents
        LinearBlock<ChainedBS> * pBlock = new LinearBlock<ChainedBS>;
        LinearBlock<ChainedBS> * copyBlock = copy.mpxBlock;

        if (pBlock == NULL) return (*this);

        // Set first block info
        mpxBlock = pBlock;
        mpxBlock->spxPrevious = NULL;
        mpxBlock->spxNext = NULL;
        mpxBLast = pBlock;
        mpxBlock->soUsed = copy.mpxBlock->soUsed;

        // Create blocks
        for (i=1; i < mlNumberOfBlocks; i++)
        {
            if (!pBlock->CreateNewBlock()) return (*this);
            pBlock->soUsed = copyBlock->soUsed;
            pBlock = pBlock->spxNext;
            copyBlock = copyBlock->spxNext;
            mpxBLast = pBlock;
        }

        // Link nodes
        Node * pNode = mpxBlock->GetData();

        mpxFirst = pNode;
        mpxFirst->mpxPrevious = NULL;
        mpxCurrent = NULL;
        pBlock = mpxBlock;

        for (i=0; i < mlNumberOfBlocks; i++)
        {
            pNode = pBlock->GetData();
            for (unsigned int j=0; j < ChainedBS; j++)
            {
                pNode->mpxPrevious = mpxCurrent;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;
                mpxCurrent = pNode;
                pNode ++;
            }
            pBlock = pBlock->spxNext;
        }

        // Fill with datas
        Node * copyNode = copy.mpxFirst;
        pNode = mpxFirst;

        for (i=0; i < mlNumberOfNodes; i++)
        {
            if ( pNode == NULL || copyNode == NULL) return (*this);
            // Copy the Node element
            pNode->Set(copyNode->Get());
            pNode = pNode->mpxNext;
            copyNode = copyNode->mpxNext;
        }


        if (pNode!=NULL) mpxLast = pNode->mpxPrevious; else mpxLast = mpxCurrent;
        mpxCurrent = pNode;
        mpxLast->mpxNext = NULL;
        mbIntegrity = true;
    }
    else
    {
        Node * pNode = NULL;

        // Define temporary variables
        Node * pNode0 = NULL;
        Node * pCopy =  copy.mpxFirst;
        for (unsigned int i = 0; i < mlNumberOfNodes; i++)
        {
            pNode = new Node;
            // Test creation
            if (pNode == NULL) return (*this);
            if (i==0) mpxFirst = pNode;
            pNode->mpxPrevious = pNode0;
            if (pNode->mpxPrevious != NULL) pNode->mpxPrevious->mpxNext = pNode;

            // Test data validity
            if (pCopy == NULL) return (*this);
            // And copy it
            pNode->Set(pCopy->Get());
            pCopy = pCopy->mpxNext;
            pNode0 = pNode;
        }

        // That's okay, copy pointers infos
        mpxLast = pNode;
        mpxCurrent = mpxLast;
        mpxBlock = mpxBLast = NULL;

    }

    return (*this);
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Add(DefaultConstParamType a, const unsigned int& pos)
{
    return Add(new U(a), pos);
}

#endif


// Default constructor
template <class U, unsigned int pow2, class SearchPolicy>
ChainedList<U, pow2, SearchPolicy>::ChainedList() : mpxFirst(NULL), mpxLast(NULL), mpxCurrent(NULL), mlNumberOfNodes(0), mlNumberOfBlocks(0), mpxBlock(NULL), mpxBLast(NULL), mbIntegrity(true)
{
    mbUseBlocks = (pow2==0) ? false: true;
}

// Destructor
template <class U, unsigned int pow2, class SearchPolicy>
ChainedList<U, pow2, SearchPolicy>::~ChainedList()
{
    Free();
}

// Move the object from the given position to the given position.
template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::moveObject(const unsigned int & uiInitialPos, const unsigned int & uiFinalPos)
{
    if ((mpxFirst==NULL)||(mpxLast==NULL)) return false;
    if (uiInitialPos >= mlNumberOfNodes) return false;

    Node * node = Goto(uiInitialPos);
    if (!node) return false;
    U* element = &node->Get();
    node->Set(NULL);

    if (!Sub(uiInitialPos)) return false;
    return Add(element, uiFinalPos);
}




//
//----------------------------------------------------------
template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Connect(Node *pNode, Node *pP, Node *pN)
{
    if (pNode == NULL) return false;

    if ( pP==NULL && pN==NULL && pNode->mpuElement!=NULL && (pNode->mpxPrevious!=NULL || pNode->mpxNext!=NULL))
    {
        // Warning connection will be lost...
        pNode->mpxPrevious = pP;
        pNode->mpxNext = pN;
        return false;
    }

    pNode->mpxPrevious = pP;
    pNode->mpxNext = pN;

    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
unsigned int ChainedList<U, pow2, SearchPolicy>::indexOf(DefaultConstParamType arg)
{
    if (mpxFirst==NULL) return (unsigned int)ChainedEnd;

    unsigned int i=0;
    Node * pNode = mpxFirst;
    while (i < mlNumberOfNodes && pNode!=NULL)
    {
        if (SearchPolicy::Compare(pNode->Get(), arg)) return i;
        i++;
        pNode = pNode->mpxNext;
    }

    return (unsigned int)ChainedEnd;
}

template <class U, unsigned int pow2, class SearchPolicy>
unsigned int ChainedList<U, pow2, SearchPolicy>::lastIndexOf(DefaultConstParamType arg)
{
    if (mpxLast==NULL) return (unsigned int)ChainedEnd;

    unsigned int i=mlNumberOfNodes;
    Node * pNode = mpxLast;
    while (i > 0 && pNode != NULL)
    {
        if (SearchPolicy::Compare(pNode->Get(), arg)) return i - 1;
        i--;
        pNode = pNode->mpxPrevious;
    }

    return (unsigned int)ChainedEnd;
}


template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::UseBlocks(const bool & arg)
{
    if (mpxBlock != NULL) return false;

    mbUseBlocks = arg;
    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Add(const ChainedList& copy, const unsigned int &pos)
{
    if ((copy.mpxFirst==NULL)||(copy.mpxLast==NULL)) return false;

    if (pos >= mlNumberOfNodes-1)
    {
        // Want to add in the end...
        if (mbUseBlocks)
        {
            // Define temporary variables
            Node * pNode = NULL;
            Node * pNode0 = mpxLast;
            Node * pCopy =  copy.mpxFirst;

            for (unsigned int i=0; i < copy.mlNumberOfNodes; i++)
            {
                pNode = CreateNode();
                if (pNode == NULL) return false;

                if (mpxFirst == NULL) mpxFirst = pNode;
                pNode->mpxPrevious = pNode0;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;

                if (pCopy == NULL) return false;
                pNode->Set(pCopy->mpuElement);
                pCopy = pCopy->mpxNext;
                pNode0 = pNode;
            }

            mlNumberOfNodes += copy.mlNumberOfNodes;
            mpxLast = pNode;
            mpxLast->mpxNext = NULL;

            return true;

        }
        else
        {

            // Define temporary variables
            Node * pNode = NULL;
            Node * pNode0 = mpxLast;
            Node * pCopy =  copy.mpxFirst;

            for (unsigned int i=0; i < copy.mlNumberOfNodes; i++)
            {
                pNode = new Node;
                if (pNode == NULL) return false;

                pNode->mpxPrevious = pNode0;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;

                if (pCopy == NULL) return false;
                pNode->Set(pCopy->mpuElement);
                pCopy = pCopy->mpxNext;
                pNode0 = pNode;
            }

            mlNumberOfNodes += copy.mlNumberOfNodes;
            mpxLast = pNode;
            mpxCurrent = mpxLast;
            mpxLast->mpxNext = NULL;

            return true;
        }
    }
    else
    {
        if (mbUseBlocks)
        {
            unsigned int i;
            Node * pNode = NULL;
            Node * pLast = mpxLast;

            // We must probably create blocks, so let's do it...
            for (i=0; i < copy.mlNumberOfNodes; i++)
            {
                pNode = CreateNode();
                if (pNode == NULL) return false;

                pNode->mpxPrevious = pLast;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;

                pLast = pNode;
            }

            mlNumberOfNodes += copy.mlNumberOfNodes;

            // Now fill new nodes with previous datas
            Node * pInsert = pNode;
            if (pInsert == NULL) return false;
            Node * pFirst = Goto(pos);
            if (pFirst == NULL) return false;

            pNode = mpxLast;
            mpxLast = pInsert;
            mpxLast->mpxNext = NULL;

            for (i=pos + copy.mlNumberOfNodes; i < mlNumberOfNodes; i++)
            {
                if ((pInsert==NULL)||(pNode ==  NULL)) return false;
                pInsert->Set(pNode->mpuElement);

                pInsert = pInsert->mpxPrevious;
                pNode = pNode->mpxPrevious;
            }

            // Let's then copy the insertion
            pInsert = pFirst;
            pNode = copy.mpxFirst;
            for (i = 0; i < copy.mlNumberOfNodes; i++)
            {
                if ((pInsert==NULL)||(pNode ==  NULL)) return false;
                pInsert->Set(pNode->mpuElement);

                pInsert = pInsert->mpxNext;
                pNode = pNode->mpxNext;
            }

            // Okay, finish..
            return true;

        }
        else
        {
            // Define temporary variables
            Node * pNode = NULL;
            Node * pFirst = Goto(pos);
            if (pFirst == NULL) return false;
            Node * pNode0 = pFirst->mpxPrevious;
            Node * pCopy =  copy.mpxFirst;

            for (unsigned int i=0; i < copy.mlNumberOfNodes; i++)
            {
                pNode = new Node;
                if (pNode == NULL) return false;

                pNode->mpxPrevious = pNode0;
                if ((i==0)&&(pos==0)) mpxFirst = pNode;
                if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;

                if (pCopy == NULL) return false;
                pNode->Set(pCopy->mpuElement);
                pCopy = pCopy->mpxNext;
                pNode0 = pNode;
            }

            mlNumberOfNodes += copy.mlNumberOfNodes;
            pNode->mpxNext = pFirst;
            pFirst->mpxPrevious = pNode;

            return true;
        }
    }

}


template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Add(U* a, const unsigned int& pos)
{
    if (pos >= mlNumberOfNodes)
    {
        // Add to end of list
        if (mbUseBlocks)
        {

            Node * pNode = CreateNode();
            if (pNode == NULL) return false;

            pNode->Set(a);
            if (mpxBlock==NULL) return false;
            mpxBLast->soUsed ++;

            // Then test if it is the first node
            if (mlNumberOfNodes==0)
                mpxFirst = pNode;
            else
                mpxLast->mpxNext = pNode;

            pNode->mpxPrevious = mpxLast;
            mpxLast = pNode;
            pNode->mpxNext = NULL;
            mlNumberOfNodes++;

            // Okay it's right so let's finish
            return true;
        }
        else
        {
            if (mpxFirst==NULL)
            {
                // List not existing, creating it
                mpxFirst = new Node(a);

                // Check creation
                if (mpxFirst == NULL) return false;

                // Connect pointers
                if (!Connect(mpxFirst,NULL,NULL)) return false;

                // Set other pointers
                mpxLast = mpxFirst;
                mpxCurrent = mpxFirst;

                // Increase number of nodes
                mlNumberOfNodes = 1;

                // Okay return
                return true;
            }
            else
            {
                // List existing, adding node
                mpxCurrent = new Node(a);

                // Check creation
                if (mpxCurrent == NULL) return false;

                // Connect pointers
                if (!Connect(mpxCurrent,mpxLast,NULL)) return false;
                mpxLast->mpxNext = mpxCurrent;
                mpxLast = mpxCurrent;

                // Increase number of nodes
                mlNumberOfNodes ++;

                // Okay return
                return true;

            }
        }
    }
    else
    {
        // Insertion mode

        // Do we use blocks ?
        if (mbUseBlocks)
        {
            // Must find the position to insert link
            Node *  pNode;
            // Test if a block exist, else call the same function
            // with correct argument
            if (mpxBlock==NULL)
            {   if (pos == 0) return Add(a); else return false;     }

            LinearBlock<ChainedBS> * pBlock = mpxBLast;
            // Inserting at the beginning
            if (pBlock->soUsed < ChainedBS)
            {
                // We have the place to store this pointer
                // so, go to the last element (mpxCurrent)
                // Check if someone parsed the list
                if (mpxCurrent->mpxPrevious == NULL || mpxCurrent->mpxPrevious->mpxNext == mpxCurrent || mpxCurrent->mpxNext == NULL)
                {
                    // Find the last node
                    if (mpxLast != NULL && mpxLast >= &pBlock->sapxBlock[0] && mpxLast < &pBlock->sapxBlock[ChainedBS - 1])
                        mpxCurrent = (mpxLast + 1);
                }
                if (mpxCurrent == NULL) return false;

                mpxLast->mpxNext = mpxCurrent;
                mpxLast = mpxCurrent;
                mpxCurrent = mpxCurrent->mpxNext;
                mpxLast->mpxNext = NULL;
                pNode = mpxLast;


                for (unsigned int i=pos; i < mlNumberOfNodes; i++)
                {
                    if (pNode->mpxPrevious !=NULL)
                    {
                        pNode->Set(pNode->mpxPrevious->mpuElement);
                        pNode = pNode->mpxPrevious;
                    }
                }

                // Okay, set first pointer data
                pNode->Set(a);

                pBlock->soUsed++;
                mlNumberOfNodes ++;
                return true;
            }
            else
            {
                unsigned int i;
                // Must create a new block...
                if (!pBlock->CreateNewBlock()) return false;

                pBlock = pBlock->spxNext;
                mpxBLast = pBlock;

                // Attach pNode with beginning of datas
                pNode = pBlock->GetData();
                // The powerful thing is here, go from pointer by adding
                // Then link list, in reverse order
                mpxCurrent = mpxLast;
                for (i = 0; i < ChainedBS; i++, pNode++)
                {
                    pNode->mpxPrevious = mpxCurrent;
                    // the following line is not really needed, because after we are only going from
                    // left to right... But, in case of a block disallocating, might
                    // create a error... So, let this line enabled
                    if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;
                    mpxCurrent = pNode;
                }

                mlNumberOfBlocks ++;
                mpxCurrent -= ChainedBS - 1;

                // We have the place to store this pointer
                // so, go to the last element (mpxCurrent)
                if (mpxCurrent == NULL) return false;

                mpxLast->mpxNext = mpxCurrent;
                mpxLast = mpxCurrent;
                mpxCurrent = mpxCurrent->mpxNext;
                mpxLast->mpxNext = NULL;
                pNode = mpxLast;


                for (i=pos; i < mlNumberOfNodes; i++)
                {
                    if (pNode->mpxPrevious !=NULL)
                    {
                        pNode->Set(pNode->mpxPrevious->mpuElement);
                        pNode = pNode->mpxPrevious;
                    }


                }

                // Okay, set first pointer data
                pNode->Set(a);

                pBlock->soUsed++;
                mlNumberOfNodes ++;
                return true;

            }

        }
        else
        {
            // List existing, adding node
            Node * pNode = new Node(a);

            // Seeking list
            if (pos!=0)
            {
                mpxCurrent = Goto(pos-1);
                if (mpxCurrent == NULL) return false;
                // Enable connections
                Connect(pNode, mpxCurrent, mpxCurrent->mpxNext);
                mpxCurrent->mpxNext = pNode;
                if (pNode->mpxNext!=NULL) pNode->mpxNext->mpxPrevious = pNode;
            } else
            {
                // Enable connections
                Connect(pNode, NULL, mpxFirst);
                mpxFirst->mpxPrevious = pNode;
                mpxFirst = pNode;

            }

            // Increase number of nodes
            mlNumberOfNodes ++;
        }
        return true;
    }
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Sub(const unsigned int& pos)
{
    if (pos >= mlNumberOfNodes-1)
    {
        // Remove last node
        if (mbUseBlocks)
        {
            // Check if list exist
            if ((mpxLast == NULL)||(mpxBlock == NULL)) return false;

            LinearBlock<ChainedBS> *        pBlock = mpxBLast;
            Node * pNode = mpxLast->mpxPrevious;

            if (pNode==NULL)
            {
                // We have to clear the list...
                if (!mpxBlock->DeleteAll()) return false;
                mpxBlock = NULL;
                mpxBLast = NULL;
                mlNumberOfNodes = 0;
                mlNumberOfBlocks = 0;
                mbUseBlocks = true;
                mbIntegrity = true;
                mpxCurrent = mpxFirst = mpxLast = NULL;
                return true;
            }

            if (pBlock->soUsed == 1)
            {
                // This block will be empty after deletion, so delete it
                pNode->mpxNext = NULL;
                mpxLast = pNode;
                mpxCurrent = NULL;
                pBlock->soUsed = 0;
                mlNumberOfNodes --;
                mlNumberOfBlocks --;
                mpxBLast = pBlock->spxPrevious;
                pBlock->Delete();

                return true;
            }

            // Check if the list was parsed before substracting this node
            if (mpxCurrent->mpxPrevious != mpxLast)
            {
                // It was, so need to restore list consistency
                if (pBlock->soUsed < ChainedBS)
                    mpxCurrent = mpxLast+1;
                else mpxCurrent = NULL;
            }

            // Okay block is at least filled with 2 pointers, can destroy the last one
            pNode->mpxNext = NULL;
            mpxLast->mpxNext = mpxCurrent;
            mpxCurrent = mpxLast;
            mpxLast = pNode;

            mlNumberOfNodes --;
            pBlock->soUsed --;

            return true;


        }
        else
        {
            if (mpxLast == NULL) return false;
            Node * pNode = mpxLast->mpxPrevious;

            if (pNode == NULL)
            {
                // Only one node in list, delete list
                delete mpxLast;
                mpxLast = mpxFirst = mpxCurrent = NULL;
                mlNumberOfNodes = 0;
                return true;
            }

            pNode->mpxNext = NULL;
            delete mpxLast;
            mpxLast = pNode;
            mpxCurrent = mpxLast;
            mlNumberOfNodes --;

            return true;
        }
    }
    else
    {
        // Remove a node
        if (mbUseBlocks)
        {
            // Check if list exist
            if ((mpxLast == NULL)||(mpxBlock == NULL)) return false;

            LinearBlock<ChainedBS> *        pBlock = mpxBLast;
            Node * pNode = Goto(pos);
            // Does this node exist ?
            if ((pNode == NULL)||(pBlock==NULL)) return false;

            // Delete the node content
            delete pNode->mpuElement;
            pNode->mpuElement = NULL;

            if (pBlock->soUsed == 1)
            {
                // We have to clear a block
                for (unsigned int i = pos+1; i < mlNumberOfNodes; i++)
                {
                    if (pNode->mpxNext == NULL) return false;
                    pNode->Set(pNode->mpxNext->mpuElement);
                    pNode = pNode->mpxNext;
                }

                // Set beginning of free tab
                mpxCurrent = NULL;
                // And decrease list size
                mpxLast = mpxLast->mpxPrevious;
                mpxLast->mpxNext = NULL;
                // Delete last block
                pBlock->soUsed = 0;
                mpxBLast = pBlock->spxPrevious;
                pBlock->Delete();

                mlNumberOfNodes --;
                mlNumberOfBlocks --;

                return true;
            }

            // We have to traverse list
            for (unsigned int i = pos+1; i < mlNumberOfNodes; i++)
            {
                if (pNode->mpxNext == NULL) return false;
                pNode->Set(pNode->mpxNext->mpuElement);
                pNode = pNode->mpxNext;
            }

            // Check if the list was parsed before substracting this node
            if (!mpxCurrent || mpxCurrent->mpxPrevious != mpxLast)
            {
                // It was, so need to restore list consistency
                if (pBlock->soUsed < ChainedBS)
                    mpxCurrent = mpxLast+1;
                else mpxCurrent = NULL;
            }
            // Restore free tab consistency
            mpxLast->mpxNext = mpxCurrent;
            // Set beginning of free tab
            mpxCurrent = mpxLast;
            // And decrease list size
            mpxLast = mpxLast->mpxPrevious;
            mpxLast->mpxNext = NULL;
            // Set the current pointer data to NULL too
            mpxCurrent->Set((U*)NULL);
            // Delete last block
            pBlock->soUsed --;

            mlNumberOfNodes --;

            return true;


        }
        else
        {
            // Get the node pointer we want to destroy, and test it
            Node * pNode = Goto(pos);
            if (pNode == NULL) return false;

            // Make connections
            if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode->mpxNext;
            if (pNode->mpxNext!=NULL) pNode->mpxNext->mpxPrevious = pNode->mpxPrevious;


            if (pNode == mpxFirst) mpxFirst = pNode->mpxNext;
            // this case shouldn't occur, but...
            if (pNode == mpxLast) mpxLast = pNode->mpxPrevious;

            // Delete node
            delete pNode;
            mlNumberOfNodes --;

            return true;
        }

    }
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Insert(DefaultConstParamType a, const unsigned int& pos)
{
    if (pos>= mlNumberOfNodes) return Add(a); // Add the node to last position

    Node * pNode;
    if (mbUseBlocks)
    {
        // Create a node in the block
        pNode = CreateNode();

        // Set integrity to false (pNode->mpxNext != pNode++)
        mbIntegrity = false;
    }
    else
    {
        pNode = new Node(a);
    }

    if (pNode == NULL) return false;
    // Set element
    pNode->Set(a);

    // Make connections
    pNode->mpxNext = Goto(pos);
    if (pNode->mpxNext==NULL) return false;

    pNode->mpxPrevious = pNode->mpxNext->mpxPrevious;
    if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;
    pNode->mpxNext->mpxPrevious = pNode;

    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Swap(const unsigned int& pos0, const unsigned int& pos1)
{
    if ( pos0>= mlNumberOfNodes || pos1>= mlNumberOfNodes) return false;
    if (pos0 == pos1) return true;

    // Get node pointers
    Node * pNode0 = Goto(pos0);
    Node * pNode1 = Goto(pos1);
    // And test them
    if ((pNode0==NULL)||(pNode1==NULL)) return false;

    U * a = pNode1->mpuElement;
    pNode1->Set(pNode0->mpuElement);
    pNode0->Set(a);
    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Remove(const unsigned int& pos)
{
    if (pos>= mlNumberOfNodes) return false;

    // Get node pointer
    Node * pNode = Goto(pos);
    if (pNode == NULL) return false;

    if (mbUseBlocks)
    {

        if (pNode==mpxLast)
        {
            LinearBlock<ChainedBS> * pBlock = mpxBLast;
            if (pBlock == NULL) return false;
            if (pBlock->soUsed == 1)
            {
                // must destroy a block
                pNode = mpxLast->mpxPrevious;
                if (pNode != NULL) pNode->mpxNext = NULL;
                mpxLast = pNode;
                mpxCurrent = NULL;


                pBlock->soUsed = 0;
                mlNumberOfNodes --;
                mlNumberOfBlocks --;
                mpxBLast = pBlock->spxPrevious;
                pBlock->Delete();

                if (!mlNumberOfBlocks) mpxBlock = NULL;
                if (!mlNumberOfNodes)  mpxFirst = NULL;
            }
            else
            {
                mpxLast->mpxNext = mpxCurrent;
                mpxCurrent = mpxLast;
                mpxLast = mpxLast->mpxPrevious;
                mpxLast->mpxNext= NULL;

                mlNumberOfNodes --;
                pBlock->soUsed --;

            }
            delete pNode->mpuElement;
            pNode->Set((U*)NULL);
            return true;
        }

        // Find block that contains that node
        LinearBlock<ChainedBS> * pBlock = mpxBlock->Find(pNode);
        if (pBlock == NULL) return false;
        pBlock->soUsed --;
        // Set integrity to false (pNode->mpxNext != pNode++)
        mbIntegrity = false;

    }

    // Make connections
    if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode->mpxNext;
    if (pNode->mpxNext!=NULL) pNode->mpxNext->mpxPrevious = pNode->mpxPrevious;


    if (pNode == mpxFirst) mpxFirst = pNode->mpxNext;
    // this case shouldn't occur, but...
    if (pNode == mpxLast) mpxLast = pNode->mpxPrevious;

    // Delete node
    if (!mbUseBlocks) delete pNode; else { delete pNode->mpuElement; pNode->Set((U*)NULL); }
    mlNumberOfNodes --;

    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
typename ChainedList<U, pow2, SearchPolicy>::Node* ChainedList<U, pow2, SearchPolicy>::CreateNode()
{
    if (mpxCurrent == NULL)
    {

        // Here no block is available, so create a new block
        LinearBlock<ChainedBS> * pBlock;
        if (mpxBlock == NULL)
        {
            mpxBlock = new LinearBlock<ChainedBS>;
            if (mpxBlock==NULL) return NULL;
            mpxBLast = mpxBlock;
            pBlock = mpxBlock;
        }
        else
        {
            // Check if used reach the needed point...
            if (mpxBlock->soUsed < ChainedBS)
            {
                // If a node (not the last one) has been deleted within this block
                // Can find it, and return it, instead...

                Node * pNode = mpxBlock->GetData();
                for (unsigned int i = 0; i < ChainedBS; i++)
                {
                    if (pNode == NULL) return NULL;
                    if (pNode->mpxNext !=  (pNode+1))
                    {
                        // pNode+1 is the free node
                        return (pNode+1);
                    }

                    pNode ++;
                }
                return NULL;
            }

            pBlock = mpxBLast;
            if (pBlock->CreateNewBlock()!=true) return NULL;
            pBlock = pBlock->spxNext;
            mpxBLast = pBlock;
        }

        // Now, pBlock contains a new free block
        // Fill it with free pointers
        Node * pNode = (Node*) pBlock->GetData();

        // The powerful thing is here, go from pointer by adding
        // Then link list, in reverse order
        for (unsigned int i = 0; i < ChainedBS; i++, pNode++)
        {
            pNode->mpxPrevious = mpxCurrent;
            // the following line is not really needed, because after we are only going from
            // left to right... But, in case of a block disallocating, might
            // create a error... So, let this line enabled
            if (pNode->mpxPrevious!=NULL) pNode->mpxPrevious->mpxNext = pNode;
            mpxCurrent = pNode;
        }

        mpxCurrent -= ChainedBS - 1; /**/
        // if (mpxCurrent->mpxNext!=NULL)    mpxCurrent->mpxNext->mpxPrevious = mpxCurrent;

        mlNumberOfBlocks ++;

        mpxCurrent = mpxCurrent->mpxNext;
        return mpxCurrent->mpxPrevious;
    }

    if (mpxCurrent==NULL) return NULL;


    // Check if someone parsed the list
    if (mpxCurrent->mpxPrevious == NULL || mpxCurrent->mpxPrevious->mpxNext == mpxCurrent || mpxCurrent->mpxNext == NULL)
    {
        // Not the last one
        if (mpxBLast != NULL && mpxBLast->soUsed < ChainedBS)
        {
            // Need to find the first free node
            Node * pNode = mpxBLast->GetData();
            for (unsigned int i = 0; i < ChainedBS; i++)
            {
                if (pNode == NULL) return NULL;
                if (pNode->mpxNext !=  (pNode+1))
                {
                    // pNode+1 is the free node

                    // Need to check if pNode+1 is outside the block
                    if ((pNode+1) > (mpxBLast->GetData() + ChainedBS))
                    {
                        // pNode + 1 is outside this block, need to create a new block
                        mpxCurrent = NULL;
                        return CreateNode();
                    }
                    else
                    // Now need to set back mpxCurrent correctly
                    {
                        mpxCurrent = (pNode + 1)->mpxNext;
                    }

                    return (pNode+1);
                }
                pNode ++;
            }
        }
        // Get the next available pointer, in the next block that need to be created
        else
        {
            mpxCurrent = NULL;
            return CreateNode();
        }
    }

    mpxCurrent = mpxCurrent->mpxNext;
    return mpxCurrent->mpxPrevious;
}



template <class U, unsigned int pow2, class SearchPolicy>
typename ChainedList<U, pow2, SearchPolicy>::Node* ChainedList<U, pow2, SearchPolicy>::Goto(const unsigned int& pos)
{
    if (mpxFirst == NULL) return NULL;

    Node* pT;
    if ( mbUseBlocks && mbIntegrity )
    {
        if (mpxBlock==NULL) return NULL;

        unsigned int distinblock = (pos>>pow2);
        LinearBlock<ChainedBS> *pBlock = mpxBlock;


        for (unsigned int i = 0; i < distinblock; i++)
        {
            if (pBlock==NULL) return NULL;
            pBlock = pBlock->spxNext;
        }

        pT = pBlock->GetData();
        pT += pos - (distinblock<<pow2);

    }
    else
    {
        if (pos <= (mlNumberOfNodes>>1))
        {
            // It's quicklier to reach pos starting by the beginning
            pT = mpxFirst;

            for (unsigned int i=0; i<pos; i++)
            {
                if (pT==NULL) return NULL;
                pT = pT->mpxNext;
            }
        } else
        {
            // It's quicklier to reach pos starting by the end
            pT = mpxLast;

            for (unsigned int i=pos+1; i < mlNumberOfNodes; i++)
            {
                if (pT==NULL) return NULL;
                pT = pT->mpxPrevious;
            }

        }
    }
    return pT;
}

template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Change(DefaultConstParamType  a, const unsigned int & pos)
{
    if (mpxFirst == NULL) return false;

    if (pos > mlNumberOfNodes) return false;
    // Can add if last pos is selected...
    if (pos == mlNumberOfNodes) Add(a);

    Node* pT = Goto(pos);
    if (pT == NULL) return false;

    if (pT->mpuElement != NULL)
        delete pT->mpuElement;

    pT->Set(a);

    return true;

}


template <class U, unsigned int pow2, class SearchPolicy>
bool ChainedList<U, pow2, SearchPolicy>::Free()
{
    if (mpxFirst == NULL) return false;

    if (mbUseBlocks)
    {
        if (mpxBlock == NULL) return false;

        mpxBlock->DeleteAll();
        mpxBlock = NULL;
        mpxFirst = mpxLast = mpxCurrent = NULL;
        mlNumberOfNodes = 0;
        mlNumberOfBlocks = 0;
        mbIntegrity = true;
        mbUseBlocks = true;

    }
    else
    {
        mpxCurrent = mpxFirst;
        for (unsigned int i=0; i < mlNumberOfNodes; i++)
        {
            mpxLast = mpxCurrent->mpxNext;
            delete mpxCurrent;
            mpxCurrent = mpxLast;
        }
        if (mpxCurrent != NULL) return false;

        mpxFirst = mpxLast = mpxCurrent = NULL;
        mlNumberOfNodes = 0;
        mbIntegrity = true;
    }

    return true;
}

template <class U, unsigned int pow2, class SearchPolicy>
U* ChainedList<U, pow2, SearchPolicy>::parseList(const bool & bInit) const
{
    if (mpxFirst == NULL) return NULL;

    if (bInit) { mpxCurrent = mpxFirst; return mpxCurrent->mpuElement; }
    else if (mpxCurrent!=mpxLast)      { mpxCurrent = mpxCurrent->mpxNext; return mpxCurrent->mpuElement; }
    else return NULL;
}

template <class U, unsigned int pow2, class SearchPolicy>
const void * ChainedList<U, pow2, SearchPolicy>::saveParsingStack() const
{
    return (const void*)mpxCurrent;
}
template <class U, unsigned int pow2, class SearchPolicy>
void ChainedList<U, pow2, SearchPolicy>::restoreParsingStack(const void * stack) const
{
    if (stack != NULL) mpxCurrent = (Node*)stack;
}


template <class U, unsigned int pow2, class SearchPolicy>
U* ChainedList<U, pow2, SearchPolicy>::parseListStart(const unsigned int & uiPos) const
{
    if (uiPos != ChainedEnd)  { mpxCurrent = Goto(uiPos); if (mpxCurrent!=NULL) return mpxCurrent->mpuElement; else return NULL; }
    else if (mpxCurrent!=NULL)     { mpxCurrent = mpxCurrent->mpxNext; return mpxCurrent->mpuElement; }
    else return NULL;
}

template <class U, unsigned int pow2, class SearchPolicy>
U* ChainedList<U, pow2, SearchPolicy>::reverseParseList(const bool & bInit) const
{
    if (mpxLast == NULL) return NULL;

    if (bInit) { mpxCurrent = mpxLast; return mpxCurrent->mpuElement; }
    else if (mpxCurrent!=mpxFirst)     { mpxCurrent = mpxCurrent->mpxPrevious; return mpxCurrent->mpuElement; }
    else return NULL;
}

template <class U, unsigned int pow2, class SearchPolicy>
U* ChainedList<U, pow2, SearchPolicy>::reverseParseListStart(const unsigned int & uiPos) const
{
    if (uiPos != ChainedEnd)  { mpxCurrent = Goto(uiPos); if (mpxCurrent!=NULL) return mpxCurrent->mpuElement; else return NULL; }
    else if (mpxCurrent!=NULL)     { mpxCurrent = mpxCurrent->mpxPrevious; return mpxCurrent->mpuElement; }
    else return NULL;
}





