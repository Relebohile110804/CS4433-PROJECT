#pragma once
#include <memory>

namespace lms::repositories {

// A unit of work. If commit() is not called, the destructor rolls back,
// so an exception part-way through a service operation leaves nothing half saved (NFR-08).
class ITransaction {
public:
    virtual ~ITransaction() = default;
    virtual void commit() = 0;
};

class ITransactionManager {
public:
    virtual ~ITransactionManager() = default;
    virtual std::unique_ptr<ITransaction> begin() = 0;
};

}  // namespace lms::repositories
