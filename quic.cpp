#include <iostream>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "list.h"
#define PATH_LENGTH_LIMIT 256
using namespace std;
extern int errno;
time_t rawtime;
int v, d, l;
int recursive_copy(char* origin_dir, char* dest_dir, unsigned long long &total_bytes_copied, unsigned int &total_files_directories, unsigned int &entities_copied, char* original_origin_dir, char* original_dest_dir, List &list1);
int recursive_copy_soft_links(char* origin_dir, char* dest_dir, unsigned long long &total_bytes_copied, unsigned int &total_files_directories, unsigned int &entities_copied, char* original_origin_dir, char* original_dest_dir);
int recursive_delete(char* origin_dir, char* dest_dir, unsigned int &entities_deleted, char* original_origin_dir, char* original_dest_dir);

int main(int argc, char const *argv[]) {
	time(&rawtime); // Compare current time to date-time of folders, to avoid a possible infinite loop
	timeval tv1, tv2;
	gettimeofday(&tv1, NULL);
	char *origin_dir = NULL, *dest_dir = NULL;
	v = 0; d = 0; l = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'v' && argv[i][2] == '\0')
				v = 1;
			else if (argv[i][1] == 'd' && argv[i][2] == '\0')
				d = 1;
			else if (argv[i][1] == 'l' && argv[i][2] == '\0')
				l = 1;
			else {
				cout << "Unexpected argument, terminating." << endl;
				return 1;
			}
		}
		else if (origin_dir == NULL) {
			origin_dir = new char[strlen(argv[i])+2];
			strcpy(origin_dir, argv[i]);
		}
		else if (dest_dir == NULL) {
			dest_dir = new char[strlen(argv[i])+2];
			strcpy(dest_dir, argv[i]);
		}
		else {
			cout << "Unexpected argument, terminating." << endl;
			return 1;
		}
	}
	if (origin_dir[strlen(origin_dir)-1] != '/')
		strcat(origin_dir, "/");
	if (dest_dir[strlen(dest_dir)-1] != '/')
		strcat(dest_dir, "/");
	cout << "Origin: " << origin_dir << endl;
	cout << "Destination: " << dest_dir << endl;
	struct stat temp_attr;
	if (stat(dest_dir, &temp_attr) != 0) {
		cout << "Destination directory doesn't exist, creating now." << endl;
		if (mkdir(dest_dir, 0755) < 0) {
			perror("mkdir main");
			exit(2);
		}
	}
	unsigned long long total_bytes_copied = 0;
	unsigned int total_files_directories = 0;
	unsigned int entities_copied = 0, entities_deleted = 0;
	List list1; // List keeps the inode number and the first path encountered, of files that have at least one hard link
	if (v) cout << "  -  COPY  -" << endl;
	recursive_copy(origin_dir, dest_dir, total_bytes_copied, total_files_directories, entities_copied, origin_dir, dest_dir, list1);
	if (l > 0) {
		if (v) cout << "  -  LINKS  -" << endl;
		recursive_copy_soft_links(origin_dir, dest_dir, total_bytes_copied, total_files_directories, entities_copied, origin_dir, dest_dir);
	}
	if (d > 0) {
		if (v) cout << "  -  DELETE  -" << endl;
		recursive_delete(origin_dir, dest_dir, entities_deleted, origin_dir, dest_dir);
	}
	gettimeofday(&tv2, NULL);
	double sec_difference = (double)tv2.tv_sec - (double)tv1.tv_sec;
	double usec_difference = (double)tv2.tv_usec - (double)tv1.tv_usec;
	double time_to_finish;
	if (usec_difference > 0)
		time_to_finish = sec_difference + usec_difference / 1000000;
	else
		time_to_finish = sec_difference - 1 - usec_difference / 1000000;
	cout << "There are " << total_files_directories << " files/directories in the hierarchy" << endl;
	cout << "Number of entities copied is " << entities_copied << endl;
	cout << "Number of entities deleted is " << entities_deleted << endl;
	if (time_to_finish > 0)
		cout << "Copied " << total_bytes_copied << " bytes in " << time_to_finish << " seconds at " << total_bytes_copied / time_to_finish << " bytes/sec" << endl;
	else
		cout << "Copied " << total_bytes_copied << " bytes in " << time_to_finish << " seconds" << endl;
	delete[] origin_dir;
	delete[] dest_dir;
	return 0;
}

