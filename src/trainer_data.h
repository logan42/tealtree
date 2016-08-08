#ifndef __tealtree__trainer_data__
#define __tealtree__trainer_data__

#include <stdio.h>
#include <vector>

#include "document.h"
#include "tree_node.h"
#include "tree.h"

struct TrainerData {
    std::vector<Document> documents;
    std::vector<DOC_ID> query_limits;

    // This vector contains all the doc_ids sorted by their label within each query.
    // For ranking only
    std::vector<DOC_ID> sorted_doc_ids;
    std::vector<DOC_ID> ranks;
    std::vector<float_t> IDCGs;

    std::unique_ptr<Tree> current_tree;
};

#endif /* defined(__tealtree__trainer_data__) */
