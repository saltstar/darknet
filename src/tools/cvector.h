
#ifndef __CVECTOR_H__
#define __CVECTOR_H__

#define MIN_LEN 256
#define CVEFAILED -1
#define CVESUCCESS 0
#define CVEPUSHBACK 1
#define CVEPOPBACK 2
#define CVEINSERT 3
#define CVERM 4
#define EXPANED_VAL 1
#define REDUSED_VAL 2

typedef void *citerator; 
typedef struct _cvector *cvector; 
    
EXTERN_ cvector   cvector_create(const size_t size); 
EXTERN_ void      cvector_destroy(const cvector cv); 
EXTERN_ size_t    cvector_length(const cvector cv); 
EXTERN_ int       cvector_pushback(const cvector cv, void *memb); 
EXTERN_ int       cvector_popback(const cvector cv, void *memb); 
EXTERN_ size_t    cvector_iter_at(const cvector cv, citerator iter); 
EXTERN_ int       cvector_iter_val(const cvector cv, citerator iter, void *memb); 
EXTERN_ citerator cvector_begin(const cvector cv); 
EXTERN_ citerator cvector_end(const cvector cv); 
EXTERN_ citerator cvector_next(const cvector cv, citerator iter); 
EXTERN_ int       cvector_val_at(const cvector cv, size_t index, void *memb); 
EXTERN_ int       cvector_insert(const cvector cv, citerator iter, void *memb); 
EXTERN_ int       cvector_insert_at(const cvector cv, size_t index, void *memb ); 
EXTERN_ int       cvector_rm(const cvector cv, citerator iter); 
EXTERN_ int       cvector_rm_at(const cvector cv, size_t index);  
	    
//for test
EXTERN_ void   cv_info(const cvector cv); 
EXTERN_ void   cv_print(const cvector cv); 

#endif
