#include "business/domain/Assignment.h"

#include "business/domain/Errors.h"

namespace lms::domain {

Extension::Extension(int id, int studentId, TimePoint newDeadline, std::string reason, int grantedBy,
                     TimePoint grantedAt)
    : id_(id),
      studentId_(studentId),
      newDeadline_(newDeadline),
      reason_(std::move(reason)),
      grantedBy_(grantedBy),
      grantedAt_(grantedAt) {
    if (isBlank(reason_)) throw RuleViolation("An extension needs a reason");
}

Assignment::Assignment(int id, int courseId, std::string title, std::string description, TimePoint deadline,
                       int maxMark)
    : id_(id),
      courseId_(courseId),
      title_(std::move(title)),
      description_(std::move(description)),
      deadline_(deadline),
      maxMark_(maxMark) {
    if (isBlank(title_)) throw RuleViolation("Assignment title cannot be empty");
    if (maxMark_ < MinMaxMark || maxMark_ > MaxMaxMark)
        throw RuleViolation("Maximum mark must be a whole number from 1 to 1000");
}

TimePoint Assignment::effectiveDeadlineFor(int studentId) const {
    const Extension* e = extensionFor(studentId);
    return e ? e->getNewDeadline() : deadline_;
}

TimePoint Assignment::classDeadline() const { return effectiveDeadlineFor(Extension::WholeClass); }

const Extension* Assignment::extensionFor(int studentId) const {
    const Extension* latest = nullptr;
    TimePoint best = deadline_;
    for (const Extension& e : extensions_) {
        bool applies = e.isWholeClass() || e.getStudentId() == studentId;
        if (applies && e.getNewDeadline() > best) {
            best = e.getNewDeadline();
            latest = &e;
        }
    }
    return latest;
}

void Assignment::addExtension(const Extension& e, TimePoint now) {
    if (e.isWholeClass()) {
        if (e.getNewDeadline() <= classDeadline())
            throw RuleViolation("The new deadline must be later than the current deadline of the class");
    } else if (e.getNewDeadline() <= effectiveDeadlineFor(e.getStudentId())) {
        throw RuleViolation("The new deadline must be later than the student's current deadline");
    }
    if (e.getNewDeadline() <= now) throw RuleViolation("The new deadline must be in the future");
    extensions_.push_back(e);
}

}  // namespace lms::domain
