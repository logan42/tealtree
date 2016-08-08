#ifndef tree_io_hpp
#define tree_io_hpp

#include <cereal/archives/json.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "buckets_collection.h"
#include "trainer.h"
#include "tree.h"

CEREAL_REGISTER_TYPE(FeatureMetadataImpl<uint8_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<int8_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<uint16_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<int16_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<uint32_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<int32_t>);
CEREAL_REGISTER_TYPE(FeatureMetadataImpl<float_t>);



class TreeWriter
{
private:
    std::unique_ptr<std::ofstream> out;
    std::unique_ptr<cereal::JSONOutputArchive> archive;
    bool metadata_written, closed, first_tree;
    std::unique_ptr<Ensemble> ensemble;
public:
    TreeWriter(const std::string & filename)
    {
        this->out = std::unique_ptr<std::ofstream>(new std::ofstream(filename));
        this->archive = std::unique_ptr<cereal::JSONOutputArchive>(new cereal::JSONOutputArchive(*out));
        this->metadata_written = false; this->closed = false;
        this->first_tree = true;
        this->ensemble = std::unique_ptr<Ensemble>(new Ensemble());
    }

    ~TreeWriter()
    {
        if (!this->closed) {
            this->close();
        }
    }

    void add_feature(FeatureMetadata feature)
    {
        this->ensemble->add_feature(std::move(feature));
    }

    void add_tree(TreeLite & tree)
    {
        this->ensemble->add_tree(tree);
    }

    void set_cost_function(const std::string & cf)
    {
        this->ensemble->set_cost_function(cf);
    }

    void write_metadata(Trainer * trainer);
    void write_tree(TreeLite & tree, uint32_t index);
    
    void close()
    {
        assert(!this->closed);
        this->closed = true;
        Ensemble & ensemble = *(this->ensemble);
        (*this->archive)(CEREAL_NVP(ensemble));
        this->archive.reset();
        this->out.reset();
    }
};

inline std::unique_ptr<Ensemble> load_ensemble(const std::string & filename)
{
    std::unique_ptr<Ensemble> result(new Ensemble());
    std::ifstream fin(filename);
    cereal::JSONInputArchive archive(fin);
        archive(*result);
    return result;
}

#endif /* tree_io_hpp */
