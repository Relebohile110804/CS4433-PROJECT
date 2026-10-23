#pragma once
#include <optional>
#include <vector>

#include "business/domain/Submission.h"

namespace lms::repositories {

class ISubmissionRepository {
public:
    virtual ~ISubmissionRepository() = default;
    // The submission with all of its versions and its mark, if any.
    virtual std::optional<domain::Submission> findById(int id) = 0;
    virtual std::optional<domain::Submission> find(int assignmentId, int studentId) = 0;
    virtual std::vector<domain::Submission> forAssignment(int assignmentId) = 0;
    virtual std::vector<domain::Submission> forStudent(int studentId) = 0;
    // Inserts a new submission (id 0) or updates an existing one, including new
    // versions and the mark. Returns the submission id.
    virtual int save(const domain::Submission& submission) = 0;
};

}  // namespace lms::repositories
