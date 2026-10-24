#include "business/services/AssignmentService.h"

#include "business/domain/Errors.h"

namespace lms::services {

using namespace lms::domain;

Assignment AssignmentService::existingAssignment(int assignmentId) {
    std::optional<Assignment> a = assignments_.findById(assignmentId);
    if (!a) throw RuleViolation("Assignment not found");
    return *a;
}

// BR-05: a lecturer may only work on the courses they teach.
void AssignmentService::requireTeaches(const Session& s, int courseId) {
    if (!courses_.isTeaching(s.getUser().getId(), courseId))
        throw AccessDenied("You do not teach this course");
}

int AssignmentService::createAssignment(const Session& s, int courseId, const std::string& title,
                                        const std::string& description, TimePoint deadline, int maxMark) {
    s.require(Permission::CreateAssignment);
    if (!courses_.findCourse(courseId)) throw RuleViolation("Course not found");
    requireTeaches(s, courseId);
    Assignment a(0, courseId, title, description, deadline, maxMark);  // checks title and max mark
    if (deadline <= clock_.now()) throw RuleViolation("The deadline must be in the future");
    return assignments_.add(a);
}

void AssignmentService::grantExtension(const Session& s, int assignmentId, int studentId, TimePoint newDeadline,
                                       const std::string& reason) {
    s.require(Permission::GrantExtension);
    Assignment a = existingAssignment(assignmentId);
    requireTeaches(s, a.getCourseId());
    if (!courses_.isEnrolled(studentId, a.getCourseId()))
        throw RuleViolation("That student is not enrolled in this course");

    TimePoint now = clock_.now();
    Extension e(0, studentId, newDeadline, reason, s.getUser().getId(), now);  // checks the reason
    a.addExtension(e, now);  // checks the new deadline (BR-03)
    assignments_.addExtension(assignmentId, e);
}

TimePoint AssignmentService::effectiveDeadline(int assignmentId, int studentId) {
    return existingAssignment(assignmentId).effectiveDeadlineFor(studentId);
}

std::vector<Assignment> AssignmentService::listForStudent(const Session& s) {
    s.require(Permission::SubmitWork);
    std::vector<Assignment> result;
    for (const Course& c : courses_.coursesForStudent(s.getUser().getId()))
        for (Assignment& a : assignments_.forCourse(c.getId())) result.push_back(std::move(a));
    return result;
}

std::vector<Course> AssignmentService::myCourses(const Session& s) {
    int me = s.getUser().getId();
    switch (s.getUser().getRole()) {
        case Role::Lecturer: return courses_.coursesForLecturer(me);
        case Role::Student: return courses_.coursesForStudent(me);
        case Role::HeadOfDepartment: {
            std::optional<Department> d = courses_.departmentOfHod(me);
            return d ? courses_.coursesInDepartment(d->getId()) : std::vector<Course>{};
        }
        case Role::Administrator: return courses_.listCourses();
    }
    return {};
}

std::vector<Assignment> AssignmentService::listForCourse(const Session& s, int courseId) {
    if (!courses_.findCourse(courseId)) throw RuleViolation("Course not found");
    int me = s.getUser().getId();
    bool allowed = false;
    switch (s.getUser().getRole()) {
        case Role::Lecturer: allowed = courses_.isTeaching(me, courseId); break;
        case Role::Student: allowed = courses_.isEnrolled(me, courseId); break;
        case Role::HeadOfDepartment: {
            std::optional<Department> d = courses_.departmentOfHod(me);
            allowed = d && d->getId() == courses_.departmentOf(courseId);
            break;
        }
        case Role::Administrator: allowed = true; break;  // read-only, for the audit log filter
    }
    if (!allowed) throw AccessDenied("You cannot see the assignments of this course");
    return assignments_.forCourse(courseId);
}

}  // namespace lms::services
