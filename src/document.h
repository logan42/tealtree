#ifndef __tealtree__document__
#define __tealtree__document__

#include <stdio.h>

#include "types.h"

struct Document
{
    DOC_ID doc_id; // do we even need it here?
    DOC_ID query_id;
    float_t target_score; // this is also known as label
    float_t score;
    float_t gradient;
    float_t hessian;
    
    Document() : doc_id(0), query_id(0), target_score(0), score(0), gradient(0) {};
};

#endif /* defined(__tealtree__document__) */
