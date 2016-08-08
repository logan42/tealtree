#include <algorithm>

#include "trainer_data.h"
#include "tree.h"

Tree::Tree(TrainerData * data, bool debug_info)
{
    this->debug_info = debug_info;
    this->nodes.push_back(std::unique_ptr<TreeNode>(new TreeNode()));
    this->nodes[0]->node_id = 0;
    std::vector<DOC_ID> & doc_ids = this->nodes[0]->doc_ids;
    doc_ids.reserve(data->documents.size());
    for (size_t i = 0; i < data->documents.size(); i++) {
        doc_ids.push_back(data->documents[i].doc_id);
    }
    if (debug_info) {
        this->nodes[0]->debug_info = std::unique_ptr<TreeNodeDebugInfo>(new TreeNodeDebugInfo());
    }
}

TreeNode * Tree::get_root()
{
    return this->nodes[0].get();
}

std::vector<std::unique_ptr<TreeNode>> & Tree::get_nodes()
{
    return this->nodes;
}


std::pair<TreeNode*, TreeNode*> Tree::split_node(TreeNode * node, SplitSignature * split_signature)
{
    std::vector<std::unique_ptr<TreeNode>> & nodes = this->nodes;
#ifndef NDEBUG
    //assert that node belongs to this tree
    bool found = false;
    std::for_each(nodes.begin(), nodes.end(),
                  [&](std::unique_ptr<TreeNode> &uptr)
                  {
                      if (uptr.get() == node) {
                          found = true;
                      }
                  });
    assert(found);
#endif
    assert(split_signature->size() == node->doc_ids.size() );
    nodes.push_back(std::unique_ptr<TreeNode>(new TreeNode()));
    nodes.push_back(std::unique_ptr<TreeNode>(new TreeNode()));
    TreeNode & left =  *nodes[nodes.size() - 2];
    TreeNode & right = *nodes[nodes.size() - 1];
    size_t left_index = nodes.size() - 2;
    left.node_id = (uint32_t)left_index;
    right.node_id = (uint32_t)left_index + 1;
    node->left = &left;
    node->right = &right;
    left.parent = node;
    right.parent = node;
    if (this->debug_info) {
        left.debug_info = std::unique_ptr<TreeNodeDebugInfo>(new TreeNodeDebugInfo());
        right.debug_info = std::unique_ptr<TreeNodeDebugInfo>(new TreeNodeDebugInfo());
    }

    auto it = split_signature->iterator();
    for (DOC_ID i = 0; i < split_signature->size(); i++) {
        uint8_t bit = it.next();
        nodes[left_index + bit]->doc_ids.push_back(node->doc_ids[i]);
    }
    return std::make_pair(&left, &right);
}



TreeLite::TreeLite(Tree & tree, BucketsProvider * buckets_provider)
{
    std::vector<std::unique_ptr<TreeNode>> & tn = tree.get_nodes();
    this->nodes.resize(tn.size());
    for (size_t i = 0; i < tn.size(); i++) {
        TreeNodeLite tnl(*tn[i], buckets_provider);;
        this->nodes[i] = tnl;
    }
}
/*
TreeLite::TreeLite(const TreeLite & other)
{
    this->nodes = other.nodes;
}
*/  

TreeLite::TreeLite(TreeLite && other)
{
    this->nodes = other.nodes;
}


const std::vector<TreeNodeLite> & TreeLite::get_nodes() const
{
    return this->nodes;
}
