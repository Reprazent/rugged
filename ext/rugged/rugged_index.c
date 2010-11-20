/*
 * The MIT License
 *
 * Copyright (c) 2010 Scott Chacon
 * Copyright (c) 2010 Vicent Marti
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "rugged.h"

extern VALUE rb_cRugged;
VALUE rb_cRuggedIndex;
VALUE rb_cRuggedIndexEntry;

/*
 * Index
 */
static VALUE rb_git_index_allocate(VALUE klass)
{
	git_index *index = NULL;
	return Data_Wrap_Struct(klass, NULL, NULL, index);
}

static VALUE rb_git_index_init(VALUE self, VALUE path)
{
	git_index *index;
	int error;

	error = git_index_open_bare(&index, RSTRING_PTR(path));
	rugged_exception_check(error);

	DATA_PTR(self) = index;
	return Qnil;
}

static VALUE rb_git_index_clear(VALUE self)
{
	git_index *index;
	Data_Get_Struct(self, git_index, index);
	git_index_clear(index);
	return Qnil;
}

static VALUE rb_git_index_read(VALUE self)
{
	git_index *index;
	int error;

	Data_Get_Struct(self, git_index, index);

	error = git_index_read(index);
	rugged_exception_check(error);

	return Qnil;
}

static VALUE rb_git_index_write(VALUE self)
{
	git_index *index;
	int error;

	Data_Get_Struct(self, git_index, index);

	error = git_index_write(index);
	rugged_exception_check(error);
		
	return Qnil;
}

static VALUE rb_git_indexentry_new(git_index_entry *entry)
{
	if (entry == NULL)
		return Qnil;

	return Data_Wrap_Struct(rb_cRuggedIndexEntry, NULL, NULL, entry);
}

static VALUE rb_git_index_get_entry_count(VALUE self)
{
	git_index *index;
	Data_Get_Struct(self, git_index, index);
	return INT2FIX(git_index_entrycount(index));
}

static VALUE rb_git_index_get(VALUE self, VALUE entry)
{
	git_index *index;
	Data_Get_Struct(self, git_index, index);

	if (TYPE(entry) == T_STRING)
		entry = INT2FIX(git_index_find(index, RSTRING_PTR(entry)));

	Check_Type(entry, T_FIXNUM);
	return rb_git_indexentry_new(git_index_get(index, FIX2INT(entry)));
}

static VALUE rb_git_index_remove(VALUE self, VALUE entry)
{
	git_index *index;
	int error;
	Data_Get_Struct(self, git_index, index);

	if (TYPE(entry) == T_STRING)
		entry = INT2FIX(git_index_find(index, RSTRING_PTR(entry)));

	Check_Type(entry, T_FIXNUM);
	
	error = git_index_remove(index, FIX2INT(entry));
	rugged_exception_check(error);

	return Qnil;
}

static VALUE rb_git_index_add(VALUE self, VALUE rb_entry)
{
	git_index *index;
	git_index_entry *entry;
	int error;

	Data_Get_Struct(self, git_index, index);

	if (rb_obj_is_kind_of(rb_entry, rb_cRuggedIndexEntry)) {
		Data_Get_Struct(rb_entry, git_index_entry, entry);
		error = git_index_insert(index, entry);
	} else if (TYPE(rb_entry) == T_STRING) {
		error = git_index_add(index, RSTRING_PTR(rb_entry), 0);
	} else {
		rb_raise(rb_eTypeError, 
			"index_entry must be an existing IndexEntry object or a path to a file in the repository");
	}

	rugged_exception_check(error);

	return Qnil;
}



/*
 * Index Entry
 */

static VALUE rb_git_indexentry_allocate(VALUE klass)
{
	git_index_entry *index_entry = malloc(sizeof(git_index_entry));
	memset(index_entry, 0x0, sizeof(git_index_entry));
	return Data_Wrap_Struct(klass, NULL, NULL, index_entry);
}

static VALUE rb_git_indexentry_init(VALUE self, VALUE path, VALUE working_dir)
{
	return Qnil;
}

#define RB_GIT_INDEXENTRY_GETSET(attr)\
static VALUE rb_git_indexentry_##attr##_GET(VALUE self) \
{\
	git_index_entry *entry;\
	Data_Get_Struct(self, git_index_entry, entry);\
	return INT2FIX(entry->attr);\
}\
static VALUE rb_git_indexentry_##attr##_SET(VALUE self, VALUE v) \
{\
	git_index_entry *entry;\
	Data_Get_Struct(self, git_index_entry, entry);\
	Check_Type(v, T_FIXNUM);\
	entry->attr = FIX2INT(v);\
	return Qnil;\
}

RB_GIT_INDEXENTRY_GETSET(dev);
RB_GIT_INDEXENTRY_GETSET(ino);
RB_GIT_INDEXENTRY_GETSET(mode);
RB_GIT_INDEXENTRY_GETSET(uid);
RB_GIT_INDEXENTRY_GETSET(gid);
RB_GIT_INDEXENTRY_GETSET(file_size);
RB_GIT_INDEXENTRY_GETSET(flags);
RB_GIT_INDEXENTRY_GETSET(flags_extended);

