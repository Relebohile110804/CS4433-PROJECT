#pragma once
// The whole program as the school will run it: a real SQLite database file, the real
// submissions folder with real files copied into it, and the real menus. Only the clock
// is fixed (so lateness can be tested) and, unless a test asks for Argon2id, the password
// hasher is a fast fake.
#include "catch2/catch_amalgamated.hpp"

#include <atomic>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <streambuf>

#include "../TestHelpers.h"
#include "../fakes/Fakes.h"
#include "business/services/AdminService.h"
#include "business/services/AssignmentService.h"
#include "business/services/AuditService.h"
#include "business/services/AuthService.h"
#include "business/services/MarkingService.h"
#include "business/services/ModerationService.h"
#include "business/services/SubmissionService.h"
#include "business/services/TenPercentPerDayPolicy.h"
#include "data/Database.h"
#include "data/FileErrorLog.h"
#include "data/LocalFileStore.h"
#include "data/SodiumPasswordHasher.h"
#include "data/SqliteAssignmentRepository.h"
#include "data/SqliteAuditRepository.h"
#include "data/SqliteCourseRepository.h"
#include "data/SqliteSubmissionRepository.h"
#include "data/SqliteUserRepository.h"
#include "presentation/App.h"
#include "presentation/ConsoleIO.h"

namespace systest {

namespace fs = std::filesystem;
using namespace lms;
using namespace lms::domain;
using testutil::hoursAfter;

inline bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

inline int countOf(const std::string& text, const std::string& part) {
    int n = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++n;
    return n;
}

// The screen text with wrapped lines joined again, for checking long text such as a
// Windows file path, which the 80-column wrap (NFR-11) may split.
inline std::string unwrapped(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && text.compare(i + 1, 4, "    ") == 0) {
            i += 4;
            while (i + 1 < text.size() && text[i + 1] == ' ') ++i;
            continue;
        }
        out += text[i];
    }
    return out;
}

inline std::string when(TimePoint t) { return presentation::ConsoleIO::formatTime(t); }

