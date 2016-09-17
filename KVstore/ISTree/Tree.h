/*=============================================================================
# Filename: Tree.h
# Author: syzz
# Mail: 1181955272@qq.com
# Last Modified: 2015-04-26 16:44
# Description: struct and interface of the B+ tree
=============================================================================*/

#ifndef _KVSTORE_ISTREE_TREE_H
#define _KVSTORE_ISTREE_TREE_H

#include "../../Util/Util.h"
#include "../../Util/Stream.h"
#include "node/Node.h"
#include "node/IntlNode.h"
#include "node/LeafNode.h"
#include "storage/Storage.h"

class ISTree
{					
protected:
	unsigned int height;		//0 indicates an empty tree
	ISNode* root;
	ISNode* leaves_head;			//the head of LeafNode-list
	ISNode* leaves_tail;			//the tail of LeafNode-list
	std::string mode;           //BETTER(to use enum)
	ISStorage* TSM;           	//Tree-Storage-Manage
	//BETTER:multiple stream maybe needed:)
	Stream* stream;

	//always alloc one more byte than length, then user can add a '\0'
	//to get a real string, instead of new and copy
	//other operations will be harmful to search, so store value in
	//transfer temporally, while length adjusted.
	//TODO: in multi-user case, multiple-search will cause problem,
	//so lock is a must. Add lock to transfer is better than to add 
	//lock to every key/value. However, modify requires a lock for a
	//key/value, and multiple search for different keys are ok!!!
	Bstr transfer[3];	//0:transfer value searched; 1:copy key-data from const char*; 2:copy val-data from const char*
	unsigned transfer_size[3];

	//tree's operations should be atom(if read nodes)
	//sum the request and send to ISStorage at last
	//ensure that all nodes operated are in memory
	long long request;
	void prepare(ISNode* _np);

	std::string storepath;
	std::string filename;      	//ok for user to change
	/* some private functions */
	std::string getFilePath();	//in UNIX system
	void CopyToTransfer(const char* _str, unsigned _len, unsigned _index);
	void release(ISNode* _np) const;

public:
	ISTree();				//always need to initial transfer
	ISTree(std::string _storepath, std::string _filename, std::string _mode);
	unsigned int getHeight() const;
	void setHeight(unsigned _h);
	ISNode* getRoot() const;
	//void setRoot(Node* _root);
	//insert, search, remove, set
	bool search(int _key, char*& _str, int& _len);
	bool insert(int _key, const char* _str, unsigned _len);
	bool modify(int _key, const char* _str, unsigned _len);
	ISNode* find(int _key, int* store, bool ifmodify);
	bool remove(int _key);
	const Bstr* getRangeValue();
	void resetStream();
	bool range_query(int  _key1, int _key2);
	bool save(); 			
	~ISTree();
	void print(std::string s);			//DEBUG(print the tree)
};
//NOTICE: need to save tree manually before delete, otherwise will cause problem. 
//(problem range between two extremes: not-modified, totally-modified)
//After saved, it's ok to continue operations on tree!

#endif

