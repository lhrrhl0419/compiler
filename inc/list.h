#pragma once

#include <list>

template <typename T>
class List : public std::list<T>
{
public:
    virtual void merge(List<T>&& other)
    {
        this->splice(this->end(), other);
    };
    virtual void merge(List<T>& other)
    {
        this->splice(this->end(), other);
    };
};