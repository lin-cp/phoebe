#ifndef ELPHPLOTAPP_H
#define ELPHPLOTAPP_H

#include <string>
#include "app.h"

/** Main driver for the transport calculation
 */
class ElPhCouplingPlotApp: public App {
public:
  void run(Context &context);
  void checkRequirements(Context &context);
};

#endif
