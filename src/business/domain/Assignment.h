#pragma once
#include <string>
#include <vector>

#include "business/domain/Time.h"

namespace lms::domain {

// An extension on one assignment, for one student or for the whole class (FR-09, BR-03, CR1).
class Extension {
public:
    // Student id used for an extension that applies to every student of the course (CR1).
    // Real user ids start at 1, so 0 never belongs to a student.
    static constexpr int WholeClass = 0;

    Extension(int id, int studentId, TimePoint newDeadline, std::string reason, int grantedBy, TimePoint grantedAt);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    int getStudentId() const { return studentId_; }
    bool isWholeClass() const { return studentId_ == WholeClass; }
    TimePoint getNewDeadline() const { return newDeadline_; }
    const std::string& getReason() const { return reason_; }
    int getGrantedBy() const { return grantedBy_; }
    TimePoint getGrantedAt() const { return grantedAt_; }

private:
    int id_;
    int studentId_;
    TimePoint newDeadline_;
    std::string reason_;
    int grantedBy_;
    TimePoint grantedAt_;
};

// An assignment set for a course (FR-08). It owns its extensions (composition in Figure 2).
class Assignment {
public:
    static constexpr int MinMaxMark = 1;
    static constexpr int MaxMaxMark = 1000;

    Assignment(int id, int courseId, std::string title, std::string description, TimePoint deadline, int maxMark);

    // The latest of the assignment deadline, every whole-class extension and the
    // student's own extensions (BR-03).
    TimePoint effectiveDeadlineFor(int studentId) const;
    // The deadline for the whole class: the assignment deadline or the latest whole-class extension.
    TimePoint classDeadline() const;
    // The extension that sets the student's effective deadline, or nullptr if they have none (FR-10).
    const Extension* extensionFor(int studentId) const;
    // Adds an extension after checking BR-03: a reason is given, the new deadline is in the
    // future and later than the current deadline of that student (or of the class).
    void addExtension(const Extension& e, TimePoint now);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    int getCourseId() const { return courseId_; }
    const std::string& getTitle() const { return title_; }
    const std::string& getDescription() const { return description_; }
    TimePoint getDeadline() const { return deadline_; }
    int getMaxMark() const { return maxMark_; }
    const std::vector<Extension>& getExtensions() const { return extensions_; }
    // Used by the repository to load extensions that were already checked when granted.
    void restoreExtension(const Extension& e) { extensions_.push_back(e); }

private:
    int id_;
    int courseId_;
    std::string title_;
    std::string description_;
    TimePoint deadline_;
    int maxMark_;
    std::vector<Extension> extensions_;
};

}  // namespace lms::domain
