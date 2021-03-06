/* *****************************************************************************
 * Copyright (c) 2013-2014 Qingtao Cao [harry.cao@nextdc.com]
 *
 * This file is part of oBIX.
 *
 * oBIX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * oBIX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with oBIX. If not, see <http://www.gnu.org/licenses/>.
 *
 * *****************************************************************************/

/*
 * An instrument to simulate how index files for history facilities
 * are read at oBIX server start-up
 *
 * Build below command:
 *
 *	$ gcc -g -Wall -Werror -I ../libs -I/usr/include/libxml2
 *		  -lxml2 -lobix-common
 *		  read_hist_index.c -o read_hist_index
 *
 * Run with following argument:
 *
 *	$ ./read_hist_index [path of oBIX histories facilities]
 *
 * If argument is omitted, then the program will fall back on the default
 * "/var/lib/obix/histories" folder.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "obix_utils.h"
#include "xml_utils.h"

static const char *XP_HIST_RECORDS = "./obj[@is='obix:HistoryFileAbstract']";
static const char *INDEX_FILENAME = "index";
static const char *INDEX_FILENAME_SUFFIX = ".xml";
static const char *OBIX_HISTORIES_DIR = "/var/lib/obix/histories";

static void test_cb(xmlNode *node, void *arg1, void *arg2)
{
	char *date, *count, *start, *end;
	char *path = (char *)arg1;	/* arg2 ignored */

	if (!node) {
		return;
	}

	date = count = start = end = NULL;

	if (!(date = xml_get_child_val(node, OBIX_OBJ_DATE, HIST_ABS_DATE)) ||
		!(count = xml_get_child_val(node, OBIX_OBJ_INT, HIST_ABS_COUNT)) ||
		!(start = xml_get_child_val(node, OBIX_OBJ_ABSTIME, HIST_ABS_START)) ||
		!(end = xml_get_child_val(node, OBIX_OBJ_ABSTIME, HIST_ABS_END))) {
		printf("Failed to get subnodes from a file abstract in the index file "
			   "of %s\n", path);
	}

	if (date) {
		free(date);
	}

	if (count) {
		free(count);
	}

	if (start) {
		free(start);
	}

	if (end) {
		free(end);
	}
}

static int read_index(const char *parent_dir, const char *dir, void *arg)
{
	struct stat statbuf;
	xmlDoc *doc = NULL, *new = NULL;
	xmlNode *root, *new_root;
	char *path;
	int ret = -1;

	if (link_pathname(&path, parent_dir, dir, INDEX_FILENAME,
					  INDEX_FILENAME_SUFFIX) < 0) {
		printf("Failed to assemble path name for the index file under "
			   "the %s folder\n", dir);
		return -1;
	}

	printf("Index file to read: %s\n", path);

	if (lstat(path, &statbuf) < 0 || S_ISREG(statbuf.st_mode) == 0 ||
		statbuf.st_size == 0) {
		printf("%s is not a valid index file\n", path);
		goto failed;
	}

	if (!(doc = xmlReadFile(path, NULL, XML_PARSE_OPTIONS_COMMON))) {
		printf("Failed to setup XML DOM tree from %s\n", path);
		goto failed;
	}

	if (!(root = xmlDocGetRootElement(doc))) {
		printf("Failed to get the root element from %s\n", path);
		goto failed;
	}

	if (!(new = xmlNewDoc(BAD_CAST XML_VERSION))) {
		printf("Failed to create a new XML document\n");
		goto failed2;
	}

	/*
	 * Re-parent the root node of the parsed document to the new document
	 */
	xmlDocSetRootElement(new, root);

	/*
	 * Have the new document manipulate the same dictionary used by the
	 * parsed document now that its root subtree has been reparented to it
	 *
	 * NOTE: the reference count on the dictionary should be bumped
	 */
	new->dict = doc->dict;
	xmlDictReference(new->dict);

	new_root = xmlDocGetRootElement(new);

	xml_xpath_for_each_item(new_root, XP_HIST_RECORDS, test_cb, path, NULL);
	ret = 0;

	/* Fall through */

failed2:
	if (new) {
		/*
		 * !! Crashes if the original document is created by xmlReadFile !!
		 */
		xmlFreeDoc(new);
	}

failed:
	free(path);
	xmlFreeDoc(doc);
	return ret;
}

int main(int argc, char *argv[])
{
	const char *histories;

	if (argc == 1) {
		histories = OBIX_HISTORIES_DIR;
	} else if (argc == 2) {
		histories = argv[1];
	} else {
		printf("Usage: ./%s [path of oBIX histories facilities]\n", argv[0]);
		return -1;
	}

	return for_each_file_name(histories, NULL, NULL, read_index, NULL);
}
