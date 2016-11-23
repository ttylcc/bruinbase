/*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "BTreeIndex.h"
#include "BTreeNode.h"
#include <iostream>
#include <cstring>

using namespace std;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
    treeHeight=0;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{
    pf.open(indexname,mode);

    if (pf.endPid()==0){
        rootPid=-1;
        treeHeight=0;

        memcpy(buffer,&rootPid ,sizeof(int));
        memcpy(buffer+4, &treeHeight ,sizeof(int));
        pf.write(0,buffer);  // write to add the first page ( pf.eid++ )

    }
    else{
        pf.read(0,buffer);
        memcpy(&rootPid, buffer, sizeof(int));   // the root pid from the page0
        memcpy(&treeHeight, buffer+4, sizeof(int)); // get the tree height from page0

    }

    return 0;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{
    memcpy(buffer,&rootPid ,sizeof(int));
    memcpy(buffer+4, &treeHeight ,sizeof(int));
    pf.write(0,buffer);

    return pf.close();
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{

    if (treeHeight==0){
        BTLeafNode newroot;
        rootPid = pf.endPid();
        newroot.insert(key,rid);
        treeHeight++;
        return newroot.write(rootPid,pf);



    }
    else{
        int toaddedkey = -1;
        int toaddedpid = -1;

        insertRec(rootPid,1,key,rid,toaddedkey,toaddedpid);


        if (toaddedkey!=-1 && toaddedpid!=-1){
            BTNonLeafNode newroot;
            int newrootpid = pf.endPid();

            newroot.initializeRoot(rootPid,toaddedkey,toaddedpid );

            newroot.write(newrootpid,pf);

            rootPid=newrootpid;
            //pf.write(0,buffer);  //  pay attention   root node and height  has already changed
            treeHeight++;
        }

    }


    return 0;
}

RC BTreeIndex::insertRec( int curpid,int curheight, int key, const RecordId& rid , int& addedkey, int& addedpid ){

    if (curheight==treeHeight){

        BTLeafNode leafNode;
        leafNode.read(curpid,pf);
        int error = leafNode.insert(key,rid);

        if (error==0){     /// no overflow in leaf node


            leafNode.write(curpid,pf);
            return 0;
        }
        else{             /// overflow in leaf node



            BTLeafNode newsibling;
            int newsiblingpid = pf.endPid();
            //newsibling.write(newsiblingpid,pf);

            /// update addedkey and addedpid for upper level to insert
            leafNode.insertAndSplit(key,rid,newsibling,addedkey);  // leafnode connect sibling?
            addedpid=newsiblingpid;

            ///  save current node and its new sibiling
            newsibling.write(newsiblingpid,pf);
            leafNode.write(curpid,pf);


            return 0;

        }


    }
    else{

        BTNonLeafNode nonLeafNode;
        nonLeafNode.read(curpid,pf);

        int toaddedkey = -1;
        int toaddedpid = -1;

        int childpid = -1;
        nonLeafNode.locateChildPtr(key, childpid);


        insertRec(childpid,curheight+1,key,rid,toaddedkey,toaddedpid);


        if (toaddedkey!=-1 && toaddedpid!=-1){

            int error = nonLeafNode.insert(toaddedkey,toaddedpid);
            if (error!=0){    /// when insert return wrong, we use insertandsplit instead

                BTNonLeafNode newsibling;
                int newsiblingpid = pf.endPid();

                nonLeafNode.insertAndSplit(toaddedkey,toaddedpid,newsibling,addedkey);
                addedpid=newsiblingpid;

                newsibling.write(newsiblingpid,pf);
                nonLeafNode.write(curpid,pf);
            }


        }
        return 0;


    }


}



/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor)
{
    int curheight=1;   // if treeheight<1   error
    int curpid=rootPid;
    BTNonLeafNode nonleafNode;
    BTLeafNode leafNode;


    while (curheight!=treeHeight){
        nonleafNode.read(curpid,pf);
        nonleafNode.locateChildPtr(searchKey,curpid);

        curheight++;
    }

    leafNode.read(curpid,pf);
    cursor.pid=curpid;

    return leafNode.locate(searchKey,cursor.eid);


}



/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid)
{

    BTLeafNode leafnode;
    leafnode.read(cursor.pid,pf);
    leafnode.readEntry(cursor.eid,key,rid);

    cursor.eid++;
    if (cursor.eid>leafnode.getKeyCount() ){
        cursor.pid=leafnode.getNextNodePtr();
        cursor.eid=1;
    }



    return 0;
}


void BTreeIndex::print()
{
    BTLeafNode root;
    root.read(rootPid, pf);
    root.print();

//	if(treeHeight==1)
//	{
//		BTLeafNode root;
//		root.read(rootPid, pf);
//		root.print();
//	}
//	else if(treeHeight>1)
//	{
//		BTNonLeafNode root;
//		root.read(rootPid, pf);
//		root.print();
//
//		PageId first, rest;
//		memcpy(&first, root.buffer, sizeof(PageId));
//		BTLeafNode firstLeaf, leaf;
//		firstLeaf.read(first, pf);
//		firstLeaf.print();
//
//		//print the rest of the leaf nodes
//		for(int i=0; i<root.getKeyCount(); i++)
//		{
//			memcpy(&rest, root.buffer+12+(8*i), sizeof(PageId));
//			leaf.read(rest, pf);
//			leaf.print();
//		}
//
//		//print each leaf node's current pid and next pid
//		cout << "----------" << endl;
//
//		for(int i=0; i<root.getKeyCount(); i++)
//		{
//			if(i==0)
//				cout << "leaf0 (pid=" << first << ") has next pid: " << firstLeaf.getNextNodePtr() << endl;
//
//			BTLeafNode tempLeaf;
//			PageId tempPid;
//			memcpy(&tempPid, root.buffer+12+(8*i), sizeof(PageId));
//
//			tempLeaf.read(tempPid, pf);;
//
//			cout << "leaf" << i+1 << " (pid=" << tempPid << ") has next pid: " << tempLeaf.getNextNodePtr() << endl;
//		}
//	}

}
