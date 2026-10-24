#include "doctest/doctest.h"

#include "ServiceFixture.h"
#include "business/domain/Errors.h"

using namespace lms::domain;
using lms::services::SubmissionService;
using testutil::hoursAfter;

TEST_CASE("FR-11: an on-time submission stores the file and version 1") {
    ServiceFixture f;
    f.clock.set(hoursAfter(10));
    SubmissionVersion v = f.submitting.submit(f.as(f.student), f.assignment, "work.pdf");
    CHECK(v.getVersionNo() == 1);
    CHECK(v.getSubmittedAt() == hoursAfter(10));
    CHECK(v.getSha256().size() == 64);
    CHECK(f.files.isStored(v.getStoredPath()));

    auto sub = f.submissions.find(f.assignment, f.student);
    REQUIRE(sub);
    CHECK(sub->getStatus() == SubmissionStatus::Submitted);
    CHECK(sub->getDaysLate() == 0);
    CHECK(f.tx.committed == 1);
}

TEST_CASE("FR-12, BR-04: resubmitting before the deadline adds a version and keeps the old one") {
    ServiceFixture f;
    f.submitAt(10);
    f.clock.set(hoursAfter(47));
    SubmissionVersion v2 = f.submitting.submit(f.as(f.student), f.assignment, "work.pdf");
    CHECK(v2.getVersionNo() == 2);
    auto sub = f.submissions.find(f.assignment, f.student);
    CHECK(sub->getVersions().size() == 2);
    CHECK(f.files.storedCount() == 2);
}

TEST_CASE("FR-13, BR-02: late work is accepted once and marked Late") {
    ServiceFixture f;
    f.clock.set(hoursAfter(50));  // 2 hours late
    CHECK(f.submitting.previewDaysLate(f.as(f.student), f.assignment) == 1);
    f.submitting.submit(f.as(f.student), f.assignment, "work.pdf");
    auto sub = f.submissions.find(f.assignment, f.student);
    CHECK(sub->getStatus() == SubmissionStatus::Late);
    CHECK(sub->getDaysLate() == 1);

    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), f.assignment, "work.pdf"),
                      "A late submission cannot be replaced (BR-02)");
    CHECK(f.files.storedCount() == 1);  // the refused copy was removed again
}

TEST_CASE("BR-02: on-time work cannot be replaced after the deadline") {
    ServiceFixture f;
    f.submitAt(10);
    f.clock.set(hoursAfter(49));
    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), f.assignment, "work.pdf"),
                      "The deadline has passed, so the version already submitted stands (BR-02)");
}

TEST_CASE("BR-03: an extension moves the student's deadline for lateness") {
    ServiceFixture f;
    f.assignments.addExtension(f.assignment, Extension(0, f.student, hoursAfter(96), "Sick", f.lecturer, hoursAfter(1)));
    f.clock.set(hoursAfter(90));
    CHECK(f.submitting.previewDaysLate(f.as(f.student), f.assignment) == 0);
    CHECK(f.submitting.previewDaysLate(f.as(f.student2), f.assignment) == 2);
}

TEST_CASE("UC-09: file checks") {
    ServiceFixture f;
    Session s = f.as(f.student);
    f.clock.set(hoursAfter(10));
    CHECK_THROWS_WITH(f.submitting.submit(s, f.assignment, "missing.pdf"),
                      "The file cannot be found or opened: missing.pdf");
    f.files.addSource("photo.jpg", 100);
    CHECK_THROWS_WITH(f.submitting.submit(s, f.assignment, "photo.jpg"),
                      "Only .pdf, .docx, .txt, .zip, .cpp and .h files can be submitted");
    f.files.addSource("big.zip", SubmissionService::MaxFileSize + 1);
    CHECK_THROWS_WITH(f.submitting.submit(s, f.assignment, "big.zip"), "The file is larger than 10 MB");
    f.files.addSource("exact.zip", SubmissionService::MaxFileSize);
    CHECK_NOTHROW(f.submitting.submit(s, f.assignment, "exact.zip"));

    CHECK(SubmissionService::isAllowedFileType("C:\\Work\\Report.PDF"));
    CHECK(SubmissionService::isAllowedFileType("main.cpp"));
    CHECK(SubmissionService::isAllowedFileType("lms.h"));
    CHECK_FALSE(SubmissionService::isAllowedFileType("notes.doc"));
    CHECK_FALSE(SubmissionService::isAllowedFileType("folder.pdf/readme"));
    CHECK_FALSE(SubmissionService::isAllowedFileType("README"));
}

TEST_CASE("UC-09 E4: if saving fails, the copied file is removed and nothing is committed") {
    ServiceFixture f;
    f.clock.set(hoursAfter(10));
    f.submissions.failNextSave = true;
    CHECK_THROWS(f.submitting.submit(f.as(f.student), f.assignment, "work.pdf"));
    CHECK(f.files.storedCount() == 0);
    CHECK(f.tx.committed == 0);
    CHECK(f.tx.rolledBack == 1);
    CHECK_FALSE(f.submissions.find(f.assignment, f.student));
}

TEST_CASE("BR-05: only enrolled students can submit") {
    ServiceFixture f;
    int outsider = f.addUser(Role::Student, "out", "2023999");
    CHECK_THROWS_AS(f.submitting.submit(f.as(outsider), f.assignment, "work.pdf"), AccessDenied);
    CHECK_THROWS_AS(f.submitting.submit(f.as(f.lecturer), f.assignment, "work.pdf"), AccessDenied);
    CHECK_THROWS_WITH(f.submitting.submit(f.as(f.student), 999, "work.pdf"), "Assignment not found");
}

TEST_CASE("FR-18, BR-05: the student sees the mark only after release") {
    ServiceFixture f;
    int id = f.markedSubmission(70);
    Session s = f.as(f.student);

    auto own = f.submitting.findOwn(s, f.assignment);
    REQUIRE(own);
    CHECK(own->getStatus() == SubmissionStatus::Marked);
    CHECK_FALSE(own->getMark());  // hidden before release
    CHECK_THROWS_WITH(f.submitting.viewReleasedMark(s, f.assignment), "Your mark has not been released yet");

    f.moderation.approve(f.as(f.hod), id);
    f.moderation.release(f.as(f.hod), f.assignment);
    Mark m = f.submitting.viewReleasedMark(s, f.assignment);
    CHECK(m.getRawMark() == 70);
    CHECK(m.getFinalMark() == doctest::Approx(70));
    CHECK(m.getFeedback() == "Good work");
    CHECK(f.submitting.findOwn(s, f.assignment)->getMark());

    CHECK_THROWS_WITH(f.submitting.viewReleasedMark(f.as(f.student2), f.assignment),
                      "You have not submitted this assignment");
    CHECK_FALSE(f.submitting.findOwn(f.as(f.student2), f.assignment));
}
