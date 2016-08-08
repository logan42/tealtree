#ifndef __tealtree__REGISTRY__
#define __tealtree__REGISTRY__

#include <boost/function.hpp>
#include <map>
#include <stdio.h>

#include "types.h"

template <typename T>
T* registry_creator()
{
    return new T();
}


// This class was inspired by:
// http://stackoverflow.com/questions/1096700/instantiate-class-from-name
template<class T>
class Registry
{
    typedef boost::function0<T *> Creator;
    typedef std::map<std::string, Creator> Creators;
        Creators _creators;

public:    
    Registry()
    {
    }
    ~Registry() {}
    void register_class(const std::string &className, const Creator &creator)
    {
        _creators[className] = creator;
    }

    template <class TT>
    void register_class()
    {
        this->register_class(TT().get_registry_name(), registry_creator<TT>);
    }


    std::unique_ptr<T> create(const std::string &className)
    {
        if (_creators.find(className) == _creators.end()) {
            throw std::runtime_error("Cannot instantiate class '" + className + "': no such class in this registry.");
        }
        return std::unique_ptr<T>(_creators[className]());
    }
};


template <class B, class T>
class StaticRegistree
{
public:
    StaticRegistree()
    {
        //B::registry.register_class<T>();
        B::registry.register_class(T().get_registry_name(), registry_creator<T>);
    }
};

#endif /* defined(__tealtree__REGISTRY__) */
