#include "ScheduleVertex.h"

#include <iostream>

int ScheduleVertex::compare(const ScheduleVertex& v1, const ScheduleVertex& v2) {
  // std::cout << "--V " << v1.name << ", " << v2.name << std::endl;
  //      return v1.type.compare(v2.type);
  if (v1.name == v2.name) {
    if (v1.type == v2.type) {
      if ("block" == v1.type || "blockalign" == v1.type) {
        if (v1.tperiod == v2.tperiod) {
          if (v1.qlo == v2.qlo) {
            if (v1.qhi == v2.qhi) {
              return v1.qil.compare(v2.qil);
            } else {
              return v1.qhi.compare(v2.qhi);
            }
          } else {
            return v1.qlo.compare(v2.qlo);
          }
        } else {
          return v1.tperiod.compare(v2.tperiod);
        }
      } else if ("flow" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("flush" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("listdst" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("noop" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("qbuf" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("qinfo" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("switch" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("tmsg" == v1.type) {
        return v1.x.compare(v2.x);
      } else if ("wait" == v1.type) {
        return v1.x.compare(v2.x);
      } else {
        return -1;
      }
    } else {
      return v1.type.compare(v2.type);
    }
  } else {
    return v1.name.compare(v2.name);
  }
}

int ScheduleVertex::compare(const ScheduleVertex& v2) {
  std::cout << "--> " << this->name << ", " << v2.name << std::endl;
  //      return this->type.compare(v2.type);
  if (this->name == v2.name) {
    return this->type.compare(v2.type);
  } else {
    return this->name.compare(v2.name);
  }
}
