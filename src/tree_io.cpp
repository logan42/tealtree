#include "tree_io.h"

/*
Json::Value feature_to_json(Feature * feature)
{
    Json::Value result;
    result["name"] = feature->get_name();
    if (feature->get_buckets() != NULL) {
        result["type"] = feature_type_to_string(feature->get_buckets()->get_type());
    }
    return result;
}

std::unique_ptr<Json::Value> metadata_to_json(Trainer * trainer)
{
    std::unique_ptr<Json::Value> result(new Json::Value());
    Json::Value features;
    for (size_t i = 0; i < trainer->get_features_count(); i++) {
        features.append(feature_to_json(trainer->get_feature(i)));
    }
    (*result)["features"] = features;
    return result;
}


TreeWriter::TreeWriter(const std::string & filename, std::unique_ptr<BucketsProvider> buckets_provider)
{
    this->out = std::move(std::unique_ptr<std::ofstream>(new std::ofstream(filename)));
    this->buckets_provider = std::move(buckets_provider);
    this->metadata_written = false;this->closed = false;
    this->first_tree = true;
    (*this->out) << "{" << std::endl;
}

TreeWriter::~TreeWriter()
{
    if (!this->closed) {
        this->close();
    }
}

void TreeWriter::write_metadata(Trainer * trainer)
{
    assert(this->metadata_written == false);
    this->metadata_written = true;
    assert(!this->closed);
    (*this->out) << "\"metadata\" :" << std::endl;
    (*this->out) << *metadata_to_json(trainer) << std::endl;
    
    (*this->out) << std::endl << ",\"trees\" : [" << std::endl;
    this->out->flush();
}

void TreeWriter::write_tree(TreeLite & tree, uint32_t index)
{
    assert(!this->closed);
    assert(this->metadata_written);
    (*this->out) << std::endl;
    if (!this->first_tree) {
        (*this->out) << "," << std::endl;
    }
    this->first_tree = false;
    (*this->out) << (*this->tree_to_json(tree, index)) << std::endl;
    this->out->flush();
}

void TreeWriter::close()
{
    assert(!this->closed);
    //assert(this->metadata_written);
    (*this->out) << "]" << std::endl;
    (*this->out) << "}" << std::endl;
    this->out->close();
    this->closed = true;
}

std::unique_ptr<Json::Value> TreeWriter::tree_to_json(TreeLite & tree, uint32_t index)
{
    std::unique_ptr<Json::Value> t(new Json::Value());
    (*t)["index"] = index;
    for(size_t i = 0; i < tree.get_nodes().size(); i++) {
        (*t)["nodes"].append((*this->node_to_json(tree.get_nodes()[i])));
    }
    return t;
}

std::unique_ptr<Json::Value> TreeWriter::node_to_json(const TreeNodeLite & node)
{
    std::unique_ptr<Json::Value> n(new Json::Value());
    if (node.left_id == 0) {
        (*n)["value"] = node.value;
    } else {
        (*n)["left_id"] = node.left_id;
        (*n)["right_id"] = node.right_id;
        (*n)["feature"] = node.split.feature;
        (*n)["threshold"] = this->buckets_provider->get_buckets(node.split.feature)->get_bucket_as_string(node.split.threshold);
        (*n)["inverse"] = node.split.inverse;
    }
    if (node.debug_info.get() != nullptr) {
        (*n)["debug_info"] = *node.debug_info;
    }
    return n;
}
*/
