#include <vector>
#include <set>
#include <algorithm>
#include "optional.h"

template<typename T>
class WorkList {
    std::vector<T> stack;
    std::set<T> set;
public:
    bool push(T elem);
    std::experimental::optional<T> pop();
};

template<typename T>
bool WorkList<T>::push(T elem) {
    auto ret = this->set.insert(elem);
    if (*ret.first) {
        this->stack.push_back(elem);
        return true;
    } else {
        return false;
    }
}

template<typename T>
std::experimental::optional<T> WorkList<T>::pop() {
    if (this->stack.empty()) {
        return std::experimental::nullopt;
    }
    auto ret = this->stack.back();
    this->stack.pop_back();
    return std::experimental::optional<T>(ret);
}

template<typename T>
class OrdWorkList {
    std::vector<T> stack;
    std::vector<T> set;
public:
    bool push(T elem);
    std::experimental::optional<T> pop();
    std::vector<T> getSet();
};

template<typename T>
bool OrdWorkList<T>::push(T elem) {
    if (std::find(this->set.begin(), this->set.end(), elem) == set.end()) {
        this->set.push_back(elem);
        this->stack.push_back(elem);
        return true;
    } else {
        return false;
    }
}

template<typename T>
std::experimental::optional<T> OrdWorkList<T>::pop() {
    if (this->stack.empty()) {
        return std::experimental::nullopt;
    }
    auto ret = this->stack.back();
    this->stack.pop_back();
    return std::experimental::optional<T>(ret);
}

template<typename T>
std::vector<T> OrdWorkList<T>::getSet() {
    return this->set;
}
