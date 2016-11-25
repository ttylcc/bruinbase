/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include "Bruinbase.h"
#include "SqlEngine.h"
#include "BTreeIndex.h"

using namespace std;

// external functions and variables for load file and sql command parsing 
extern FILE* sqlin;
int sqlparse(void);


RC SqlEngine::run(FILE* commandline)
{
  fprintf(stdout, "Bruinbase> ");

    
  // set the command line input and start parsing user input
  sqlin = commandline;
  sqlparse();  // sqlparse() is defined in SqlParser.tab.c generated from
               // SqlParser.y by bison (bison is GNU equivalent of yacc)

  return 0;
}

//RC SqlEngine::select(int attr, const string& table, const vector<SelCond>& cond)
//{
//  RecordFile rf;   // RecordFile containing the table
//  RecordId   rid;  // record cursor for table scanning
//
//  RC     rc;
//  int    key;
//  string value;
//  int    count;
//  int    diff;
//
//  // open the table file
//  if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
//    fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
//    return rc;
//  }
//
//  // scan the table file from the beginning
//  rid.pid = rid.sid = 0;
//  count = 0;
//  while (rid < rf.endRid()) {
//    // read the tuple
//    if ((rc = rf.read(rid, key, value)) < 0) {
//      fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
//      goto exit_select;
//    }
//
//    // check the conditions on the tuple
//    for (unsigned i = 0; i < cond.size(); i++) {
//      // compute the difference between the tuple value and the condition value
//      switch (cond[i].attr) {
//      case 1:
//	diff = key - atoi(cond[i].value);
//	break;
//      case 2:
//	diff = strcmp(value.c_str(), cond[i].value);
//	break;
//      }
//
//      // skip the tuple if any condition is not met
//      switch (cond[i].comp) {
//      case SelCond::EQ:
//	if (diff != 0) goto next_tuple;
//	break;
//      case SelCond::NE:
//	if (diff == 0) goto next_tuple;
//	break;
//      case SelCond::GT:
//	if (diff <= 0) goto next_tuple;
//	break;
//      case SelCond::LT:
//	if (diff >= 0) goto next_tuple;
//	break;
//      case SelCond::GE:
//	if (diff < 0) goto next_tuple;
//	break;
//      case SelCond::LE:
//	if (diff > 0) goto next_tuple;
//	break;
//      }
//    }
//
//    // the condition is met for the tuple.
//    // increase matching tuple counter
//    count++;
//
//    // print the tuple
//    switch (attr) {
//    case 1:  // SELECT key
//      fprintf(stdout, "%d\n", key);
//      break;
//    case 2:  // SELECT value
//      fprintf(stdout, "%s\n", value.c_str());
//      break;
//    case 3:  // SELECT *
//      fprintf(stdout, "%d '%s'\n", key, value.c_str());
//      break;
//    }
//
//    // move to the next tuple
//    next_tuple:
//    ++rid;
//  }
//
//  // print matching tuple count if "select count(*)"
//  if (attr == 4) {
//    fprintf(stdout, "%d\n", count);
//  }
//  rc = 0;
//
//  // close the table file and return
//  exit_select:
//  rf.close();
//  return rc;
//}


