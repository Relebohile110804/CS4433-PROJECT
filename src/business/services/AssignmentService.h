#pragma once
#include "business/ports/IClock.h"
#include "business/repositories/IAssignmentRepository.h"
#include "business/repositories/ICourseRepository.h"
#include "business/services/IAssignmentService.h"

namespace lms::services {

class AssignmentService : public IAssignmentService {
public:
    AssignmentService(repositories::IAssignmentRepository& assignments, repositories::ICourseRepository& courses,
                      ports::IClock& clock)
        : assignments_(assignments), courses_(courses), clock_(clock) {}

    int createAssignment(const domain::Session& s, int courseId, const std::string& title,
                         const std::string& description, domain::TimePoint deadline, int maxMark) override;
    void grantExtension(const domain::Session& s, int assignmentId, int studentId, domain::TimePoint newDeadline,
                        const std::string& reason) override;
    void grantClassExtension(const domain::Session& s, int assignmentId, domain::TimePoint newDeadline,
                             const std::string& reason) override;
    domain::TimePoint effectiveDeadline(int assignmentId, int studentId) override;
    std::vector<domain::Assignment> listForStudent(const domain::Session& s) override;
    std::vector<domain::Course> myCourses(const domain::Session& s) override;
    std::vector<domain::Assignment> listForCourse(const domain::Session& s, int courseId) override;

private:
    domain::Assignment existingAssignment(int assignmentId);
    void requireTeaches(const domain::Session& s, int courseId);
    void addExtension(const domain::Session& s, domain::Assignment& a, int studentId, domain::TimePoint newDeadline,
                      const std::string& reason);

    repositories::IAssignmentRepository& assignments_;
    repositories::ICourseRepository& courses_;
    ports::IClock& clock_;
};

}  // namespace lms::services
