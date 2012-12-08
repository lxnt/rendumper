#if !defined(STRINGVECST_H)
#define STRINGVECST_H

#include <string>
#include "svector.h"
#include "files.h"

class pstringst {
  public:
    std::string dat;
};

class stringvectst {
  public:
    svector<pstringst *> str;

    void add_string(const string &st) {
	pstringst *newp = new pstringst;
	newp->dat = st;
	str.push_back(newp);
    }

    long add_unique_string(const string &st) {
	long i;
	
	for(i=(long)str.size()-1;i>=0;i--)
	    if(str[i]->dat==st)
		return i;

	add_string(st);
	    
	return (long)str.size()-1;
    }

    void add_string(const char *st) {
	if(st != NULL) {
	    pstringst *newp = new pstringst;
	    newp->dat = st;
	    str.push_back(newp);
	}
    }

    void insert_string(long k, const string &st) {
	pstringst *newp = new pstringst;
	newp->dat = st;
	if (str.size() - k > 0)
	    str.insert(k, newp);
	else 
	    str.push_back(newp);
    }

    ~stringvectst() {
	clean();
    }

    void clean() {
	while(str.size() > 0) {
	    delete str[0];
	    str.erase(0);
	}
    }

    void read_file(file_compressorst &filecomp, long /* loadversion */) {
	long dummy;
	filecomp.read_file(dummy);
	str.resize(dummy);

	long s;
	for(s=0;s<dummy;s++) {
	    str[s]=new pstringst;
	    filecomp.read_file(str[s]->dat);
	}
    }
    
    void write_file(file_compressorst &filecomp) {
	long dummy = str.size();
	filecomp.write_file(dummy);

	long s;
	for(s=0;s<dummy;s++) 
	    filecomp.write_file(str[s]->dat);
    }

    void copy_from(stringvectst &src) {
	clean();

	str.resize(src.str.size());

	long s;
	for(s=(long)src.str.size()-1;s>=0;s--) {
	    str[s] = new pstringst;
	    str[s]->dat = src.str[s]->dat;
	}
    }

    bool has_string(const string &st) {
	long i;
	for(i=(long)str.size()-1;i>=0;i--)
	    if(str[i]->dat == st)
		return true;
	return false;
    }

    void remove_string(const string &st) {
	for(size_t i = 0; i < str.size(); i++)
	    if(str[i]->dat == st) {
		delete str[i];
		str.erase(i);
	    }
    }

    void operator=(stringvectst &two);
};
#endif