int recursive_copy(char* origin_dir, char* dest_dir, unsigned long long &total_bytes_copied, unsigned int &total_files_directories, unsigned int &entities_copied, char* original_origin_dir, char* original_dest_dir, List &list1) {
	DIR *origin_dirp;
	FILE *origin_fptr, *dest_fptr;
	char c;
	struct dirent *origin_entry;
	struct stat origin_attr, dest_attr;
	char *origin_file_path, *dest_file_path;
	origin_file_path = new char[PATH_LENGTH_LIMIT];
	dest_file_path = new char[PATH_LENGTH_LIMIT];
	int stat_return;
	origin_dirp = opendir(origin_dir);
	if (origin_dirp == NULL) {
		perror("opendir origin directory");
		exit(1);
	}
	while ((origin_entry = readdir(origin_dirp)) != NULL) { // Read all entities in the current directory
		if (origin_entry->d_type == DT_REG && strcmp(origin_entry->d_name, ".") != 0 && strcmp(origin_entry->d_name, "..") != 0) { // If the origin_entry is a directory and not . or ..
			total_files_directories++;
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, origin_entry->d_name);
			if (v) cout << origin_file_path << endl;
			stat_return = stat(origin_file_path, &origin_attr);
			if (stat_return < 0) {
				perror("stat origin file");
				exit(2);
			}
			if (origin_attr.st_nlink > 1) { // If this file has more than 1 hard links
				if (l) { // If we're supposed to copy links, make a hard link in the destination, don't copy the file.
					if (list1.already_in_list(origin_attr.st_ino)) { // If the file has been copied already, make a hard link, don't copy it again
						strcpy(dest_file_path, dest_dir);
						strcat(dest_file_path, origin_entry->d_name);
						// If the file we're about to create exists, we check how many hard links it has. If it's 1, it means it was created at a previous execution of the program
						// ...without the -l argument, so we have to delete it and make a hard link in its place.
						stat_return = stat(dest_file_path, &dest_attr);
						if (stat_return == 0 && dest_attr.st_nlink == 1) {
							if (v) cout << "File already exists in destination, replacing with a hard link..." << endl;
							if (remove(dest_file_path) != 0) {
								perror("REMOVE ERROR");
								exit(3);
							}
						}
						else if (stat_return < 0 && errno != ENOENT) {
							perror("stat hard link");
							exit(4);
						}
						char* hard_link_target = new char[PATH_LENGTH_LIMIT];
						char* bufer = list1.get_path(origin_attr.st_ino);
						strcpy(hard_link_target, bufer);
						// If the link inside the origin points inside the origin, the link in destination should point inside the destination.
						if (strncmp(original_origin_dir, hard_link_target, strlen(original_origin_dir)) == 0) { // Search for the original_origin_dir in the link target path
							// If it exists, replace it with original_dest_dir
							char temp[PATH_LENGTH_LIMIT];
							memset(temp, '\0', PATH_LENGTH_LIMIT);
							strcpy(temp, original_dest_dir);
							strcat(temp, hard_link_target + strlen(original_origin_dir));
							strcpy(hard_link_target, temp);
						}
						stat_return = stat(dest_file_path, &dest_attr);
						if (stat_return == 0) {
							if (v) cout << "Hard link already exists" << endl;
							delete[] hard_link_target;
							continue;
						}
						else if (stat_return < 0 && errno != ENOENT) {
							perror("stat");
							exit(4);
						}
						if (link(hard_link_target, dest_file_path) != 0)
							perror("link");
						else
							entities_copied++;
						delete[] hard_link_target;
						continue;
					}
					else { // If the file has not been copied before, just copy it normally, and add it to the list, so when we find another hard link to it, we know to make a link in the destination and not copy the file.
						list1.insert(origin_attr.st_ino, origin_file_path);
					}
				}
			}
			size_t origin_size = origin_attr.st_size; // I'm getting the origin size here because I need it in both cases, whether files are the same or not.
			strcpy(dest_file_path, dest_dir);
			strcat(dest_file_path, origin_entry->d_name); // Look for the stats of the same file in dest_dir
			stat_return = stat(dest_file_path, &dest_attr); // 0=success, -1=failure
			if (stat_return == 0) { // If file exists in dest directory, check if it's the same
				size_t dest_size = dest_attr.st_size;
				time_t dest_time = dest_attr.st_mtime;
				time_t origin_time = origin_attr.st_mtime;
				if (origin_size == dest_size && origin_time <= dest_time) { // If files are "the same", don't copy
					continue;
				}
			}
			else if (stat_return < 0 && errno != ENOENT) {
				perror("stat");
				exit(4);
			}
			origin_fptr = fopen(origin_file_path, "r");
			if (origin_fptr == NULL) {
				perror("fopen file copy origin");
				exit(2);
			}
			dest_fptr = fopen(dest_file_path, "w");
			if (dest_fptr == NULL) {
				perror("fopen file copy destination");
				exit(2);
			}
			for (int i = 0; i < (int) origin_size; ++i) { // I copy <origin size> bytes and don't check for EOF because I was having problems with some files in Greek.
				c = fgetc(origin_fptr);
				fputc(c, dest_fptr);
				total_bytes_copied++;
			}
			entities_copied++;
			fclose(origin_fptr);
			fclose(dest_fptr);
		}
		else if (origin_entry->d_type == DT_DIR && strcmp(origin_entry->d_name, ".") != 0 && strcmp(origin_entry->d_name, "..") != 0) { // If the origin_entry is a directory and not . or ..
			// If the folder we're about to copy was modified after the program started, don't copy it.
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, origin_entry->d_name);
			if (stat(origin_file_path, &origin_attr) < 0) {
				perror("stat origin folder");
				exit(4);
			}
			time_t dest_time = origin_attr.st_mtime;
			if (dest_time + 2 > rawtime) // This is to prevent an infinite loop, for example if the destination folder is in the origin folder
				continue;
			total_files_directories++;
			// If folder doesn't exist in the destination, create it
			strcpy(dest_file_path, dest_dir);
			strcat(dest_file_path, origin_entry->d_name);
			if (v) cout << origin_file_path << "/" << endl;
			stat_return = stat(dest_file_path, &dest_attr);
			if (stat_return < 0 && errno == ENOENT) {
				if (v) cout << "Created directory " << dest_file_path << endl;
				mkdir(dest_file_path, 0755);
				entities_copied++;
			}
			else if (stat_return < 0 && errno != ENOENT) {
				perror("stat destination folder");
				exit(4);
			}
			// Call recursion on this folder
			char* recursion_origin_dir = new char[strlen(origin_dir)+strlen(origin_entry->d_name)+2];
			strcpy(recursion_origin_dir, origin_dir);
			strcat(recursion_origin_dir, origin_entry->d_name);
			strcat(recursion_origin_dir, "/");
			char* recursion_dest_dir = new char[strlen(dest_dir)+strlen(origin_entry->d_name)+2];
			strcpy(recursion_dest_dir, dest_dir);
			strcat(recursion_dest_dir, origin_entry->d_name);
			strcat(recursion_dest_dir, "/");
			recursive_copy(recursion_origin_dir, recursion_dest_dir, total_bytes_copied, total_files_directories, entities_copied, original_origin_dir, original_dest_dir, list1);
			delete[] recursion_origin_dir;
			delete[] recursion_dest_dir;
		}
	}
	delete[] origin_file_path;
	delete[] dest_file_path;
	closedir(origin_dirp);
	return 0;
}

