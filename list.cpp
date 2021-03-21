#include "list.h"

List::List() {
	this->list_root = NULL;
}
int List::insert(int new_inode_id, char* new_inode_file_path) {
	if (this->list_root == NULL) {
		this->list_root = new ListNode;
		this->list_root->inode_id = new_inode_id;
		this->list_root->inode_file_path = new char[strlen(new_inode_file_path) + 1];
		strcpy(this->list_root->inode_file_path, new_inode_file_path);
		this->list_root->Next = NULL;
		return 0;
	}
	ListNode* temp_node = this->list_root;
	while (temp_node->Next != NULL) {
		temp_node = temp_node->Next;
	}
	ListNode* new_node = new ListNode;
	new_node->inode_id = new_inode_id;
	new_node->inode_file_path = new char[strlen(new_inode_file_path) + 1];
	strcpy(new_node->inode_file_path, new_inode_file_path);
	new_node->Next = temp_node->Next;
	temp_node->Next = new_node;
	return 0;
}
void List::print() {
	cout << "List print" << endl;
	ListNode* current_node = this->list_root;
	while (current_node != NULL) {
		cout << current_node->inode_id << " - " << current_node->inode_file_path << endl;
		current_node = current_node->Next;
	}
	cout << "List print end" << endl;
}
int List::already_in_list(int inode_id_to_find) {
	ListNode* current_node = this->list_root;
	while (current_node != NULL) {
		if (current_node->inode_id == inode_id_to_find)
			return 1;
		current_node = current_node->Next;
	}
	return 0;
}
char* List::get_path(int inode_id_to_find) {
	ListNode* current_node = this->list_root;
	while (current_node != NULL) {
		if (current_node->inode_id == inode_id_to_find)
			return current_node->inode_file_path;
		current_node = current_node->Next;
	}
	return NULL;
}
List::~List() {
	ListNode* current_node = this->list_root;
	while (current_node != NULL) {
		ListNode* to_delete = current_node;
		current_node = current_node->Next;
		delete[] to_delete->inode_file_path;
		delete to_delete;
	}
}