// A folder of its own for each test, removed at the end. It is the first member of the
// fixture, so it is removed only after the database file has been closed (Windows, D-08).
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        path = fs::temp_directory_path() /
               ("lms_system_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                std::to_string(counter++));
        fs::create_directories(path / "work");
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

// Feeds typed lines to the program one at a time and, like a terminal, echoes each line
// where it was typed. With echoText false only the line break is echoed, so the transcript
// holds the program's own output (used for the 80-column check, NFR-11).
class TypedInput : public std::streambuf {
public:
    TypedInput(const std::string& input, std::ostream& out, bool echoText) : out_(out), echoText_(echoText) {
        std::istringstream lines(input);
        for (std::string line; std::getline(lines, line);) lines_.push_back(line + "\n");
    }

protected:
    int_type underflow() override {
        if (lines_.empty()) return traits_type::eof();
        current_ = lines_.front();
        lines_.pop_front();
        out_ << (echoText_ ? current_ : "\n");
        setg(current_.data(), current_.data(), current_.data() + current_.size());
        return traits_type::to_int_type(current_[0]);
    }

private:
    std::ostream& out_;
    bool echoText_;
    std::deque<std::string> lines_;
    std::string current_;
};

struct SystemFixture {
    TempDir dir;
    data::Database db{(dir.path / "lms.db").string()};
    data::SqliteUserRepository users{db};
    data::SqliteCourseRepository courses{db};
    data::SqliteAssignmentRepository assignmentRepo{db};
    data::SqliteSubmissionRepository submissionRepo{db};
    data::SqliteAuditRepository auditRepo{db};
    fakes::FakePasswordHasher fakeHasher;
    data::SodiumPasswordHasher realHasher;
    ports::IPasswordHasher& hasher;
    data::LocalFileStore files{(dir.path / "submissions").string()};
    fakes::FakeClock clock{testutil::base()};
    data::FileErrorLog errorLog{(dir.path / "lms-errors.log").string(), clock};
    services::TenPercentPerDayPolicy policy;
    services::AuthService auth{users, hasher, clock};
    services::AdminService admin{users, courses, hasher, db};
    services::AssignmentService assignments{assignmentRepo, courses, clock};
    services::AuditService audit{auditRepo, courses, clock};
    services::SubmissionService submitting{submissionRepo, assignmentRepo, courses, files, policy, clock, db};
    services::MarkingService marking{submissionRepo, assignmentRepo, courses, policy, audit, clock, db};
    services::ModerationService moderation{submissionRepo, assignmentRepo, courses, audit, clock, db};

    // Filled in by seed(): two departments, each with its HoD, a lecturer and a course.
    int adminId = 0, hod = 0, hod2 = 0, lect = 0, lect2 = 0, ann = 0, ben = 0, cat = 0;
    int cs = 0, maths = 0, cs4433 = 0, ma1000 = 0, project = 0, homework = 0;
    TimePoint due = hoursAfter(48);
    std::string allOutput;  // everything the program printed in this test

    explicit SystemFixture(bool realHashing = false) : hasher(realHashing ? static_cast<ports::IPasswordHasher&>(realHasher) : fakeHasher) {}

    std::string run(const std::string& input, bool echoText = true) {
        std::ostringstream out;
        TypedInput typed(input, out, echoText);
        std::istream in(&typed);
        presentation::ConsoleIO io(in, out);
        presentation::App app({auth, admin, assignments, submitting, marking, moderation, audit}, io, &errorLog);
        app.run();
        allOutput += out.str();
        return out.str();
    }

    Session as(const std::string& username) {
        return auth.login(username, username == "admin" ? "admin-pass" : "password1");
    }

    void seed(const std::string& csTitle = "Software Engineering", const std::string& projectTitle = "Project",
              const std::string& hodName = "Mrs Head") {
        adminId = admin.createFirstAdministrator("admin", "System Admin", "S001", "admin-pass");
        Session s = as("admin");
        hod = admin.createUser(s, Role::HeadOfDepartment, "hod", hodName, "S100", "password1");
        hod2 = admin.createUser(s, Role::HeadOfDepartment, "hod2", "Mr Maths", "S101", "password1");
        lect = admin.createUser(s, Role::Lecturer, "lect", "Mr Teacher", "S200", "password1");
        lect2 = admin.createUser(s, Role::Lecturer, "lect2", "Ms Numbers", "S201", "password1");
        ann = admin.createUser(s, Role::Student, "ann", "Ann Student", "2023001", "password1");
        ben = admin.createUser(s, Role::Student, "ben", "Ben Student", "2023002", "password1");
        cat = admin.createUser(s, Role::Student, "cat", "Cat Student", "2023003", "password1");
        cs = admin.createDepartment(s, "Computer Science", hod);
        maths = admin.createDepartment(s, "Mathematics", hod2);
        cs4433 = admin.createCourse(s, cs, "CS4433", csTitle);
        ma1000 = admin.createCourse(s, maths, "MA1000", "Calculus");
        admin.assignLecturer(s, cs4433, lect);
        admin.assignLecturer(s, ma1000, lect2);
        for (int student : {ann, ben, cat}) admin.enrolStudent(s, cs4433, student);
        admin.enrolStudent(s, ma1000, ann);
        project = assignments.createAssignment(as("lect"), cs4433, projectTitle, "Final project", due, 100);
        homework = assignments.createAssignment(as("lect2"), ma1000, "Homework", "Exercises", hoursAfter(72), 50);
    }

    // A real file in the work folder, as a student would have on a USB drive.
    std::string file(const std::string& name, std::uintmax_t bytes = 2000) {
        fs::path p = dir.path / "work" / name;
        std::ofstream(p, std::ios::binary) << std::string(static_cast<std::size_t>(std::min<std::uintmax_t>(bytes, 4096)), 'x');
        if (bytes > 4096) fs::resize_file(p, bytes);
        return p.string();
    }

    std::string username(int studentId) { return studentId == ann ? "ann" : studentId == ben ? "ben" : "cat"; }

    // Service-level set-up steps, used to reach the state a test starts from.
    int submitAt(double hour, int student, int assignment = 0) {
        clock.set(hoursAfter(hour));
        submitting.submit(as(username(student)), assignment ? assignment : project,
                          file(username(student) + std::to_string(static_cast<int>(hour)) + ".pdf"));
        return submission(student, assignment).getId();
    }
    int markedAt(double hour, int student, int raw, const std::string& feedback = "Good work") {
        int id = submitAt(hour, student);
        marking.mark(as("lect"), id, raw, feedback, "");
        return id;
    }
    Submission submission(int student, int assignment = 0) {
        return *submissionRepo.find(assignment ? assignment : project, student);
    }
    std::string errorLogText() {
        std::ifstream in(dir.path / "lms-errors.log");
        return std::string(std::istreambuf_iterator<char>(in), {});
    }
    std::size_t storedFiles() {
        std::size_t n = 0;
        for (const auto& e : fs::directory_iterator(dir.path / "submissions"))
            if (e.is_regular_file()) ++n;
        return n;
    }
};

}  // namespace systest
