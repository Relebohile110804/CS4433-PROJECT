#pragma once
#include <optional>
#include <vector>

#include "business/domain/Assignment.h"

namespace lms::repositories {

class IAssignmentRepository {
public:
    virtual ~IAssignmentRepository() = default;
    // The assignment with all of its extensions.
    virtual std::optional<domain::Assignment> findById(int id) = 0;
    virtual std::vector<domain::Assignment> forCourse(int courseId) = 0;
    virtual int add(const domain::Assignment& assignment) = 0;
    virtual int addExtension(int assignmentId, const domain::Extension& extension) = 0;
};

}  // namespace lms::repositories