int recursive_copy_soft_links(char* origin_dir, char* dest_dir, unsigned long long &total_bytes_copied, unsigned int &total_files_directories, unsigned int &entities_copied, char* original_origin_dir, char* original_dest_dir) {
	DIR *origin_dirp;
	struct dirent *origin_entry;
	struct stat origin_attr, dest_attr;
	char *origin_file_path, *dest_file_path;
	origin_file_path = new char[PATH_LENGTH_LIMIT];
	dest_file_path = new char[PATH_LENGTH_LIMIT];
	int stat_return;
	char* soft_link_target;
	origin_dirp = opendir(origin_dir);
	if (origin_dirp == NULL) {
		perror("opendir soft links");
		exit(1);
	}
	while ((origin_entry = readdir(origin_dirp)) != NULL) {
		if (origin_entry->d_type == DT_DIR && strcmp(origin_entry->d_name, ".") != 0 && strcmp(origin_entry->d_name, "..") != 0) { // If the origin_entry is a directory and not . or ..
			// If the folder we're about to copy was modified after the program started, don't copy it.
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, origin_entry->d_name);
			stat_return = stat(origin_file_path, &origin_attr);
			if (stat_return < 0) {
				perror("stat origin folder");
				exit(2);
			}
			time_t dest_time = origin_attr.st_mtime;
			if (dest_time + 2 > rawtime) // This is to prevent an infinite loop, for example if the destination folder is in the origin folder
				continue;
			// If folder doesn't exist in the destination, create it
			strcpy(dest_file_path, dest_dir);
			strcat(dest_file_path, origin_entry->d_name);
			if (v) cout << origin_file_path + strlen(original_origin_dir) << "/" << endl;
			stat_return = stat(dest_file_path, &dest_attr);
			if (stat_return < 0 && errno == ENOENT) {
				if (v) cout << "Created directory " << dest_file_path << endl;
				mkdir(dest_file_path, 0755);
				entities_copied++;
			}
			else if (stat_return < 0 && errno != ENOENT) {
				perror("stat destination folder");
				exit(2);
			}
			// Call recursion on this folder
			char* recursion_origin_dir = new char[strlen(origin_dir)+strlen(origin_entry->d_name)+2];
			strcpy(recursion_origin_dir, origin_dir);
			strcat(recursion_origin_dir, origin_entry->d_name);
			strcat(recursion_origin_dir, "/");
			char* recursion_dest_dir = new char[strlen(dest_dir)+strlen(origin_entry->d_name)+2];
			strcpy(recursion_dest_dir, dest_dir);
			strcat(recursion_dest_dir, origin_entry->d_name);
			strcat(recursion_dest_dir, "/");
			recursive_copy_soft_links(recursion_origin_dir, recursion_dest_dir, total_bytes_copied, total_files_directories, entities_copied, original_origin_dir, original_dest_dir);
			delete[] recursion_origin_dir;
			delete[] recursion_dest_dir;
		}
		else if (origin_entry->d_type == DT_LNK && strcmp(origin_entry->d_name, ".") != 0 && strcmp(origin_entry->d_name, "..") != 0) {
			total_files_directories++; // In the "copy shortcuts" function we only count the links as entities, because folders have already been counted in the regular "copy" function
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, origin_entry->d_name);
			if (v) cout << origin_file_path << endl;
			soft_link_target = new char[PATH_LENGTH_LIMIT];
			memset(soft_link_target, '\0', PATH_LENGTH_LIMIT);
			readlink(origin_file_path, soft_link_target, PATH_LENGTH_LIMIT);
			// cout << "original: " << soft_link_target << endl;
			char* absolute_soft_link_target = realpath(soft_link_target, NULL);
			if (absolute_soft_link_target == NULL) {
				perror("realpath cannot resolve link");
				delete[] soft_link_target;
				continue;
			}
			// cout << "absolute: " << absolute_soft_link_target << endl;
			// If the link inside the origin points inside the origin, the link in destination should point inside the destination.
			if (strncmp(original_origin_dir, absolute_soft_link_target, strlen(original_origin_dir)) == 0) { // Search for the original_origin_dir in the link target path
				// If it exists, replace it with original_dest_dir
				char temp[PATH_LENGTH_LIMIT];
				memset(temp, '\0', PATH_LENGTH_LIMIT);
				strcpy(temp, original_dest_dir);
				strcat(temp, absolute_soft_link_target + strlen(original_origin_dir));
				strcpy(absolute_soft_link_target, temp);
			}
			char existing_soft_link_target[PATH_LENGTH_LIMIT];
			memset(existing_soft_link_target, '\0', PATH_LENGTH_LIMIT);
			strcpy(dest_file_path, dest_dir);
			strcat(dest_file_path, origin_entry->d_name);
			stat_return = stat(dest_file_path, &origin_attr);
			if (stat_return == 0) { // If link exists in destination and points to the correct place, don't copy it again
				readlink(dest_file_path, existing_soft_link_target, PATH_LENGTH_LIMIT);
				if (strcmp(existing_soft_link_target, absolute_soft_link_target) == 0) {
					if (v) cout << "Soft link already exists in destination" << endl;
					free(absolute_soft_link_target);
					delete[] soft_link_target;
					continue;
				}
			}
			else if (stat_return < 0 && errno != ENOENT) {
				perror("stat soft link");
				exit(2);
			}
			if (symlink(absolute_soft_link_target, dest_file_path) != 0)
				perror("symlink");
			else
				entities_copied++;
			free(absolute_soft_link_target);
			delete[] soft_link_target;
		}
	}
	delete[] origin_file_path;
	delete[] dest_file_path;
	closedir(origin_dirp);
	return 0;
}

