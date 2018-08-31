#ifndef AUTOMATION_ANDCONSTRAINT_H
#define AUTOMATION_ANDCONSTRAINT_H

#include "CompositeConstraint.h"

namespace automation {

  class AndConstraint : public CompositeConstraint {
  public:
    AndConstraint(const vector<Constraint *> &constraints) :
        CompositeConstraint("AND", constraints) {
    }

    bool checkValue() override {
      for (Constraint *pConstraint : constraints) {
        if (!pConstraint->test())
          return false;
      }
      return true;
    }
  };
}

#endif