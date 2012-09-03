#ifndef BOUNDED_CLIENT__
#define BOUNDED_CLIENT__

#include "bounded/manager.hh"

#include <boost/noncopyable.hpp>

namespace bounded {

class Client : boost::noncopyable {
  public:
    virtual ~Client();

    const Config &GetConfig() const { return manager_.GetConfig(); }

  protected:
    explicit Client(Manager &manager);

    Manager &manager_;
};

/* This will not register/deregister with the manager class.  The reason is
 * that the manager can call Spill at any time while registered, so the class
 * must be fully constructed to handle these calls.  
 * Therefore, the inheriting class should call AddSpiller and RemoveSpiller
 * itself.  
 */
class SpillClient : public Client {
  public:
    virtual ~SpillClient();

    virtual std::size_t SpillableSize() = 0;

    // Spill.  Return when done.  
    virtual void Spill() = 0;

  protected:
    explicit SpillClient(Manager &manager);
};

} // namespace bounded

#endif // BOUNDED_CLIENT__