static VALUE rb_git_indexentry_path_GET(VALUE self)
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);

	if (entry->path == NULL)
		return Qnil;

	return rb_str_new2(entry->path);
}

static VALUE rb_git_indexentry_path_SET(VALUE self, VALUE val)
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);

	Check_Type(val, T_STRING);

	if (entry->path != NULL)
		free(entry->path);

	entry->path = strdup(RSTRING_PTR(val));
	return Qnil;
}

static VALUE rb_git_indexentry_oid_GET(VALUE self)
{
	git_index_entry *entry;
	char out[40];
	Data_Get_Struct(self, git_index_entry, entry);
	git_oid_fmt(out, &entry->oid);
	return rb_str_new(out, 40);
}

static VALUE rb_git_indexentry_oid_SET(VALUE self, VALUE v) 
{
	git_index_entry *entry;
	int error;

	Data_Get_Struct(self, git_index_entry, entry);
	Check_Type(v, T_STRING);

	error = git_oid_mkstr(&entry->oid, RSTRING_PTR(v));
	rugged_exception_check(error);

	return Qnil;
}

static VALUE rb_git_indexentry_mtime_GET(VALUE self)
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);
	return rb_time_new(entry->mtime.seconds, entry->mtime.nanoseconds / 1000);
}

static VALUE rb_git_indexentry_ctime_GET(VALUE self)
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);
	return rb_time_new(entry->ctime.seconds, entry->ctime.nanoseconds / 1000);
}

static VALUE rb_git_indexentry_mtime_SET(VALUE self, VALUE v) 
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);

	/* TODO: check for time */

	entry->mtime.seconds = NUM2LONG(rb_funcall(v, rb_intern("to_i"), 0));
	entry->mtime.nanoseconds = NUM2LONG(rb_funcall(v, rb_intern("usec"), 0)) * 1000;

	return Qnil;
}

static VALUE rb_git_indexentry_ctime_SET(VALUE self, VALUE v) 
{
	git_index_entry *entry;
	Data_Get_Struct(self, git_index_entry, entry);

	/* TODO: check for time */

	entry->ctime.seconds = NUM2LONG(rb_funcall(v, rb_intern("to_i"), 0));
	entry->ctime.nanoseconds = NUM2LONG(rb_funcall(v, rb_intern("usec"), 0)) * 1000;

	return Qnil;
}



void Init_rugged_index()
{
	/*
	 * Index 
	 */
	rb_cRuggedIndex = rb_define_class_under(rb_cRugged, "Index", rb_cObject);
	rb_define_alloc_func(rb_cRuggedIndex, rb_git_index_allocate);
	rb_define_method(rb_cRuggedIndex, "initialize", rb_git_index_init, 1);
	rb_define_method(rb_cRuggedIndex, "entry_count", rb_git_index_get_entry_count, 0);
	rb_define_method(rb_cRuggedIndex, "refresh", rb_git_index_read, 0);
	rb_define_method(rb_cRuggedIndex, "clear", rb_git_index_clear, 0);
	rb_define_method(rb_cRuggedIndex, "write", rb_git_index_write, 0);
	rb_define_method(rb_cRuggedIndex, "get_entry", rb_git_index_get, 1);
	rb_define_method(rb_cRuggedIndex, "[]", rb_git_index_get, 1);
	rb_define_method(rb_cRuggedIndex, "add", rb_git_index_add, 1);
	rb_define_method(rb_cRuggedIndex, "remove", rb_git_index_remove, 1);

	/*
	 * Index Entry
	 */
	rb_cRuggedIndexEntry = rb_define_class_under(rb_cRugged, "IndexEntry", rb_cObject);
	rb_define_alloc_func(rb_cRuggedIndexEntry, rb_git_indexentry_allocate);
	rb_define_method(rb_cRuggedIndexEntry, "initialize", rb_git_indexentry_init, 0);

	rb_define_method(rb_cRuggedIndexEntry, "path", rb_git_indexentry_path_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "path=", rb_git_indexentry_path_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "sha", rb_git_indexentry_oid_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "sha=", rb_git_indexentry_oid_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "mtime", rb_git_indexentry_mtime_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "mtime=", rb_git_indexentry_mtime_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "ctime", rb_git_indexentry_ctime_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "ctime=", rb_git_indexentry_ctime_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "dev", rb_git_indexentry_dev_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "dev=", rb_git_indexentry_dev_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "ino", rb_git_indexentry_ino_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "ino=", rb_git_indexentry_ino_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "mode", rb_git_indexentry_mode_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "mode=", rb_git_indexentry_mode_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "uid", rb_git_indexentry_uid_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "uid=", rb_git_indexentry_uid_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "gid", rb_git_indexentry_gid_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "gid=", rb_git_indexentry_gid_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "file_size", rb_git_indexentry_file_size_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "file_size=", rb_git_indexentry_file_size_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "flags", rb_git_indexentry_flags_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "flags=", rb_git_indexentry_flags_SET, 1);

	rb_define_method(rb_cRuggedIndexEntry, "flags_extended", rb_git_indexentry_flags_extended_GET, 0);
	rb_define_method(rb_cRuggedIndexEntry, "flags_extended=", rb_git_indexentry_flags_extended_SET, 1);

}
