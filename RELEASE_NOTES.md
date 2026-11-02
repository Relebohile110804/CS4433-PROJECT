# Moderated LMS release notes

## v1.0.0 (November 2026): first release to the school

A console program for one school computer that lets lecturers set and mark assignments, students hand in work and see their marks, and the Head of Department moderate every mark before students can see it. It runs fully offline.

### What it does
- **Accounts and security:** four roles (Administrator, Lecturer, Head of Department, Student). Passwords are stored as Argon2id hashes and are not shown while typed. Accounts lock for 15 minutes after 5 wrong passwords.
- **Administration:** users, departments, courses, lecturers and enrolment; backup and restore of the database.
- **Assignments:** deadlines in Lesotho time, extensions for single students, and a list of each student's own assignments and status.
- **Submissions:** .pdf .docx .txt .zip .cpp .h files up to 10 MB, with a receipt and a SHA-256 file check. Resubmission before the deadline keeps every version. Late work is accepted with a 10% per day penalty for up to 3 days, then 0.
- **Marking and moderation:** lecturers mark with feedback; the Head of Department approves, adjusts or returns each mark and then releases them together. Students see only released marks.
- **Audit log:** every mark change, with the old and new value, who made it, when and why. Entries cannot be changed or deleted.

### Changes since v0.9.0
- Passwords are no longer shown while they are typed (risk R13, review finding CR-03).
- New Windows package LMS-1.0.0-win64.zip with lms.exe and the DLLs it needs, so nothing else has to be installed.
- The program shows its version when it starts, and waits for Enter if it has to stop because of an error.

### Quality
206 automated tests (unit, integration, system and acceptance) pass on Windows and Linux; business-layer line coverage is 98%. cppcheck and clang-tidy report no findings, and no function has a cyclomatic complexity above 10.

### Known limitations
- **Backups:** Backup and restore saves the database only. The handed-in files are in the submissions folder, which must be copied to the backup drive as well (installation guide, Section 3.4).
- **One computer:** the program and its data are on one computer and are used by one person at a time.
- **Files are chosen by typing their full path;** there is no file browser in a console program.
- **Time zone:** Lesotho time (UTC+2) is fixed in the program, which is correct because Lesotho has no daylight saving.
- **Usability trial:** the NFR-10 trial with five school staff has not been run yet (risk R14).
- **Linux:** there is no ready-made Linux package; build from the source with the linux-debug preset.

### Install
See the installation guide (Week Seven report, Section 3) and README.txt inside the zip.
