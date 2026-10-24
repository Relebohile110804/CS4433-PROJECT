#pragma once
#include <string>
#include <vector>

#include "business/domain/Assignment.h"
#include "business/domain/Session.h"

namespace lms::services {

// UC-08, UC-14 and UC-15.
class IAssignmentService {
public:
    virtual ~IAssignmentService() = default;

    // FR-08: a lecturer sets an assignment for a course they teach.
    virtual int createAssignment(const domain::Session& s, int courseId, const std::string& title,
                                 const std::string& description, domain::TimePoint deadline, int maxMark) = 0;
    // FR-09, BR-03: a lecturer gives one student a later deadline, with a reason.
    virtual void grantExtension(const domain::Session& s, int assignmentId, int studentId,
                                domain::TimePoint newDeadline, const std::string& reason) = 0;
    // The student's own deadline: the latest extension if there is one, otherwise the assignment deadline.
    virtual domain::TimePoint effectiveDeadline(int assignmentId, int studentId) = 0;
    // FR-10: the assignments in every course the student is enrolled in.
    virtual std::vector<domain::Assignment> listForStudent(const domain::Session& s) = 0;
    // The assignments of one course, for its lecturers, its HoD or its students.
    virtual std::vector<domain::Assignment> listForCourse(const domain::Session& s, int courseId) = 0;
};

}  // namespace lms::services
