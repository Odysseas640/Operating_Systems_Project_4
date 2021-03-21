#ifndef __ODYS_LIST__
#define __ODYS_LIST__
#include <iostream>
#include <cstring>
using namespace std;

typedef struct list_node ListNode;
struct list_node {
	int inode_id;
	char* inode_file_path;
	ListNode* Next;
};

class List {
private:
	ListNode* list_root;
public:
	List();
	int insert(int new_inode_id, char* new_inode_file_path);
	void print();
	int already_in_list(int inode_id_to_find);
	char* get_path(int inode_id_to_find);
	~List();
};

#endif