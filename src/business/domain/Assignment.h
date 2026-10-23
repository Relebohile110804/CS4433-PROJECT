#pragma once
#include <string>
#include <vector>

#include "business/domain/Time.h"

namespace lms::domain {

// An individual extension for one student on one assignment (FR-09, BR-03).
class Extension {
public:
    Extension(int id, int studentId, TimePoint newDeadline, std::string reason, int grantedBy, TimePoint grantedAt);

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }
    int getStudentId() const { return studentId_; }
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

    // The extension deadline if the student has one, otherwise the assignment deadline (BR-03).
    TimePoint effectiveDeadlineFor(int studentId) const;
    // Adds an extension after checking BR-03: a reason is given and the new
    // deadline is later than the student's current effective deadline.
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