RC SqlEngine::select(int attr, const string& table, const vector<SelCond>& cond)
{
    RecordFile rf;   // RecordFile containing the table
    RecordId   rid;  // record cursor for table scanning

    BTreeIndex tree;
    int errortree = tree.open(table + ".idx", 'r');
//    cout<< "errortree"<<errortree<<endl;
//    cout<< "treeHeight"<<tree.treeHeight<<endl;

    IndexCursor cursor;


    RC     rc;
    int    key;
    string value;
    int    count;
    int    diff;

    // open the table file
    if ((rc = rf.open(table + ".tbl", 'r')) < 0) {
        fprintf(stderr, "Error: table %s does not exist\n", table.c_str());
        return rc;
    }

    //scan the table by b+index tree

    int min=-1;
    int max=-1;
    bool couldminequal=false;
    bool couldmaxequal=false;
    bool useBindextree=false;
    bool needread = false;
    if (attr==2 || attr==3){
        needread=true;
    }
    else{
        if (cond.size()==0){
            useBindextree=true;
        }
    }

    for (unsigned i = 0; i < cond.size(); i++) {

        if (cond[i].attr==2) {
            needread=true;
            continue;
        }
        int tmpvalue = atoi(cond[i].value);

        if (cond[i].comp==SelCond::NE){
            continue;
        }
        if (cond[i].comp==SelCond::EQ){
            if (min==-1 || tmpvalue>min ){
                min=tmpvalue;
                couldminequal=true;
                max=tmpvalue;
                couldmaxequal=true;
            }

        }
        if (cond[i].comp==SelCond::GT){
            if (min==-1 || tmpvalue>min){
                min=tmpvalue;
                couldminequal=false;
            }

        }
        if (cond[i].comp==SelCond::GE){
            if (min==-1 || tmpvalue>min){
                min=tmpvalue;
                couldminequal=true;
            }
        }
        if (cond[i].comp==SelCond::LT){
            if (max==-1 || tmpvalue<max ){
                max=tmpvalue;
                couldmaxequal=false;
            }
        }
        if (cond[i].comp==SelCond::LE){
            if (max==-1 || tmpvalue<max){
                max=tmpvalue;
                couldmaxequal=true;
            }
        }
        useBindextree=true;

    }


//    cout<<"min"<<min<<endl;
//    cout<<"max"<<max<<endl;

  //  useBindextree=false;
    if (errortree==0 && useBindextree){
        cout<< "using Bindex tree now"<<endl;
        if (couldminequal){
            tree.locate(min,cursor);
        }
        else{
            tree.locate(min+1,cursor);
        }
        int tmpcount=0;
        count = 0;
        while(tree.readForward(cursor, key, rid)==0){

//            cout<< "cursor.pid"<<cursor.pid<<endl;
//            cout<< "cursor.eid"<<cursor.eid<<endl;
           // cout<< "tmp count"<<tmpcount<<endl;

            if (max!=-1 && ((key>max && couldmaxequal) ||(key>=max && !couldmaxequal))){
                break;

            }
            if (needread){
                rc = rf.read(rid, key, value);
                if (rc<0) {
                    fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
                    continue;
                }
            }


            for (unsigned i = 0; i < cond.size(); i++) {
                // compute the difference between the tuple value and the condition value
                switch (cond[i].attr) {
                    case 1:
                        diff = key - atoi(cond[i].value);
                        break;
                    case 2:
                        diff = strcmp(value.c_str(), cond[i].value);
                        break;
                }

                // skip the tuple if any condition is not met
                switch (cond[i].comp) {
                    case SelCond::EQ:
                        if (diff != 0) goto next_key;
                        break;
                    case SelCond::NE:
                        if (diff == 0) goto next_key;
                        break;
                    case SelCond::GT:
                        if (diff <= 0) goto next_key;
                        break;
                    case SelCond::LT:
                        if (diff >= 0) goto next_key;
                        break;
                    case SelCond::GE:
                        if (diff < 0) goto next_key;
                        break;
                    case SelCond::LE:
                        if (diff > 0) goto next_key;
                        break;
                }
            }
            count++;
            // print the tuple
            switch (attr) {
                case 1:  // SELECT key
                    fprintf(stdout, "%d\n", key);
                    break;
                case 2:  // SELECT value
                    fprintf(stdout, "%s\n", value.c_str());
                    break;
                case 3:  // SELECT *
                    fprintf(stdout, "%d '%s'\n", key, value.c_str());
                    break;
            }


            next_key:
        tmpcount++;
            if (cursor.pid==0) break;
        }


        if (attr == 4) {
            fprintf(stdout, "%d\n", count);
        }
        rc = 0;

    }
    else{
        cout<< "full scan the recode file "<<endl;
        // scan the table file from the beginning
        rid.pid = rid.sid = 0;
        count = 0;
        while (rid < rf.endRid()) {
            // read the tuple
            if ((rc = rf.read(rid, key, value)) < 0) {
                fprintf(stderr, "Error: while reading a tuple from table %s\n", table.c_str());
                goto exit_select;
            }

            // check the conditions on the tuple
            for (unsigned i = 0; i < cond.size(); i++) {
                // compute the difference between the tuple value and the condition value
                switch (cond[i].attr) {
                    case 1:
                        diff = key - atoi(cond[i].value);
                        break;
                    case 2:
                        diff = strcmp(value.c_str(), cond[i].value);
                        break;
                }

                // skip the tuple if any condition is not met
                switch (cond[i].comp) {
                    case SelCond::EQ:
                        if (diff != 0) goto next_tuple;
                        break;
                    case SelCond::NE:
                        if (diff == 0) goto next_tuple;
                        break;
                    case SelCond::GT:
                        if (diff <= 0) goto next_tuple;
                        break;
                    case SelCond::LT:
                        if (diff >= 0) goto next_tuple;
                        break;
                    case SelCond::GE:
                        if (diff < 0) goto next_tuple;
                        break;
                    case SelCond::LE:
                        if (diff > 0) goto next_tuple;
                        break;
                }
            }

            // the condition is met for the tuple.
            // increase matching tuple counter
            count++;

            // print the tuple
            switch (attr) {
                case 1:  // SELECT key
                    fprintf(stdout, "%d\n", key);
                    break;
                case 2:  // SELECT value
                    fprintf(stdout, "%s\n", value.c_str());
                    break;
                case 3:  // SELECT *
                    fprintf(stdout, "%d '%s'\n", key, value.c_str());
                    break;
            }

            // move to the next tuple
            next_tuple:
            ++rid;
        }

        // print matching tuple count if "select count(*)"
        if (attr == 4) {
            fprintf(stdout, "%d\n", count);
        }


        rc = 0;

    }



    // close the table file and return
    exit_select:


    rf.close();
    return rc;
}

