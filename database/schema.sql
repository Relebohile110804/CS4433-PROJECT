PRAGMA foreign_keys = ON;

CREATE TABLE users (
    user_id        INTEGER PRIMARY KEY,
    username       TEXT    NOT NULL UNIQUE,
    password_hash  TEXT    NOT NULL,
    full_name      TEXT    NOT NULL,
    role           TEXT    NOT NULL
                   CHECK (role IN ('ADMIN', 'LECTURER', 'HOD', 'STUDENT')),
    person_number  TEXT    NOT NULL UNIQUE,
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    failed_logins  INTEGER NOT NULL DEFAULT 0 CHECK (failed_logins >= 0),
    locked_until   TEXT
);

CREATE TABLE departments (
    department_id  INTEGER PRIMARY KEY,
    name           TEXT    NOT NULL UNIQUE,
    hod_id         INTEGER NOT NULL UNIQUE REFERENCES users (user_id)
);

CREATE TABLE courses (
    course_id      INTEGER PRIMARY KEY,
    department_id  INTEGER NOT NULL REFERENCES departments (department_id),
    code           TEXT    NOT NULL UNIQUE,
    title          TEXT    NOT NULL
);

CREATE TABLE course_lecturers (
    course_id      INTEGER NOT NULL REFERENCES courses (course_id),
    lecturer_id    INTEGER NOT NULL REFERENCES users (user_id),
    PRIMARY KEY (course_id, lecturer_id)
);

CREATE TABLE enrolments (
    course_id      INTEGER NOT NULL REFERENCES courses (course_id),
    student_id     INTEGER NOT NULL REFERENCES users (user_id),
    enrolled_at    TEXT    NOT NULL,
    PRIMARY KEY (course_id, student_id)
);

CREATE TABLE assignments (
    assignment_id  INTEGER PRIMARY KEY,
    course_id      INTEGER NOT NULL REFERENCES courses (course_id),
    title          TEXT    NOT NULL,
    description    TEXT    NOT NULL DEFAULT '',
    deadline       TEXT    NOT NULL,
    max_mark       INTEGER NOT NULL CHECK (max_mark BETWEEN 1 AND 1000)
);

CREATE TABLE extensions (
    extension_id   INTEGER PRIMARY KEY,
    assignment_id  INTEGER NOT NULL REFERENCES assignments (assignment_id),
    student_id     INTEGER NOT NULL REFERENCES users (user_id),
    new_deadline   TEXT    NOT NULL,
    reason         TEXT    NOT NULL CHECK (length(trim(reason)) > 0),
    granted_by     INTEGER NOT NULL REFERENCES users (user_id),
    granted_at     TEXT    NOT NULL
);

CREATE TABLE submissions (
    submission_id  INTEGER PRIMARY KEY,
    assignment_id  INTEGER NOT NULL REFERENCES assignments (assignment_id),
    student_id     INTEGER NOT NULL REFERENCES users (user_id),
    status         TEXT    NOT NULL
                   CHECK (status IN ('SUBMITTED', 'LATE', 'MARKED', 'RETURNED',
                                     'MODERATED', 'RELEASED')),
    days_late      INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
    moderated_by   INTEGER REFERENCES users (user_id),
    moderated_at   TEXT,
    released_at    TEXT,
    hod_comment    TEXT,
    UNIQUE (assignment_id, student_id)
);

CREATE TABLE submission_versions (
    submission_id  INTEGER NOT NULL REFERENCES submissions (submission_id),
    version_no     INTEGER NOT NULL CHECK (version_no >= 1),
    stored_path    TEXT    NOT NULL UNIQUE,
    sha256         TEXT    NOT NULL CHECK (length(sha256) = 64),
    submitted_at   TEXT    NOT NULL,
    PRIMARY KEY (submission_id, version_no)
);

CREATE TABLE marks (
    submission_id  INTEGER PRIMARY KEY REFERENCES submissions (submission_id),
    raw_mark       INTEGER NOT NULL CHECK (raw_mark >= 0),
    penalty        REAL    NOT NULL CHECK (penalty >= 0),
    final_mark     REAL    NOT NULL CHECK (final_mark >= 0),
    feedback       TEXT    NOT NULL CHECK (length(trim(feedback)) > 0),
    marked_by      INTEGER NOT NULL REFERENCES users (user_id),
    marked_at      TEXT    NOT NULL
);

CREATE TABLE audit_log (
    audit_id       INTEGER PRIMARY KEY,
    submission_id  INTEGER NOT NULL REFERENCES submissions (submission_id),
    changed_by     INTEGER NOT NULL REFERENCES users (user_id),
    changed_at     TEXT    NOT NULL,
    old_value      REAL,
    new_value      REAL    NOT NULL,
    reason         TEXT    NOT NULL CHECK (length(trim(reason)) > 0)
);

-- Audit entries can be added but never changed or removed (BR-07)
CREATE TRIGGER audit_log_no_update BEFORE UPDATE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be changed'); END;

CREATE TRIGGER audit_log_no_delete BEFORE DELETE ON audit_log
BEGIN SELECT RAISE(ABORT, 'audit entries cannot be deleted'); END;

-- Indexes for the list screens (NFR-02)
CREATE INDEX idx_courses_department   ON courses (department_id);
CREATE INDEX idx_enrolments_student   ON enrolments (student_id);
CREATE INDEX idx_assignments_course   ON assignments (course_id);
CREATE INDEX idx_extensions_lookup    ON extensions (assignment_id, student_id);
CREATE INDEX idx_submissions_student  ON submissions (student_id);
CREATE INDEX idx_audit_submission     ON audit_log (submission_id);

-- Upgrades: everything below this line can be run again on an existing database without harm
-- (IF NOT EXISTS). The program runs it every time it opens lms.db and after a restore, so a
-- database made by an older version is brought up to date by itself.

-- v1.1.0, CR1: an extension for every student of the course on one assignment (BR-03)
CREATE TABLE IF NOT EXISTS class_extensions (
    class_extension_id  INTEGER PRIMARY KEY,
    assignment_id       INTEGER NOT NULL REFERENCES assignments (assignment_id),
    new_deadline        TEXT    NOT NULL,
    reason              TEXT    NOT NULL CHECK (length(trim(reason)) > 0),
    granted_by          INTEGER NOT NULL REFERENCES users (user_id),
    granted_at          TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_class_extensions_assignment ON class_extensions (assignment_id);
