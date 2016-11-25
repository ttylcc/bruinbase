#include <cmath>
#include <cstdlib>
#include "BTreeNode.h"
#include <iostream>
#include <cstring>

using namespace std;

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf) {
    if (pid < 0 || pid > pf.endPid()) return RC_INVALID_PID;
//    int tmp =0;
//    memcpy(buffer, &tmp, sizeof(int));
//    cout<< getKeyCount()<<"      ";
    return pf.read(pid, buffer);
}

/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf) {
    if (pid < 0 || pid > pf.endPid()) return RC_INVALID_PID;
    return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount() {

    // the first 4 bytes store the number of keys in the leaf node
    int numKeys;
    memcpy(&numKeys, buffer, sizeof(numKeys));
    return numKeys;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid) {

    int numKeys = getKeyCount();

    int sizeRid = sizeof(RecordId);
    int sizeKey = sizeof(int);
    int sizePair = sizeRid + sizeKey;    // 12 bytes per pair

    if (numKeys == maxKeys) {
        return RC_NODE_FULL;
    }

    int i = 1;
    int tmpKey;
    for (; i <= numKeys; i++) {
        memcpy(&tmpKey, buffer + sizeof(numKeys)+ sizeRid + (i - 1) * sizePair , sizeKey);
        if (key < tmpKey) {
            break;
        }
    }
    memmove(buffer + sizeof(numKeys) + i * sizePair, buffer + sizeof(numKeys) + (i - 1) * sizePair, (numKeys - i + 1) * sizePair);
    memcpy(buffer + sizeof(numKeys) + (i - 1) * sizePair + sizeRid, &key, sizeKey);
    memcpy(buffer + sizeof(numKeys) + (i - 1) * sizePair, &rid, sizeRid);

    numKeys++;
    memcpy(buffer, &numKeys, sizeof(numKeys));

    return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid,
                              BTLeafNode& sibling, int& siblingKey) {

    int sizeKey = sizeof(key);
    int sizeRid = sizeof(rid);
    int sizePair = sizeKey + sizeRid;

    int numKeys = getKeyCount();
    if (numKeys < maxKeys) return RC_NO_NEED_SPLIT;


    // split algorithm
    // insert key, and split into two part, the first ceiling(n/2) keys in the left node, the rest in the right node

    // move all pairs into tmpBuffer, the size of tmpBuffer is (numKeys + 1)
    char tmpBuffer[(numKeys + 1) * sizePair];
    memmove(tmpBuffer, buffer + sizeof(numKeys), numKeys * sizePair);

    int i=1;
    int tmpKey;
    for (; i <= numKeys; i++) {
        memcpy(&tmpKey, tmpBuffer + (i - 1) * sizePair + sizeRid, sizeKey);
        if (key < tmpKey) break;
    }

    // i is the position of the key should be inserted
    memmove(tmpBuffer + i * sizePair, tmpBuffer + (i - 1) * sizePair, (numKeys - i + 1) * sizePair);
    memcpy(tmpBuffer + (i - 1) * sizePair + sizeRid, &key, sizeKey);
    memcpy(tmpBuffer + (i - 1) * sizePair, &rid, sizeRid);


    int lefthalfNumKeys = ceil( (maxKeys + 1) / 2.0);
    int righthalfNumKeys = maxKeys + 1 - lefthalfNumKeys;




    // move the content of tmpBuffer to buffer and sibling.buffer
    memset(buffer, 0, sizeof(buffer) - sizeof(PageId));
    memcpy(buffer, &lefthalfNumKeys, sizeof(lefthalfNumKeys));
    memmove(buffer + sizeof(lefthalfNumKeys), tmpBuffer, lefthalfNumKeys * sizePair);

    memcpy(sibling.buffer, &righthalfNumKeys, sizeof(righthalfNumKeys));
    memmove(sibling.buffer + sizeof(righthalfNumKeys), tmpBuffer + lefthalfNumKeys * sizePair, righthalfNumKeys * sizePair);

    memcpy(&siblingKey, sibling.buffer + sizeof(righthalfNumKeys) + sizeRid, sizeKey);

   // free(tmpBuffer);

    return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid) {

    int numKeys = getKeyCount();
    // what if numKeys == 0 ?
    // before call this method, node cannot be empty

    int sizeRid = sizeof(RecordId);
    int sizeKey = sizeof(int);
    int sizePair = sizeRid + sizeKey;

    int i = 1;
    int tmpKey;
    for (; i <= numKeys; i++) {
        memcpy(&tmpKey, buffer + sizeof(numKeys) + (i - 1) * sizePair + sizeRid, sizeof(tmpKey));
        if (searchKey == tmpKey) {
            eid = i;
            return 0;
        }
        else if (searchKey < tmpKey) {
            eid = i;
            return RC_NO_SUCH_RECORD;
        }
    }

    return 0;
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid) {

    int sizeRid = sizeof(RecordId);    // 8 bytes
    int sizeKey = sizeof(int);         // 4 bytes
    int sizePair = sizeRid + sizeKey;  // 8 + 4 = 12 bytes per pair

    int numKeys = getKeyCount();
    if (eid < 1 || eid > numKeys) return RC_INVALID_EID;

    memcpy(&rid, buffer + sizeof(numKeys) + (eid - 1) * sizePair, sizeRid);
    memcpy(&key, buffer + sizeof(numKeys) + (eid - 1) * sizePair + sizeRid, sizeKey);

    return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node
 */
PageId BTLeafNode::getNextNodePtr() {
    PageId pid = 0;
    memcpy(&pid, buffer + PageFile::PAGE_SIZE - sizeof(pid), sizeof(pid));
    return pid;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid) {

    // the last 4 bytes store the next node pointer
    if (pid < 0 ) return RC_INVALID_PID;
    memcpy(buffer + PageFile::PAGE_SIZE - sizeof(pid), &pid, sizeof(pid));
    return 0;
}

BTLeafNode::BTLeafNode(){
    maxKeys=80;
    //setNextNodePtr(-1);  // no use
    memset(buffer,0,sizeof(buffer) );
}

void BTLeafNode::print() {

    int pairSize = sizeof(RecordId) + sizeof(int);
    char *temp = buffer;
    temp=temp+12;
    cout << "--------leaf node---------" << endl;
    for (int i = 0; i < getKeyCount() * pairSize; i += pairSize) {
        int insideKey;
        memcpy(&insideKey, temp, sizeof(int));

        cout << insideKey << " ";

        temp += pairSize;
    }
    cout <<endl<< "---------------------------" << endl;
}


// =============================================================================



BTNonLeafNode::BTNonLeafNode(){
    maxKeys=125;
    memset(buffer,0,sizeof(buffer) );
}


/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf) {
    if (pid < 0 || pid > pf.endPid()) return RC_INVALID_PID;
    return pf.read(pid, buffer);
}

/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf) {
    if (pid < 0 || pid > pf.endPid()) return RC_INVALID_PID;
    return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount() {
    // the first 4 bytes store the number of keys in the non-leaf node
    int numKeys;
    memcpy(&numKeys, buffer, sizeof(numKeys));
    return numKeys;
}


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid) {

    //how to deal with the first pageid?

    int numKeys = getKeyCount();

    int sizePageId = sizeof(PageId);
    int sizeKey = sizeof(key);
    int sizePair = sizePageId + sizeKey;    // 8 bytes per pair

    if (numKeys == maxKeys) {
        return RC_NODE_FULL;
    }

    // if numKeys == 0, no need to go into loop
    int i = 1;
    int tmpKey;
    for (; i <= numKeys; i++) {
        memcpy(&tmpKey, buffer + sizeof(numKeys) + sizePageId + (i - 1) * sizePair , sizeKey);
        if (key < tmpKey) {
            break;
        }
    }

    memmove(buffer + sizeof(numKeys) + sizePageId + i * sizePair, buffer + sizeof(numKeys) + sizePageId + (i - 1) * sizePair, (numKeys - i + 1) * sizePair);
    memcpy(buffer + sizeof(numKeys) + sizePageId + (i - 1) * sizePair, &key, sizeKey);
    memcpy(buffer + sizeof(numKeys) + sizePageId + (i - 1) * sizePair + sizeKey, &pid, sizePageId);

    numKeys++;
    memcpy(buffer, &numKeys, sizeof(numKeys));

    return 0;
}

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey) {

    int numKeys = getKeyCount();
    if (numKeys < maxKeys) return RC_NO_NEED_SPLIT;

    int sizeKey = sizeof(key);
    int sizePageId = sizeof(pid);
    int sizePair = sizeKey + sizePageId;

    char tmpBuffer[(maxKeys + 1) * sizePair];
    memcpy(tmpBuffer, buffer + sizeof(numKeys) + sizePageId, maxKeys * sizePair);

    int i = 1;
    int tmpKey;
    for (; i <= numKeys; i++) {
        memcpy(&tmpKey, tmpBuffer + (i - 1) * sizePair, sizeKey);
        if (key < tmpKey) break;
    }

    memmove(tmpBuffer + i * sizePair, tmpBuffer + (i - 1) * sizePair, (numKeys - i + 1) * sizePair);
    memcpy(tmpBuffer + (i - 1) * sizePair, &key, sizeKey);
    memcpy(tmpBuffer + (i - 1) * sizePair + sizeKey, &pid, sizePageId);

    int lefthalfNumKeys = ceil((maxKeys + 1) / 2.0) - 1;
    int righthalfNumKeys = maxKeys - lefthalfNumKeys;



    // move the content of tmpBuffer to buffer and sibling.buffer
    memset(buffer + sizeof(numKeys) + sizePageId, 0, maxKeys * sizePair);
    memcpy(buffer, &lefthalfNumKeys, sizeof(lefthalfNumKeys));
    memmove(buffer + sizeof(lefthalfNumKeys) + sizePageId, tmpBuffer, lefthalfNumKeys * sizePair);
    memcpy(sibling.buffer, &righthalfNumKeys, sizeof(righthalfNumKeys));
//    memmove(sibling.buffer + sizeof(righthalfNumKeys) + sizePageId, tmpBuffer + lefthalfNumKeys * sizePair, righthalfNumKeys * sizePair);
    memmove(sibling.buffer + sizeof(righthalfNumKeys), tmpBuffer + lefthalfNumKeys * sizePair+ sizeof(int) , righthalfNumKeys * sizePair+sizeof(int) );


    memcpy(&midKey, tmpBuffer +  lefthalfNumKeys * sizePair, sizeKey);



   // free(tmpBuffer);


    return 0;
}

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid) {

    int sizePageId = sizeof(PageId);
    int sizeKey = sizeof(searchKey);
    int sizePair = sizePageId + sizeKey;

    int numKeys = getKeyCount();
    if (numKeys < 1) return RC_LOCATECHILD_FAILED;

    int i;
    int tmpKey;
    for (i = 1; i <= numKeys; i++) {
        memcpy(&tmpKey, buffer + sizeof(numKeys) + (i - 1) * sizePair + sizePageId, sizeKey);
        if (searchKey < tmpKey) {
            memcpy(&pid, buffer + sizeof(numKeys) + (i - 1) * sizePair, sizePageId);
            return 0;
        }
        // the last pointer
        if (i == numKeys) {
            memcpy(&pid, buffer + sizeof(numKeys) + i * sizePair, sizePageId);
            return 0;
        }
    }

    return 0;
}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2) {

    int numKeys = getKeyCount();
    if (numKeys != 0) return RC_ROOT_INITIAL_FAILED;



    numKeys++;
    memcpy(buffer, &numKeys, sizeof(numKeys));
    memcpy(buffer + sizeof(numKeys), &pid1, sizeof(pid1));
    memcpy(buffer + sizeof(numKeys) + sizeof(pid1), &key, sizeof(key));
    memcpy(buffer + sizeof(numKeys) + sizeof(pid1) + sizeof(key), &pid2, sizeof(pid2));

    return 0;
}

void BTNonLeafNode::print()
{
    //This is the size in bytes of an entry pair
    int pairSize = sizeof(PageId) + sizeof(int);

    //Skip the first 8 offset bytes, since there's no key there
    char* temp = buffer+8;

    cout << "-----------nonleaf node------------------" << endl;

    for(int i=8; i<getKeyCount()*pairSize+8; i+=pairSize)
    {
        int insideKey;
        memcpy(&insideKey, temp, sizeof(int)); //Save the current key inside buffer as insideKey

        cout << insideKey << " ";

        //Otherwise, searchKey is greater than or equal to insideKey, so we keep checking
        temp += pairSize; //Jump temp over to the next key
    }

    cout <<endl<< "------------------------" << endl;
}