RC SqlEngine::load(const string& table, const string& loadfile, bool index)
{
  /* your code here */

    index=true;


    RC rc = 0;
    
    int key;
    string value;
    RecordId rid;
    BTreeIndex tree;
    
    
    string tablename = std::string(table)+".tbl";
    RecordFile rf;
    BTreeIndex Bindex;

    std::ifstream myfile(loadfile.c_str());
    rf.open(tablename.c_str(),'w');

    string line;

    tree.open(table + ".idx", 'w');

    int count=0;

    while ( getline (myfile,line) )
    {

        if ((rc= parseLoadLine(line,key,value))<0 ){
            fprintf(stderr, "error");
        }
        if ((rc=rf.append(key,value,rid))<0){
            fprintf(stderr, "error1" );
        }

        if (index){

            if ((tree.insert(key, rid))<0){
                return RC_FILE_WRITE_FAILED;
            }
            count++;
            cout<< "tuple number: "<< key <<endl;
        }
        
    }
    tree.print();
    tree.close();
    rf.close();
    myfile.close();
    

    

  return rc;
}

RC SqlEngine::parseLoadLine(const string& line, int& key, string& value)
{
    const char *s;
    char        c;
    string::size_type loc;
    
    // ignore beginning white spaces
    c = *(s = line.c_str());
    while (c == ' ' || c == '\t') { c = *++s; }

    // get the integer key value
    key = atoi(s);

    // look for comma
    s = strchr(s, ',');
    if (s == NULL) { return RC_INVALID_FILE_FORMAT; }

    // ignore white spaces
    do { c = *++s; } while (c == ' ' || c == '\t');
    
    // if there is nothing left, set the value to empty string
    if (c == 0) { 
        value.erase();
        return 0;
    }

    // is the value field delimited by ' or "?
    if (c == '\'' || c == '"') {
        s++;
    } else {
        c = '\n';
    }

    // get the value string
    value.assign(s);
    loc = value.find(c, 0);
    if (loc != string::npos) { value.erase(loc); }

    return 0;
}
