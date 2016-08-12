#ifndef __tealtree__FEATURE_METADATA__
#define __tealtree__FEATURE_METADATA__

#include <stdio.h>
#include <string>

#include "cereal.h"
#include "registry.h"
#include "types.h"
#include "util.h"

class AbstractFeatureMetadataImpl
{
public:
    static Registry<AbstractFeatureMetadataImpl> registry;
    static std::unique_ptr<AbstractFeatureMetadataImpl> create(const std::string & class_name)
    {
        return registry.create(class_name);
    }
    virtual const std::string get_registry_name() = 0;

    virtual std::string value_to_string(const FeatureValue & value)const = 0;
    virtual FeatureValue string_to_value(const std::string & s) const = 0;
    virtual bool is_greater_or_equal(const FeatureValue & a, const FeatureValue &b) const = 0;
};

class FeatureMetadata
{
private:
    std::string name;
    std::unique_ptr<AbstractFeatureMetadataImpl> impl;
public:
    FeatureMetadata() :
        name(""),
        impl(std::unique_ptr<AbstractFeatureMetadataImpl>(nullptr))
    {}

    FeatureMetadata(FeatureMetadata && other) :
        name(std::move(other.name)),
        impl(std::move(other.impl))
    {}

    FeatureMetadata & operator=(FeatureMetadata & other)
    {
        name = std::move(other.name);
        impl = std::move(other.impl);
        return *this;
    }

    const std::string & get_name() const
    {
        return name;
    }

    void set_name(std::string & name)
    {
        this->name = name;
    }

    void set_impl(std::unique_ptr<AbstractFeatureMetadataImpl> impl)
    {
        this->impl = std::move(impl);
    }

    std::string get_type() const 
    {
return impl->get_registry_name();
    }

    template<class Archive>
    void save (Archive & archive) const
    {
        std::string type = this->get_type();
        archive(
            CEREAL_NVP(name),
            CEREAL_NVP(type));
    }

    template<class Archive>
    void load (Archive & archive)
    {
        std::string type;
        archive(
            CEREAL_NVP(name),
            CEREAL_NVP(type));
        impl = std::unique_ptr<AbstractFeatureMetadataImpl>(AbstractFeatureMetadataImpl::create(type));
    }

    std::string value_to_string(const FeatureValue & value)const 
    {
        return impl->value_to_string(value);
    }
    FeatureValue string_to_value(const std::string & s) const
    {
        return impl->string_to_value(s);
    }
    bool is_greater_or_equal(const FeatureValue & a, const FeatureValue &b) const
    {
        return impl->is_greater_or_equal(a, b);
    }
};

template<typename T>
class  FeatureMetadataImpl : public AbstractFeatureMetadataImpl
{
public:
    virtual const std::string get_registry_name()
    {
        return to_string(get_feature_type_from_template<T>());
    }

    virtual std::string value_to_string(const FeatureValue & value) const
    {
        const T * typed_value = reinterpret_cast<const T*>(&value);
        return to_string<T>(*typed_value);
    }
    virtual FeatureValue string_to_value(const std::string & s) const
    {
        FeatureValue result;
        T * typed_result = reinterpret_cast<T*>(&result);
        *typed_result = parse_string<T>(s.c_str());;
        return result;
    }

    virtual bool is_greater_or_equal(const FeatureValue & a, const FeatureValue &b) const
    {
        const T * typed_a = reinterpret_cast<const T*>(&a);
        const T * typed_b = reinterpret_cast<const T*>(&b);
        return (*typed_a) >= (*typed_b);
    }
};

#endif /* defined(__tealtree__FEATURE_METADATA__) */
