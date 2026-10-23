#pragma once
#include <string>

#include "business/domain/Errors.h"

namespace lms::domain {

// A department and its one HoD (FR-05). The "heads" association is kept as the HoD's user id.
class Department {
public:
    Department(int id, std::string name, int hodId) : id_(id), name_(std::move(name)), hodId_(hodId) {
        if (isBlank(name_)) throw RuleViolation("Department name cannot be empty");
    }

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) {
        if (isBlank(name)) throw RuleViolation("Department name cannot be empty");
        name_ = name;
    }
    int getHodId() const { return hodId_; }
    void setHodId(int hodId) { hodId_ = hodId; }

private:
    int id_;
    std::string name_;
    int hodId_;
};

}  // namespace lms::domain
