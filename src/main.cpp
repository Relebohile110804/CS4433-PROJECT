// Moderated LMS - composition root.
// Creates the concrete Data Access classes, passes them into the services and starts the menus.
#include <exception>
#include <iostream>

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
#include "data/SystemClock.h"
#include "data/Toolchain.h"
#include "presentation/App.h"

#ifndef LMS_VERSION
#define LMS_VERSION "development build"
#endif

int main() {
    using namespace lms;
    std::cout << "Moderated LMS, version " << LMS_VERSION << "\n";
    if (!data::initSodium()) {
        std::cerr << "Could not start the password library (libsodium).\n";
        return 1;
    }
    try {
        // Data Access and External
        data::Database db("lms.db");
        data::SqliteUserRepository users(db);
        data::SqliteCourseRepository courses(db);
        data::SqliteAssignmentRepository assignments(db);
        data::SqliteSubmissionRepository submissions(db);
        data::SqliteAuditRepository auditLog(db);
        data::SodiumPasswordHasher hasher;
        data::LocalFileStore files("submissions");
        data::SystemClock clock;
        data::FileErrorLog errorLog("lms-errors.log", clock);

        // Business Logic
        services::TenPercentPerDayPolicy policy;
        services::AuthService auth(users, hasher, clock);
        services::AdminService admin(users, courses, hasher, db);
        services::AssignmentService assignmentService(assignments, courses, clock);
        services::AuditService audit(auditLog, courses, clock);
        services::SubmissionService submitting(submissions, assignments, courses, files, policy, clock, db);
        services::MarkingService marking(submissions, assignments, courses, policy, audit, clock, db);
        services::ModerationService moderation(submissions, assignments, courses, audit, clock, db);

        // Presentation
        presentation::ConsoleIO io(std::cin, std::cout, presentation::terminalEcho);  // passwords not shown (R13)
        presentation::App app({auth, admin, assignmentService, submitting, marking, moderation, audit}, io, &errorLog);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "The program had to stop: " << e.what() << "\n";
        // Keep the window open when the program was started by double-clicking it.
        std::cerr << "Press Enter to close this window.\n";
        std::cin.clear();
        std::cin.get();
        return 1;
    }
    return 0;
}