int recursive_delete(char* origin_dir, char* dest_dir, unsigned int &entities_deleted, char* original_origin_dir, char* original_dest_dir) {
	DIR *origin_dirp, *dest_dirp;
	struct dirent *dest_entry;
	char origin_file_path[PATH_LENGTH_LIMIT], dest_file_path[PATH_LENGTH_LIMIT];
	dest_dirp = opendir(dest_dir);
	if (dest_dirp == NULL) {
		cout << "Could not open specified directory. Terminating." << endl;
		exit(1);
	}
	while ((dest_entry = readdir(dest_dirp)) != NULL) {
		if ((dest_entry->d_type == DT_REG || dest_entry->d_type == DT_LNK) && strcmp(dest_entry->d_name, ".") != 0 && strcmp(dest_entry->d_name, "..") != 0) { // If the dest_entry is a directory and not . or ..
			// If file doesn't exist in the origin directory, delete it from destination
			// if (v) cout << dest_dir << dest_entry->d_name << endl;
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, dest_entry->d_name);
			if (access(origin_file_path, F_OK) != 0) { // If file doesn't exist in origin
				if (v) cout << "File: " << origin_file_path << " doesn't exist in origin, deleting..." << endl;
				entities_deleted++;
				// Delete file in destination
				strcpy(dest_file_path, dest_dir);
				strcat(dest_file_path, dest_entry->d_name);
				if (remove(dest_file_path) != 0) {
					perror("remove");
					exit(3);
				}
			}
		}
		else if (dest_entry->d_type == DT_DIR && strcmp(dest_entry->d_name, ".") != 0 && strcmp(dest_entry->d_name, "..") != 0) { // If the origin_entry is a directory and not . or ..
			// if (v) cout << dest_dir << dest_entry->d_name << "/" << endl;
			// If folder doesn't exist in the origin directory, call recursion on it and then delete it from destination
			char* recursion_origin_dir = new char[strlen(origin_dir)+strlen(dest_entry->d_name)+5];
			strcpy(recursion_origin_dir, origin_dir);
			strcat(recursion_origin_dir, dest_entry->d_name);
			strcat(recursion_origin_dir, "/");
			char* recursion_dest_dir = new char[strlen(dest_dir)+strlen(dest_entry->d_name)+5];
			strcpy(recursion_dest_dir, dest_dir);
			strcat(recursion_dest_dir, dest_entry->d_name);
			strcat(recursion_dest_dir, "/");
			recursive_delete(recursion_origin_dir, recursion_dest_dir, entities_deleted, original_origin_dir, original_dest_dir);
			delete[] recursion_origin_dir;
			delete[] recursion_dest_dir;
			// If directory doesn't exist in origin
			strcpy(origin_file_path, origin_dir);
			strcat(origin_file_path, dest_entry->d_name);
			origin_dirp = opendir(origin_file_path);
			if (origin_dirp == NULL && errno == ENOENT) { // If this directory doesn't exist in origin
				if (v) cout << "Directory: " << origin_file_path << " doesn't exist in origin, deleting..." << endl;
				entities_deleted++;
				// Delete directory
				strcpy(dest_file_path, dest_dir);
				strcat(dest_file_path, dest_entry->d_name);
				if (rmdir(dest_file_path) != 0) {
					perror("rmdir");
					exit(3);
				}
			}
			if (origin_dirp != NULL)
				closedir(origin_dirp);
		}
	}
	closedir(dest_dirp);
	return 0;
}